#include "duplex_speaker.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

namespace esphome {
namespace i2s_audio_duplex {

static const char *const TAG = "i2s_duplex.spk";

// dB volume curve matching upstream i2s_audio speaker (silence + 49dB..0dB in 0.5dB steps).
// Q15 fixed-point factors from esphome/components/i2s_audio/speaker/i2s_audio_speaker.cpp
static const int16_t Q15_VOLUME_FACTORS[] = {
    0,     116,   122,   130,   137,   146,   154,   163,   173,   183,   194,   206,   218,   231,   244,
    259,   274,   291,   308,   326,   345,   366,   388,   411,   435,   461,   488,   517,   548,   580,
    615,   651,   690,   731,   774,   820,   868,   920,   974,   1032,  1094,  1158,  1227,  1300,  1377,
    1459,  1545,  1637,  1734,  1837,  1946,  2061,  2184,  2313,  2450,  2596,  2750,  2913,  3085,  3269,
    3462,  3668,  3885,  4116,  4360,  4619,  4893,  5183,  5490,  5816,  6161,  6527,  6914,  7324,  7758,
    8218,  8706,  9222,  9770,  10349, 10963, 11613, 12302, 13032, 13805, 14624, 15491, 16410, 17384, 18415,
    19508, 20665, 21891, 23189, 24565, 26022, 27566, 29201, 30933, 32767};
static constexpr size_t Q15_VOLUME_FACTORS_COUNT = sizeof(Q15_VOLUME_FACTORS) / sizeof(Q15_VOLUME_FACTORS[0]);

// Convert linear volume (0.0-1.0) to dB-scaled float factor using the Q15 table.
static float volume_to_db_factor(float volume) {
  if (volume <= 0.0f) return 0.0f;
  if (volume >= 1.0f) return 1.0f;
  size_t idx = static_cast<size_t>(volume * (Q15_VOLUME_FACTORS_COUNT - 1));
  if (idx >= Q15_VOLUME_FACTORS_COUNT) idx = Q15_VOLUME_FACTORS_COUNT - 1;
  return static_cast<float>(Q15_VOLUME_FACTORS[idx]) / 32767.0f;
}

void I2SAudioDuplexSpeaker::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2S Audio Duplex Speaker...");

  this->active_listeners_semaphore_ = xSemaphoreCreateCounting(MAX_LISTENERS, MAX_LISTENERS);
  if (this->active_listeners_semaphore_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create semaphore");
    this->mark_failed();
    return;
  }

  this->audio_stream_info_ = audio::AudioStreamInfo(16, 1, this->parent_->get_sample_rate());

  // Forward frame-played notifications from I2S audio task to mixer callbacks.
  // Without this, mixer source speakers can't track pending_playback_frames.
  this->parent_->add_speaker_output_callback([this](uint32_t frames, int64_t timestamp) {
    this->audio_output_callback_.call(frames, timestamp);
  });
}

void I2SAudioDuplexSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG, "I2S Audio Duplex Speaker:");
  ESP_LOGCONFIG(TAG, "  Sample Rate: %u Hz", this->parent_->get_sample_rate());
  ESP_LOGCONFIG(TAG, "  Bits Per Sample: 16");
  ESP_LOGCONFIG(TAG, "  Channels: 1 (mono)");
}

void I2SAudioDuplexSpeaker::start() {
  if (this->is_failed())
    return;

  // Clear finishing flag on new stream start
  this->finishing_ = false;

  // Idempotent: register listener only once per stream session.
  bool expected = false;
  if (!this->listener_registered_.compare_exchange_strong(expected, true))
    return;

  if (xSemaphoreTake(this->active_listeners_semaphore_, 0) != pdTRUE) {
    this->listener_registered_.store(false);
    ESP_LOGW(TAG, "No free semaphore slots");
    return;
  }
}

void I2SAudioDuplexSpeaker::stop() {
  if (this->is_failed())
    return;

  if (!this->listener_registered_.exchange(false))
    return;

  xSemaphoreGive(this->active_listeners_semaphore_);
}

void I2SAudioDuplexSpeaker::finish() {
  // Non-blocking: set flag, loop() will call stop() once buffer is drained
  this->finishing_ = true;
}

size_t I2SAudioDuplexSpeaker::play(const uint8_t *data, size_t length) {
  return this->play(data, length, 0);
}

size_t I2SAudioDuplexSpeaker::play(const uint8_t *data, size_t length,
                                    TickType_t ticks_to_wait) {
  if (this->state_ != speaker::STATE_RUNNING) {
    this->start();
  }

  return this->parent_->play(data, length, ticks_to_wait);
}

bool I2SAudioDuplexSpeaker::has_buffered_data() const {
  return this->parent_->get_speaker_buffer_available() > 0;
}

void I2SAudioDuplexSpeaker::set_volume(float volume) {
  speaker::Speaker::set_volume(volume);

#ifdef USE_AUDIO_DAC
  if (this->audio_dac_ != nullptr) {
    if (volume > 0.0f) {
      this->audio_dac_->set_mute_off();
    }
    this->audio_dac_->set_volume(volume);
  } else
#endif
  {
    this->parent_->set_speaker_volume(volume_to_db_factor(volume));
  }
}

void I2SAudioDuplexSpeaker::set_mute_state(bool mute_state) {
  speaker::Speaker::set_mute_state(mute_state);

#ifdef USE_AUDIO_DAC
  if (this->audio_dac_ != nullptr) {
    if (mute_state) {
      this->audio_dac_->set_mute_on();
    } else {
      this->audio_dac_->set_mute_off();
    }
  } else
#endif
  {
    if (mute_state) {
      this->parent_->set_speaker_volume(0.0f);
    } else {
      this->parent_->set_speaker_volume(volume_to_db_factor(this->volume_));
    }
  }
}

void I2SAudioDuplexSpeaker::set_pause_state(bool pause_state) {
  this->pause_state_ = pause_state;
  this->parent_->set_speaker_paused(pause_state);
  ESP_LOGD(TAG, "Pause state: %s", pause_state ? "PAUSED" : "PLAYING");
}

void I2SAudioDuplexSpeaker::loop() {
  // Propagate I2S errors from parent audio task
  if (this->parent_->has_i2s_error() && !this->status_has_error()) {
    ESP_LOGE(TAG, "I2S error detected in audio task");
    this->status_set_error(LOG_STR("I2S write error in audio task"));
  }

  UBaseType_t count = uxSemaphoreGetCount(this->active_listeners_semaphore_);

  if ((count < MAX_LISTENERS) && (this->state_ == speaker::STATE_STOPPED)) {
    this->state_ = speaker::STATE_STARTING;
  }

  if ((count == MAX_LISTENERS) && (this->state_ == speaker::STATE_RUNNING)) {
    this->state_ = speaker::STATE_STOPPING;
  }

  switch (this->state_) {
    case speaker::STATE_STARTING:
      if (this->status_has_error()) {
        break;
      }
      this->parent_->start_speaker();
      this->state_ = speaker::STATE_RUNNING;
      break;

    case speaker::STATE_RUNNING:
      // Non-blocking finish: drain buffer then stop
      if (this->finishing_ && !this->has_buffered_data()) {
        this->finishing_ = false;
        this->stop();
      }
      break;

    case speaker::STATE_STOPPING:
      this->parent_->stop_speaker();
      this->state_ = speaker::STATE_STOPPED;
      break;

    case speaker::STATE_STOPPED:
      break;
  }
}

}  // namespace i2s_audio_duplex
}  // namespace esphome

#endif  // USE_ESP32
