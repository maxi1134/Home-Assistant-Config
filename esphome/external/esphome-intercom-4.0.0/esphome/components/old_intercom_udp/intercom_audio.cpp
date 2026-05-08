#include "intercom_audio.h"

#ifdef USE_I2S_AUDIO_DUPLEX
#include "esphome/components/i2s_audio_duplex/i2s_audio_duplex.h"
#endif

#ifdef USE_ESP_AEC
#include "esphome/components/esp_aec/esp_aec.h"
#endif

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#ifdef USE_SPEAKER
#include "esphome/components/audio/audio.h"
#endif

#include <lwip/netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstring>

namespace esphome {
namespace intercom_audio {

static const char *const TAG = "intercom_audio";

// Audio parameters
static const uint32_t SAMPLE_RATE = 16000;
static const size_t FRAME_SAMPLES = 256;  // 16ms @ 16kHz
static const size_t FRAME_BYTES = FRAME_SAMPLES * sizeof(int16_t);
static const size_t RX_MAX_SAMPLES = 512;  // Max samples per UDP packet
static const size_t RX_MAX_BYTES = RX_MAX_SAMPLES * sizeof(int16_t);

void IntercomAudio::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Intercom Audio...");

  // Create mutexes for shared buffers
  this->mic_mutex_ = xSemaphoreCreateMutex();
  this->ref_mutex_ = xSemaphoreCreateMutex();
  if (!this->mic_mutex_ || !this->ref_mutex_) {
    ESP_LOGE(TAG, "Failed to create mutexes");
    this->mark_failed();
    return;
  }

  // Pre-allocate mic conversion buffer
  this->mic_convert_buf_.resize(FRAME_SAMPLES);

  // Allocate frame buffers (internal RAM: touched on every UDP packet / mic callback)
  RAMAllocator<int16_t> internal_i16(RAMAllocator<int16_t>::ALLOC_INTERNAL);
  this->rx_frame_ = internal_i16.allocate(RX_MAX_SAMPLES);
  this->tx_frame_ = internal_i16.allocate(FRAME_SAMPLES);
  if (!this->rx_frame_ || !this->tx_frame_) {
    ESP_LOGE(TAG, "Failed to allocate frame buffers");
    this->mark_failed();
    return;
  }

  // Create ring buffers
  this->rx_buffer_ = RingBuffer::create(this->buffer_size_);
  this->mic_input_buffer_ = RingBuffer::create(this->buffer_size_);
  if (!this->rx_buffer_ || !this->mic_input_buffer_) {
    ESP_LOGE(TAG, "Failed to create ring buffers");
    this->mark_failed();
    return;
  }

  // Create speaker reference buffer for AEC (if AEC configured)
#ifdef USE_ESP_AEC
  if (this->aec_ != nullptr) {
    this->speaker_ref_buffer_ = RingBuffer::create(this->buffer_size_);
    this->aec_mic_frame_ = internal_i16.allocate(FRAME_SAMPLES);
    this->aec_ref_frame_ = internal_i16.allocate(FRAME_SAMPLES);
    this->aec_out_frame_ = internal_i16.allocate(FRAME_SAMPLES);
    if (!this->speaker_ref_buffer_ || !this->aec_mic_frame_ || !this->aec_ref_frame_ || !this->aec_out_frame_) {
      ESP_LOGW(TAG, "AEC buffer alloc failed - disabling AEC");
      this->speaker_ref_buffer_.reset();
      if (this->aec_mic_frame_) { internal_i16.deallocate(this->aec_mic_frame_, FRAME_SAMPLES); this->aec_mic_frame_ = nullptr; }
      if (this->aec_ref_frame_) { internal_i16.deallocate(this->aec_ref_frame_, FRAME_SAMPLES); this->aec_ref_frame_ = nullptr; }
      if (this->aec_out_frame_) { internal_i16.deallocate(this->aec_out_frame_, FRAME_SAMPLES); this->aec_out_frame_ = nullptr; }
      this->aec_enabled_ = false;
    } else {
      ESP_LOGI(TAG, "AEC buffers ready");
    }
  }
#endif

  // Register microphone callback
#ifdef USE_I2S_AUDIO_DUPLEX
  if (this->duplex_ != nullptr) {
    this->duplex_->add_mic_data_callback([this](const uint8_t *data, size_t len) {
      this->on_microphone_data_(data, len);
    });
  } else
#endif
#ifdef USE_MICROPHONE
  if (this->microphone_ != nullptr) {
    this->microphone_->add_data_callback([this](const std::vector<uint8_t> &data) {
      this->on_microphone_data_(data);
    });
  }
#else
  {}
#endif

  // NOTE: Audio hardware started in audio_task_ when streaming begins
  // Starting here without data causes speaker timeout/crash
#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    audio::AudioStreamInfo stream_info(16, 1, SAMPLE_RATE);
    this->speaker_->set_audio_stream_info(stream_info);
  }
#endif

  // Create audio task ONCE - runs forever, controlled by streaming_ flag.
  // Stack: 8KB on the internal heap (FreeRTOS dynamic task). Keeps the
  // legacy component compatible with plain ESP32 boards without PSRAM,
  // which is the whole point of old_intercom_udp (baby monitor, two-room
  // direct UDP link without Home Assistant).
  BaseType_t ok = xTaskCreatePinnedToCore(
      audio_task,
      "intercom_audio",
      8192,
      this,
      5,
      &this->audio_task_handle_,
      0  // Core 0
  );
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create audio task");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Intercom Audio ready, listen port: %d", this->listen_port_);
}

void IntercomAudio::dump_config() {
  ESP_LOGCONFIG(TAG, "Intercom Audio:");
  ESP_LOGCONFIG(TAG, "  Listen Port: %d", this->listen_port_);
  ESP_LOGCONFIG(TAG, "  Buffer Size: %zu bytes", this->buffer_size_);
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->get_mode_str());
  if (this->aec_ == nullptr) {
    ESP_LOGCONFIG(TAG, "  AEC: not configured");
  } else {
    ESP_LOGCONFIG(TAG, "  AEC: %s", this->aec_enabled_ ? "enabled" : "disabled");
  }
}

void IntercomAudio::loop() {
  // Nothing needed - task does all the work
}

void IntercomAudio::start() {
  this->start(this->get_remote_ip(), this->get_remote_port());
}

void IntercomAudio::start(const std::string &remote_ip, uint16_t remote_port) {
  if (this->streaming_.load(std::memory_order_acquire)) {
    ESP_LOGW(TAG, "Already streaming");
    return;
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot start: component failed");
    return;
  }

  ESP_LOGI(TAG, "Starting stream to %s:%d", remote_ip.c_str(), remote_port);

  // Store remote address
  this->remote_ip_ = remote_ip;
  this->remote_port_ = remote_port;

  // Setup sockets
  if (!this->setup_sockets_()) {
    ESP_LOGE(TAG, "Failed to setup sockets");
    return;
  }

  // Reset metrics
  this->tx_packets_.store(0, std::memory_order_relaxed);
  this->rx_packets_.store(0, std::memory_order_relaxed);
  this->tx_drops_.store(0, std::memory_order_relaxed);
  this->rx_drops_.store(0, std::memory_order_relaxed);

  // Increment session to invalidate any stale data, then reset buffers
  this->session_.fetch_add(1, std::memory_order_acq_rel);

  // Reset DC offset tracking for clean start
  this->dc_sum_ = 0;

  if (this->rx_buffer_) this->rx_buffer_->reset();
  if (xSemaphoreTake(this->mic_mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (this->mic_input_buffer_) this->mic_input_buffer_->reset();
    xSemaphoreGive(this->mic_mutex_);
  }
  if (xSemaphoreTake(this->ref_mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (this->speaker_ref_buffer_) this->speaker_ref_buffer_->reset();
    xSemaphoreGive(this->ref_mutex_);
  }

  // Enable streaming and wake up task
  this->streaming_.store(true, std::memory_order_release);
  if (this->audio_task_handle_) {
    xTaskNotifyGive(this->audio_task_handle_);
  }

  this->start_trigger_.trigger();
  ESP_LOGI(TAG, "Streaming started");
}

void IntercomAudio::stop() {
  if (!this->streaming_.load(std::memory_order_acquire)) {
    return;
  }

  // Diagnostic logging before stop
  ESP_LOGW(TAG, "STOP: heap_free=%u, rx_avail=%zu",
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           this->rx_buffer_ ? this->rx_buffer_->available() : 0);
#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    ESP_LOGW(TAG, "STOP: speaker_running=%d", this->speaker_->is_running());
  }
#endif

  ESP_LOGI(TAG, "Stopping stream");

  // Disable streaming first
  this->streaming_.store(false, std::memory_order_release);

  // Invalidate any in-flight operations
  this->session_.fetch_add(1, std::memory_order_acq_rel);

  // Wake up task FIRST so it sees streaming_=false
  if (this->audio_task_handle_) {
    xTaskNotifyGive(this->audio_task_handle_);
  }

  // CRITICAL: Wait for audio_task to stop processing before resetting buffers
  // This prevents race condition where task is mid-write to speaker
  vTaskDelay(pdMS_TO_TICKS(100));

  // Close sockets
  this->close_sockets_();

  // Reset buffers (now safe - audio_task has seen streaming_=false)
  if (this->rx_buffer_) this->rx_buffer_->reset();
  this->rx_fill_.store(0, std::memory_order_release);
  if (xSemaphoreTake(this->mic_mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (this->mic_input_buffer_) this->mic_input_buffer_->reset();
    xSemaphoreGive(this->mic_mutex_);
  }
  if (xSemaphoreTake(this->ref_mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (this->speaker_ref_buffer_) this->speaker_ref_buffer_->reset();
    xSemaphoreGive(this->ref_mutex_);
  }

#ifdef USE_I2S_AUDIO_DUPLEX
  // Duplex can be safely stopped (no ESPHome cleanup bug)
  if (this->duplex_ != nullptr) {
    this->duplex_->stop();
  }
#endif
  // DO NOT stop ESPHome speaker/microphone - keep them running to avoid cleanup bugs

  // Diagnostic logging after stop
  ESP_LOGW(TAG, "STOP DONE: heap_free=%u", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  this->stop_trigger_.trigger();
  ESP_LOGI(TAG, "Streaming stopped");
}

bool IntercomAudio::setup_sockets_() {
  // Ensure clean state
  this->close_sockets_();

  // Create RX socket
  this->rx_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (this->rx_socket_ < 0) {
    ESP_LOGE(TAG, "Failed to create RX socket: %d", errno);
    return false;
  }

  int reuse = 1;
  setsockopt(this->rx_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  int rcvbuf = 16384;
  setsockopt(this->rx_socket_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

  struct sockaddr_in bind_addr{};
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = INADDR_ANY;
  bind_addr.sin_port = htons(this->listen_port_);

  if (bind(this->rx_socket_, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
    ESP_LOGE(TAG, "Failed to bind: %d", errno);
    this->close_sockets_();
    return false;
  }

  // Set non-blocking
  int flags = fcntl(this->rx_socket_, F_GETFL, 0);
  if (flags >= 0) {
    fcntl(this->rx_socket_, F_SETFL, flags | O_NONBLOCK);
  }

  // Create TX socket
  this->tx_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (this->tx_socket_ < 0) {
    ESP_LOGE(TAG, "Failed to create TX socket: %d", errno);
    this->close_sockets_();
    return false;
  }

  int sndbuf = 16384;
  setsockopt(this->tx_socket_, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

  // Setup remote address
  if (this->remote_ip_.empty()) {
    ESP_LOGE(TAG, "Remote IP is empty");
    this->close_sockets_();
    return false;
  }

  memset(&this->remote_addr_, 0, sizeof(this->remote_addr_));
  this->remote_addr_.sin_family = AF_INET;
  this->remote_addr_.sin_port = htons(this->remote_port_);

  if (inet_pton(AF_INET, this->remote_ip_.c_str(), &this->remote_addr_.sin_addr) <= 0) {
    ESP_LOGE(TAG, "Invalid remote IP: %s", this->remote_ip_.c_str());
    this->close_sockets_();
    return false;
  }

  ESP_LOGD(TAG, "Sockets ready: RX :%d, TX %s:%d",
           this->listen_port_, this->remote_ip_.c_str(), this->remote_port_);
  return true;
}

void IntercomAudio::close_sockets_() {
  if (this->rx_socket_ >= 0) {
    close(this->rx_socket_);
    this->rx_socket_ = -1;
  }
  if (this->tx_socket_ >= 0) {
    close(this->tx_socket_);
    this->tx_socket_ = -1;
  }
}

void IntercomAudio::on_microphone_data_(const uint8_t *data, size_t len) {
  // Quick exit if not streaming
  if (!this->streaming_.load(std::memory_order_acquire)) {
    return;
  }
  if (data == nullptr || len == 0) {
    return;
  }

  // Capture session to detect stop/start during processing
  const uint32_t captured_session = this->session_.load(std::memory_order_acquire);

  const int16_t *mic_samples = nullptr;
  size_t num_samples = 0;
  size_t expected_16bit_size = FRAME_BYTES;

  if (len == expected_16bit_size * 2) {
    // 32-bit data, convert to 16-bit
    num_samples = len / sizeof(int32_t);
    if (num_samples > FRAME_SAMPLES) num_samples = FRAME_SAMPLES;

    const int32_t *src = reinterpret_cast<const int32_t *>(data);

    for (size_t i = 0; i < num_samples; i++) {
      int32_t sample = src[i] >> 16;

      // IIR-based DC offset removal (matches ESPHome's approach)
      // Uses shift operations for efficiency: ~1/8192 smoothing factor
      if (this->dc_offset_removal_) {
        this->dc_sum_ = ((this->dc_sum_ >> 13) - (this->dc_sum_ >> 17)) + sample;
        sample -= static_cast<int32_t>(this->dc_sum_ >> 13);
      }

      // Apply gain
      sample *= this->mic_gain_;
      if (sample > 32767) sample = 32767;
      if (sample < -32768) sample = -32768;
      this->mic_convert_buf_[i] = static_cast<int16_t>(sample);
    }
    mic_samples = this->mic_convert_buf_.data();
  } else {
    // Already 16-bit (from duplex or other source)
    // NOTE: Don't apply mic_gain here - duplex already handles its own gain
    num_samples = len / sizeof(int16_t);
    if (num_samples > FRAME_SAMPLES) num_samples = FRAME_SAMPLES;
    mic_samples = reinterpret_cast<const int16_t *>(data);
  }

  // Write to buffer under mutex - verify session hasn't changed
  if (this->mic_input_buffer_ != nullptr && this->mic_mutex_ != nullptr) {
    if (xSemaphoreTake(this->mic_mutex_, 1) == pdTRUE) {
      // Re-check atomics under lock
      if (this->streaming_.load(std::memory_order_acquire) &&
          this->session_.load(std::memory_order_acquire) == captured_session) {
        size_t bytes = num_samples * sizeof(int16_t);
        size_t written = this->mic_input_buffer_->write_without_replacement((void *)mic_samples, bytes, 0, true);
        if (written < bytes) {
          this->tx_drops_.fetch_add(1, std::memory_order_relaxed);
        }
      }
      xSemaphoreGive(this->mic_mutex_);
    } else {
      this->tx_drops_.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

bool IntercomAudio::send_audio_(const uint8_t *data, size_t bytes) {
  if (this->tx_socket_ < 0) {
    return false;
  }
  ssize_t sent = sendto(this->tx_socket_, data, bytes, 0,
                        (struct sockaddr *)&this->remote_addr_, sizeof(this->remote_addr_));
  if (sent > 0) {
    this->tx_packets_.fetch_add(1, std::memory_order_relaxed);
    return true;
  }
  return false;
}

size_t IntercomAudio::receive_audio_(int16_t *buffer, size_t max_samples) {
  if (this->rx_socket_ < 0) {
    return 0;
  }
  struct sockaddr_in sender_addr;
  socklen_t sender_len = sizeof(sender_addr);
  ssize_t received = recvfrom(this->rx_socket_, buffer, max_samples * sizeof(int16_t), 0,
                              (struct sockaddr *)&sender_addr, &sender_len);
  if (received > 0) {
    this->rx_packets_.fetch_add(1, std::memory_order_relaxed);
    return received / sizeof(int16_t);
  }
  return 0;
}

void IntercomAudio::audio_task(void *param) {
  IntercomAudio *self = static_cast<IntercomAudio *>(param);
  self->audio_task_();
  vTaskDelete(nullptr);
}

void IntercomAudio::audio_task_() {
  ESP_LOGI(TAG, "Audio task started (runs forever)");

  uint32_t seen_session = this->session_.load(std::memory_order_acquire);
  bool prebuffered = false;
  bool hw_started = false;  // Track if we started hardware

  // AEC hold-last buffer
  int16_t last_ref[FRAME_SAMPLES];
  bool have_last_ref = false;
  memset(last_ref, 0, sizeof(last_ref));

  // Compute AEC state
  bool use_aec = false;
  auto recompute_aec = [&]() {
#ifdef USE_ESP_AEC
    use_aec = (this->aec_ != nullptr && this->aec_enabled_ &&
               this->aec_mic_frame_ != nullptr && this->aec_ref_frame_ != nullptr &&
               this->aec_out_frame_ != nullptr && this->speaker_ref_buffer_ != nullptr);
#else
    use_aec = false;
#endif
  };
  recompute_aec();

  while (true) {
    // Wait for notification or timeout
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5));

    // Check if streaming
    if (!this->streaming_.load(std::memory_order_acquire)) {
      // Not streaming - reset state and wait
      prebuffered = false;
      seen_session = this->session_.load(std::memory_order_acquire);
      have_last_ref = false;
      // NOTE: Don't stop hardware - keep it running to avoid cleanup crash
      continue;
    }

    // Start hardware
#ifdef USE_I2S_AUDIO_DUPLEX
    // For duplex: start fresh each session (duplex handles its own state)
    if (this->duplex_ != nullptr) {
      if (!this->duplex_->is_running()) {
        ESP_LOGI(TAG, "Starting duplex audio...");
        this->duplex_->start();
      }
    } else
#endif
    // For separate mic/speaker: start once and keep running (avoid ESPHome cleanup crash)
    if (!hw_started) {
      ESP_LOGI(TAG, "Starting audio hardware...");
#ifdef USE_SPEAKER
      if (this->speaker_ != nullptr) {
        this->speaker_->start();
      }
#endif
#ifdef USE_MICROPHONE
      if (this->microphone_ != nullptr) {
        this->microphone_->start();
      }
#endif
      hw_started = true;
    }

    // Check for session change (stop/start happened)
    uint32_t current_session = this->session_.load(std::memory_order_acquire);
    if (current_session != seen_session) {
      seen_session = current_session;
      prebuffered = false;
      have_last_ref = false;
      recompute_aec();
      continue;
    }

    // Multi-frame processing limits
    const int max_frames_per_iter = 4;
    int frames_processed = 0;

    // === RX: UDP -> ring buffer -> speaker ===
    size_t samples = this->receive_audio_(this->rx_frame_, RX_MAX_SAMPLES);
    if (samples > 0) {
      size_t bytes = samples * sizeof(int16_t);
      size_t written = this->rx_buffer_->write(this->rx_frame_, bytes);
      if (written < bytes) {
        this->rx_drops_.fetch_add(1, std::memory_order_relaxed);
      }
      this->rx_fill_.store(this->rx_buffer_->available(), std::memory_order_release);
    }

    // Wait for prebuffer before playing
    if (!prebuffered) {
      if (this->rx_buffer_->available() >= this->prebuffer_size_) {
        prebuffered = true;
        ESP_LOGD(TAG, "Prebuffer filled");
      }
    }

    // Play from RX buffer to speaker (multiple frames per iteration)
    if (prebuffered) {
      frames_processed = 0;
      while (this->rx_buffer_->available() >= FRAME_BYTES && frames_processed < max_frames_per_iter) {
        size_t read = this->rx_buffer_->read(this->rx_frame_, FRAME_BYTES, 0);
        this->rx_fill_.store(this->rx_buffer_->available(), std::memory_order_release);

        if (read != FRAME_BYTES || !this->streaming_.load(std::memory_order_acquire)) {
          break;
        }

        // Store speaker ref for AEC
        if (this->speaker_ref_buffer_ != nullptr && this->ref_mutex_ != nullptr) {
          if (xSemaphoreTake(this->ref_mutex_, pdMS_TO_TICKS(1)) == pdTRUE) {
            this->speaker_ref_buffer_->write_without_replacement((void *)this->rx_frame_, FRAME_BYTES, 0, true);
            xSemaphoreGive(this->ref_mutex_);
          }
        }

        // Send to speaker (skip if volume is 0 to reduce crosstalk)
#ifdef USE_I2S_AUDIO_DUPLEX
        if (this->duplex_ != nullptr) {
          this->duplex_->play((uint8_t *)this->rx_frame_, FRAME_BYTES, pdMS_TO_TICKS(10));
        }
#endif
#ifdef USE_SPEAKER
#ifdef USE_I2S_AUDIO_DUPLEX
        else
#endif
        if (this->speaker_ != nullptr && this->speaker_->get_volume() > 0.001f) {
          this->speaker_->play((uint8_t *)this->rx_frame_, FRAME_BYTES, pdMS_TO_TICKS(10));
        }
#endif
        frames_processed++;
      }
    }

    // === TX: mic buffer -> [AEC] -> UDP ===
    frames_processed = 0;

    while (frames_processed < max_frames_per_iter) {
      size_t got_mic = 0;
      if (xSemaphoreTake(this->mic_mutex_, pdMS_TO_TICKS(2)) == pdTRUE) {
        if (this->mic_input_buffer_->available() >= FRAME_BYTES) {
          got_mic = this->mic_input_buffer_->read(this->tx_frame_, FRAME_BYTES, 0);
        }
        xSemaphoreGive(this->mic_mutex_);
      }

      if (got_mic != FRAME_BYTES) {
        break;  // No more data
      }

#ifdef USE_ESP_AEC
      if (use_aec && this->aec_->is_initialized()) {
        // Get speaker reference (use ref_mutex_)
        size_t got_ref = 0;
        if (this->ref_mutex_ != nullptr && xSemaphoreTake(this->ref_mutex_, pdMS_TO_TICKS(1)) == pdTRUE) {
          if (this->speaker_ref_buffer_->available() >= FRAME_BYTES) {
            got_ref = this->speaker_ref_buffer_->read(this->aec_ref_frame_, FRAME_BYTES, 0);
          }
          xSemaphoreGive(this->ref_mutex_);
        }

        if (got_ref == FRAME_BYTES) {
          memcpy(last_ref, this->aec_ref_frame_, sizeof(last_ref));
          have_last_ref = true;
        } else if (have_last_ref) {
          memcpy(this->aec_ref_frame_, last_ref, sizeof(last_ref));
        } else {
          memset(this->aec_ref_frame_, 0, FRAME_BYTES);
        }

        // Process AEC
        memcpy(this->aec_mic_frame_, this->tx_frame_, FRAME_BYTES);
        this->aec_->process(this->aec_mic_frame_, this->aec_ref_frame_, this->aec_out_frame_, FRAME_SAMPLES);
        this->send_audio_(reinterpret_cast<uint8_t *>(this->aec_out_frame_), FRAME_BYTES);
      } else
#endif
      {
        // No AEC - send directly
        this->send_audio_(reinterpret_cast<uint8_t *>(this->tx_frame_), FRAME_BYTES);
      }
      frames_processed++;
    }
  }
}

void IntercomAudio::set_volume(float volume) {
#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    this->speaker_->set_volume(volume);
  }
#endif
}

float IntercomAudio::get_volume() const {
#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    return this->speaker_->get_volume();
  }
#endif
  return 0.0f;
}

void IntercomAudio::set_mic_gain(int gain) {
  this->mic_gain_ = gain;
#ifdef USE_I2S_AUDIO_DUPLEX
  // Forward to duplex if available (duplex uses float 0.0-2.0, we use int 1-10)
  if (this->duplex_ != nullptr) {
    float duplex_gain = static_cast<float>(gain) / 5.0f;  // 1->0.2, 5->1.0, 10->2.0
    this->duplex_->set_mic_gain(duplex_gain);
  }
#endif
}

}  // namespace intercom_audio
}  // namespace esphome

#endif  // USE_ESP32
