#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/microphone/microphone.h"
#include "../i2s_audio_duplex.h"

#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

namespace esphome {
namespace i2s_audio_duplex {

class I2SAudioDuplexMicrophone : public microphone::Microphone,
                                  public Component,
                                  public Parented<I2SAudioDuplex> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // microphone::Microphone interface
  void start() override;
  void stop() override;

  void set_pre_aec(bool pre_aec) { this->pre_aec_ = pre_aec; }

 protected:
  void on_audio_data_(const uint8_t *data, size_t len);

  bool pre_aec_{false};  // If true, receives raw (pre-AEC) mic data for wake word detection
  std::vector<uint8_t> audio_buffer_;

  // Reference counting for multiple listeners (voice_assistant, wake_word, intercom, etc.)
  SemaphoreHandle_t active_listeners_semaphore_{nullptr};

  // Event group for reliable start/stop synchronization
  EventGroupHandle_t event_group_{nullptr};
  static constexpr EventBits_t EVENT_STARTED = (1 << 0);
  static constexpr EventBits_t EVENT_STOPPED = (1 << 1);
};

}  // namespace i2s_audio_duplex
}  // namespace esphome

#endif  // USE_ESP32
