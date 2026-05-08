#include "esp_afe.h"
#include "../audio_processor/audio_utils.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <esp_timer.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <string>

namespace esphome {
namespace esp_afe {

static const char *const TAG = "esp_afe";
static const TickType_t CONFIG_MUTEX_TIMEOUT = pdMS_TO_TICKS(250);
// Max wait for process() to finish its current frame when a config change
// needs to rebuild the AFE instance. ~1.5 frame periods at 16 kHz / 512 samples.
static const TickType_t DRAIN_WAIT_TIMEOUT = pdMS_TO_TICKS(50);

// Validate function pointer using ESP-IDF's memory map knowledge.
static inline bool is_valid_func(const void *ptr) {
  return ptr != nullptr && esp_ptr_executable(ptr);
}

static inline void update_peak_atomic(std::atomic<uint32_t> &peak, uint32_t value) {
  uint32_t current = peak.load(std::memory_order_relaxed);
  while (value > current &&
         !peak.compare_exchange_weak(current, value, std::memory_order_relaxed,
                                     std::memory_order_relaxed)) {
  }
}

static inline void decrement_if_nonzero(std::atomic<uint32_t> &counter) {
  uint32_t current = counter.load(std::memory_order_relaxed);
  while (current > 0 &&
         !counter.compare_exchange_weak(current, current - 1, std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {
  }
}

static inline void copy_passthrough_frame(const int16_t *in, int input_samples,
                                          int mic_channels, int16_t *out, int output_samples) {
  if (out == nullptr || output_samples <= 0) {
    return;
  }
  if (in == nullptr || input_samples <= 0) {
    memset(out, 0, output_samples * sizeof(int16_t));
    return;
  }
  size_t copy_samples = std::min(input_samples, output_samples);
  if (copy_samples > 0) {
    if (mic_channels <= 1) {
      memcpy(out, in, copy_samples * sizeof(int16_t));
    } else {
      for (size_t i = 0; i < copy_samples; i++) {
        out[i] = in[i * mic_channels];
      }
    }
  }
  if (output_samples > static_cast<int>(copy_samples)) {
    memset(out + copy_samples, 0, (output_samples - copy_samples) * sizeof(int16_t));
  }
}

aec_mode_t EspAfe::derive_aec_mode_() const {
  if (this->afe_type_ == AFE_TYPE_SR) {
    return (this->afe_mode_ == AFE_MODE_HIGH_PERF) ? AEC_MODE_SR_HIGH_PERF : AEC_MODE_SR_LOW_COST;
  }
  return (this->afe_mode_ == AFE_MODE_HIGH_PERF) ? AEC_MODE_VOIP_HIGH_PERF : AEC_MODE_VOIP_LOW_COST;
}

int EspAfe::afe_mic_channels_() const {
  if (this->mic_num_ < 2) {
    return 1;
  }
  return this->se_enabled_ ? 2 : 1;
}

const char *EspAfe::memory_alloc_mode_to_str_() const {
  switch (this->memory_alloc_mode_) {
    case AFE_MEMORY_ALLOC_MORE_INTERNAL:
      return "MORE_INTERNAL";
    case AFE_MEMORY_ALLOC_INTERNAL_PSRAM_BALANCE:
      return "INTERNAL_PSRAM_BALANCE";
    case AFE_MEMORY_ALLOC_MORE_PSRAM:
      return "MORE_PSRAM";
    default:
      return "UNKNOWN";
  }
}

bool EspAfe::build_instance_(AfeInstance *instance) {
  if (instance == nullptr) {
    return false;
  }

  const int afe_mic_channels = this->afe_mic_channels_();
  // Stack-allocated format string: "MR" (1 mic) or "MMR" (2 mic). No heap alloc.
  char fmt[4];
  for (int i = 0; i < afe_mic_channels && i < 2; i++) fmt[i] = 'M';
  fmt[afe_mic_channels] = 'R';
  fmt[afe_mic_channels + 1] = '\0';

  afe_config_t *cfg = afe_config_init(fmt, nullptr,
                                      static_cast<afe_type_t>(this->afe_type_),
                                      static_cast<afe_mode_t>(this->afe_mode_));

  if (cfg == nullptr) {
    ESP_LOGW(TAG, "afe_config_init returned NULL, using manual config");
    cfg = afe_config_alloc();
    if (cfg == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate AFE config");
      return false;
    }
    if (!afe_parse_input_format(fmt, &cfg->pcm_config)) {
      ESP_LOGE(TAG, "Failed to parse input format: %s", fmt);
      afe_config_free(cfg);
      return false;
    }
    cfg->pcm_config.sample_rate = 16000;
    cfg->afe_mode = static_cast<afe_mode_t>(this->afe_mode_);
    cfg->afe_type = static_cast<afe_type_t>(this->afe_type_);
  }

  cfg->aec_init = true;  // always init: AEC is LIVE_TOGGLE via vtable
  cfg->aec_filter_length = this->aec_filter_length_;
  cfg->aec_mode = this->derive_aec_mode_();

  cfg->se_init = afe_mic_channels >= 2 && this->se_enabled_;

  cfg->ns_init = this->ns_enabled_;
  cfg->ns_model_name = nullptr;
  cfg->afe_ns_mode = AFE_NS_MODE_WEBRTC;

  cfg->vad_init = this->vad_enabled_;
  cfg->vad_mode = static_cast<vad_mode_t>(this->vad_mode_);
  cfg->vad_model_name = nullptr;
  cfg->vad_min_speech_ms = this->vad_min_speech_ms_;
  cfg->vad_min_noise_ms = this->vad_min_noise_ms_;
  cfg->vad_delay_ms = this->vad_delay_ms_;
  cfg->vad_mute_playback = this->vad_mute_playback_;
  cfg->vad_enable_channel_trigger = this->vad_enable_channel_trigger_;

  cfg->wakenet_init = false;
  cfg->wakenet_model_name = nullptr;
  cfg->wakenet_model_name_2 = nullptr;

  cfg->agc_init = this->agc_enabled_;
  cfg->agc_mode = AFE_AGC_MODE_WEBRTC;
  cfg->agc_compression_gain_db = this->agc_compression_gain_;
  cfg->agc_target_level_dbfs = this->agc_target_level_;

  // esp-sr spawns an internal worker task for BSS/SE using these fields.
  // Inert for sr_low_cost 1-mic (no worker), active for 2-mic BSS and for
  // voip_high_perf / sr_high_perf modes.
  cfg->afe_perferred_core = this->task_core_;
  cfg->afe_perferred_priority = this->task_priority_;
  cfg->afe_ringbuf_size = this->ringbuf_size_;
  cfg->memory_alloc_mode = static_cast<afe_memory_alloc_mode_t>(this->memory_alloc_mode_);
  cfg->afe_linear_gain = this->afe_linear_gain_;
  cfg->debug_init = false;
  // After wake-word fires, channel 0 of fetch result returns raw mic audio
  // (not AEC-processed). Required for VA's STT pipeline to receive clean input.
  cfg->fixed_first_channel = true;

  afe_config_check(cfg);

  const esp_afe_sr_iface_t *handle = esp_afe_handle_from_config(cfg);
  if (handle == nullptr) {
    ESP_LOGE(TAG, "esp_afe_handle_from_config returned NULL");
    afe_config_free(cfg);
    return false;
  }

  esp_afe_sr_data_t *data = handle->create_from_config(cfg);
  if (data == nullptr) {
    ESP_LOGE(TAG, "create_from_config returned NULL (insufficient memory?)");
    afe_config_free(cfg);
    return false;
  }

  int feed_chunksize = handle->get_feed_chunksize(data);
  int fetch_chunksize = handle->get_fetch_chunksize(data);
  int process_chunksize = std::gcd(feed_chunksize, fetch_chunksize);
  if (process_chunksize <= 0) {
    process_chunksize = (fetch_chunksize > 0) ? fetch_chunksize : feed_chunksize;
  }
  // Use official API for feed channel count instead of config struct (more robust
  // if esp-sr changes internal channel mapping in future versions).
  int total_channels = handle->get_feed_channel_num(data);
  if (total_channels <= 0) {
    ESP_LOGW(TAG, "get_feed_channel_num returned %d, falling back to cfg->pcm_config.total_ch_num=%d",
             total_channels, cfg->pcm_config.total_ch_num);
    total_channels = cfg->pcm_config.total_ch_num;  // fallback
  }
  size_t feed_bytes = static_cast<size_t>(feed_chunksize) * total_channels * sizeof(int16_t);

  const uint32_t feed_buf_caps = this->feed_buf_in_psram_ ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL;
  int16_t *feed_buf = static_cast<int16_t *>(heap_caps_aligned_alloc(16, feed_bytes, feed_buf_caps));
  if (feed_buf == nullptr && this->feed_buf_in_psram_) {
    ESP_LOGW(TAG, "feed_buf (%u bytes) fell back to internal RAM (PSRAM full/unavailable)",
             static_cast<unsigned>(feed_bytes));
    feed_buf = static_cast<int16_t *>(heap_caps_aligned_alloc(16, feed_bytes, MALLOC_CAP_INTERNAL));
  }
  if (feed_buf == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate feed buffer (%u bytes)", static_cast<unsigned>(feed_bytes));
    handle->destroy(data);
    afe_config_free(cfg);
    return false;
  }

  instance->handle = handle;
  instance->data = data;
  instance->config = cfg;
  instance->feed_buf = feed_buf;
  instance->feed_chunksize = feed_chunksize;
  instance->fetch_chunksize = fetch_chunksize;
  instance->process_chunksize = process_chunksize;
  instance->total_channels = total_channels;

  return true;
}

void EspAfe::destroy_instance_(AfeInstance *instance) {
  if (instance == nullptr) {
    return;
  }
  if (instance->handle != nullptr && instance->data != nullptr) {
    instance->handle->destroy(instance->data);
    instance->data = nullptr;
  }
  if (instance->config != nullptr) {
    afe_config_free(instance->config);
    instance->config = nullptr;
  }
  if (instance->feed_buf != nullptr) {
    heap_caps_free(instance->feed_buf);
    instance->feed_buf = nullptr;
  }
  instance->handle = nullptr;
  instance->feed_chunksize = 0;
  instance->fetch_chunksize = 0;
  instance->process_chunksize = 0;
  instance->total_channels = 0;
}

bool EspAfe::install_instance_(AfeInstance *instance) {
  this->afe_handle_ = instance->handle;
  this->afe_data_ = instance->data;
  this->afe_config_ = instance->config;
  this->feed_buf_ = instance->feed_buf;
  this->feed_chunksize_ = instance->feed_chunksize;
  this->fetch_chunksize_ = instance->fetch_chunksize;
  this->process_chunksize_ = instance->process_chunksize;
  this->total_channels_ = instance->total_channels;
  this->staged_input_samples_ = 0;

  instance->handle = nullptr;
  instance->data = nullptr;
  instance->config = nullptr;
  instance->feed_buf = nullptr;
  instance->feed_chunksize = 0;
  instance->fetch_chunksize = 0;
  instance->process_chunksize = 0;
  instance->total_channels = 0;

  // fetch_task blocks directly on fetch_with_delay (canonical Espressif
  // pattern) with no semaphore gating from the feed side. Gating the two
  // sides with a counting semaphore deadlocks on any transient feed
  // reject: no signal given -> fetch sleeps -> rb_out never drained ->
  // esp-sr back-pressure keeps rb_in full -> feed keeps rejecting.
  // Startup "Ringbuffer empty" warnings are cosmetic.

  if (!this->start_feed_task_()) {
    ESP_LOGE(TAG, "Installed AFE instance but feed task failed to start");
    return false;
  }

  if (!this->start_fetch_task_()) {
    this->stop_feed_task_();
    ESP_LOGE(TAG, "Installed AFE instance but fetch task failed to start");
    return false;
  }

  // AEC is always initialized (LIVE_TOGGLE). Disable via vtable if config says off.
  if (!this->aec_enabled_) {
    this->afe_handle_->disable_aec(this->afe_data_);
  }

  if (this->afe_handle_->print_pipeline != nullptr) {
    this->afe_handle_->print_pipeline(this->afe_data_);
  }

  return true;
}

EspAfe::AfeInstance EspAfe::detach_instance_() {
  // Drain protocol has already quiesced process() before detach is called,
  // so no more enqueues can race. Stop fetch first, then feed.
  this->stop_fetch_task_();
  this->stop_feed_task_();

  AfeInstance instance;
  instance.handle = this->afe_handle_;
  instance.data = this->afe_data_;
  instance.config = this->afe_config_;
  instance.feed_buf = this->feed_buf_;
  instance.feed_chunksize = this->feed_chunksize_;
  instance.fetch_chunksize = this->fetch_chunksize_;
  instance.process_chunksize = this->process_chunksize_;
  instance.total_channels = this->total_channels_;

  this->afe_handle_ = nullptr;
  this->afe_data_ = nullptr;
  this->afe_config_ = nullptr;
  this->feed_buf_ = nullptr;
  this->feed_chunksize_ = 0;
  this->fetch_chunksize_ = 0;
  this->process_chunksize_ = 0;
  this->total_channels_ = 0;
  this->staged_input_samples_ = 0;

  return instance;
}

bool EspAfe::recreate_instance_(bool require_same_frame_sizes) {
  if (this->config_mutex_ == nullptr) {
    this->config_mutex_ = xSemaphoreCreateMutex();
    if (this->config_mutex_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create config mutex");
      return false;
    }
  }

  if (xSemaphoreTake(this->config_mutex_, CONFIG_MUTEX_TIMEOUT) != pdTRUE) {
    ESP_LOGW(TAG, "Timed out waiting to rebuild AFE instance");
    return false;
  }

  // Drain protocol: signal process() to bail, then wait for any in-flight
  // call to complete. process_busy_ is cleared at the end of process() with
  // release semantics; our acquire load here pairs with that store so we
  // observe a quiesced state before touching the instance. Timeout is a
  // safety net: if i2s_audio_task is stuck elsewhere we proceed anyway to
  // avoid deadlocking the reconfiguration.
  this->drain_request_.store(true, std::memory_order_release);
  TickType_t drain_deadline = xTaskGetTickCount() + DRAIN_WAIT_TIMEOUT;
  while (this->process_busy_.load(std::memory_order_acquire)) {
    if (xTaskGetTickCount() >= drain_deadline) {
      ESP_LOGW(TAG, "Drain timeout waiting for process() to quiesce, proceeding");
      break;
    }
    vTaskDelay(1);
  }

  // esp-sr FFT resources are global: only one AFE instance can exist.
  // Must destroy old before creating new.
  int old_process = this->process_chunksize_;
  int old_fetch = this->fetch_chunksize_;
  AfeInstance old = this->detach_instance_();
  this->destroy_instance_(&old);

  // If every user-facing feature is disabled there is nothing to build.
  // Stay in the torn-down state; process() will passthrough via the
  // afe_stopped_ fast path using the last-known frame spec.
  //
  // Guard: only take this shortcut once we have a cached spec to hand out
  // via frame_spec(). On the very first call (setup() before any successful
  // install) last_spec_* are zero; build the instance normally so downstream
  // learns the frame shape, then subsequent toggles can tear down cleanly.
  if (this->all_features_disabled_() && this->last_spec_process_size_ > 0 &&
      this->last_spec_fetch_size_ > 0) {
    bool was_running = !this->afe_stopped_.load(std::memory_order_acquire);
    this->afe_stopped_.store(true, std::memory_order_release);
    this->drain_request_.store(false, std::memory_order_release);
    xSemaphoreGive(this->config_mutex_);
    if (was_running) {
      ESP_LOGI(TAG, "AFE stopped (all features disabled); audio path in passthrough");
    }
    return true;
  }

  AfeInstance next;
  if (!this->build_instance_(&next)) {
    ESP_LOGE(TAG, "Failed to build new AFE instance. AFE is DOWN until successful rebuild.");
    this->drain_request_.store(false, std::memory_order_release);
    xSemaphoreGive(this->config_mutex_);
    return false;
  }

  if (require_same_frame_sizes && old_process > 0 && old_fetch > 0 &&
      (next.process_chunksize != old_process || next.fetch_chunksize != old_fetch)) {
    ESP_LOGW(TAG, "Reinit changed external frame sizes (%d/%d -> %d/%d), rejecting",
             old_process, old_fetch, next.process_chunksize, next.fetch_chunksize);
    this->destroy_instance_(&next);
    this->drain_request_.store(false, std::memory_order_release);
    xSemaphoreGive(this->config_mutex_);
    return false;
  }

  // Compare against the last successfully-installed spec, not against the
  // (already-detached) `this->*_chunksize_` fields which are zero at this
  // point. Using the last_spec_* members means a rollback to the previous
  // config does not spuriously bump frame_spec_revision_, which would make
  // i2s_audio_duplex try to restart its audio task concurrently with our
  // fetch task recreation and race inside FreeRTOS.
  int new_mic_ch = this->afe_mic_channels_();
  bool spec_changed = (new_mic_ch != this->last_spec_mic_ch_ ||
                       next.process_chunksize != this->last_spec_process_size_ ||
                       next.fetch_chunksize != this->last_spec_fetch_size_);
  (void) old_process;
  (void) old_fetch;

  if (!this->install_instance_(&next)) {
    AfeInstance failed = this->detach_instance_();
    this->destroy_instance_(&failed);
    this->drain_request_.store(false, std::memory_order_release);
    xSemaphoreGive(this->config_mutex_);
    return false;
  }

  this->last_spec_process_size_ = this->process_chunksize_;
  this->last_spec_fetch_size_ = this->fetch_chunksize_;

  if (spec_changed) {
    int old_mic_ch = this->last_spec_mic_ch_;
    this->last_spec_mic_ch_ = new_mic_ch;
    // Release barrier ensures new frame_spec stores happen-before consumers
    // observe the bumped revision via acquire load.
    uint32_t new_rev = this->frame_spec_revision_.fetch_add(1, std::memory_order_release) + 1;
    ESP_LOGI(TAG, "Frame spec changed: mic_ch=%d->%d, process=%d, fetch=%d (revision %u, audio task will restart)",
             old_mic_ch, new_mic_ch, this->process_chunksize_, this->fetch_chunksize_, (unsigned) new_rev);
  }
  this->warmup_remaining_ = 3;
  this->frame_count_.store(0, std::memory_order_relaxed);
  this->glitch_count_.store(0, std::memory_order_relaxed);
  this->input_ring_drop_.store(0, std::memory_order_relaxed);
  this->feed_ok_.store(0, std::memory_order_relaxed);
  this->feed_rejected_.store(0, std::memory_order_relaxed);
  this->fetch_ok_.store(0, std::memory_order_relaxed);
  this->fetch_timeout_.store(0, std::memory_order_relaxed);
  this->output_ring_drop_.store(0, std::memory_order_relaxed);
  this->feed_queue_frames_.store(0, std::memory_order_relaxed);
  this->feed_queue_peak_.store(0, std::memory_order_relaxed);
  this->fetch_queue_frames_.store(0, std::memory_order_relaxed);
  this->fetch_queue_peak_.store(0, std::memory_order_relaxed);
  this->process_us_last_.store(0, std::memory_order_relaxed);
  this->process_us_max_.store(0, std::memory_order_relaxed);
  this->feed_us_last_.store(0, std::memory_order_relaxed);
  this->feed_us_max_.store(0, std::memory_order_relaxed);
  this->fetch_us_last_.store(0, std::memory_order_relaxed);
  this->fetch_us_max_.store(0, std::memory_order_relaxed);
  this->ringbuf_free_pct_.store(1.0f, std::memory_order_relaxed);
  this->voice_present_.store(false, std::memory_order_relaxed);
  this->input_volume_dbfs_.store(-120.0f, std::memory_order_relaxed);
  this->output_rms_dbfs_.store(-120.0f, std::memory_order_relaxed);
  // Clear the stopped flag: a live instance is running. Paired with the
  // acquire load in process() so the hot path sees the transition cleanly.
  bool was_stopped = this->afe_stopped_.exchange(false, std::memory_order_release);
  if (was_stopped) {
    ESP_LOGI(TAG, "AFE restarted (feature re-enabled)");
  }
  // Release drain: new instance is fully installed (feed/fetch tasks started
  // inside install_instance_). process() can resume real work on next frame.
  this->drain_request_.store(false, std::memory_order_release);
  xSemaphoreGive(this->config_mutex_);
  return true;
}

bool EspAfe::set_aec_enabled_runtime_(bool enabled) {
  if (this->aec_enabled_ == enabled) {
    return true;
  }
  if (this->config_mutex_ == nullptr) {
    ESP_LOGW(TAG, "AEC toggle requested before setup");
    return false;
  }

  // AFE currently torn down (all features were off). Flip the flag and let
  // recreate_instance_ rebuild (or stay stopped if still all-off).
  if (this->afe_stopped_.load(std::memory_order_acquire)) {
    this->aec_enabled_ = enabled;
    if (!enabled) {
      return true;  // nothing to rebuild
    }
    return this->recreate_instance_(false);
  }

  if (!this->is_initialized()) {
    ESP_LOGW(TAG, "AEC toggle requested before initialization");
    return false;
  }
  if (xSemaphoreTake(this->config_mutex_, CONFIG_MUTEX_TIMEOUT) != pdTRUE) {
    ESP_LOGW(TAG, "Timed out waiting to toggle AEC");
    return false;
  }

  auto func = enabled ? this->afe_handle_->enable_aec : this->afe_handle_->disable_aec;
  if (!is_valid_func(reinterpret_cast<const void *>(func))) {
    xSemaphoreGive(this->config_mutex_);
    ESP_LOGW(TAG, "Cannot %s AEC: vtable function unavailable (ptr=%p)",
             enabled ? "enable" : "disable", reinterpret_cast<const void *>(func));
    return false;
  }

  int ret = func(this->afe_data_);
  xSemaphoreGive(this->config_mutex_);
  if (ret < 0) {
    ESP_LOGW(TAG, "%s_aec failed (ret=%d)", enabled ? "enable" : "disable", ret);
    return false;
  }

  ESP_LOGI(TAG, "AEC %s (ret=%d)", enabled ? "enabled" : "disabled", ret);
  this->aec_enabled_ = enabled;

  // Live toggle left AFE running. If the user just turned AEC off and every
  // other feature was already off, we should stop the whole pipeline.
  if (!enabled && this->all_features_disabled_()) {
    this->recreate_instance_(false);  // no-features path tears down inside
  }
  return true;
}

bool EspAfe::set_reinit_flag_(bool &flag, bool enabled, const char *name) {
  if (flag == enabled) {
    return true;
  }
  // AFE torn down (all-off) and a feature is coming back: commit the flag and
  // rebuild via recreate_instance_.
  if (this->afe_stopped_.load(std::memory_order_acquire)) {
    flag = enabled;
    if (!enabled) {
      return true;  // stays torn down
    }
    return this->recreate_instance_(false);
  }
  if (!this->is_initialized() || this->config_mutex_ == nullptr || this->feed_chunksize_ == 0 ||
      this->fetch_chunksize_ == 0) {
    // Not yet running: commit immediately, build_instance_ will use it at setup/start
    flag = enabled;
    ESP_LOGD(TAG, "Deferring %s=%s until AFE is initialized",
             name, enabled ? "true" : "false");
    return true;
  }
  // Staged config: set flag, rebuild, rollback on failure.
  // Flag must be set before rebuild because build_instance_ reads it.
  // The mutex in recreate_instance_ ensures process() either sees the old
  // instance (passthrough) or the new one, never a mix.
  //
  // SE toggle changes mic_channels (MR<->MMR) which may alter frame sizes.
  // Allow frame size changes for SE; require same sizes for NS/VAD/AGC.
  bool allow_frame_change = (&flag == &this->se_enabled_);
  bool old_value = flag;
  flag = enabled;
  ESP_LOGI(TAG, "Applying %s=%s (rebuild, frame_size_change=%s)",
           name, enabled ? "true" : "false", allow_frame_change ? "allowed" : "locked");
  if (this->recreate_instance_(!allow_frame_change)) {
    return true;
  }
  // Rebuild failed: restore flag and try to rebuild with old config
  ESP_LOGW(TAG, "Failed to apply %s=%s, rolling back", name, enabled ? "true" : "false");
  flag = old_value;
  if (!this->recreate_instance_(!allow_frame_change)) {
    ESP_LOGE(TAG, "Rollback also failed for %s, AFE is down", name);
  }
  return false;
}

void EspAfe::setup() {
  if (!this->recreate_instance_(false)) {
    this->mark_failed();
  }
}

void EspAfe::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP AFE (Audio Front End):");
  ESP_LOGCONFIG(TAG, "  Type: %s", this->afe_type_ == AFE_TYPE_SR ? "SR" : "VC");
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->afe_mode_ == AFE_MODE_LOW_COST ? "LOW_COST" : "HIGH_PERF");
  ESP_LOGCONFIG(TAG, "  Microphones: transport=%d, afe=%d", this->mic_num_, this->afe_mic_channels_());
  ESP_LOGCONFIG(TAG, "  AEC: %s (filter_length=%d)", this->aec_enabled_ ? "ON" : "OFF", this->aec_filter_length_);
  ESP_LOGCONFIG(TAG, "  NS: %s (WebRTC)", this->ns_enabled_ ? "ON" : "OFF");
  ESP_LOGCONFIG(TAG, "  VAD: %s (mode=%d, speech=%dms, noise=%dms, delay=%dms)",
                this->vad_enabled_ ? "ON" : "OFF", this->vad_mode_, this->vad_min_speech_ms_,
                this->vad_min_noise_ms_, this->vad_delay_ms_);
  ESP_LOGCONFIG(TAG, "  AGC: %s (gain=%ddB, target=-%ddBFS)",
                this->agc_enabled_ ? "ON" : "OFF", this->agc_compression_gain_, this->agc_target_level_);
  if (this->mic_num_ >= 2) {
    ESP_LOGCONFIG(TAG, "  SE (Beamforming): %s", this->se_enabled_ ? "ON" : "OFF");
  } else {
    ESP_LOGCONFIG(TAG, "  SE (Beamforming): unavailable (mic_num < 2)");
  }
  ESP_LOGCONFIG(TAG, "  Alloc: %s, linear_gain=%.2f", this->memory_alloc_mode_to_str_(), this->afe_linear_gain_);
  ESP_LOGCONFIG(TAG, "  Task: core=%d, priority=%d, ringbuf=%d", this->task_core_, this->task_priority_, this->ringbuf_size_);
  ESP_LOGCONFIG(TAG, "  Process: %d samples, Feed: %d samples, Fetch: %d samples, Channels: %d",
                this->process_chunksize_, this->feed_chunksize_, this->fetch_chunksize_, this->total_channels_);
  ESP_LOGCONFIG(TAG, "  Initialized: %s", this->is_initialized() ? "YES" : "NO");
}

FrameSpec EspAfe::frame_spec() const {
  FrameSpec spec;
  spec.sample_rate = 16000;
  spec.mic_channels = this->afe_mic_channels_();
  spec.ref_channels = 1;
  // While running, use the live instance sizes. While torn down (all-off) the
  // live values are zero; fall back to the last successfully-installed spec
  // so consumers keep a stable frame shape for the passthrough fast path.
  int live_in = this->process_chunksize_ > 0 ? this->process_chunksize_ : this->fetch_chunksize_;
  int live_out = this->fetch_chunksize_;
  spec.input_samples = live_in > 0 ? live_in : this->last_spec_process_size_;
  spec.output_samples = live_out > 0 ? live_out : this->last_spec_fetch_size_;
  return spec;
}

FeatureControl EspAfe::feature_control(AudioFeature feature) const {
  switch (feature) {
    case AudioFeature::AEC:
      return FeatureControl::LIVE_TOGGLE;
    case AudioFeature::NS:
    case AudioFeature::AGC:
    case AudioFeature::VAD:
      return FeatureControl::RESTART_REQUIRED;
    case AudioFeature::SE:
      return this->mic_num_ >= 2 ? FeatureControl::RESTART_REQUIRED : FeatureControl::NOT_SUPPORTED;
    default:
      return FeatureControl::NOT_SUPPORTED;
  }
}

bool EspAfe::set_feature(AudioFeature feature, bool enabled) {
  switch (feature) {
    case AudioFeature::AEC: return enabled ? this->enable_aec() : this->disable_aec();
    case AudioFeature::SE:  return enabled ? this->enable_se()  : this->disable_se();
    case AudioFeature::NS:  return enabled ? this->enable_ns()  : this->disable_ns();
    case AudioFeature::VAD: return enabled ? this->enable_vad() : this->disable_vad();
    case AudioFeature::AGC: return enabled ? this->enable_agc() : this->disable_agc();
    default: return false;
  }
}

ProcessorTelemetry EspAfe::telemetry() const {
  ProcessorTelemetry t;
  t.voice_present = this->voice_present_.load(std::memory_order_relaxed);
  t.input_volume_dbfs = this->input_volume_dbfs_.load(std::memory_order_relaxed);
  t.output_rms_dbfs = this->output_rms_dbfs_.load(std::memory_order_relaxed);
  t.ringbuf_free_pct = this->ringbuf_free_pct_.load(std::memory_order_relaxed);
  t.glitch_count = this->glitch_count_.load(std::memory_order_relaxed);
  t.frame_count = this->frame_count_.load(std::memory_order_relaxed);
  t.input_ring_drop = this->input_ring_drop_.load(std::memory_order_relaxed);
  t.feed_ok = this->feed_ok_.load(std::memory_order_relaxed);
  t.feed_rejected = this->feed_rejected_.load(std::memory_order_relaxed);
  t.fetch_ok = this->fetch_ok_.load(std::memory_order_relaxed);
  t.fetch_timeout = this->fetch_timeout_.load(std::memory_order_relaxed);
  t.output_ring_drop = this->output_ring_drop_.load(std::memory_order_relaxed);
  t.feed_queue_frames = this->feed_queue_frames_.load(std::memory_order_relaxed);
  t.feed_queue_peak = this->feed_queue_peak_.load(std::memory_order_relaxed);
  t.fetch_queue_frames = this->fetch_queue_frames_.load(std::memory_order_relaxed);
  t.fetch_queue_peak = this->fetch_queue_peak_.load(std::memory_order_relaxed);
  t.process_us_last = this->process_us_last_.load(std::memory_order_relaxed);
  t.process_us_max = this->process_us_max_.load(std::memory_order_relaxed);
  t.feed_us_last = this->feed_us_last_.load(std::memory_order_relaxed);
  t.feed_us_max = this->feed_us_max_.load(std::memory_order_relaxed);
  t.fetch_us_last = this->fetch_us_last_.load(std::memory_order_relaxed);
  t.fetch_us_max = this->fetch_us_max_.load(std::memory_order_relaxed);
  if (this->feed_task_handle_ != nullptr) {
    t.feed_stack_high_water =
        uxTaskGetStackHighWaterMark(this->feed_task_handle_) * sizeof(StackType_t);
  }
  if (this->fetch_task_handle_ != nullptr) {
    t.fetch_stack_high_water =
        uxTaskGetStackHighWaterMark(this->fetch_task_handle_) * sizeof(StackType_t);
  }
  return t;
}

bool EspAfe::reconfigure(int type, int mode) {
  int old_type = this->afe_type_;
  int old_mode = this->afe_mode_;
  this->afe_type_ = type;
  this->afe_mode_ = mode;
  if (this->recreate_instance_(false)) {
    ESP_LOGI(TAG, "AFE reconfigured: type=%s, mode=%s",
             this->afe_type_ == AFE_TYPE_SR ? "SR" : "VC",
             this->afe_mode_ == AFE_MODE_LOW_COST ? "LOW_COST" : "HIGH_PERF");
    return true;
  }
  // Rollback on failure: restore old config and rebuild to avoid leaving
  // the DSP permanently non-functional.
  ESP_LOGW(TAG, "reconfigure: new type=%d mode=%d build failed, rolling back to type=%d mode=%d",
           type, mode, old_type, old_mode);
  this->afe_type_ = old_type;
  this->afe_mode_ = old_mode;
  if (!this->recreate_instance_(false)) {
    ESP_LOGE(TAG, "reconfigure: rollback rebuild ALSO failed - AFE is DOWN");
  }
  return false;
}

bool EspAfe::process(const int16_t *in_mic, const int16_t *in_ref, int16_t *out,
                     uint8_t mic_channels_in) {
  const int transport_mic_channels = mic_channels_in;
  int qs = this->process_chunksize_ > 0 ? this->process_chunksize_ : this->fetch_chunksize_;
  int os = this->fetch_chunksize_;
  // Fast path when user has disabled every AFE feature: the instance is torn
  // down, but the caller still expects a frame-shaped output. Use the last
  // known spec so mic_afe consumers (MWW, VA) keep receiving audio.
  if (this->afe_stopped_.load(std::memory_order_acquire)) {
    int pqs = this->last_spec_process_size_ > 0 ? this->last_spec_process_size_ : qs;
    int pos = this->last_spec_fetch_size_ > 0 ? this->last_spec_fetch_size_ : os;
    if (pqs > 0 && pos > 0) copy_passthrough_frame(in_mic, pqs, transport_mic_channels, out, pos);
    return false;
  }
  if (!this->is_initialized()) {
    if (qs > 0 && os > 0) copy_passthrough_frame(in_mic, qs, transport_mic_channels, out, os);
    return false;
  }

  const int64_t process_start_us = esp_timer_get_time();
  auto finish_process_timing = [this, process_start_us]() {
    uint32_t elapsed_us = static_cast<uint32_t>(std::max<int64_t>(0, esp_timer_get_time() - process_start_us));
    this->process_us_last_.store(elapsed_us, std::memory_order_relaxed);
    update_peak_atomic(this->process_us_max_, elapsed_us);
  };

  // Drain protocol entry: mark busy before observing drain flag. The release
  // on busy is paired with the acquire on drain_request_ in the writer
  // (recreate_instance_), so either the writer sees busy=true and waits, or
  // we see drain_request_=true and bail. We cannot see both false/false and
  // then observe a torn instance.
  this->process_busy_.store(true, std::memory_order_release);
  if (this->drain_request_.load(std::memory_order_acquire)) {
    this->process_busy_.store(false, std::memory_order_release);
    copy_passthrough_frame(in_mic, qs, transport_mic_channels, out, os);
    finish_process_timing();
    return false;
  }

  const int afe_mic_channels = this->afe_mic_channels_();
  qs = this->process_chunksize_ > 0 ? this->process_chunksize_ : this->fetch_chunksize_;
  os = this->fetch_chunksize_;
  int fs = this->feed_chunksize_;
  if (qs <= 0 || os <= 0 || fs <= 0 || this->feed_buf_ == nullptr) {
    copy_passthrough_frame(in_mic, qs, transport_mic_channels, out, os);
    this->process_busy_.store(false, std::memory_order_release);
    finish_process_timing();
    return false;
  }

  // Step 1: stage new input and feed it to AFE when a full frame is assembled.
  int offset = this->staged_input_samples_;
  if (offset + qs > fs) {
    ESP_LOGW(TAG, "AFE staging overflow (%d + %d > %d), dropping staged input", offset, qs, fs);
    offset = 0;
  }

  // Input RMS: compute on raw mic BEFORE feeding to AFE pipeline.
  // Replaces data_volume (always 0 without WakeNet).
  if (this->input_volume_sensor_enabled_ && !this->warmup_remaining_) {
    this->input_volume_dbfs_.store(
        compute_rms_dbfs_i16(in_mic, qs * transport_mic_channels > 0 ? qs : 0),
        std::memory_order_relaxed);
  }

  bool in_warmup = (this->warmup_remaining_ > 0) && (offset + qs >= fs);

  if (!in_warmup) {
    const int tc = this->total_channels_;
    int16_t *dst = this->feed_buf_ + offset * tc;
    if (afe_mic_channels == 1) {
      if (in_ref != nullptr) {
        for (int i = 0; i < qs; i++) {
          *dst++ = in_mic[i * transport_mic_channels];
          *dst++ = in_ref[i];
        }
      } else {
        for (int i = 0; i < qs; i++) {
          *dst++ = in_mic[i * transport_mic_channels];
          *dst++ = 0;
        }
      }
    } else if (transport_mic_channels >= 2) {
      if (in_ref != nullptr) {
        for (int i = 0; i < qs; i++) {
          *dst++ = in_mic[i * 2];
          *dst++ = in_mic[i * 2 + 1];
          *dst++ = in_ref[i];
        }
      } else {
        for (int i = 0; i < qs; i++) {
          *dst++ = in_mic[i * 2];
          *dst++ = in_mic[i * 2 + 1];
          *dst++ = 0;
        }
      }
    } else {
      // AFE wants 2 mic channels but caller sent 1 (SE transition): zero-fill.
      if (in_ref != nullptr) {
        for (int i = 0; i < qs; i++) {
          *dst++ = in_mic[i];
          *dst++ = 0;
          *dst++ = in_ref[i];
        }
      } else {
        for (int i = 0; i < qs; i++) {
          *dst++ = in_mic[i];
          *dst++ = 0;
          *dst++ = 0;
        }
      }
    }
  }
  offset += qs;

  if (offset == fs) {
    if (this->warmup_remaining_ > 0) {
      memset(this->feed_buf_, 0, static_cast<size_t>(fs) * this->total_channels_ * sizeof(int16_t));
      this->warmup_remaining_--;
    }
    // Enqueue the full frame into the NOSPLIT ring; feed_task pops it off
    // and calls afe_handle_->feed() at low priority. Non-blocking send: if
    // the ring is full we drop the frame (input_ring_drop_) rather than
    // stall the i2s_audio_task realtime path.
    size_t feed_bytes = static_cast<size_t>(fs) * this->total_channels_ * sizeof(int16_t);
    if (this->feed_input_ring_ != nullptr) {
      if (!xRingbufferSend(this->feed_input_ring_, this->feed_buf_, feed_bytes, 0)) {
        this->input_ring_drop_.fetch_add(1, std::memory_order_relaxed);
      } else {
        uint32_t queued = this->feed_queue_frames_.fetch_add(1, std::memory_order_relaxed) + 1;
        update_peak_atomic(this->feed_queue_peak_, queued);
      }
    }
    offset = 0;
  }
  this->staged_input_samples_ = offset;

  // Step 2: try to pull a processed frame that the fetch task has pushed into
  // our side of the bridge. Non-blocking: if nothing is ready we emit
  // passthrough for this call. The one-frame latency is a consequence of the
  // decoupled feed/fetch topology mandated by esp-sr.
  size_t output_bytes = static_cast<size_t>(os) * sizeof(int16_t);
  bool processed = false;
  if (this->fetch_output_ring_) {
    size_t got = this->fetch_output_ring_->read(reinterpret_cast<uint8_t *>(out), output_bytes, 0);
    if (got == output_bytes) {
      processed = true;
      decrement_if_nonzero(this->fetch_queue_frames_);
    }
  }
  if (!processed) {
    copy_passthrough_frame(in_mic, qs, transport_mic_channels, out, os);
    this->glitch_count_.fetch_add(1, std::memory_order_relaxed);
  }

  // Release drain guard BEFORE emitting telemetry / counters: those touch
  // only atomics on this and can safely race with a rebuild.
  this->process_busy_.store(false, std::memory_order_release);

  // Output-side RMS depends on the samples handed to the caller. VAD, input
  // volume and ringbuf_free_pct are written by fetch_task_loop_ when it pulls
  // a frame from AFE, so this function only needs to refresh output RMS.
  if (processed && this->output_rms_sensor_enabled_) {
    this->output_rms_dbfs_.store(compute_rms_dbfs_i16(out, os), std::memory_order_relaxed);
  }
  this->frame_count_.fetch_add(1, std::memory_order_relaxed);
  finish_process_timing();
  return processed;
}

bool EspAfe::reinit_by_name(const std::string &name) {
  return this->reinit_by_name(name.c_str());
}

bool EspAfe::reinit_by_name(const char *name) {
  int type, mode;
  if (strcmp(name, "sr_low_cost") == 0) { type = AFE_TYPE_SR; mode = AFE_MODE_LOW_COST; }
  else if (strcmp(name, "sr_high_perf") == 0) { type = AFE_TYPE_SR; mode = AFE_MODE_HIGH_PERF; }
  else if (strcmp(name, "voip_low_cost") == 0) { type = AFE_TYPE_VC; mode = AFE_MODE_LOW_COST; }
  else if (strcmp(name, "voip_high_perf") == 0) { type = AFE_TYPE_VC; mode = AFE_MODE_HIGH_PERF; }
  else {
    ESP_LOGW(TAG, "Unknown AFE mode: %s", name);
    return false;
  }
  return this->reconfigure(type, mode);
}

bool EspAfe::enable_aec() { return this->set_aec_enabled_runtime_(true); }
bool EspAfe::disable_aec() { return this->set_aec_enabled_runtime_(false); }
bool EspAfe::enable_se() {
  if (this->mic_num_ < 2) {
    ESP_LOGW(TAG, "SE requires mic_num >= 2");
    return false;
  }
  return this->set_reinit_flag_(this->se_enabled_, true, "se_enabled");
}
bool EspAfe::disable_se() {
  if (this->mic_num_ < 2) {
    ESP_LOGW(TAG, "SE requires mic_num >= 2");
    return false;
  }
  return this->set_reinit_flag_(this->se_enabled_, false, "se_enabled");
}
bool EspAfe::enable_ns() { return this->set_reinit_flag_(this->ns_enabled_, true, "ns_enabled"); }
bool EspAfe::disable_ns() { return this->set_reinit_flag_(this->ns_enabled_, false, "ns_enabled"); }
bool EspAfe::enable_vad() { return this->set_reinit_flag_(this->vad_enabled_, true, "vad_enabled"); }
bool EspAfe::disable_vad() { return this->set_reinit_flag_(this->vad_enabled_, false, "vad_enabled"); }
bool EspAfe::enable_agc() { return this->set_reinit_flag_(this->agc_enabled_, true, "agc_enabled"); }
bool EspAfe::disable_agc() { return this->set_reinit_flag_(this->agc_enabled_, false, "agc_enabled"); }

// ---- Feed task: calls afe_handle_->feed() off the realtime path ----

void EspAfe::feed_task_trampoline(void *arg) {
  static_cast<EspAfe *>(arg)->feed_task_loop_();
  vTaskDelete(nullptr);
}

void EspAfe::feed_task_loop_() {
  while (this->feed_task_running_.load(std::memory_order_acquire)) {
    size_t item_size = 0;
    void *item = xRingbufferReceive(this->feed_input_ring_, &item_size, pdMS_TO_TICKS(100));
    if (item == nullptr) {
      continue;  // timeout: re-check running flag for shutdown
    }
    decrement_if_nonzero(this->feed_queue_frames_);
    if (this->feed_task_running_.load(std::memory_order_acquire)) {
      const int64_t feed_start_us = esp_timer_get_time();
      int ret = this->afe_handle_->feed(this->afe_data_, static_cast<int16_t *>(item));
      uint32_t feed_us = static_cast<uint32_t>(std::max<int64_t>(0, esp_timer_get_time() - feed_start_us));
      this->feed_us_last_.store(feed_us, std::memory_order_relaxed);
      update_peak_atomic(this->feed_us_max_, feed_us);
      if (ret > 0) {
        this->feed_ok_.fetch_add(1, std::memory_order_relaxed);
      } else {
        this->feed_rejected_.fetch_add(1, std::memory_order_relaxed);
      }
    }
    vRingbufferReturnItem(this->feed_input_ring_, item);
  }
}

bool EspAfe::start_feed_task_() {
  if (this->feed_task_handle_ != nullptr) {
    return true;
  }
  if (this->afe_handle_ == nullptr || this->afe_data_ == nullptr ||
      this->feed_chunksize_ <= 0) {
    return false;
  }

  if (this->feed_input_ring_ == nullptr) {
    // NOSPLIT ring: xRingbufferSend is atomic per-item; Receive returns a
    // complete frame. 4-frame capacity absorbs BSS jitter without memory
    // bloat. Each NOSPLIT item adds an 8-byte header, included in sizing.
    const size_t frame_bytes = static_cast<size_t>(this->feed_chunksize_) *
                               this->total_channels_ * sizeof(int16_t);
    const size_t ring_size = (frame_bytes + 8) * 4;
    const uint32_t feed_ring_caps = this->feed_ring_in_psram_ ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL;
    this->feed_input_ring_storage_ = static_cast<uint8_t *>(heap_caps_malloc(ring_size, feed_ring_caps));
    if (this->feed_input_ring_storage_ == nullptr && this->feed_ring_in_psram_) {
      ESP_LOGW(TAG, "feed_input_ring (%u bytes) fell back to internal RAM (PSRAM full/unavailable)",
               (unsigned) ring_size);
      this->feed_input_ring_storage_ = static_cast<uint8_t *>(
          heap_caps_malloc(ring_size, MALLOC_CAP_INTERNAL));
    }
    if (this->feed_input_ring_storage_ == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate AFE feed input ring storage (%u bytes)",
               (unsigned) ring_size);
      return false;
    }
    this->feed_input_ring_struct_ = static_cast<StaticRingbuffer_t *>(
        heap_caps_malloc(sizeof(StaticRingbuffer_t), MALLOC_CAP_INTERNAL));
    if (this->feed_input_ring_struct_ == nullptr) {
      heap_caps_free(this->feed_input_ring_storage_);
      this->feed_input_ring_storage_ = nullptr;
      ESP_LOGE(TAG, "Failed to allocate AFE feed input ring struct");
      return false;
    }
    this->feed_input_ring_ = xRingbufferCreateStatic(
        ring_size, RINGBUF_TYPE_NOSPLIT,
        this->feed_input_ring_storage_, this->feed_input_ring_struct_);
    if (this->feed_input_ring_ == nullptr) {
      heap_caps_free(this->feed_input_ring_storage_);
      this->feed_input_ring_storage_ = nullptr;
      heap_caps_free(this->feed_input_ring_struct_);
      this->feed_input_ring_struct_ = nullptr;
      ESP_LOGE(TAG, "Failed to create AFE feed input ring");
      return false;
    }
    ESP_LOGI(TAG, "Feed input ring: %u bytes (%u per frame, 4 slots, NOSPLIT)",
             (unsigned) ring_size, (unsigned) frame_bytes);
  }

  // Always size the feed stack for the widest pipeline (single-mic MR, which
  // runs AEC + WebRTC NS inside the feed task). A 2-mic YAML that later
  // turns SE off drops esp-sr into MR mode even with mic_num_=2, and the
  // smaller "DualMic" stack overflows the MR code path.
  //
  // Dynamic task creation (not static): a static TCB reused across
  // stop/start gets its xStateListItem re-initialised while the prior task
  // may still be referenced by FreeRTOS' termination list on the target
  // core, corrupting the ready list inside prvAddNewTaskToReadyList. The
  // heap cost is ~16 KB internal RAM churn per reconfigure.
  const uint32_t stack_words = kFeedTaskStackWordsSingleMic;

  this->feed_task_running_.store(true, std::memory_order_release);
  BaseType_t rc = xTaskCreatePinnedToCore(
      &EspAfe::feed_task_trampoline, "afe_feed", stack_words, this,
      kFeedTaskPriority, &this->feed_task_handle_,
      this->task_core_ >= 0 ? this->task_core_ : tskNO_AFFINITY);
  if (rc != pdPASS || this->feed_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create AFE feed task");
    this->feed_task_running_.store(false, std::memory_order_release);
    this->feed_task_handle_ = nullptr;
    this->feed_input_ring_ = nullptr;
    heap_caps_free(this->feed_input_ring_storage_);
    this->feed_input_ring_storage_ = nullptr;
    heap_caps_free(this->feed_input_ring_struct_);
    this->feed_input_ring_struct_ = nullptr;
    return false;
  }
  ESP_LOGI(TAG, "AFE feed task started (core=%d, priority=%u, stack=%uB)",
           this->task_core_, (unsigned) kFeedTaskPriority,
           (unsigned) (stack_words * sizeof(StackType_t)));
  return true;
}

void EspAfe::stop_feed_task_() {
  if (this->feed_task_handle_ == nullptr) {
    return;
  }
  this->feed_task_running_.store(false, std::memory_order_release);
  for (int i = 0; i < 25; i++) {
    if (eTaskGetState(this->feed_task_handle_) == eDeleted) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  this->feed_task_handle_ = nullptr;
  this->feed_input_ring_ = nullptr;
  if (this->feed_input_ring_storage_ != nullptr) {
    heap_caps_free(this->feed_input_ring_storage_);
    this->feed_input_ring_storage_ = nullptr;
  }
  if (this->feed_input_ring_struct_ != nullptr) {
    heap_caps_free(this->feed_input_ring_struct_);
    this->feed_input_ring_struct_ = nullptr;
  }
}

// ---- Fetch task: drains AFE output into the ring for process() ----

void EspAfe::fetch_task_trampoline(void *arg) {
  static_cast<EspAfe *>(arg)->fetch_task_loop_();
  vTaskDelete(nullptr);
}

void EspAfe::fetch_task_loop_() {
  // Canonical Espressif pattern on every topology: block on fetch_with_delay
  // with a finite timeout. rb_out inside esp-sr is the natural sync point.
  const TickType_t fetch_timeout = pdMS_TO_TICKS(2000);

  while (this->fetch_task_running_.load(std::memory_order_acquire)) {
    // fetch_with_delay: blocks on esp-sr's output ring with a finite timeout.
    // 2s allows clean shutdown when fetch_task_running_ goes false without
    // risking false "ringbuffer empty" warnings under BSS worker jitter.
    const int64_t fetch_start_us = esp_timer_get_time();
    afe_fetch_result_t *result =
        this->afe_handle_->fetch_with_delay(this->afe_data_, fetch_timeout);
    uint32_t fetch_us = static_cast<uint32_t>(std::max<int64_t>(0, esp_timer_get_time() - fetch_start_us));
    this->fetch_us_last_.store(fetch_us, std::memory_order_relaxed);
    update_peak_atomic(this->fetch_us_max_, fetch_us);
    if (!this->fetch_task_running_.load(std::memory_order_acquire)) {
      break;
    }
    if (result == nullptr || result->ret_value != ESP_OK || result->data == nullptr) {
      this->fetch_timeout_.fetch_add(1, std::memory_order_relaxed);
      continue;
    }
    this->fetch_ok_.fetch_add(1, std::memory_order_relaxed);

    // Telemetry from the frame the worker just handed us.
    this->ringbuf_free_pct_.store(result->ringbuff_free_pct, std::memory_order_relaxed);
    const bool new_voice = this->vad_enabled_ && result->vad_state == VAD_SPEECH;
    const bool prev_voice = this->voice_present_.exchange(new_voice, std::memory_order_relaxed);
    if (new_voice != prev_voice) {
      ESP_LOGD(TAG, "VAD transition: %s -> %s (raw_state=%d)",
               prev_voice ? "speech" : "silence", new_voice ? "speech" : "silence",
               static_cast<int>(result->vad_state));
    }

    if (this->fetch_output_ring_) {
      const size_t want = static_cast<size_t>(result->data_size);
      size_t wrote = this->fetch_output_ring_->write_without_replacement(result->data, want,
                                                                         pdMS_TO_TICKS(5));
      if (wrote != want) {
        this->output_ring_drop_.fetch_add(1, std::memory_order_relaxed);
      } else {
        uint32_t queued = this->fetch_queue_frames_.fetch_add(1, std::memory_order_relaxed) + 1;
        update_peak_atomic(this->fetch_queue_peak_, queued);
      }
    }
  }
}

bool EspAfe::start_fetch_task_() {
  if (this->fetch_task_handle_ != nullptr) {
    return true;  // already running
  }
  if (this->afe_handle_ == nullptr || this->afe_data_ == nullptr ||
      this->fetch_chunksize_ <= 0) {
    return false;
  }

  if (!this->fetch_output_ring_) {
    // 4 frames of headroom (~128 ms at 16 kHz / 512-sample fetch) absorb
    // consumer-side jitter when process() is preempted by higher-priority
    // tasks. Placement is YAML-controlled: internal saves ~6.8 us/frame on
    // Core 0 read, PSRAM saves ~4 KB internal RAM (set fetch_ring_in_psram).
    const size_t frame_bytes = static_cast<size_t>(this->fetch_chunksize_) * sizeof(int16_t);
    this->fetch_output_ring_ = this->fetch_ring_in_psram_
        ? audio_processor::create_prefer_psram(frame_bytes * 4, "esp_afe.fetch_output_ring")
        : audio_processor::create_internal(frame_bytes * 4, "esp_afe.fetch_output_ring");
    if (!this->fetch_output_ring_) {
      ESP_LOGE(TAG, "Failed to allocate AFE fetch output ring buffer");
      return false;
    }
  }

  const int fetch_priority = this->task_priority_ > 1 ? this->task_priority_ - 1 : 1;
  const int fetch_core = (this->task_core_ >= 0) ? this->task_core_ : tskNO_AFFINITY;

  // Dynamic task creation: see feed task rationale for why static TCB reuse
  // was unsafe across reconfigure.
  this->fetch_task_running_.store(true, std::memory_order_release);
  BaseType_t rc = xTaskCreatePinnedToCore(
      &EspAfe::fetch_task_trampoline, "afe_fetch", kFetchTaskStackWords, this,
      fetch_priority, &this->fetch_task_handle_, fetch_core);
  if (rc != pdPASS || this->fetch_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create AFE fetch task");
    this->fetch_task_running_.store(false, std::memory_order_release);
    this->fetch_task_handle_ = nullptr;
    return false;
  }
  ESP_LOGI(TAG, "AFE fetch task started (core=%d, priority=%d)",
           fetch_core, fetch_priority);
  return true;
}

void EspAfe::stop_fetch_task_() {
  if (this->fetch_task_handle_ == nullptr) {
    return;
  }
  this->fetch_task_running_.store(false, std::memory_order_release);
  // Wait up to ~250ms for the task to mark itself deleted.
  for (int i = 0; i < 25; i++) {
    if (eTaskGetState(this->fetch_task_handle_) == eDeleted) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  this->fetch_task_handle_ = nullptr;
  this->fetch_output_ring_.reset();
}

EspAfe::~EspAfe() {
  // Quiesce: acquire mutex to ensure process() is not mid-frame
  if (this->config_mutex_ != nullptr) {
    xSemaphoreTake(this->config_mutex_, pdMS_TO_TICKS(500));
  }
  AfeInstance instance = this->detach_instance_();
  this->destroy_instance_(&instance);
  if (this->config_mutex_ != nullptr) {
    xSemaphoreGive(this->config_mutex_);
    vSemaphoreDelete(this->config_mutex_);
    this->config_mutex_ = nullptr;
  }
}

}  // namespace esp_afe
}  // namespace esphome

#endif  // USE_ESP32
