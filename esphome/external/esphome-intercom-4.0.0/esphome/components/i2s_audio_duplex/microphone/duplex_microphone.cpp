#include "duplex_microphone.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

namespace esphome {
namespace i2s_audio_duplex {

static const char *const TAG = "i2s_duplex.mic";

void I2SAudioDuplexMicrophone::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2S Audio Duplex Microphone...");

  // Counting semaphore for listener refcounting (take to decrement, give
  // to increment). Initial count == max count == MAX_LISTENERS.
  this->active_listeners_semaphore_ = xSemaphoreCreateCounting(MAX_LISTENERS, MAX_LISTENERS);
  if (this->active_listeners_semaphore_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create semaphore");
    this->mark_failed();
    return;
  }

  // Event group to sync start/stop transitions.
  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    vSemaphoreDelete(this->active_listeners_semaphore_);
    this->active_listeners_semaphore_ = nullptr;
    this->mark_failed();
    return;
  }
  xEventGroupSetBits(this->event_group_, EVENT_STOPPED);

  // Configure audio stream info for 16-bit mono PCM at output rate
  // When decimation is active, mic delivers data at output_sample_rate (e.g. 16kHz)
  // AudioStreamInfo constructor: (bits_per_sample, channels, sample_rate)
  this->audio_stream_info_ = audio::AudioStreamInfo(16, 1, this->parent_->get_output_sample_rate());

  // Pre-allocate callback buffer to avoid heap allocation in the RT audio task.
  // 2048 bytes covers worst case (1024 samples mono 16-bit, no decimation).
  this->audio_buffer_.reserve(2048);

  // Register callback with the parent I2SAudioDuplex to receive mic data
  // pre_aec=true: raw mic for wake word detection (not suppressed by AEC)
  // pre_aec=false: AEC-processed mic for voice assistant STT
  if (this->pre_aec_) {
    this->parent_->add_raw_mic_data_callback(
        [this](const uint8_t *data, size_t len) { this->on_audio_data_(data, len); });
  } else {
    this->parent_->add_mic_data_callback(
        [this](const uint8_t *data, size_t len) { this->on_audio_data_(data, len); });
  }
}

void I2SAudioDuplexMicrophone::dump_config() {
  ESP_LOGCONFIG(TAG, "I2S Audio Duplex Microphone:");
  ESP_LOGCONFIG(TAG, "  Sample Rate: %u Hz", this->parent_->get_output_sample_rate());
  ESP_LOGCONFIG(TAG, "  Bits Per Sample: 16");
  ESP_LOGCONFIG(TAG, "  Channels: 1 (mono)");
}

void I2SAudioDuplexMicrophone::start() {
  if (this->is_failed())
    return;

  // Take semaphore to register as active listener
  if (xSemaphoreTake(this->active_listeners_semaphore_, 0) != pdTRUE) {
    ESP_LOGW(TAG, "No free semaphore slots");
    return;
  }

  // Signal start request and wait for loop() to process it
  if (this->event_group_ != nullptr) {
    xEventGroupClearBits(this->event_group_, EVENT_STOPPED);
    xEventGroupWaitBits(this->event_group_, EVENT_STARTED, false, true, pdMS_TO_TICKS(100));
  }
}

void I2SAudioDuplexMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED || this->is_failed())
    return;

  // Give semaphore to unregister as listener
  xSemaphoreGive(this->active_listeners_semaphore_);

  // Wait for loop() to process stop transition
  if (this->event_group_ != nullptr) {
    xEventGroupClearBits(this->event_group_, EVENT_STARTED);
    xEventGroupWaitBits(this->event_group_, EVENT_STOPPED, false, true, pdMS_TO_TICKS(100));
  }
}

void I2SAudioDuplexMicrophone::on_audio_data_(const uint8_t *data, size_t len) {
  if (this->state_ != microphone::STATE_RUNNING) {
    return;
  }

  if (this->mute_state_) {
    // Mute: drop the frame entirely. Both MWW and VoiceAssistant write incoming
    // data to a ring buffer with no watchdog on absence; skipping dispatch is
    // equivalent to silence at the consumer (no detection, no STT) and saves
    // a 2 KB memset plus the downstream inference cycles on the MWW task.
    return;
  }
  this->audio_buffer_.assign(data, data + len);
  this->data_callbacks_.call(this->audio_buffer_);
}

void I2SAudioDuplexMicrophone::loop() {
  // Propagate I2S errors from parent audio task
  if (this->parent_->has_i2s_error() && !this->status_has_error()) {
    ESP_LOGE(TAG, "I2S error detected in audio task");
    this->status_set_error(LOG_STR("I2S read error in audio task"));
  }

  // Check semaphore count to decide when to start/stop
  UBaseType_t count = uxSemaphoreGetCount(this->active_listeners_semaphore_);

  // Start the microphone if any semaphores are taken (listeners active)
  if ((count < MAX_LISTENERS) && (this->state_ == microphone::STATE_STOPPED)) {
    this->state_ = microphone::STATE_STARTING;
  }

  // Stop the microphone if all semaphores are returned (no listeners)
  if ((count == MAX_LISTENERS) && (this->state_ == microphone::STATE_RUNNING)) {
    this->state_ = microphone::STATE_STOPPING;
  }

  switch (this->state_) {
    case microphone::STATE_STARTING:
      if (this->status_has_error()) {
        break;
      }
      ESP_LOGI(TAG, "Microphone started");
      this->parent_->register_mic_consumer(this);
      this->state_ = microphone::STATE_RUNNING;
      if (this->event_group_ != nullptr) {
        xEventGroupSetBits(this->event_group_, EVENT_STARTED);
      }
      break;

    case microphone::STATE_RUNNING:
      break;

    case microphone::STATE_STOPPING:
      ESP_LOGI(TAG, "Microphone stopped");
      this->parent_->unregister_mic_consumer(this);
      this->state_ = microphone::STATE_STOPPED;
      if (this->event_group_ != nullptr) {
        xEventGroupSetBits(this->event_group_, EVENT_STOPPED);
      }
      break;

    case microphone::STATE_STOPPED:
      break;
  }
}

}  // namespace i2s_audio_duplex
}  // namespace esphome

#endif  // USE_ESP32
