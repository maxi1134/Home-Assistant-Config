#pragma once

#ifdef USE_ESP32

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esp_audio_stack.h"

namespace esphome {
namespace esp_audio_stack {

class TdmSlotLevelSensor : public sensor::Sensor, public PollingComponent, public Parented<ESPAudioStack> {
 public:
  void set_slot(uint8_t slot) { this->slot_ = slot; }

  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_tdm_slot_level_dbfs(this->slot_));
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("audio_stack.tdm_slot_sensor", "ESP Audio Stack TDM Slot %u Level Sensor", this->slot_);
  }

 protected:
  uint8_t slot_{0};
};

}  // namespace esp_audio_stack
}  // namespace esphome

#endif  // USE_ESP32
