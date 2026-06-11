#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/microphone/microphone.h"
#include "../esp_audio_stack.h"

#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace esphome {
namespace esp_audio_stack {

class ESPAudioStackMicrophone : public microphone::Microphone,
                                  public Component,
                                  public Parented<ESPAudioStack> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // microphone::Microphone interface
  void start() override;
  void stop() override;

 protected:
  void on_audio_data_(const uint8_t *data, size_t len);

  std::vector<uint8_t> audio_buffer_;

  // Reference counting for multiple listeners (voice_assistant, wake_word, intercom, etc.)
  SemaphoreHandle_t active_listeners_semaphore_{nullptr};
};

}  // namespace esp_audio_stack
}  // namespace esphome

#endif  // USE_ESP32
