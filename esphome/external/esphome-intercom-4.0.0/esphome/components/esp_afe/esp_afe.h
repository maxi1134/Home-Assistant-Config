#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "../audio_processor/audio_processor.h"

#ifdef USE_ESP32

#include <esp_afe_sr_iface.h>
#include <esp_afe_sr_models.h>
#include <esp_afe_config.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "esphome/core/ring_buffer.h"
#include "../audio_processor/ring_buffer_caps.h"

#include <atomic>
#include <memory>

namespace esphome {
namespace esp_afe {

using audio_processor::AudioFeature;
using audio_processor::AudioProcessor;
using audio_processor::FeatureControl;
using audio_processor::FrameSpec;
using audio_processor::ProcessorTelemetry;

/// Full Espressif AFE pipeline wrapper.
///
/// Implements AudioProcessor on top of esp-sr's AFE (AEC + NS + VAD +
/// AGC + optional SE beamforming). The component runs two FreeRTOS tasks:
///
///   feed_task   - pulls frames from the input ring, interleaves mic/ref,
///                 calls afe_data->feed().
///   fetch_task  - calls afe_data->fetch_with_delay(), pushes processed
///                 frames into fetch_output_ring_ for consumers to read
///                 via process().
///
/// Runtime reconfiguration (toggling AEC/NS/AGC/VAD/SE, switching SR/VC
/// mode) must tear the esp-sr instance down and rebuild it. Because
/// process() is called from the consumer audio task (prio 19) while
/// config mutations come from the main thread, the two ends coordinate
/// through a lock-free drain handshake:
///
///   1. Config change acquires config_mutex_ and flips drain_request_
///      to true.
///   2. process() observes drain_request_ at the top of every frame and
///      returns a passthrough copy immediately, without touching the
///      esp-sr handle.
///   3. Config change waits for process_busy_ to clear, then it is safe
///      to destroy + rebuild the esp-sr instance.
///   4. After rebuild, drain_request_ is cleared; process() resumes
///      normal operation on the next frame.
///
/// This avoids blocking the consumer's audio task on any global mutex.
class EspAfe : public Component, public AudioProcessor {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  // AudioProcessor interface
  bool is_initialized() const override {
    return this->afe_data_ != nullptr && this->afe_handle_ != nullptr && this->feed_buf_ != nullptr;
  }
  FrameSpec frame_spec() const override;
  bool process(const int16_t *in_mic, const int16_t *in_ref, int16_t *out,
               uint8_t mic_channels_in = 1) override;
  uint32_t frame_spec_revision() const override {
    return this->frame_spec_revision_.load(std::memory_order_acquire);
  }
  FeatureControl feature_control(AudioFeature feature) const override;
  bool set_feature(AudioFeature feature, bool enabled) override;
  ProcessorTelemetry telemetry() const override;
  bool reconfigure(int type, int mode) override;

  // Config setters (called from Python codegen)
  void set_afe_type(int type) { this->afe_type_ = type; }
  void set_afe_mode(int mode) { this->afe_mode_ = mode; }
  void set_mic_num(int num) { this->mic_num_ = num; }
  void set_aec_enabled(bool en) { this->aec_enabled_ = en; }
  void set_aec_filter_length(int len) { this->aec_filter_length_ = len; }
  void set_se_enabled(bool en) { this->se_enabled_ = en; }
  void set_ns_enabled(bool en) { this->ns_enabled_ = en; }
  void set_vad_enabled(bool en) { this->vad_enabled_ = en; }
  void set_vad_mode(int mode) { this->vad_mode_ = mode; }
  void set_vad_min_speech_ms(int ms) { this->vad_min_speech_ms_ = ms; }
  void set_vad_min_noise_ms(int ms) { this->vad_min_noise_ms_ = ms; }
  void set_vad_delay_ms(int ms) { this->vad_delay_ms_ = ms; }
  void set_vad_mute_playback(bool en) { this->vad_mute_playback_ = en; }
  void set_vad_enable_channel_trigger(bool en) { this->vad_enable_channel_trigger_ = en; }
  void set_agc_enabled(bool en) { this->agc_enabled_ = en; }
  void set_agc_compression_gain(int gain) { this->agc_compression_gain_ = gain; }
  void set_agc_target_level(int level) { this->agc_target_level_ = level; }
  void set_memory_alloc_mode(int mode) { this->memory_alloc_mode_ = mode; }
  void set_afe_linear_gain(float gain) { this->afe_linear_gain_ = gain; }
  void set_task_core(int core) { this->task_core_ = core; }
  void set_task_priority(int prio) { this->task_priority_ = prio; }
  void set_ringbuf_size(int size) { this->ringbuf_size_ = size; }
  void set_feed_buf_in_psram(bool psram) { this->feed_buf_in_psram_ = psram; }
  void set_feed_ring_in_psram(bool psram) { this->feed_ring_in_psram_ = psram; }
  void set_fetch_ring_in_psram(bool psram) { this->fetch_ring_in_psram_ = psram; }
  void set_input_volume_sensor_enabled(bool en) { this->input_volume_sensor_enabled_ = en; }
  void set_output_rms_sensor_enabled(bool en) { this->output_rms_sensor_enabled_ = en; }

  // Runtime toggles (for switches and automations)
  bool enable_aec();
  bool disable_aec();
  bool enable_ns();
  bool disable_ns();
  bool enable_se();
  bool disable_se();
  bool enable_vad();
  bool disable_vad();
  bool enable_agc();
  bool disable_agc();

  bool is_aec_enabled() const { return this->aec_enabled_; }
  bool is_se_enabled() const { return this->mic_num_ >= 2 && this->se_enabled_; }
  bool is_ns_enabled() const { return this->ns_enabled_; }
  bool is_vad_enabled() const { return this->vad_enabled_; }
  bool is_agc_enabled() const { return this->agc_enabled_; }
  // Current mode string ("sr_low_cost", "sr_high_perf", "voip_low_cost",
  // "voip_high_perf"). Used by UI templates to publish the actual live mode
  // after a set_action so optimistic selects don't drift from reality.
  std::string get_mode_name() const {
    // afe_type_: 0 = SR, 1 = VC;  afe_mode_: 0 = LOW_COST, 1 = HIGH_PERF.
    if (this->afe_type_ == 0) {
      return this->afe_mode_ == 1 ? "sr_high_perf" : "sr_low_cost";
    }
    return this->afe_mode_ == 1 ? "voip_high_perf" : "voip_low_cost";
  }
  bool is_voice_present() const { return this->voice_present_.load(std::memory_order_relaxed); }
  float get_input_volume_dbfs() const { return this->input_volume_dbfs_.load(std::memory_order_relaxed); }
  float get_output_rms_dbfs() const { return this->output_rms_dbfs_.load(std::memory_order_relaxed); }

  // Reinit with a new mode string (e.g. "sr_low_cost", "voip_high_perf").
  // Caller must stop audio processing before calling this.
  bool reinit_by_name(const std::string &name);
  bool reinit_by_name(const char *name);

  ~EspAfe() override;

 protected:
  // Derive aec_mode_t from afe_type + afe_mode
  aec_mode_t derive_aec_mode_() const;
  int afe_mic_channels_() const;

  struct AfeInstance {
    const esp_afe_sr_iface_t *handle{nullptr};
    esp_afe_sr_data_t *data{nullptr};
    afe_config_t *config{nullptr};
    int16_t *feed_buf{nullptr};
    int feed_chunksize{0};
    int fetch_chunksize{0};
    int process_chunksize{0};
    int total_channels{0};
  };

  bool build_instance_(AfeInstance *instance);
  bool recreate_instance_(bool require_same_frame_sizes);
  bool set_aec_enabled_runtime_(bool enabled);
  bool set_reinit_flag_(bool &flag, bool enabled, const char *name);
  void destroy_instance_(AfeInstance *instance);
  bool install_instance_(AfeInstance *instance);
  AfeInstance detach_instance_();
  const char *memory_alloc_mode_to_str_() const;

  // True when the user has every AFE feature turned off. In that state
  // running the pipeline is pointless, so recreate_instance_ and the runtime
  // toggle paths tear the instance down instead of rebuilding it.
  bool all_features_disabled_() const {
    return !this->aec_enabled_ && !this->ns_enabled_ && !this->agc_enabled_ &&
           !this->vad_enabled_ && !this->is_se_enabled();
  }

  // AFE vtable, opaque data, and config (config must outlive afe_data)
  const esp_afe_sr_iface_t *afe_handle_{nullptr};
  esp_afe_sr_data_t *afe_data_{nullptr};
  afe_config_t *afe_config_{nullptr};

  // Feed buffer: interleaved [mic, ref, mic, ref, ...] or [mic1, mic2, ref, ...]
  int16_t *feed_buf_{nullptr};
  int feed_chunksize_{0};   // per-channel samples expected by feed()
  int fetch_chunksize_{0};  // mono output samples returned by fetch()
  int process_chunksize_{0};  // external process() input chunk size
  int total_channels_{2};   // 2 for "MR", 3 for "MMR"
  int staged_input_samples_{0};

  // esp-sr AFE pipeline. Canonical Espressif pattern (esp-skainet,
  // test_afe.cpp): feed() and fetch() run on dedicated tasks at low priority
  // so that BSS/AEC processing never blocks the caller of process().
  //
  //   process() (called by i2s_audio_duplex audio_task on core 0 at prio 19)
  //     -> assembles a full feed frame in feed_buf_ and pushes it into the
  //        NOSPLIT ring (non-blocking, atomic frame).
  //   feed_task (core task_core_, priority kFeedTaskPriority = 5)
  //     -> pops the frame and calls afe_handle_->feed(). feed() may block
  //        on the internal AFE ring when BSS is saturated; that blocking
  //        stays off the realtime I2S path because the task priority is low.
  //   fetch_task (core task_core_, priority task_priority_ - 1)
  //     -> blocks on fetch_with_delay() and writes processed frames into
  //        fetch_output_ring_. process() reads this ring non-blocking.
  //
  // Fixed low priority for feed_task matches esp-skainet reference. Must
  // stay well below i2s_audio_task (prio 19) and the speaker pipeline (1)
  // so that BSS saturation never starves the realtime path.
  static constexpr UBaseType_t kFeedTaskPriority = 5;

  RingbufHandle_t feed_input_ring_{nullptr};
  uint8_t *feed_input_ring_storage_{nullptr};
  StaticRingbuffer_t *feed_input_ring_struct_{nullptr};
  TaskHandle_t feed_task_handle_{nullptr};
  std::atomic<bool> feed_task_running_{false};
  // 1-mic feeds WebRTC NS/AGC inline (~8KB stack on WebRtcNs_ProcessCore);
  // dual-mic BSS runs in the esp-sr worker so feed_task only enqueues frames.
  // 12KB covers the worst-case feed path (single-mic MR: AEC + WebRTC NS).
  // MMR (2-mic with BSS) could run on 6KB, but the pipeline can switch to
  // MR at runtime (SE off) without the stack being reallocated, so the
  // feed task is always sized for the MR worst case.
  static constexpr uint32_t kFeedTaskStackWordsSingleMic = 12288 / sizeof(StackType_t);

  static void feed_task_trampoline(void *arg);
  void feed_task_loop_();
  bool start_feed_task_();
  void stop_feed_task_();

  // Fetch bridge: fetch task writes, process() reads non-blocking
  std::unique_ptr<RingBuffer> fetch_output_ring_;
  TaskHandle_t fetch_task_handle_{nullptr};
  std::atomic<bool> fetch_task_running_{false};
  static constexpr uint32_t kFetchTaskStackWords = 4096 / sizeof(StackType_t);

  static void fetch_task_trampoline(void *arg);
  void fetch_task_loop_();
  bool start_fetch_task_();
  void stop_fetch_task_();

  // Config (set from Python, used in setup())
  int afe_type_{0};         // AFE_TYPE_SR
  int afe_mode_{0};         // AFE_MODE_LOW_COST
  int mic_num_{1};  // physical microphone channels available from transport
  bool aec_enabled_{true};
  int aec_filter_length_{4};
  bool se_enabled_{false};
  bool ns_enabled_{true};
  bool vad_enabled_{false};
  int vad_mode_{VAD_MODE_3};
  int vad_min_speech_ms_{128};
  int vad_min_noise_ms_{1000};
  int vad_delay_ms_{128};
  bool vad_mute_playback_{false};
  bool vad_enable_channel_trigger_{false};
  bool agc_enabled_{true};
  int agc_compression_gain_{9};
  int agc_target_level_{3};
  int memory_alloc_mode_{AFE_MEMORY_ALLOC_MORE_PSRAM};
  float afe_linear_gain_{1.0f};
  int task_core_{1};
  int task_priority_{8};
  int ringbuf_size_{8};
  bool feed_buf_in_psram_{false};   // ~3 KB scratch (default internal, ~41 us/frame faster on Core 0)
  bool feed_ring_in_psram_{false};  // ~12 KB staging ring (default internal, ~20 us/frame faster on Core 0)
  bool fetch_ring_in_psram_{false}; // ~4 KB output ring (default internal, ~6.8 us/frame faster on Core 0)

  // config_mutex_ serialises config-change paths (recreate_instance_,
  // set_aec_enabled_runtime_, destructor). It is NOT taken by process() on
  // the hot path. process() uses the drain protocol below instead.
  SemaphoreHandle_t config_mutex_{nullptr};

  // Drain protocol for process() vs recreate_instance_:
  //   recreate_instance_ sets drain_request_ = true and waits until
  //   process_busy_ == false before touching instance state. process() marks
  //   itself busy, then checks drain_request_; if set, it bails with
  //   passthrough. This removes mutex overhead from the per-frame path while
  //   preserving the invariant that process() never observes a
  //   half-demolished instance.
  std::atomic<bool> drain_request_{false};
  std::atomic<bool> process_busy_{false};

  // afe_stopped_ == true means the AFE handle + feed/fetch tasks are torn
  // down (all user-facing features off). process() takes a cheap passthrough
  // path using the last-known chunksizes. Re-enabling any feature rebuilds.
  std::atomic<bool> afe_stopped_{false};

  std::atomic<bool> voice_present_{false};
  std::atomic<float> input_volume_dbfs_{-120.0f};
  std::atomic<float> output_rms_dbfs_{-120.0f};
  bool input_volume_sensor_enabled_{false};
  bool output_rms_sensor_enabled_{false};
  int warmup_remaining_{3};
  std::atomic<uint32_t> frame_count_{0};
  std::atomic<uint32_t> glitch_count_{0};
  // Feed/fetch diagnostics.
  std::atomic<uint32_t> input_ring_drop_{0}; // process() could not enqueue (NOSPLIT full)
  std::atomic<uint32_t> feed_ok_{0};         // feed_task successfully called feed()
  std::atomic<uint32_t> feed_rejected_{0};   // feed() returned <= 0
  std::atomic<uint32_t> fetch_ok_{0};        // fetch task drained a frame
  std::atomic<uint32_t> fetch_timeout_{0};   // fetch_with_delay timed out
  std::atomic<uint32_t> output_ring_drop_{0};// fetch_output_ring_ full
  std::atomic<uint32_t> feed_queue_frames_{0};
  std::atomic<uint32_t> feed_queue_peak_{0};
  std::atomic<uint32_t> fetch_queue_frames_{0};
  std::atomic<uint32_t> fetch_queue_peak_{0};
  std::atomic<uint32_t> process_us_last_{0};
  std::atomic<uint32_t> process_us_max_{0};
  std::atomic<uint32_t> feed_us_last_{0};
  std::atomic<uint32_t> feed_us_max_{0};
  std::atomic<uint32_t> fetch_us_last_{0};
  std::atomic<uint32_t> fetch_us_max_{0};
  std::atomic<float> ringbuf_free_pct_{1.0f};
  std::atomic<uint32_t> frame_spec_revision_{0};
  int last_spec_mic_ch_{1};  // last published mic_channels for revision tracking (1 = default mono)
  int last_spec_process_size_{0};
  int last_spec_fetch_size_{0};
};

class AfeSwitchBase : public switch_::Switch, public Component, public Parented<EspAfe> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->get_parent_state_());
    }
  }

 protected:
  virtual bool get_parent_state_() const = 0;

  void publish_parent_state_() {
    if (this->parent_ != nullptr) {
      this->publish_state(this->get_parent_state_());
    }
  }
};

// Switch platform classes
class AfeAecSwitch : public AfeSwitchBase {
 public:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;
    if ((state && this->parent_->enable_aec()) || (!state && this->parent_->disable_aec())) {
      this->publish_state(state);
    } else {
      this->publish_parent_state_();
    }
  }

 protected:
  bool get_parent_state_() const override { return this->parent_->is_aec_enabled(); }
};

class AfeNsSwitch : public AfeSwitchBase {
 public:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;
    if ((state && this->parent_->enable_ns()) || (!state && this->parent_->disable_ns())) {
      this->publish_state(state);
    } else {
      this->publish_parent_state_();
    }
  }

 protected:
  bool get_parent_state_() const override { return this->parent_->is_ns_enabled(); }
};

class AfeSeSwitch : public AfeSwitchBase {
 public:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;
    if ((state && this->parent_->enable_se()) || (!state && this->parent_->disable_se())) {
      this->publish_state(state);
    } else {
      this->publish_parent_state_();
    }
  }

 protected:
  bool get_parent_state_() const override { return this->parent_->is_se_enabled(); }
};

class AfeVadSwitch : public AfeSwitchBase {
 public:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;
    if ((state && this->parent_->enable_vad()) || (!state && this->parent_->disable_vad())) {
      this->publish_state(state);
    } else {
      this->publish_parent_state_();
    }
  }

 protected:
  bool get_parent_state_() const override { return this->parent_->is_vad_enabled(); }
};

class AfeAgcSwitch : public AfeSwitchBase {
 public:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;
    if ((state && this->parent_->enable_agc()) || (!state && this->parent_->disable_agc())) {
      this->publish_state(state);
    } else {
      this->publish_parent_state_();
    }
  }

 protected:
  bool get_parent_state_() const override { return this->parent_->is_agc_enabled(); }
};

class AfeVadBinarySensor : public binary_sensor::BinarySensor, public PollingComponent, public Parented<EspAfe> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->is_voice_present());
    }
  }

  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->is_voice_present());
    }
  }
};

class AfeInputVolumeSensor : public sensor::Sensor, public PollingComponent, public Parented<EspAfe> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_input_volume_dbfs());
    }
  }
};

class AfeOutputRmsSensor : public sensor::Sensor, public PollingComponent, public Parented<EspAfe> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_output_rms_dbfs());
    }
  }
};

// Action: esp_afe.set_mode
template<typename... Ts>
class SetModeAction : public Action<Ts...>, public Parented<EspAfe> {
 public:
  TEMPLATABLE_VALUE(std::string, mode)
  void play(const Ts &...x) override {
    this->parent_->reinit_by_name(this->mode_.value(x...));
  }
};

}  // namespace esp_afe
}  // namespace esphome

#endif  // USE_ESP32
