#pragma once

#ifdef USE_ESP32

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "i2s_audio_duplex.h"

namespace esphome {
namespace i2s_audio_duplex {

class AECSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(I2SAudioDuplex *parent) { this->parent_ = parent; }

  void setup() override {
    // Initialize state from parent
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->is_processor_enabled());
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("aec_switch", "AEC Switch");
  }

 protected:
  void write_state(bool state) override {
    if (this->parent_ != nullptr) {
      this->parent_->set_processor_enabled(state);
      this->publish_state(state);
    }
  }

  I2SAudioDuplex *parent_{nullptr};
};

}  // namespace i2s_audio_duplex
}  // namespace esphome

#endif  // USE_ESP32
