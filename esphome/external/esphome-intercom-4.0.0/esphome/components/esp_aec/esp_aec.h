#pragma once

#include "../audio_processor/audio_processor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

#include <esp_aec.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace esphome {
namespace esp_aec {

using audio_processor::AudioFeature;
using audio_processor::AudioProcessor;
using audio_processor::FeatureControl;
using audio_processor::FrameSpec;
using audio_processor::ProcessorTelemetry;

/// Standalone Espressif AEC wrapper.
///
/// Implements AudioProcessor with only the echo canceller enabled
/// (WebRTC or neural, depending on mode). Lower memory footprint than
/// EspAfe. Intended for devices that only need echo cancellation for an
/// intercom TX path without the full AFE pipeline.
///
/// Consumers (i2s_audio_duplex, intercom_api) see AEC as just another
/// AudioProcessor; picking EspAec vs EspAfe is a YAML choice.
class EspAec : public Component, public AudioProcessor {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  // Config setters (called from Python)
  void set_sample_rate(int sample_rate) { this->sample_rate_ = sample_rate; }
  void set_filter_length(int filter_length) { this->filter_length_ = filter_length; }
  void set_mode(int mode) { this->mode_ = static_cast<aec_mode_t>(mode); }

  // AudioProcessor interface
  bool is_initialized() const override { return this->handle_ != nullptr; }
  FrameSpec frame_spec() const override;
  bool process(const int16_t *in_mic, const int16_t *in_ref, int16_t *out,
               uint8_t mic_channels_in = 1) override;
  FeatureControl feature_control(AudioFeature feature) const override;
  bool set_feature(AudioFeature feature, bool enabled) override;
  ProcessorTelemetry telemetry() const override;
  bool reconfigure(int type, int mode) override;
  uint32_t frame_spec_revision() const override {
    return this->frame_spec_revision_.load(std::memory_order_acquire);
  }

  aec_mode_t get_mode() const { return this->mode_; }
  static const char *get_mode_name(aec_mode_t mode);
  const char *get_mode_name() const { return get_mode_name(this->mode_); }

  ~EspAec() override;

 protected:
  bool reinit_(aec_mode_t new_mode);

  aec_handle_t *handle_{nullptr};
  // L1 fix: guards handle_ lifecycle between process() and reinit_() / destructor.
  SemaphoreHandle_t handle_mutex_{nullptr};
  int sample_rate_{16000};
  int filter_length_{4};
  int cached_frame_size_{512};
  aec_mode_t mode_{AEC_MODE_SR_LOW_COST};
  std::atomic<uint32_t> frame_count_{0};
  std::atomic<uint32_t> glitch_count_{0};
  std::atomic<uint32_t> frame_spec_revision_{0};
  int last_frame_size_{0};
};

// Action: esp_aec.set_mode
template<typename... Ts>
class SetModeAction : public Action<Ts...>, public Parented<EspAec> {
 public:
  TEMPLATABLE_VALUE(std::string, mode)
  void play(const Ts &...x) override {
    std::string name = this->mode_.value(x...);
    aec_mode_t m;
    if (name == "sr_low_cost") m = AEC_MODE_SR_LOW_COST;
    else if (name == "sr_high_perf") m = AEC_MODE_SR_HIGH_PERF;
    else if (name == "voip_low_cost") m = static_cast<aec_mode_t>(3);
    else if (name == "voip_high_perf") m = static_cast<aec_mode_t>(4);
    else return;
    this->parent_->reconfigure(
        (m <= AEC_MODE_SR_HIGH_PERF) ? 0 : 1,  // type: SR=0, VC=1
        (m == AEC_MODE_SR_LOW_COST || m == static_cast<aec_mode_t>(3)) ? 0 : 1  // mode: LOW=0, HIGH=1
    );
  }
};

}  // namespace esp_aec
}  // namespace esphome

#endif  // USE_ESP32
