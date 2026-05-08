#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "intercom_audio.h"

namespace esphome {
namespace intercom_audio {

class IntercomAudioSensor : public sensor::Sensor, public PollingComponent {
 public:
  void update() override {
    if (this->parent_ == nullptr) return;

    switch (this->sensor_type_) {
      case 0:  // TX packets
        this->publish_state(this->parent_->get_tx_packets());
        break;
      case 1:  // RX packets
        this->publish_state(this->parent_->get_rx_packets());
        break;
      case 2:  // Buffer fill
        this->publish_state(this->parent_->get_buffer_fill());
        break;
    }
  }

  void set_parent(IntercomAudio *parent) { this->parent_ = parent; }
  void set_sensor_type(uint8_t type) { this->sensor_type_ = type; }

 protected:
  IntercomAudio *parent_{nullptr};
  uint8_t sensor_type_{0};
};

}  // namespace intercom_audio
}  // namespace esphome
