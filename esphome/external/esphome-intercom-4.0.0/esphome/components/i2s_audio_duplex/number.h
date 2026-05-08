#pragma once

#ifdef USE_ESP32

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "i2s_audio_duplex.h"
#include <cmath>

namespace esphome {
namespace i2s_audio_duplex {

class MicGainNumber : public number::Number, public Component {
 public:
  void set_parent(I2SAudioDuplex *parent) { this->parent_ = parent; }
  void set_pre_aec(bool pre_aec) { this->pre_aec_ = pre_aec; }

  void setup() override {
    float value;
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    if (this->pref_.load(&value)) {
      this->control(value);
    } else {
      this->publish_state(0.0f);  // 0 dB = unity gain
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("mic_gain", "Mic Gain Number (dB, %s)", this->pre_aec_ ? "pre-AEC" : "post-AEC");
  }

 protected:
  void control(float value) override {
    if (this->parent_ != nullptr) {
      float linear = std::pow(10.0f, value / 20.0f);
      if (this->pre_aec_) {
        this->parent_->set_mic_attenuation(linear);
      } else {
        this->parent_->set_mic_gain(linear);
      }
      this->publish_state(value);
      this->pref_.save(&value);
    }
  }

  I2SAudioDuplex *parent_{nullptr};
  ESPPreferenceObject pref_;
  bool pre_aec_{false};
};

class SpeakerVolumeNumber : public number::Number, public Component {
 public:
  void set_parent(I2SAudioDuplex *parent) { this->parent_ = parent; }

  void setup() override {
    float value;
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    if (this->pref_.load(&value)) {
      this->control(value);
    } else if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_speaker_volume());
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("speaker_volume", "Speaker Volume Number");
  }

 protected:
  void control(float value) override {
    if (this->parent_ != nullptr) {
      this->parent_->set_speaker_volume(value);
      this->publish_state(value);
      this->pref_.save(&value);
    }
  }

  I2SAudioDuplex *parent_{nullptr};
  ESPPreferenceObject pref_;
};

}  // namespace i2s_audio_duplex
}  // namespace esphome

#endif  // USE_ESP32
