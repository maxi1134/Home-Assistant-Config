#pragma once

#ifdef USE_ESP32

#include "esphome/components/number/number.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esp_audio_stack.h"
#include <cmath>

namespace esphome {
namespace esp_audio_stack {

class MicGainNumber : public number::Number, public Component {
 public:
  void set_parent(ESPAudioStack *parent) { this->parent_ = parent; }
  void set_min_db(float min_db) { this->min_db_ = min_db; }
  void set_max_db(float max_db) { this->max_db_ = max_db; }

  void setup() override {
    float value;
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    if (this->pref_.load(&value)) {
      value = clamp_db_(value);
      this->apply_(value);
      this->publish_state(value);
    } else {
      this->publish_state(this->clamp_db_(0.0f));  // 0 dB = unity gain when allowed
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("audio_stack.mic_gain", "Mic Gain Number (post-processor dB, range %.1f..%.1f)",
                  this->min_db_, this->max_db_);
  }

 protected:
  float clamp_db_(float value) const {
    if (!std::isfinite(value)) return 0.0f;
    if (value < this->min_db_) return this->min_db_;
    if (value > this->max_db_) return this->max_db_;
    return value;
  }

  void apply_(float value) {
    if (this->parent_ != nullptr) {
      float linear = std::pow(10.0f, value / 20.0f);
      this->parent_->set_mic_gain(linear);
    }
  }

  void control(float value) override {
    if (this->parent_ != nullptr) {
      value = clamp_db_(value);
      this->apply_(value);
      this->publish_state(value);
      this->pref_.save(&value);
    }
  }

  ESPAudioStack *parent_{nullptr};
  ESPPreferenceObject pref_;
  float min_db_{-20.0f};
  float max_db_{30.0f};
};

class MasterVolumeNumber : public number::Number, public Component {
 public:
  void set_parent(ESPAudioStack *parent) { this->parent_ = parent; }
  void set_speaker(speaker::Speaker *speaker) { this->speaker_ = speaker; }

  void setup() override {
    float value;
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    if (this->pref_.load(&value)) {
      value = clamp_percent_(value);
      this->apply_(value);
      this->publish_state(value);
    } else if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_master_volume() * 100.0f);
    } else if (this->speaker_ != nullptr) {
      this->publish_state(this->speaker_->get_volume() * 100.0f);
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("audio_stack.master_volume", "Master Volume Number%s",
                  this->speaker_ != nullptr ? " (speaker-backed)" : "");
  }

 protected:
  static float clamp_percent_(float value) {
    if (!std::isfinite(value)) return 0.0f;
    if (value < 0.0f) return 0.0f;
    if (value > 100.0f) return 100.0f;
    return value;
  }

  void apply_(float value) {
    float volume = value / 100.0f;
    if (this->parent_ != nullptr) {
      this->parent_->set_master_volume(volume);
    } else if (this->speaker_ != nullptr) {
      this->speaker_->set_volume(volume);
    }
  }

  void control(float value) override {
    if (this->speaker_ != nullptr || this->parent_ != nullptr) {
      value = clamp_percent_(value);
      this->apply_(value);
      this->publish_state(value);
      this->pref_.save(&value);
    }
  }

  ESPAudioStack *parent_{nullptr};
  speaker::Speaker *speaker_{nullptr};
  ESPPreferenceObject pref_;
};

}  // namespace esp_audio_stack
}  // namespace esphome

#endif  // USE_ESP32
