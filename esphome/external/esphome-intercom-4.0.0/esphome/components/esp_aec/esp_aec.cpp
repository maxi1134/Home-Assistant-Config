#include "esp_aec.h"

#ifdef USE_ESP32

#include <cstring>

#include "esp_heap_caps.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp_aec {

static const char *const TAG = "esp_aec";

// HIGH_PERF AEC modes (SR_HIGH_PERF, VOIP_HIGH_PERF) rely on FFT cos/sin
// windows tables that esp-sr allocates from DMA-capable internal RAM via
// raw calloc. Upstream fails silently on low memory: aec_create returns
// a non-null but half-initialized handle (log "failed to calloc fft
// cos/sin windows table"), and the next process() dereferences the NULL
// table → LoadProhibited in fft_esp_proc. Guard the switch with a
// largest-free-block threshold; reject mode change early when the
// contiguous DMA-capable block is too small. The threshold scales with
// filter_length because esp-sr's FFT scratch and dios_ssp tail buffer grow
// linearly with the number of taps (40 KB validated on S3 codec devices
// running filter_length=4; doubling filter_length doubles the requirement).
static constexpr size_t HIGH_PERF_MIN_DMA_BLOCK_PER_4_TAPS = 40 * 1024;

static size_t high_perf_min_dma_block_(int filter_length) {
  int multiplier = filter_length / 4;
  if (multiplier < 1) multiplier = 1;
  return HIGH_PERF_MIN_DMA_BLOCK_PER_4_TAPS * static_cast<size_t>(multiplier);
}

static bool is_high_perf_mode_(aec_mode_t m) {
  return m == AEC_MODE_SR_HIGH_PERF || static_cast<int>(m) == 4;  // 4 = VOIP_HIGH_PERF
}

const char *EspAec::get_mode_name(aec_mode_t mode) {
  switch (static_cast<int>(mode)) {
    case AEC_MODE_SR_LOW_COST: return "sr_low_cost";
    case AEC_MODE_SR_HIGH_PERF: return "sr_high_perf";
    case 3: return "voip_low_cost";
    case 4: return "voip_high_perf";
    default: return "sr_low_cost";
  }
}

void EspAec::setup() {
  this->handle_mutex_ = xSemaphoreCreateMutex();
  if (this->handle_mutex_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create handle_mutex_");
    this->mark_failed();
    return;
  }
  this->handle_ = aec_create(this->sample_rate_, this->filter_length_, 1, this->mode_);
  if (this->handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create AEC instance");
    vSemaphoreDelete(this->handle_mutex_);
    this->handle_mutex_ = nullptr;
    this->mark_failed();
    return;
  }
  this->cached_frame_size_ = aec_get_chunksize(this->handle_);
}

EspAec::~EspAec() {
  if (this->handle_mutex_ != nullptr) {
    xSemaphoreTake(this->handle_mutex_, portMAX_DELAY);
  }
  if (this->handle_ != nullptr) {
    aec_destroy(this->handle_);
    this->handle_ = nullptr;
  }
  if (this->handle_mutex_ != nullptr) {
    xSemaphoreGive(this->handle_mutex_);
    vSemaphoreDelete(this->handle_mutex_);
    this->handle_mutex_ = nullptr;
  }
}

void EspAec::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP AEC (ESP-SR standalone):");
  ESP_LOGCONFIG(TAG, "  Sample Rate: %d Hz", this->sample_rate_);
  ESP_LOGCONFIG(TAG, "  Filter Length: %d", this->filter_length_);
  ESP_LOGCONFIG(TAG, "  Frame Size: %d samples", this->cached_frame_size_);
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->get_mode_name());
  ESP_LOGCONFIG(TAG, "  Initialized: %s", this->is_initialized() ? "YES" : "NO");
}

FrameSpec EspAec::frame_spec() const {
  FrameSpec spec;
  spec.sample_rate = this->sample_rate_;
  spec.mic_channels = 1;
  spec.ref_channels = 1;
  spec.input_samples = this->cached_frame_size_;
  spec.output_samples = this->cached_frame_size_;
  return spec;
}

bool EspAec::process(const int16_t *in_mic, const int16_t *in_ref, int16_t *out,
                     uint8_t mic_channels_in) {
  // EspAec is single-mic only, mic_channels_in parameter accepted for interface compatibility
  // L1 fix: serialize against reinit_() so handle_ cannot be destroyed mid-process.
  if (this->handle_mutex_ == nullptr ||
      xSemaphoreTake(this->handle_mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
    memcpy(out, in_mic, this->cached_frame_size_ * sizeof(int16_t));
    this->glitch_count_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  if (this->handle_ == nullptr) {
    memcpy(out, in_mic, this->cached_frame_size_ * sizeof(int16_t));
    this->glitch_count_.fetch_add(1, std::memory_order_relaxed);
    xSemaphoreGive(this->handle_mutex_);
    return false;
  }
  aec_process(this->handle_,
              const_cast<int16_t *>(in_mic),
              const_cast<int16_t *>(in_ref),
              out);
  this->frame_count_.fetch_add(1, std::memory_order_relaxed);
  xSemaphoreGive(this->handle_mutex_);
  return true;
}

FeatureControl EspAec::feature_control(AudioFeature feature) const {
  if (feature == AudioFeature::AEC)
    return FeatureControl::RESTART_REQUIRED;  // reinit needed for mode change
  return FeatureControl::NOT_SUPPORTED;
}

bool EspAec::set_feature(AudioFeature feature, bool enabled) {
  // Standalone AEC has no per-feature toggles. Use reconfigure() for mode changes.
  return false;
}

ProcessorTelemetry EspAec::telemetry() const {
  ProcessorTelemetry t;
  t.frame_count = this->frame_count_.load(std::memory_order_relaxed);
  t.glitch_count = this->glitch_count_.load(std::memory_order_relaxed);
  return t;
}

bool EspAec::reconfigure(int type, int mode) {
  aec_mode_t new_mode;
  if (type == 0) {  // SR
    new_mode = (mode == 1) ? AEC_MODE_SR_HIGH_PERF : AEC_MODE_SR_LOW_COST;
  } else {  // VC
    new_mode = (mode == 1) ? static_cast<aec_mode_t>(4) : static_cast<aec_mode_t>(3);
  }
  return this->reinit_(new_mode);
}

bool EspAec::reinit_(aec_mode_t new_mode) {
  ESP_LOGI(TAG, "Reinitializing AEC: mode %d -> %d (%s -> %s)",
           (int) this->mode_, (int) new_mode, get_mode_name(this->mode_), get_mode_name(new_mode));
  // L1 fix: serialize against process() and preserve rollback capability.
  if (this->handle_mutex_ == nullptr) {
    ESP_LOGE(TAG, "reinit_: handle_mutex_ not initialized");
    return false;
  }

  // Pre-flight guard against esp-sr's silent OOM on HIGH_PERF modes.
  // We must do this BEFORE acquiring the handle mutex / touching the live
  // instance: a rejected switch leaves the audio task running with the
  // previous (working) mode, no frame_spec bump, no audio gap.
  if (is_high_perf_mode_(new_mode)) {
    const size_t threshold = high_perf_min_dma_block_(this->filter_length_);
    size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (largest_internal < threshold) {
      ESP_LOGE(TAG,
               "reinit_: %s with filter_length=%d needs >=%u bytes contiguous "
               "DMA-capable internal RAM (largest free block: %u). Rejecting "
               "mode change to avoid silent fft calloc fail in aec_create. "
               "Keeping mode=%s. Free DRAM first (e.g. CONFIG_ESP_WIFI_IRAM_OPT=n) "
               "to use this mode.",
               get_mode_name(new_mode), this->filter_length_,
               (unsigned) threshold,
               (unsigned) largest_internal,
               get_mode_name(this->mode_));
      return false;
    }
  }

  xSemaphoreTake(this->handle_mutex_, portMAX_DELAY);
  aec_handle_t *old_handle = this->handle_;
  aec_mode_t old_mode = this->mode_;
  // Try to build new handle BEFORE destroying old (rollback if fails).
  aec_handle_t *new_handle = aec_create(this->sample_rate_, this->filter_length_, 1, new_mode);
  if (new_handle == nullptr) {
    ESP_LOGE(TAG, "Failed to create new AEC instance, keeping old (mode=%s)", get_mode_name(old_mode));
    xSemaphoreGive(this->handle_mutex_);
    return false;
  }
  this->handle_ = new_handle;
  this->mode_ = new_mode;
  this->cached_frame_size_ = aec_get_chunksize(new_handle);
  if (old_handle != nullptr) {
    aec_destroy(old_handle);
  }
  if (this->cached_frame_size_ != this->last_frame_size_) {
    this->last_frame_size_ = this->cached_frame_size_;
    this->frame_spec_revision_.fetch_add(1, std::memory_order_release);
  }
  ESP_LOGI(TAG, "AEC reinitialized: mode=%d, frame=%d (%dms)",
           (int) this->mode_, this->cached_frame_size_,
           this->cached_frame_size_ * 1000 / this->sample_rate_);
  xSemaphoreGive(this->handle_mutex_);
  return true;
}

}  // namespace esp_aec
}  // namespace esphome

#endif  // USE_ESP32
