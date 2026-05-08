#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "intercom_audio.h"

namespace esphome {
namespace intercom_audio {

class IntercomAudioTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void update() override {
    if (this->parent_ == nullptr) return;

    const char *state_str = this->parent_->is_streaming() ? "STREAMING" : "IDLE";
    this->publish_state(state_str);
  }

  void set_parent(IntercomAudio *parent) { this->parent_ = parent; }

 protected:
  IntercomAudio *parent_{nullptr};
};

// Mode text sensor - shows "TX Only", "RX Only", "Full Duplex", etc.
class IntercomAudioModeTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void setup() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_mode_str());
    }
  }

  void set_parent(IntercomAudio *parent) { this->parent_ = parent; }

 protected:
  IntercomAudio *parent_{nullptr};
};

}  // namespace intercom_audio
}  // namespace esphome
