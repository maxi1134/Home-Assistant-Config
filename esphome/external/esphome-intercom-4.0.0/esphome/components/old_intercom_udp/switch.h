#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "intercom_audio.h"

namespace esphome {
namespace intercom_audio {

class IntercomAudioSwitch : public switch_::Switch, public Component {
 public:
  void setup() override {}

  void write_state(bool state) override {
    if (this->parent_ == nullptr) return;

    if (state) {
      this->parent_->start();
    } else {
      this->parent_->stop();
    }
    this->publish_state(state);
  }

  void set_parent(IntercomAudio *parent) { this->parent_ = parent; }

 protected:
  IntercomAudio *parent_{nullptr};
};

class IntercomAudioAecSwitch : public switch_::Switch, public Component {
 public:
  void setup() override {
    // Publish initial state
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->is_aec_enabled());
    }
  }

  void write_state(bool state) override {
    if (this->parent_ == nullptr) return;
    this->parent_->set_aec_enabled(state);
    this->publish_state(state);
  }

  void set_parent(IntercomAudio *parent) { this->parent_ = parent; }

 protected:
  IntercomAudio *parent_{nullptr};
};

}  // namespace intercom_audio
}  // namespace esphome
