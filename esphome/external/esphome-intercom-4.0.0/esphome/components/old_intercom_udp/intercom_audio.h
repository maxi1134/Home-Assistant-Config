#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/ring_buffer.h"
#include "esphome/core/optional.h"

#ifdef USE_MICROPHONE
#include "esphome/components/microphone/microphone.h"
#endif
#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif

#include <lwip/sockets.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward declare esp_aec if available
namespace esphome {
namespace esp_aec {
class EspAec;
}  // namespace esp_aec
}  // namespace esphome

// Forward declare i2s_audio_duplex (only if available)
#ifdef USE_I2S_AUDIO_DUPLEX
namespace esphome {
namespace i2s_audio_duplex {
class I2SAudioDuplex;
}  // namespace i2s_audio_duplex
}  // namespace esphome
#endif

namespace esphome {
namespace intercom_audio {

// Simplified: only IDLE and STREAMING (no transitional states)
enum class StreamState : uint8_t {
  IDLE,
  STREAMING,
};

class IntercomAudio : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Configuration setters
#ifdef USE_MICROPHONE
  void set_microphone(microphone::Microphone *mic) { this->microphone_ = mic; }
#endif
#ifdef USE_SPEAKER
  void set_speaker(speaker::Speaker *spk) { this->speaker_ = spk; }
#endif
#ifdef USE_I2S_AUDIO_DUPLEX
  void set_duplex(i2s_audio_duplex::I2SAudioDuplex *duplex) { this->duplex_ = duplex; }
#endif
  void set_aec(esp_aec::EspAec *aec) { this->aec_ = aec; }

  void set_listen_port(uint16_t port) { this->listen_port_ = port; }

  // Lambda setters for dynamic IP/port (evaluated at start() time)
  void set_remote_ip_lambda(std::function<std::string()> &&f) { this->remote_ip_lambda_ = std::move(f); }
  void set_remote_port_lambda(std::function<uint16_t()> &&f) { this->remote_port_lambda_ = std::move(f); }

  // Get current remote IP/port (evaluates lambda)
  std::string get_remote_ip() const {
    if (this->remote_ip_lambda_.has_value()) {
      return this->remote_ip_lambda_.value()();
    }
    return this->remote_ip_;
  }
  uint16_t get_remote_port() const {
    if (this->remote_port_lambda_.has_value()) {
      return this->remote_port_lambda_.value()();
    }
    return this->remote_port_;
  }

  void set_buffer_size(size_t size) { this->buffer_size_ = size; }
  void set_prebuffer_size(size_t size) { this->prebuffer_size_ = size; }

  // Runtime control - simple: set flags, open/close sockets
  void start();
  void start(const std::string &remote_ip, uint16_t remote_port);
  void stop();
  bool is_streaming() const { return this->streaming_.load(std::memory_order_acquire); }

  // State getters (StreamState kept for API compatibility)
  StreamState get_state() const {
    return this->streaming_.load(std::memory_order_acquire) ? StreamState::STREAMING : StreamState::IDLE;
  }
  uint32_t get_tx_packets() const { return this->tx_packets_.load(std::memory_order_relaxed); }
  uint32_t get_rx_packets() const { return this->rx_packets_.load(std::memory_order_relaxed); }
  size_t get_buffer_fill() const { return this->rx_fill_.load(std::memory_order_acquire); }

  // Get audio mode as string
  const char *get_mode_str() const {
#ifdef USE_I2S_AUDIO_DUPLEX
    if (this->duplex_ != nullptr) return "Full Duplex";
#endif
    bool has_mic = false;
    bool has_spk = false;
#ifdef USE_MICROPHONE
    has_mic = (this->microphone_ != nullptr);
#endif
#ifdef USE_SPEAKER
    has_spk = (this->speaker_ != nullptr);
#endif
    if (has_mic && has_spk) return "Full Duplex";
    if (has_mic) return "TX Only";
    if (has_spk) return "RX Only";
    return "None";
  }

  // Reset packet counters
  void reset_counters() {
    this->tx_packets_.store(0, std::memory_order_relaxed);
    this->rx_packets_.store(0, std::memory_order_relaxed);
    this->tx_drops_.store(0, std::memory_order_relaxed);
    this->rx_drops_.store(0, std::memory_order_relaxed);
  }

  // Drop counters (buffer overruns)
  uint32_t get_tx_drops() const { return this->tx_drops_.load(std::memory_order_relaxed); }
  uint32_t get_rx_drops() const { return this->rx_drops_.load(std::memory_order_relaxed); }

  // Volume control (delegates to speaker)
  void set_volume(float volume);
  float get_volume() const;

  // Mic gain control (for 32->16 bit conversion, or forwarded to duplex)
  void set_mic_gain(int gain);
  int get_mic_gain() const { return this->mic_gain_; }

  // DC offset removal (for microphones with significant DC bias like SPH0645)
  void set_dc_offset_removal(bool enabled) { this->dc_offset_removal_ = enabled; }
  bool get_dc_offset_removal() const { return this->dc_offset_removal_; }

  // AEC control
  void set_aec_enabled(bool enabled) { this->aec_enabled_ = enabled; }
  bool is_aec_enabled() const { return this->aec_enabled_; }

  // Triggers for automations
  Trigger<> *get_start_trigger() { return &this->start_trigger_; }
  Trigger<> *get_stop_trigger() { return &this->stop_trigger_; }

 protected:
  // Audio task - created ONCE in setup(), runs forever
  static void audio_task(void *param);
  void audio_task_();

  // Microphone callback
  void on_microphone_data_(const uint8_t *data, size_t len);
  void on_microphone_data_(const std::vector<uint8_t> &data) {
    on_microphone_data_(data.data(), data.size());
  }

  // UDP helpers
  bool setup_sockets_();
  void close_sockets_();
  bool send_audio_(const uint8_t *data, size_t bytes);
  size_t receive_audio_(int16_t *buffer, size_t max_samples);

  // Components
#ifdef USE_I2S_AUDIO_DUPLEX
  i2s_audio_duplex::I2SAudioDuplex *duplex_{nullptr};
#endif
#ifdef USE_MICROPHONE
  microphone::Microphone *microphone_{nullptr};
#endif
#ifdef USE_SPEAKER
  speaker::Speaker *speaker_{nullptr};
#endif
  esp_aec::EspAec *aec_{nullptr};

  // Network config
  uint16_t listen_port_{12346};
  std::string remote_ip_;
  uint16_t remote_port_{12346};
  optional<std::function<std::string()>> remote_ip_lambda_;
  optional<std::function<uint16_t()>> remote_port_lambda_;

  // Buffer config
  size_t buffer_size_{8192};
  size_t prebuffer_size_{2048};

  // Mic gain for 32->16 bit conversion
  int mic_gain_{4};

  // DC offset removal for mics with significant DC bias
  bool dc_offset_removal_{false};
  int64_t dc_sum_{0};  // Running sum for IIR-based DC offset removal

  // Core state: just two atomics
  std::atomic<bool> streaming_{false};       // True = actively streaming
  std::atomic<uint32_t> session_{0};         // Incremented on start/stop to invalidate in-flight ops

  // Task handle (task created once in setup, runs forever).
  // Dynamic task with internal-heap stack (8KB) so the legacy component
  // stays compatible with plain ESP32 boards without PSRAM.
  TaskHandle_t audio_task_handle_{nullptr};

  // Separate mutexes to reduce contention
  SemaphoreHandle_t mic_mutex_{nullptr};  // Protects mic_input_buffer_
  SemaphoreHandle_t ref_mutex_{nullptr};  // Protects speaker_ref_buffer_

  // Sockets
  int rx_socket_{-1};
  int tx_socket_{-1};
  struct sockaddr_in remote_addr_{};

  // Ring buffers
  std::unique_ptr<RingBuffer> rx_buffer_;        // UDP RX -> speaker
  std::unique_ptr<RingBuffer> mic_input_buffer_; // Mic -> UDP TX
  std::unique_ptr<RingBuffer> speaker_ref_buffer_; // Speaker ref for AEC

  bool aec_enabled_{false};

  // Mic data conversion buffer (pre-allocated in setup)
  std::vector<int16_t> mic_convert_buf_;

  // Frame buffers (allocated once in setup)
  int16_t *rx_frame_{nullptr};
  int16_t *tx_frame_{nullptr};

  // AEC frame buffers
#ifdef USE_ESP_AEC
  int16_t *aec_mic_frame_{nullptr};
  int16_t *aec_ref_frame_{nullptr};
  int16_t *aec_out_frame_{nullptr};
#endif

  // Metrics (atomic for cross-thread access)
  std::atomic<uint32_t> tx_packets_{0};
  std::atomic<uint32_t> rx_packets_{0};
  std::atomic<uint32_t> tx_drops_{0};
  std::atomic<uint32_t> rx_drops_{0};
  std::atomic<size_t> rx_fill_{0};

  // Automations
  Trigger<> start_trigger_;
  Trigger<> stop_trigger_;
};

// Actions
template<typename... Ts>
class StartAction : public Action<Ts...>, public Parented<IntercomAudio> {
 public:
  void set_remote_ip(std::function<std::string(Ts...)> func) { this->remote_ip_ = std::move(func); }
  void set_remote_port(std::function<uint16_t(Ts...)> func) { this->remote_port_ = std::move(func); }

  void play(Ts... x) override {
    if (this->remote_ip_.has_value() && this->remote_port_.has_value()) {
      this->parent_->start(this->remote_ip_.value()(x...), this->remote_port_.value()(x...));
    } else if (this->remote_ip_.has_value()) {
      this->parent_->start(this->remote_ip_.value()(x...), this->parent_->get_remote_port());
    } else {
      this->parent_->start();
    }
  }

 protected:
  optional<std::function<std::string(Ts...)>> remote_ip_;
  optional<std::function<uint16_t(Ts...)>> remote_port_;
};

template<typename... Ts>
class StopAction : public Action<Ts...>, public Parented<IntercomAudio> {
 public:
  void play(Ts... x) override { this->parent_->stop(); }
};

template<typename... Ts>
class ResetCountersAction : public Action<Ts...>, public Parented<IntercomAudio> {
 public:
  void play(Ts... x) override { this->parent_->reset_counters(); }
};

}  // namespace intercom_audio
}  // namespace esphome

#endif  // USE_ESP32
