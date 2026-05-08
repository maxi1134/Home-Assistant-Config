#pragma once

#ifdef USE_ESP32

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "i2s_audio_duplex.h"

namespace esphome {
namespace i2s_audio_duplex {

class TdmSlotLevelSensor : public sensor::Sensor, public PollingComponent, public Parented<I2SAudioDuplex> {
 public:
  void set_slot(uint8_t slot) { this->slot_ = slot; }

  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_tdm_slot_level_dbfs(this->slot_));
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("i2s_tdm_slot_sensor", "I2S Audio Duplex TDM Slot %u Level Sensor", this->slot_);
  }

 protected:
  uint8_t slot_{0};
};

}  // namespace i2s_audio_duplex
}  // namespace esphome

#endif  // USE_ESP32
