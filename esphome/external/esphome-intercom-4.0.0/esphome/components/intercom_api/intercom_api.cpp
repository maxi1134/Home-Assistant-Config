#include "intercom_api.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../audio_processor/audio_utils.h"
#include "../audio_processor/ring_buffer_caps.h"

namespace {
constexpr size_t MIC_CONVERTED_SAMPLES = 512;
// speaker_task_ batches up to AUDIO_CHUNK_SIZE * 4 bytes per iteration (intercom_api.cpp speaker_task_),
// scale loop writes one int16 per input byte/2, so the buffer must hold AUDIO_CHUNK_SIZE * 4 / 2 samples.
constexpr size_t SPK_REF_SCALED_SAMPLES = (esphome::intercom_api::AUDIO_CHUNK_SIZE * 4) / sizeof(int16_t);
}  // namespace

namespace esphome {
namespace intercom_api {

static const char *const TAG = "intercom_api";

void IntercomApi::setup() {
  ESP_LOGI(TAG, "Setting up Intercom API...");

  // Transactional setup: any early failure triggers cleanup to prevent orphaned
  // tasks, leaked buffers, or dangling mutexes when mark_failed() is called.
  auto cleanup_partial = [this]() {
    if (this->speaker_task_handle_ != nullptr) {
      vTaskDelete(this->speaker_task_handle_);
      this->speaker_task_handle_ = nullptr;
    }
    if (this->tx_task_handle_ != nullptr) {
      vTaskDelete(this->tx_task_handle_);
      this->tx_task_handle_ = nullptr;
    }
    if (this->server_task_handle_ != nullptr) {
      vTaskDelete(this->server_task_handle_);
      this->server_task_handle_ = nullptr;
    }
    // Static-task stacks are only allocated when tasks_stack_in_psram_ is
    // true. In dynamic mode the stack pointers stay null and FreeRTOS
    // releases the heap-resident stack as part of vTaskDelete above.
    if (this->tasks_stack_in_psram_) {
      RAMAllocator<StackType_t> psram_stack(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
      if (this->speaker_task_stack_ != nullptr) {
        psram_stack.deallocate(this->speaker_task_stack_, IntercomApi::kSpeakerTaskStackWords);
        this->speaker_task_stack_ = nullptr;
      }
      if (this->tx_task_stack_ != nullptr) {
        psram_stack.deallocate(this->tx_task_stack_, IntercomApi::kTxTaskStackWords);
        this->tx_task_stack_ = nullptr;
      }
      if (this->server_task_stack_ != nullptr) {
        psram_stack.deallocate(this->server_task_stack_, IntercomApi::kServerTaskStackWords);
        this->server_task_stack_ = nullptr;
      }
    }
    RAMAllocator<int16_t> psram_i16;
    RAMAllocator<uint8_t> psram_u8;
    if (this->spk_ref_scaled_ != nullptr) {
      psram_i16.deallocate(this->spk_ref_scaled_, SPK_REF_SCALED_SAMPLES);
      this->spk_ref_scaled_ = nullptr;
    }
    if (this->mic_converted_ != nullptr) {
      psram_i16.deallocate(this->mic_converted_, MIC_CONVERTED_SAMPLES);
      this->mic_converted_ = nullptr;
    }
    if (this->rx_buffer_ != nullptr) {
      psram_u8.deallocate(this->rx_buffer_, MAX_MESSAGE_SIZE);
      this->rx_buffer_ = nullptr;
    }
    if (this->tx_buffer_ != nullptr) {
      psram_u8.deallocate(this->tx_buffer_, MAX_MESSAGE_SIZE);
      this->tx_buffer_ = nullptr;
    }
    this->speaker_buffer_.reset();
    this->mic_buffer_.reset();
#ifdef USE_AUDIO_PROCESSOR
    if (this->spk_ref_mutex_ != nullptr) {
      vSemaphoreDelete(this->spk_ref_mutex_);
      this->spk_ref_mutex_ = nullptr;
    }
#endif
    if (this->speaker_stopped_sem_ != nullptr) {
      vSemaphoreDelete(this->speaker_stopped_sem_);
      this->speaker_stopped_sem_ = nullptr;
    }
    if (this->send_mutex_ != nullptr) {
      vSemaphoreDelete(this->send_mutex_);
      this->send_mutex_ = nullptr;
    }
    if (this->client_mutex_ != nullptr) {
      vSemaphoreDelete(this->client_mutex_);
      this->client_mutex_ = nullptr;
    }
  };

  // Create mutexes
  this->client_mutex_ = xSemaphoreCreateMutex();
  this->send_mutex_ = xSemaphoreCreateMutex();

  if (!this->client_mutex_ || !this->send_mutex_) {
    ESP_LOGE(TAG, "Failed to create mutexes");
    cleanup_partial();
    this->mark_failed();
    return;
  }

  const bool use_intercom_aec = this->has_intercom_processor_();
  ESP_LOGI(TAG, "Intercom Processor: %s (tasks: %s)", use_intercom_aec ? "YES" : "NO",
           use_intercom_aec ? "server+tx+speaker" : "server only");

  if (use_intercom_aec) {
    // Full 3-task mode: need speaker semaphore, speaker_buffer, audio_tx_buffer, spk_ref_scaled
    this->speaker_stopped_sem_ = xSemaphoreCreateBinary();
    if (!this->speaker_stopped_sem_) {
      ESP_LOGE(TAG, "Failed to create speaker semaphore");
      cleanup_partial();
      this->mark_failed();
      return;
    }
  }

  // mic_buffer: mic callback writes, server_task or tx_task reads.
  this->mic_buffer_ = audio_processor::create_prefer_psram(TX_BUFFER_SIZE, "intercom.mic");
  if (!this->mic_buffer_) {
    ESP_LOGE(TAG, "Failed to allocate mic ring buffer");
    cleanup_partial();
    this->mark_failed();
    return;
  }

  if (use_intercom_aec) {
    // speaker_buffer: bridges network to speaker with AEC reference feed.
    this->speaker_buffer_ = audio_processor::create_prefer_psram(RX_BUFFER_SIZE, "intercom.speaker");
    if (!this->speaker_buffer_) {
      ESP_LOGE(TAG, "Failed to allocate speaker ring buffer");
      cleanup_partial();
      this->mark_failed();
      return;
    }
  }

  // Protocol/audio scratch buffers: PSRAM-first (RAMAllocator default falls
  // back to internal when PSRAM is absent). None are DMA-bound.
  RAMAllocator<uint8_t> psram_u8;
  // Frame staging buffers (mic_converted_, spk_ref_scaled_): placement gated
  // by frame_buffers_in_psram_ (same flag that controls aec_mic/ref/out below).
  // Default false = internal RAM, saving ~14 us/frame on the mic path and up
  // to ~54 us/iter on the speaker path when volume scaling is active.
  RAMAllocator<int16_t> psram_i16 = this->frame_buffers_in_psram_
      ? RAMAllocator<int16_t>()
      : RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_INTERNAL);
  this->tx_buffer_ = psram_u8.allocate(MAX_MESSAGE_SIZE);
  this->rx_buffer_ = psram_u8.allocate(MAX_MESSAGE_SIZE);

  if (!this->tx_buffer_ || !this->rx_buffer_) {
    ESP_LOGE(TAG, "Failed to allocate frame buffers");
    cleanup_partial();
    this->mark_failed();
    return;
  }

  this->mic_converted_ = psram_i16.allocate(MIC_CONVERTED_SAMPLES);
  if (!this->mic_converted_) {
    ESP_LOGE(TAG, "Failed to allocate mic processing buffer");
    cleanup_partial();
    this->mark_failed();
    return;
  }

  if (use_intercom_aec) {
    this->spk_ref_scaled_ = psram_i16.allocate(SPK_REF_SCALED_SAMPLES);
    if (!this->spk_ref_scaled_) {
      ESP_LOGE(TAG, "Failed to allocate speaker ref buffer");
      cleanup_partial();
      this->mark_failed();
      return;
    }
  }

  // Setup microphone callback
#ifdef USE_MICROPHONE
  if (this->microphone_source_ != nullptr) {
    this->microphone_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
      this->on_microphone_data_(data.data(), data.size());
    });
  }
#endif

#ifdef USE_AUDIO_PROCESSOR
  // Validate AEC but defer buffer allocation to set_aec_enabled(true)
  // This saves ~13KB of internal RAM when AEC is disabled (the default)
  if (this->aec_ != nullptr && this->aec_->is_initialized()) {
    this->aec_frame_samples_ = this->aec_->frame_spec().input_samples;
    if (this->aec_frame_samples_ <= 0 || this->aec_frame_samples_ > 1024) {
      ESP_LOGW(TAG, "AEC frame_size invalid (%d)", this->aec_frame_samples_);
    } else {
      this->spk_ref_mutex_ = xSemaphoreCreateMutex();
      ESP_LOGI(TAG, "AEC validated: frame_size=%d samples (%dms) - enable via switch",
               this->aec_frame_samples_,
               this->aec_frame_samples_ * 1000 / SAMPLE_RATE);
    }
  }
  this->aec_enabled_ = false;
#endif

  // Two task creation paths:
  //  - Static (tasks_stack_in_psram_=true): stack allocated from PSRAM,
  //    saves ~28 KB of internal heap. Requires a board with PSRAM and
  //    CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y. Recommended for
  //    full-experience S3/P4 builds where every KB of internal RAM
  //    competes with BSS, MWW and LVGL.
  //  - Dynamic (tasks_stack_in_psram_=false, default): standard
  //    xTaskCreatePinnedToCore with stack on the internal heap. Costs
  //    ~28 KB of internal RAM total (8+12+8 KB across the three tasks)
  //    but is the only option on plain ESP32 boards without PSRAM.
  RAMAllocator<StackType_t> psram_stack(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);

  auto create_task = [this, &psram_stack](TaskFunction_t fn, const char *name, uint32_t stack_words,
                                           UBaseType_t prio, BaseType_t core, TaskHandle_t *handle_out,
                                           StaticTask_t *tcb_out, StackType_t **stack_out) -> bool {
    if (this->tasks_stack_in_psram_) {
      *stack_out = psram_stack.allocate(stack_words);
      if (*stack_out == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM stack for %s", name);
        return false;
      }
      *handle_out = xTaskCreateStaticPinnedToCore(fn, name, stack_words, this, prio,
                                                   *stack_out, tcb_out, core);
    } else {
      BaseType_t ok = xTaskCreatePinnedToCore(fn, name, stack_words * sizeof(StackType_t),
                                               this, prio, handle_out, core);
      if (ok != pdPASS) {
        *handle_out = nullptr;
      }
    }
    if (*handle_out == nullptr) {
      ESP_LOGE(TAG, "Failed to create %s task", name);
      return false;
    }
    return true;
  };

  // Server task (Core 1): TCP connections, receive path, plus TX/speaker when
  // no dedicated intercom processor is configured.
  if (!create_task(IntercomApi::server_task, "intercom_srv", IntercomApi::kServerTaskStackWords, 5, 1,
                   &this->server_task_handle_, &this->server_task_tcb_, &this->server_task_stack_)) {
    cleanup_partial();
    this->mark_failed();
    return;
  }

  if (use_intercom_aec) {
    // TX task (Core 0): mic capture and AEC processing when intercom drives its own AEC.
    if (!create_task(IntercomApi::tx_task, "intercom_tx", IntercomApi::kTxTaskStackWords, 5, 0,
                     &this->tx_task_handle_, &this->tx_task_tcb_, &this->tx_task_stack_)) {
      cleanup_partial();
      this->mark_failed();
      return;
    }

    // Speaker task (Core 0): playback and AEC reference feed.
    if (!create_task(IntercomApi::speaker_task, "intercom_spk", IntercomApi::kSpeakerTaskStackWords, 4, 0,
                     &this->speaker_task_handle_, &this->speaker_task_tcb_, &this->speaker_task_stack_)) {
      cleanup_partial();
      this->mark_failed();
      return;
    }
  }

  // Load persisted settings from flash (volume, mic gain, auto-answer, AEC)
  this->load_settings_();

  // Deferred publish of initial sensor values (wait for sensors to be fully ready)
  this->set_timeout(250, [this]() {
    this->publish_state_();
    this->publish_destination_();
  });

  ESP_LOGI(TAG, "Intercom API ready on port %d", INTERCOM_PORT);
}

void IntercomApi::loop() {
  // Main loop - mostly handled by FreeRTOS tasks

  // Check call timeout (if configured and FSM in RINGING or OUTGOING state)
  // Use FSM state to handle case where TCP connection closed but call_state_ is stuck
  // Both timeouts send STOP to the other side to keep both ESPs in sync
  if (this->ringing_timeout_ms_ > 0) {
    uint32_t now = millis();

    // Timeout for RINGING state (incoming call not answered)
    if (this->call_state_ == CallState::RINGING) {
      if (now - this->ringing_start_time_ >= this->ringing_timeout_ms_) {
        ESP_LOGI(TAG, "Ringing timeout after %u ms - sending STOP to caller", this->ringing_timeout_ms_);
        // close_client_socket_() sends STOP before closing
        this->close_client_socket_();
        this->state_ = ConnectionState::DISCONNECTED;
        if (this->full_mode_) {
          this->publish_caller_("");
        }
        this->end_call_(CallEndReason::TIMEOUT);
      }
    }

    // Timeout for OUTGOING state (call not connected/answered)
    // Uses same timeout value as ringing
    if (this->call_state_ == CallState::OUTGOING) {
      if (now - this->outgoing_start_time_ >= this->ringing_timeout_ms_) {
        ESP_LOGI(TAG, "Outgoing call timeout after %u ms - sending STOP", this->ringing_timeout_ms_);
        // close_client_socket_() sends STOP before closing
        this->close_client_socket_();
        this->set_active_(false);  // Stop mic/speaker (start() enabled them)
        this->state_ = ConnectionState::DISCONNECTED;
        this->end_call_(CallEndReason::TIMEOUT);
      }
    }
  }
}

void IntercomApi::dump_config() {
  ESP_LOGCONFIG(TAG, "Intercom API:");
  ESP_LOGCONFIG(TAG, "  Port: %d", INTERCOM_PORT);
#ifdef USE_MICROPHONE
  ESP_LOGCONFIG(TAG, "  Microphone: %s", this->microphone_source_ ? "configured" : "none");
#endif
#ifdef USE_SPEAKER
  ESP_LOGCONFIG(TAG, "  Speaker: %s", this->speaker_ ? "configured" : "none");
#endif
#ifdef USE_AUDIO_PROCESSOR
  if (this->aec_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  AEC: configured (frame_size=%d samples)", this->aec_frame_samples_);
  } else {
    ESP_LOGCONFIG(TAG, "  AEC: none");
  }
#endif
  ESP_LOGCONFIG(TAG, "  Tasks: %s", this->has_intercom_processor_() ?
                "server+tx+speaker" : "server only");
  ESP_LOGCONFIG(TAG, "  Device Name: %s",
                this->device_name_.empty() ? "(unset)" : this->device_name_.c_str());
  if (this->ringing_timeout_ms_ > 0) {
    ESP_LOGCONFIG(TAG, "  Ringing Timeout: %u ms", this->ringing_timeout_ms_);
  } else {
    ESP_LOGCONFIG(TAG, "  Ringing Timeout: disabled");
  }
  ESP_LOGCONFIG(TAG, "  Contacts: %zu configured", this->contacts_.size());
}

void IntercomApi::publish_entity_states() {
  // Restore switch states using ESPHome's restore_mode mechanism
  // Call this from YAML: api: on_client_connected: - lambda: 'id(intercom).publish_entity_states();'

  // Auto-answer switch: restore state and apply to internal flag
  if (this->auto_answer_switch_ != nullptr) {
    auto initial = this->auto_answer_switch_->get_initial_state_with_restore_mode();
    if (initial.has_value()) {
      this->auto_answer_ = *initial;
      this->auto_answer_switch_->publish_state(*initial);
    }
  }

#ifdef USE_AUDIO_PROCESSOR
  // AEC switch: restore state and enable AEC if needed
  if (this->aec_switch_ != nullptr) {
    auto initial = this->aec_switch_->get_initial_state_with_restore_mode();
    if (initial.has_value()) {
      if (*initial) {
        this->set_aec_enabled(true);
      }
      this->aec_switch_->publish_state(this->aec_enabled_);
    }
  }
#endif

#ifdef USE_AUDIO_PROCESSOR
  ESP_LOGI(TAG, "Entity states synced (vol=%.0f%%, mic=%.1fdB, auto=%s, aec=%s)",
           this->volume_.load() * 100.0f, this->mic_gain_db_,
           this->auto_answer_ ? "ON" : "OFF", this->aec_enabled_.load() ? "ON" : "OFF");
#else
  ESP_LOGI(TAG, "Entity states synced (vol=%.0f%%, mic=%.1fdB, auto=%s)",
           this->volume_.load() * 100.0f, this->mic_gain_db_,
           this->auto_answer_ ? "ON" : "OFF");
#endif

  // For numbers: publish our internal values (loaded from flash)
  if (this->volume_number_ != nullptr) {
    this->volume_number_->publish_state(this->volume_.load() * 100.0f);
  }
  if (this->mic_gain_number_ != nullptr) {
    this->mic_gain_number_->publish_state(this->mic_gain_db_);
  }
}

// === Settings persistence ===

void IntercomApi::load_settings_() {
  // Use a fixed hash for the preference key (component doesn't have get_object_id_hash)
  this->settings_pref_ = global_preferences->make_preference<StoredSettings>(fnv1_hash("intercom_api_settings"));

  StoredSettings stored;
  if (this->settings_pref_.load(&stored) && stored.version == SETTINGS_VERSION) {
    this->suppress_save_ = true;  // Don't save while loading

    // Apply volume only when intercom_api owns the speaker_volume number
    // entity. When it is a template number the template owns DAC + AEC
    // reference sync via its own restore + on_boot lambda.
    this->volume_ = stored.volume_pct / 100.0f;
    if (this->volume_number_ != nullptr) {
#ifdef USE_SPEAKER
      if (this->speaker_ != nullptr) {
        this->speaker_->set_volume(this->volume_);
      }
#endif
      ESP_LOGD(TAG, "Loaded volume: %d%%", stored.volume_pct);
    }

    // Apply mic gain only if intercom_api owns the mic_gain number entity.
    // When i2s_audio_duplex provides mic_gain, it handles its own persistence.
    this->mic_gain_db_ = stored.mic_gain_db;
    if (this->mic_gain_number_ != nullptr) {
      this->mic_gain_ = std::pow(10.0f, this->mic_gain_db_ / 20.0f);
      ESP_LOGD(TAG, "Loaded mic_gain: %.1fdB", this->mic_gain_db_);
    }

    // NOTE: auto_answer and AEC are handled by switch restore_mode, not here
    // This avoids conflicts between two persistence mechanisms

    this->suppress_save_ = false;
  } else {
    ESP_LOGD(TAG, "No saved settings, using defaults");
  }
}

void IntercomApi::schedule_save_settings_() {
  if (this->suppress_save_ || this->save_scheduled_) {
    return;
  }
  this->save_scheduled_ = true;
  // Debounce: save after 250ms to avoid rapid writes when slider moves
  this->set_timeout(250, [this]() {
    this->save_scheduled_ = false;
    this->save_settings_();
  });
}

void IntercomApi::save_settings_() {
  StoredSettings stored;
  stored.version = SETTINGS_VERSION;
  stored.volume_pct = static_cast<uint8_t>(std::lround(this->volume_ * 100.0f));
  stored.mic_gain_db = static_cast<int8_t>(std::lround(this->mic_gain_db_));

  this->settings_pref_.save(&stored);
  ESP_LOGD(TAG, "Saved settings: vol=%d%%, mic=%ddB",
           stored.volume_pct, stored.mic_gain_db);
}

void IntercomApi::start() {
  if (this->call_state_ != CallState::IDLE) {
    ESP_LOGW(TAG, "Cannot start call: already %s", call_state_to_str(this->call_state_));
    return;
  }

  const auto &dest = this->get_current_destination();
  ESP_LOGI(TAG, "%s -> %s: calling...", this->device_name_.c_str(), dest.c_str());
  this->set_active_(true);
  this->set_call_state_(CallState::OUTGOING);
  this->outgoing_start_time_ = millis();

  if (this->server_task_handle_) xTaskNotifyGive(this->server_task_handle_);
  if (this->tx_task_handle_) xTaskNotifyGive(this->tx_task_handle_);
  if (this->speaker_task_handle_) xTaskNotifyGive(this->speaker_task_handle_);
}

void IntercomApi::stop() {
  if (!this->active_.load(std::memory_order_acquire) && this->call_state_ == CallState::IDLE) {
    return;
  }

  ESP_LOGI(TAG, "%s: hanging up", this->device_name_.c_str());
  this->set_active_(false);
  this->close_client_socket_();
  this->state_ = ConnectionState::DISCONNECTED;
  this->end_call_(CallEndReason::LOCAL_HANGUP);
}

void IntercomApi::answer_call() {
  if (!this->is_ringing()) {
    ESP_LOGW(TAG, "Cannot answer: not ringing (state=%s)", call_state_to_str(this->call_state_));
    return;
  }

  int sock = this->client_.socket.load();
  if (sock < 0) {
    ESP_LOGW(TAG, "Cannot answer: no connection");
    return;
  }

  ESP_LOGI(TAG, "%s: answering call", this->device_name_.c_str());
  this->send_message_(sock, MessageType::ANSWER);
  this->set_call_state_(CallState::ANSWERING);
  this->set_active_(true);
  this->set_streaming_(true);
}

void IntercomApi::decline_call() {
  if (this->is_ringing()) {
    ESP_LOGI(TAG, "%s: declining incoming call", this->device_name_.c_str());
  } else if (this->is_outgoing()) {
    ESP_LOGI(TAG, "%s: cancelling outgoing call to %s",
             this->device_name_.c_str(), this->get_current_destination().c_str());
  } else {
    ESP_LOGW(TAG, "Cannot decline: no active call (state=%s)", call_state_to_str(this->call_state_));
    return;
  }

  // Send ERROR to remote if connected, then close
  int sock = this->client_.socket.load();
  if (sock >= 0) {
    uint8_t reason = static_cast<uint8_t>(ErrorCode::BUSY);
    this->send_message_(sock, MessageType::ERROR, MessageFlags::NONE, &reason, 1);
    this->close_client_socket_();
  }

  // Always end the call locally (outgoing may not have a socket yet)
  this->set_active_(false);
  this->state_ = ConnectionState::DISCONNECTED;
  this->end_call_(CallEndReason::DECLINED);
}

void IntercomApi::call_toggle() {
  if (this->is_ringing()) {
    this->answer_call();
  } else if (this->is_active()) {
    this->stop();
  } else {
    this->start();
  }
}

void IntercomApi::set_volume(float volume) {
  this->volume_ = std::max(0.0f, std::min(1.0f, volume));
#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    this->speaker_->set_volume(this->volume_);
  }
#endif
  this->schedule_save_settings_();
}

void IntercomApi::set_auto_answer(bool enabled) {
  this->auto_answer_ = enabled;
  ESP_LOGI(TAG, "Auto-answer set to %s", enabled ? "ON" : "OFF");
  // NOTE: persistence handled by switch restore_mode, not save_settings_()
}

void IntercomApi::set_mic_gain_db(float db) {
  // Convert dB to linear gain: gain = 10^(dB/20)
  // Range: -20dB (0.1x) to +20dB (10x)
  db = std::max(-20.0f, std::min(20.0f, db));
  this->mic_gain_db_ = db;
  this->mic_gain_ = std::pow(10.0f, db / 20.0f);
  ESP_LOGD(TAG, "Mic gain set to %.1f dB (%.2fx)", db, this->mic_gain_.load());
  this->schedule_save_settings_();
}

#ifdef USE_AUDIO_PROCESSOR
void IntercomApi::reset_aec_buffers_() {
  if (!this->aec_enabled_ || this->spk_ref_buffer_ == nullptr) return;

  this->aec_mic_fill_ = 0;
  if (this->spk_ref_mutex_ != nullptr &&
      xSemaphoreTake(this->spk_ref_mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    this->spk_ref_buffer_->reset();
    // Pre-fill reference buffer with silence to create delay
    // This compensates for I2S DMA latency + acoustic delay
    // The mic captures echo from audio played N ms ago, so we delay the reference
    // Use stack-based loop instead of heap allocation for the silence block
    const size_t delay_bytes = (SAMPLE_RATE * this->aec_ref_delay_ms_ / 1000) * sizeof(int16_t);
    uint8_t zeros[256] = {};
    size_t remaining = delay_bytes;
    while (remaining > 0) {
      size_t chunk = std::min(remaining, sizeof(zeros));
      this->spk_ref_buffer_->write(zeros, chunk);
      remaining -= chunk;
    }
    ESP_LOGD(TAG, "AEC buffers reset, pre-filled %ums silence", (unsigned) this->aec_ref_delay_ms_);
    xSemaphoreGive(this->spk_ref_mutex_);
  }
}

void IntercomApi::set_aec_enabled(bool enabled) {
  if (enabled) {
    // Only allow enabling if AEC is properly initialized
    if (this->aec_ == nullptr || !this->aec_->is_initialized()) {
      ESP_LOGW(TAG, "Cannot enable AEC: not initialized");
      this->aec_enabled_ = false;
      return;
    }
    // Lazy allocate AEC buffers on first enable (saves ~13KB when AEC unused).
    if (this->aec_mic_ == nullptr) {
      const size_t frame_samples = static_cast<size_t>(this->aec_frame_samples_);
      const size_t ref_delay_bytes = (SAMPLE_RATE * this->aec_ref_delay_ms_ / 1000) * sizeof(int16_t);
      this->spk_ref_buffer_ = audio_processor::create_prefer_psram(
          ref_delay_bytes + RX_BUFFER_SIZE, "intercom.spk_ref");
      // Placement YAML-controlled: internal saves ~20 us/frame on Core 0 (3 KB R+W),
      // PSRAM saves 3 KB internal RAM (set frame_buffers_in_psram). RAMAllocator's
      // default-constructed instance uses INTERNAL|EXTERNAL (PSRAM-first).
      RAMAllocator<int16_t> aec_alloc = this->frame_buffers_in_psram_
          ? RAMAllocator<int16_t>()
          : RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_INTERNAL);
      this->aec_mic_ = aec_alloc.allocate(frame_samples);
      this->aec_ref_ = aec_alloc.allocate(frame_samples);
      this->aec_out_ = aec_alloc.allocate(frame_samples);

      if (!this->spk_ref_buffer_ || !this->aec_mic_ || !this->aec_ref_ || !this->aec_out_) {
        ESP_LOGE(TAG, "AEC buffer allocation failed");
        if (this->aec_mic_) { aec_alloc.deallocate(this->aec_mic_, frame_samples); this->aec_mic_ = nullptr; }
        if (this->aec_ref_) { aec_alloc.deallocate(this->aec_ref_, frame_samples); this->aec_ref_ = nullptr; }
        if (this->aec_out_) { aec_alloc.deallocate(this->aec_out_, frame_samples); this->aec_out_ = nullptr; }
        this->aec_enabled_ = false;
        return;
      }
      ESP_LOGD(TAG, "AEC buffers allocated (frame=%d samples, ref_buf=%zu bytes, delay=%ums)",
               this->aec_frame_samples_, ref_delay_bytes + RX_BUFFER_SIZE,
               (unsigned) this->aec_ref_delay_ms_);
    }
  }
  this->aec_enabled_ = enabled;
  if (enabled) {
    this->reset_aec_buffers_();
  } else {
    this->aec_mic_fill_ = 0;
  }
  ESP_LOGI(TAG, "AEC %s", enabled ? "enabled" : "disabled");
}
#endif


const char *IntercomApi::get_state_str() const {
  // Use CallState for human-readable status (capitalizes first letter)
  switch (this->call_state_) {
    case CallState::IDLE: return "Idle";
    case CallState::OUTGOING: return "Outgoing";
    case CallState::INCOMING: return "Incoming";
    case CallState::RINGING: return "Ringing";
    case CallState::ANSWERING: return "Answering";
    case CallState::STREAMING: return "Streaming";
    default: return "Unknown";
  }
}

void IntercomApi::publish_state_() {
  if (this->state_sensor_ != nullptr) {
    this->state_sensor_->publish_state(this->get_state_str());
  }
}

// === Contacts Management ===

void IntercomApi::set_contacts(const std::string &contacts_csv) {
  // Full mode only - in simple mode, contacts are not used
  if (!this->full_mode_) return;

  // Save current selection to preserve it if possible
  const std::string previous = this->get_current_destination();

  // Parse CSV: "Home Assistant,Intercom Mini,Intercom Xiaozhi"
  // Exclude this device's own name from contacts
  this->contacts_.clear();
  this->contacts_.reserve(16);

  if (contacts_csv.empty()) {
    this->contacts_.push_back("Home Assistant");
  } else {
    size_t start = 0;
    while (start <= contacts_csv.size()) {
      size_t pos = contacts_csv.find(',', start);
      size_t end = (pos == std::string::npos) ? contacts_csv.size() : pos;

      std::string name = contacts_csv.substr(start, end - start);

      // Trim whitespace
      while (!name.empty() && name.front() == ' ') name.erase(0, 1);
      while (!name.empty() && name.back() == ' ') name.pop_back();

      // Add if not empty and not this device
      if (!name.empty() && name != this->device_name_) {
        this->contacts_.push_back(name);
      }

      if (pos == std::string::npos) break;
      start = pos + 1;
    }
  }

  // Ensure at least "Home Assistant" is available
  if (this->contacts_.empty()) {
    this->contacts_.push_back("Home Assistant");
  }

  // Preserve selection if contact still exists, otherwise reset to 0
  auto it = std::find(this->contacts_.begin(), this->contacts_.end(), previous);
  this->contact_index_ = (it != this->contacts_.end())
      ? static_cast<size_t>(std::distance(this->contacts_.begin(), it))
      : 0;

  this->publish_destination_();
  this->publish_contacts_();  // Publish updated contacts list

  ESP_LOGI(TAG, "Contacts updated: %zu devices", this->contacts_.size());
}

bool IntercomApi::set_contact(const std::string &name) {
  if (this->contacts_.empty()) {
    ESP_LOGW(TAG, "set_contact('%s') failed: contacts list is empty", name.c_str());
    std::string msg = std::string("Contact not found: ") + name;
    this->defer([this, msg]() { this->call_failed_trigger_.trigger(msg); });
    return false;
  }
  for (size_t i = 0; i < this->contacts_.size(); i++) {
    if (this->contacts_[i] == name) {
      this->contact_index_ = i;
      this->publish_destination_();
      ESP_LOGI(TAG, "Selected contact: %s", name.c_str());
      return true;
    }
  }
  ESP_LOGW(TAG, "set_contact('%s') failed: not found in %zu contacts", name.c_str(), this->contacts_.size());
  std::string msg = std::string("Contact not found: ") + name;
  this->defer([this, msg]() { this->call_failed_trigger_.trigger(msg); });
  return false;
}

void IntercomApi::next_contact() {
  if (!this->full_mode_) return;  // Full mode only
  if (this->contacts_.empty()) return;
  this->contact_index_ = (this->contact_index_ + 1) % this->contacts_.size();
  this->publish_destination_();
  ESP_LOGD(TAG, "Selected contact: %s", this->get_current_destination().c_str());
}

void IntercomApi::prev_contact() {
  if (!this->full_mode_) return;  // Full mode only
  if (this->contacts_.empty()) return;
  this->contact_index_ = (this->contact_index_ + this->contacts_.size() - 1) % this->contacts_.size();
  this->publish_destination_();
  ESP_LOGD(TAG, "Selected contact: %s", this->get_current_destination().c_str());
}

const std::string &IntercomApi::get_current_destination() const {
  static const std::string default_dest = "Home Assistant";
  if (this->contacts_.empty()) return default_dest;
  return this->contacts_[this->contact_index_ % this->contacts_.size()];
}

void IntercomApi::publish_destination_() {
  if (this->destination_sensor_ != nullptr) {
    this->destination_sensor_->publish_state(this->get_current_destination());
  }
}

void IntercomApi::publish_caller_(const std::string &caller_name) {
  if (this->caller_sensor_ != nullptr) {
    this->caller_sensor_->publish_state(caller_name);
  }
}

void IntercomApi::publish_contacts_() {
  if (this->contacts_sensor_ != nullptr) {
    // Publish count only (e.g. "3 contacts"), not the full CSV
    // Full list available via get_contacts_csv() if needed
    char buf[32];
    snprintf(buf, sizeof(buf), "%zu contact%s",
             this->contacts_.size(),
             this->contacts_.size() == 1 ? "" : "s");
    this->contacts_sensor_->publish_state(buf);
  }
}

std::string IntercomApi::get_contacts_csv() const {
  // Full CSV available for lambdas/debugging, not published to sensor
  std::string result;
  for (size_t i = 0; i < this->contacts_.size(); i++) {
    if (i > 0) result += ",";
    result += this->contacts_[i];
  }
  return result;
}

// === State Helpers ===

void IntercomApi::set_active_(bool on) {
  bool was = this->active_.exchange(on, std::memory_order_acq_rel);
  if (was == on) return;  // No change

  if (on) {
    // Starting - clear any pending stop request and start hardware
    this->speaker_stop_requested_.store(false, std::memory_order_release);

#ifdef USE_MICROPHONE
    if (this->microphone_source_) {
      this->microphone_source_->start();
    }
#endif
#ifdef USE_SPEAKER
    if (this->speaker_) {
      this->speaker_->start();
    }
#endif
  } else {
    // Stopping

#ifdef USE_SPEAKER
    if (this->speaker_) {
      if (this->has_intercom_processor_() && this->speaker_stopped_sem_) {
        // AEC mode: use single-owner model. speaker_task owns the hardware
        // 1. Request speaker_task to stop the speaker
        // 2. Wait for acknowledgment (with timeout)
        this->speaker_stop_requested_.store(true, std::memory_order_release);
        if (xSemaphoreTake(this->speaker_stopped_sem_, pdMS_TO_TICKS(500)) != pdTRUE) {
          ESP_LOGW(TAG, "Speaker stop timeout");
        }
        this->speaker_stop_requested_.store(false, std::memory_order_release);
      } else {
        // No AEC: no speaker_task exists, stop speaker directly from server_task (Core 1)
        // speaker_->stop() is safe here; it just sets state + signals the mixer
        this->speaker_->stop();
      }
    }
#endif

#ifdef USE_MICROPHONE
    if (this->microphone_source_) {
      this->microphone_source_->stop();
    }
#endif
  }
}

void IntercomApi::set_streaming_(bool on) {
  this->client_.streaming.store(on, std::memory_order_release);
  this->state_ = on ? ConnectionState::STREAMING : ConnectionState::CONNECTED;
  if (on) {
    // Reset audio buffers for new call - prevents stale data on quick reconnect
    // RingBuffer::reset() is thread-safe (FreeRTOS xRingbuffer), no mutex needed
    if (this->mic_buffer_) {
      this->mic_buffer_->reset();
    }
    if (this->speaker_buffer_) {  // Only exists when has_intercom_processor_()
      this->speaker_buffer_->reset();
    }
    this->dc_offset_ = 0;  // Reset DC filter state for new session

#ifdef USE_AUDIO_PROCESSOR
    // Reset AEC state for new call - critical for proper echo cancellation
    this->reset_aec_buffers_();
#endif

    this->set_call_state_(CallState::STREAMING);  // FSM - publishes state internally
  } else {
    this->publish_state_();  // Only publish when stopping (set_call_state_ already publishes)
  }
}

void IntercomApi::set_call_state_(CallState new_state) {
  if (this->call_state_ == new_state) return;

  CallState old_state = this->call_state_;
  this->call_state_ = new_state;

  if (new_state == CallState::STREAMING && this->caller_sensor_ != nullptr &&
      !this->caller_sensor_->state.empty()) {
    ESP_LOGI(TAG, "%s: %s -> %s with %s", this->device_name_.c_str(),
             call_state_to_str(old_state), call_state_to_str(new_state),
             this->caller_sensor_->state.c_str());
  } else {
    ESP_LOGI(TAG, "%s: %s -> %s", this->device_name_.c_str(),
             call_state_to_str(old_state), call_state_to_str(new_state));
  }

  // Defer all YAML trigger fires to the main loop. Call-state changes are
  // initiated from the intercom_srv task on Core 1; YAML on_* actions often
  // touch LVGL (label set_text, page show), which must run on the ESPHome
  // main loop. Running them inline here blocks Core 1 long enough to trip
  // the task watchdog (5s default) under concurrent audio load.
  switch (new_state) {
    case CallState::IDLE:
      this->defer([this]() { this->idle_trigger_.trigger(); });
      break;
    case CallState::OUTGOING:
      this->defer([this]() { this->outgoing_call_trigger_.trigger(); });
      break;
    case CallState::INCOMING:
      break;
    case CallState::RINGING:
      this->defer([this]() { this->ringing_trigger_.trigger(); });
      break;
    case CallState::ANSWERING:
      this->defer([this]() { this->answered_trigger_.trigger(); });
      break;
    case CallState::STREAMING:
      this->defer([this]() { this->streaming_trigger_.trigger(); });
      break;
  }

  this->publish_state_();
}

void IntercomApi::end_call_(CallEndReason reason) {
  if (this->call_state_ == CallState::IDLE) return;

  std::string reason_str = call_end_reason_to_str(reason);
  ESP_LOGI(TAG, "%s: call ended (%s)", this->device_name_.c_str(), reason_str.c_str());

  // Fire appropriate trigger based on reason type (deferred to main loop).
  if (reason == CallEndReason::UNREACHABLE ||
      reason == CallEndReason::BUSY ||
      reason == CallEndReason::PROTOCOL_ERROR ||
      reason == CallEndReason::BRIDGE_ERROR) {
    this->defer([this, reason_str]() { this->call_failed_trigger_.trigger(reason_str); });
  } else {
    this->defer([this, reason_str]() { this->hangup_trigger_.trigger(reason_str); });
  }

  this->set_call_state_(CallState::IDLE);
}

// === Server Task ===

void IntercomApi::server_task(void *param) {
  static_cast<IntercomApi *>(param)->server_task_();
}

void IntercomApi::server_task_() {
  ESP_LOGD(TAG, "Server task started");

  // Set up the listening socket immediately
  if (!this->setup_server_socket_()) {
    ESP_LOGE(TAG, "Failed to setup server socket on startup");
  }

  while (true) {
    // When streaming, don't wait - poll as fast as possible
    // When idle, wait up to 100ms to save CPU
    if (this->client_.streaming.load()) {
      // During streaming: just check notification without blocking
      ulTaskNotifyTake(pdTRUE, 0);  // Non-blocking
    } else {
      // When idle: wait for activation signal
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
    }

    // Server mode - listen for connections
    if (this->server_socket_ < 0) {
      if (!this->setup_server_socket_()) {
        delay(1000);
        continue;
      }
    }

    // Accept new connection if none
    if (this->client_.socket.load() < 0) {
      this->accept_client_();
    }

    // Handle existing client
    int client_fd = this->client_.socket.load();
    if (client_fd >= 0) {
      // Check for incoming data
      fd_set read_fds;
      FD_ZERO(&read_fds);
      FD_SET(client_fd, &read_fds);
      struct timeval tv = {.tv_sec = 0, .tv_usec = 10000};  // 10ms

      int ret = ::select(client_fd + 1, &read_fds, nullptr, nullptr, &tv);
      if (ret > 0 && FD_ISSET(client_fd, &read_fds)) {
        MessageHeader header;
        if (this->receive_message_(client_fd, header, this->rx_buffer_, MAX_MESSAGE_SIZE)) {
          this->handle_message_(header, this->rx_buffer_ + HEADER_SIZE);
        } else {
          // Connection closed or error
          ESP_LOGI(TAG, "Client disconnected");
          // IMPORTANT: Order matters to avoid race conditions
          // 1. Stop streaming flag first
          this->client_.streaming.store(false);
          // 2. Close socket immediately
          this->close_client_socket_();
          // 3. Now stop audio hardware
          this->set_active_(false);
          this->state_ = ConnectionState::DISCONNECTED;

          // Clear caller sensor in full mode
          if (this->full_mode_) {
            this->publish_caller_("");
          }

          // If call was in progress, end it properly to reset FSM
          if (this->call_state_ != CallState::IDLE) {
            this->end_call_(CallEndReason::REMOTE_HANGUP);
          } else {
            this->publish_state_();
          }
        }
      }

      // Send ping if needed - but NOT during streaming to avoid interference with audio
      if (this->state_ != ConnectionState::STREAMING &&
          millis() - this->client_.last_ping > PING_INTERVAL_MS) {
        this->send_message_(this->client_.socket.load(), MessageType::PING);
        this->client_.last_ping = millis();
      }

      // Inline TX: when no tx_task exists, read mic_buffer and send from server_task
      // Cannot call send() from mic callback (runs in audio_task prio 19 on Core 0)
      // so we use mic_buffer as the bridge, same as tx_task does
      if (!this->has_intercom_processor_() &&
          this->active_.load(std::memory_order_acquire) &&
          this->client_.streaming.load(std::memory_order_acquire) &&
          client_fd >= 0) {
        // Drain mic_buffer: send all available chunks
        while (this->mic_buffer_->available() >= AUDIO_CHUNK_SIZE) {
          uint8_t audio_chunk[AUDIO_CHUNK_SIZE];
          size_t read = this->mic_buffer_->read(audio_chunk, AUDIO_CHUNK_SIZE, 0);
          if (read != AUDIO_CHUNK_SIZE) break;

          // Use send_message_ (takes send_mutex_, uses tx_buffer_). Safe because
          // server_task is the only sender when tx_task doesn't exist
          this->send_message_(client_fd, MessageType::AUDIO, MessageFlags::NONE,
                              audio_chunk, AUDIO_CHUNK_SIZE);
        }
      }
    }

    delay(1);  // Yield
  }
}

// === TX Task (Core 0) - Mic to Network ===

void IntercomApi::tx_task(void *param) {
  static_cast<IntercomApi *>(param)->tx_task_();
}

void IntercomApi::send_chunk_(const uint8_t *data, size_t length) {
  if (!this->active_.load(std::memory_order_acquire))
    return;
  int socket = this->client_.socket.load();
  if (socket < 0)
    return;

  MessageHeader header;
  header.type = static_cast<uint8_t>(MessageType::AUDIO);
  header.flags = static_cast<uint8_t>(MessageFlags::NONE);
  header.length = length;

  // Scatter/gather: hand header + payload to lwIP without an intermediate
  // memcpy into a staging buffer. Best effort, no retry on partial sends.
  struct iovec iov[2];
  iov[0].iov_base = &header;
  iov[0].iov_len = HEADER_SIZE;
  iov[1].iov_base = const_cast<uint8_t *>(data);
  iov[1].iov_len = length;
  struct msghdr msg{};
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;

  ssize_t sent = sendmsg(socket, &msg, MSG_DONTWAIT);
  if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    // Only log if still streaming - avoid noise during shutdown.
    if (this->client_.streaming.load(std::memory_order_acquire)) {
      ESP_LOGW(TAG, "TX send error: %d", errno);
    }
  }
}

#ifdef USE_AUDIO_PROCESSOR
void IntercomApi::process_aec_chunk_(const uint8_t *audio_chunk) {
  // Mic chunk (AUDIO_CHUNK_SIZE = 512 samples) can be larger than the AEC
  // frame (e.g. 256 in VOIP mode). Drain the chunk into aec_mic_ in
  // frame_size pieces, AEC-process each complete frame, and forward to
  // send_chunk_(). aec_mic_fill_ persists across calls so partial frames
  // resume on the next chunk.
  const int16_t *mic_samples = reinterpret_cast<const int16_t *>(audio_chunk);
  const size_t num_samples = AUDIO_CHUNK_SIZE / sizeof(int16_t);
  const size_t frame_size = static_cast<size_t>(this->aec_frame_samples_);
  const size_t ref_bytes_needed = frame_size * sizeof(int16_t);
  const size_t out_bytes = frame_size * sizeof(int16_t);
  size_t consumed = 0;

  while (consumed < num_samples) {
    size_t space = frame_size - this->aec_mic_fill_;
    size_t to_copy = std::min(num_samples - consumed, space);
    memcpy(this->aec_mic_ + this->aec_mic_fill_, mic_samples + consumed, to_copy * sizeof(int16_t));
    this->aec_mic_fill_ += to_copy;
    consumed += to_copy;

    if (this->aec_mic_fill_ < frame_size)
      continue;

    // Frame complete: pull reference (or zero-fill on miss) and process.
    if (this->spk_ref_mutex_ != nullptr &&
        xSemaphoreTake(this->spk_ref_mutex_, pdMS_TO_TICKS(2)) == pdTRUE) {
      if (this->spk_ref_buffer_->available() >= ref_bytes_needed) {
        this->spk_ref_buffer_->read(this->aec_ref_, ref_bytes_needed, 0);
      } else {
        memset(this->aec_ref_, 0, ref_bytes_needed);
      }
      xSemaphoreGive(this->spk_ref_mutex_);
    } else {
      memset(this->aec_ref_, 0, ref_bytes_needed);
    }

    this->aec_->process(this->aec_mic_, this->aec_ref_, this->aec_out_);
    this->aec_mic_fill_ = 0;

    this->send_chunk_(reinterpret_cast<const uint8_t *>(this->aec_out_), out_bytes);
  }
}
#endif

void IntercomApi::tx_task_() {
  ESP_LOGD(TAG, "TX task started");

  uint8_t audio_chunk[AUDIO_CHUNK_SIZE];

  while (true) {
    if (!this->active_.load(std::memory_order_acquire) ||
        this->client_.socket.load() < 0 ||
        !this->client_.streaming.load()) {
#ifdef USE_AUDIO_PROCESSOR
      this->aec_mic_fill_ = 0;
#endif
      delay(20);
      continue;
    }

    if (this->mic_buffer_->available() < AUDIO_CHUNK_SIZE) {
      delay(2);
      continue;
    }

    if (this->mic_buffer_->read(audio_chunk, AUDIO_CHUNK_SIZE, 0) != AUDIO_CHUNK_SIZE)
      continue;

#ifdef USE_AUDIO_PROCESSOR
    if (this->aec_enabled_ && this->aec_ != nullptr && this->aec_mic_ != nullptr) {
      this->process_aec_chunk_(audio_chunk);
      yield();
      continue;
    }
#endif

    this->send_chunk_(audio_chunk, AUDIO_CHUNK_SIZE);
    yield();
  }
}

// === Speaker Task (Core 0) - Network to Speaker ===

void IntercomApi::speaker_task(void *param) {
  static_cast<IntercomApi *>(param)->speaker_task_();
}

void IntercomApi::speaker_task_() {
  ESP_LOGD(TAG, "Speaker task started");

#ifdef USE_SPEAKER
  uint8_t audio_chunk[AUDIO_CHUNK_SIZE * 4];
  bool speaker_was_idle = true;

  while (true) {
    // Check for stop request - single-owner model: only this task stops speaker
    if (this->speaker_stop_requested_.load(std::memory_order_acquire)) {
      if (this->speaker_ != nullptr) {
        ESP_LOGD(TAG, "Speaker task: stopping speaker");
        this->speaker_->stop();
      }
      // Signal that speaker has stopped
      if (this->speaker_stopped_sem_ != nullptr) {
        xSemaphoreGive(this->speaker_stopped_sem_);
      }
      // Wait for next activation
      while (this->speaker_stop_requested_.load(std::memory_order_acquire)) {
        delay(10);
      }
      continue;
    }

    // Wait until active
    if (!this->active_.load(std::memory_order_acquire) || this->speaker_ == nullptr) {
      delay(20);
      speaker_was_idle = true;
      continue;
    }

    // First audio after becoming active: give the main loop time to
    // initialize the mixer + resampler pipeline. Reading from an
    // uninitialized SourceSpeaker ring buffer from the mixer task would
    // starve it. Poll in 10 ms steps up to 150 ms; typical warmup is
    // 20-40 ms.
    if (speaker_was_idle) {
      speaker_was_idle = false;
      for (int i = 0; i < 15; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (!this->active_.load(std::memory_order_acquire) || this->speaker_ == nullptr) break;
        if (this->speaker_buffer_ != nullptr && this->speaker_buffer_->available() >= AUDIO_CHUNK_SIZE) break;
      }
      continue;
    }

    // Read from speaker buffer (RingBuffer is thread-safe, no mutex needed)
    size_t avail = this->speaker_buffer_->available();
    if (avail < AUDIO_CHUNK_SIZE) {
      delay(1);
      continue;
    }

    // Read up to 4 chunks at once to reduce overhead
    size_t to_read = avail;
    if (to_read > AUDIO_CHUNK_SIZE * 4) to_read = AUDIO_CHUNK_SIZE * 4;
    // Align to chunk size
    to_read = (to_read / AUDIO_CHUNK_SIZE) * AUDIO_CHUNK_SIZE;

    size_t read = this->speaker_buffer_->read(audio_chunk, to_read, 0);

    if (read > 0 && this->volume_ > 0.001f) {
      this->speaker_->play(audio_chunk, read, 0);

#ifdef USE_AUDIO_PROCESSOR
      // Feed speaker reference buffer for AEC
      // IMPORTANT: Apply same volume scaling as speaker output so reference matches actual echo
      if (this->aec_enabled_ && this->spk_ref_buffer_ != nullptr && this->spk_ref_mutex_ != nullptr) {
        if (xSemaphoreTake(this->spk_ref_mutex_, pdMS_TO_TICKS(2)) == pdTRUE) {
          // Use pre-allocated buffer for scaled reference (don't modify audio_chunk!)
          if (this->volume_ != 1.0f) {
            const int16_t *src = reinterpret_cast<const int16_t *>(audio_chunk);
            size_t num_samples = read / sizeof(int16_t);
            for (size_t i = 0; i < num_samples; i++) {
              this->spk_ref_scaled_[i] = scale_sample(src[i], this->volume_);
            }
            this->spk_ref_buffer_->write(this->spk_ref_scaled_, read);
          } else {
            this->spk_ref_buffer_->write(audio_chunk, read);
          }
          xSemaphoreGive(this->spk_ref_mutex_);
        }
      }
#endif
    }

    // Minimal delay
    yield();
  }
#else
  // No speaker, just idle
  while (true) {
    delay(1000);
  }
#endif
}

// === Protocol ===

bool IntercomApi::send_message_(int socket, MessageType type, MessageFlags flags,
                                 const uint8_t *data, size_t len) {
  if (socket < 0) return false;

  // Take mutex to protect tx_buffer_ from concurrent access
  if (xSemaphoreTake(this->send_mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
    // Could not get mutex - another task is sending
    return false;
  }

  MessageHeader header;
  header.type = static_cast<uint8_t>(type);
  header.flags = static_cast<uint8_t>(flags);
  header.length = static_cast<uint16_t>(len);

  // Build message in tx_buffer
  memcpy(this->tx_buffer_, &header, HEADER_SIZE);
  if (data && len > 0) {
    memcpy(this->tx_buffer_ + HEADER_SIZE, data, len);
  }

  size_t total = HEADER_SIZE + len;
  size_t offset = 0;
  uint32_t start_ms = millis();

  // Handle partial sends with retry
  while (offset < total) {
    ssize_t sent = send(socket, this->tx_buffer_ + offset, total - offset, MSG_DONTWAIT);

    if (sent > 0) {
      offset += static_cast<size_t>(sent);
      continue;
    }

    if (sent == 0) {
      // Connection closed
      xSemaphoreGive(this->send_mutex_);
      return false;
    }

    // sent < 0
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // Buffer full - wait briefly and retry
      if (millis() - start_ms > 20) {
        xSemaphoreGive(this->send_mutex_);
        return false;
      }
      delay(1);
      continue;
    }

    // Real error - only log if we expect the connection to be valid
    if (this->client_.streaming.load(std::memory_order_relaxed)) {
      ESP_LOGW(TAG, "Send failed: errno=%d sent=%zd offset=%zu total=%zu", errno, sent, offset, total);
    }
    xSemaphoreGive(this->send_mutex_);
    return false;
  }

  xSemaphoreGive(this->send_mutex_);
  return true;
}

bool IntercomApi::receive_message_(int socket, MessageHeader &header, uint8_t *buffer, size_t buffer_size) {
  // Read header - handle partial reads (non-blocking socket)
  size_t header_read = 0;
  int retry = 0;
  // 300 ms max wait for a complete message. A tighter cap starves
  // under concurrent mixer/WiFi load on S3.
  const int MAX_RETRY = 300;

  while (header_read < HEADER_SIZE && retry < MAX_RETRY) {
    ssize_t received = recv(socket, buffer + header_read, HEADER_SIZE - header_read, 0);
    if (received > 0) {
      header_read += received;
      retry = 0;  // Reset on progress
      continue;
    }
    if (received == 0) {
      return false;  // Connection closed
    }
    // received < 0
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      retry++;
      delay(1);
      continue;
    }
    return false;  // Real error
  }

  if (header_read != HEADER_SIZE) {
    if (header_read > 0) {
      ESP_LOGW(TAG, "Header incomplete: %zu/%zu", header_read, HEADER_SIZE);
    }
    return false;
  }

  memcpy(&header, buffer, HEADER_SIZE);

  if (header.length > buffer_size - HEADER_SIZE) {
    ESP_LOGW(TAG, "Message too large: %d", header.length);
    return false;
  }

  // Read payload
  if (header.length > 0) {
    size_t payload_read = 0;
    retry = 0;
    while (payload_read < header.length && retry < MAX_RETRY) {
      ssize_t received = recv(socket, buffer + HEADER_SIZE + payload_read,
                              header.length - payload_read, 0);
      if (received > 0) {
        payload_read += received;
        retry = 0;
        continue;
      }
      if (received == 0) {
        return false;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        retry++;
        delay(1);
        continue;
      }
      return false;
    }

    if (payload_read != header.length) {
      ESP_LOGW(TAG, "Payload incomplete: %zu/%d", payload_read, header.length);
      return false;
    }
  }

  return true;
}

void IntercomApi::handle_message_(const MessageHeader &header, const uint8_t *data) {
  MessageType type = static_cast<MessageType>(header.type);

  switch (type) {
    case MessageType::AUDIO: {
#ifdef USE_SPEAKER
      if (this->speaker_buffer_) {
        // AEC mode: write to speaker_buffer, speaker_task reads and feeds AEC ref
        size_t written = this->speaker_buffer_->write(data, header.length);
        if (written != header.length) {
          this->spk_overflow_count_++;
          if (this->spk_overflow_count_ <= 5 || this->spk_overflow_count_ % 100 == 0) {
            ESP_LOGW(TAG, "SPK buffer overflow: %zu/%d (drops=%u)",
                     written, header.length, this->spk_overflow_count_);
          }
        }
      } else if (this->speaker_) {
        // No AEC: play directly from server_task. speaker_->play() is non-blocking
        // (writes to mixer ring buffer, mixer task does the actual I2S output)
        if (this->volume_ > 0.001f) {
          this->speaker_->play(data, header.length, 0);
        }
      }
#endif
      if (this->state_ != ConnectionState::STREAMING) {
        this->state_ = ConnectionState::STREAMING;
      }
      // If we're in OUTGOING state (caller waiting for dest to answer),
      // receiving audio means dest answered - transition to STREAMING
      if (this->call_state_ == CallState::OUTGOING) {
        ESP_LOGI(TAG, "Dest answered - received audio, transitioning to STREAMING");
        this->set_call_state_(CallState::STREAMING);  // trigger fired in set_call_state_
      }
      break;
    }

    case MessageType::START: {
      // Check for NO_RING flag (used for caller in bridge mode - skip ringing)
      const bool no_ring = (header.flags & static_cast<uint8_t>(MessageFlags::NO_RING)) != 0;

      // Extract caller name from payload (if present)
      std::string caller_name;
      if (header.length > 0 && data != nullptr) {
        // Payload is the caller name (null-terminated or up to length)
        size_t name_len = strnlen(reinterpret_cast<const char *>(data), header.length);
        caller_name.assign(reinterpret_cast<const char *>(data), name_len);
      }

      const char *local = this->device_name_.c_str();
      const char *remote = caller_name.empty() ? "Home Assistant" : caller_name.c_str();
      if (no_ring) {
        ESP_LOGI(TAG, "%s -> %s: calling (bridged)...", local, remote);
      } else {
        ESP_LOGI(TAG, "%s <- %s: incoming call", local, remote);
      }

      // Publish caller name (even if empty - clears previous)
      if (this->full_mode_) {
        this->publish_caller_(caller_name);
      }

      if (no_ring) {
        // NO_RING flag: we are the CALLER in a bridge, not the callee
        // Go to OUTGOING state and wait for audio (dest to answer)
        this->outgoing_start_time_ = millis();  // Start timeout counter BEFORE state change
        this->set_call_state_(CallState::OUTGOING);  // FSM: outgoing call
        this->set_active_(true);
        // Enable audio flow, but don't set STREAMING yet - wait for first audio
        this->client_.streaming.store(true, std::memory_order_release);
        this->state_ = ConnectionState::STREAMING;
        this->send_message_(this->client_.socket.load(), MessageType::PONG);
      } else if (this->auto_answer_) {
        // Auto-answer ON: start streaming immediately, skip INCOMING/RINGING states
        // This skips INCOMING/RINGING states (no on_ringing trigger fires)
        this->set_call_state_(CallState::ANSWERING);  // FSM: go directly to answering
        this->set_active_(true);
        this->set_streaming_(true);  // This will set CallState::STREAMING
        this->send_message_(this->client_.socket.load(), MessageType::PONG);
      } else {
        // Auto-answer OFF: go to ringing state, wait for local answer
        this->set_call_state_(CallState::INCOMING);  // FSM: incoming call first
        this->state_ = ConnectionState::CONNECTED;  // Stay connected but not streaming
        this->send_message_(this->client_.socket.load(), MessageType::RING);
        ESP_LOGI(TAG, "%s: ringing (waiting for local answer)", this->device_name_.c_str());
        this->ringing_start_time_ = millis();  // Start ringing timeout timer
        this->set_call_state_(CallState::RINGING);  // FSM: then ringing (triggers on_ringing)
      }
      break;
    }

    case MessageType::STOP:
      ESP_LOGI(TAG, "%s: remote hung up", this->device_name_.c_str());
      // Clear caller name in full mode
      if (this->full_mode_) {
        this->publish_caller_("");
      }
      // IMPORTANT: Order matters to avoid race conditions
      // 1. Stop streaming flag first (TX task checks this)
      this->set_streaming_(false);
      // 2. Close socket immediately (before set_active_ which takes time)
      this->close_client_socket_();
      // 3. Now stop audio hardware (waits for tasks)
      this->set_active_(false);
      this->state_ = ConnectionState::DISCONNECTED;
      this->end_call_(CallEndReason::REMOTE_HANGUP);  // FSM with reason
      break;

    case MessageType::PING:
      this->send_message_(this->client_.socket.load(), MessageType::PONG);
      break;

    case MessageType::PONG:
      this->client_.last_ping = millis();
      break;

    case MessageType::ANSWER:
      // ANSWER: call was answered (either our outgoing call or remote answer)
      if (this->call_state_ == CallState::OUTGOING) {
        // We called them, they answered - start streaming
        ESP_LOGI(TAG, "%s: destination answered, streaming", this->device_name_.c_str());
        this->set_streaming_(true);
        this->send_message_(this->client_.socket.load(), MessageType::PONG);
      } else if (this->call_state_ == CallState::RINGING) {
        ESP_LOGI(TAG, "%s: answered remotely (by HA)", this->device_name_.c_str());
        this->set_call_state_(CallState::ANSWERING);  // FSM
        this->set_active_(true);
        this->set_streaming_(true);  // This will set CallState::STREAMING
        this->send_message_(this->client_.socket.load(), MessageType::PONG);
      } else {
        ESP_LOGW(TAG, "ANSWER received in unexpected state: %s", call_state_to_str(this->call_state_));
      }
      break;

    case MessageType::ERROR:
      if (header.length > 0) {
        ESP_LOGE(TAG, "Received ERROR: %d", data[0]);
        // Close connection and fail the call (e.g. BUSY means remote device rejected us)
        this->close_client_socket_();
        this->set_active_(false);
        this->state_ = ConnectionState::DISCONNECTED;
        CallEndReason err_reason = (data[0] == static_cast<uint8_t>(ErrorCode::BUSY))
                                       ? CallEndReason::BUSY
                                       : CallEndReason::PROTOCOL_ERROR;
        this->end_call_(err_reason);
      }
      break;

    default:
      ESP_LOGW(TAG, "Unknown message type: 0x%02X", header.type);
      break;
  }
}

// === Socket Helpers ===

bool IntercomApi::setup_server_socket_() {
  this->server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (this->server_socket_ < 0) {
    ESP_LOGE(TAG, "Failed to create server socket: %d", errno);
    return false;
  }

  int opt = 1;
  setsockopt(this->server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Set non-blocking
  int flags = fcntl(this->server_socket_, F_GETFL, 0);
  fcntl(this->server_socket_, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(INTERCOM_PORT);

  if (bind(this->server_socket_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
    ESP_LOGE(TAG, "Bind failed: %d", errno);
    close(this->server_socket_);
    this->server_socket_ = -1;
    return false;
  }

  if (listen(this->server_socket_, 1) < 0) {
    ESP_LOGE(TAG, "Listen failed: %d", errno);
    close(this->server_socket_);
    this->server_socket_ = -1;
    return false;
  }

  ESP_LOGI(TAG, "Server listening on port %d", INTERCOM_PORT);
  return true;
}

void IntercomApi::close_server_socket_() {
  if (this->server_socket_ >= 0) {
    close(this->server_socket_);
    this->server_socket_ = -1;
  }
}

void IntercomApi::close_client_socket_() {
  // Lock-free socket close: atomically get and invalidate socket
  // This prevents race conditions without needing mutex timeout hacks
  this->client_.streaming.store(false);

  int sock = this->client_.socket.exchange(-1);
  if (sock >= 0) {
    // Try to send STOP before closing (best effort)
    this->send_message_(sock, MessageType::STOP);
    // Graceful shutdown then close
    shutdown(sock, SHUT_RDWR);
    close(sock);
  }
}

void IntercomApi::accept_client_() {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);

  int client_sock = accept(this->server_socket_, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
  if (client_sock < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGW(TAG, "Accept error: %d", errno);
    }
    return;
  }

  // Helper to reject with BUSY error
  auto reject_busy = [&](const char *reason) {
    ESP_LOGW(TAG, "Rejecting connection - %s", reason);
    MessageHeader header;
    header.type = static_cast<uint8_t>(MessageType::ERROR);
    header.flags = 0;
    header.length = 1;
    uint8_t msg[HEADER_SIZE + 1];
    memcpy(msg, &header, HEADER_SIZE);
    msg[HEADER_SIZE] = static_cast<uint8_t>(ErrorCode::BUSY);
    send(client_sock, msg, sizeof(msg), 0);
    close(client_sock);
  };

  // Check if already have a client
  if (this->client_.socket.load() >= 0) {
    reject_busy("already have client");
    return;
  }

  // Check if we're in a state that shouldn't accept new connections
  // Allow IDLE (normal) and OUTGOING (ESP called someone, waiting for answer)
  CallState cs = this->call_state_.load(std::memory_order_acquire);
  if (cs != CallState::IDLE && cs != CallState::OUTGOING) {
    reject_busy(call_state_to_str(cs));
    return;
  }

  // Set socket options
  int opt = 1;
  setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

  // Set non-blocking for async operation
  int flags = fcntl(client_sock, F_GETFL, 0);
  fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);

  char ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
  ESP_LOGI(TAG, "Client connected from %s", ip_str);

  // Use mutex for non-atomic addr field
  xSemaphoreTake(this->client_mutex_, portMAX_DELAY);
  this->client_.socket.store(client_sock);
  this->client_.addr = client_addr;
  this->client_.last_ping = millis();
  this->client_.streaming.store(false);
  xSemaphoreGive(this->client_mutex_);

  this->state_ = ConnectionState::CONNECTED;
}

// === Microphone Callback ===

void IntercomApi::on_microphone_data_(const uint8_t *data, size_t len) {
  if (!this->active_.load(std::memory_order_acquire) ||
      this->client_.socket.load() < 0 ||
      !this->client_.streaming.load()) {
    return;
  }

  // NOTE: With MicrophoneSource pattern, data arrives as 16-bit regardless of mic hardware.
  // MicrophoneSource handles bit conversion internally.
  // We only apply DC offset removal and gain if configured.

  const int16_t *src = reinterpret_cast<const int16_t *>(data);
  size_t num_samples = std::min(len / sizeof(int16_t), MIC_CONVERTED_SAMPLES);
  // Only apply mic_gain if intercom owns the mic_gain number entity.
  // When i2s_audio_duplex provides mic_gain, it's already applied in the audio task.
  float effective_gain = (this->mic_gain_number_ != nullptr) ? this->mic_gain_.load() : 1.0f;
  bool needs_processing = effective_gain != 1.0f || this->dc_offset_removal_;

  // RingBuffer is thread-safe, no mutex needed
  if (needs_processing) {
    for (size_t i = 0; i < num_samples; i++) {
      int32_t s = src[i];
      if (this->dc_offset_removal_) {
        // IIR high-pass ~2.5Hz cutoff at 16kHz (matches i2s_audio_duplex)
        this->dc_offset_ += (s - this->dc_offset_) >> 10;
        s = s - this->dc_offset_;
      }
      this->mic_converted_[i] = scale_sample(static_cast<int16_t>(s), effective_gain);
    }

    this->mic_buffer_->write(this->mic_converted_, num_samples * sizeof(int16_t));
  } else {
    // Direct passthrough (gain=1.0, no DC offset)
    this->mic_buffer_->write(data, len);
  }
}

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32
