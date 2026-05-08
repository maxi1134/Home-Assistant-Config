#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/preferences.h"
#include "esphome/core/ring_buffer.h"

#ifdef USE_MICROPHONE
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/microphone/microphone_source.h"
#endif
#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif

#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifdef USE_AUDIO_PROCESSOR
#include "esphome/components/audio_processor/audio_processor.h"
#endif

#include "intercom_protocol.h"

#include <lwip/sockets.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace intercom_api {

using audio_processor::AudioProcessor;

// TCP connection state (low-level)
enum class ConnectionState : uint8_t {
  DISCONNECTED,
  CONNECTED,
  STREAMING,
};

// Call state (high-level FSM for display/triggers)
enum class CallState : uint8_t {
  IDLE,        // No call in progress
  OUTGOING,    // We initiated a call, waiting for remote to answer
  INCOMING,    // Someone is calling us (before ringing starts)
  RINGING,     // Actively ringing/notifying user
  ANSWERING,   // Answer accepted, setting up stream
  STREAMING,   // Audio active
};

// Hangup/failure reasons
enum class CallEndReason : uint8_t {
  LOCAL_HANGUP,
  REMOTE_HANGUP,
  DECLINED,
  TIMEOUT,
  BUSY,
  UNREACHABLE,
  PROTOCOL_ERROR,
  BRIDGE_ERROR,
};

inline const char *call_state_to_str(CallState state) {
  switch (state) {
    case CallState::IDLE: return "idle";
    case CallState::OUTGOING: return "outgoing";
    case CallState::INCOMING: return "incoming";
    case CallState::RINGING: return "ringing";
    case CallState::ANSWERING: return "answering";
    case CallState::STREAMING: return "streaming";
    default: return "unknown";
  }
}

inline const char *call_end_reason_to_str(CallEndReason reason) {
  switch (reason) {
    case CallEndReason::LOCAL_HANGUP: return "local_hangup";
    case CallEndReason::REMOTE_HANGUP: return "remote_hangup";
    case CallEndReason::DECLINED: return "declined";
    case CallEndReason::TIMEOUT: return "timeout";
    case CallEndReason::BUSY: return "busy";
    case CallEndReason::UNREACHABLE: return "unreachable";
    case CallEndReason::PROTOCOL_ERROR: return "protocol_error";
    case CallEndReason::BRIDGE_ERROR: return "bridge_error";
    default: return "unknown";
  }
}

// Client info - socket and streaming are atomic for thread safety
struct ClientInfo {
  std::atomic<int> socket{-1};
  struct sockaddr_in addr{};
  uint32_t last_ping{0};
  std::atomic<bool> streaming{false};
};

/// TCP-based intercom peer component.
///
/// Implements a custom TCP protocol (port 6054) for ESP-to-ESP calls,
/// complementing voice_assistant and micro_wake_word with bi-directional
/// audio streaming between devices on the same network. Optionally
/// integrates an AudioProcessor (set via processor_id) when it owns its
/// own mic/speaker path; on devices where i2s_audio_duplex owns the
/// processor, intercom_api stays mic/speaker-passthrough and the
/// processor side is handled upstream.
///
/// Runs up to three FreeRTOS tasks depending on configuration
/// (server/tx/speaker); see README.md for the task topology and stack
/// budget per mode.
class IntercomApi : public Component {
 public:
  // FreeRTOS task stack sizes (in words). Mirrors esp_afe's convention
  // of keeping stack footprint explicit at the class level rather than
  // as magic numbers at xTaskCreate sites.
  static constexpr uint32_t kServerTaskStackWords = 8192 / sizeof(StackType_t);
  static constexpr uint32_t kTxTaskStackWords = 12288 / sizeof(StackType_t);
  static constexpr uint32_t kSpeakerTaskStackWords = 8192 / sizeof(StackType_t);

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Call from YAML api: on_client_connected: to publish restored entity states to HA
  void publish_entity_states();

  // Configuration
#ifdef USE_MICROPHONE
  void set_microphone_source(microphone::MicrophoneSource *source) { this->microphone_source_ = source; }
#endif
#ifdef USE_SPEAKER
  void set_speaker(speaker::Speaker *spk) { this->speaker_ = spk; }
#endif

  void set_dc_offset_removal(bool enabled) { this->dc_offset_removal_ = enabled; }
  void set_tasks_stack_in_psram(bool enabled) { this->tasks_stack_in_psram_ = enabled; }
  void set_frame_buffers_in_psram(bool enabled) { this->frame_buffers_in_psram_ = enabled; }
  void set_device_name(const std::string &name) { this->device_name_ = name; }

#ifdef USE_AUDIO_PROCESSOR
  void set_aec(AudioProcessor *aec) { this->aec_ = aec; }
  void set_aec_reference_delay_ms(uint32_t delay_ms) { this->aec_ref_delay_ms_ = delay_ms; }
  void set_aec_enabled(bool enabled);
  bool is_aec_enabled() const { return this->aec_enabled_; }
#endif

  // Runtime control
  void start();
  void stop();
  bool is_active() const {
    // Use FSM state instead of atomic flag for more accurate state
    CallState cs = this->call_state_.load(std::memory_order_acquire);
    return cs == CallState::STREAMING || cs == CallState::ANSWERING || cs == CallState::OUTGOING;
  }
  bool is_connected() const {
    ConnectionState s = this->state_.load(std::memory_order_acquire);
    return s == ConnectionState::CONNECTED || s == ConnectionState::STREAMING;
  }

  // Volume control
  void set_volume(float volume);
  float get_volume() const { return this->volume_; }

  // Auto-answer control (for incoming calls)
  void set_auto_answer(bool enabled);
  bool is_auto_answer() const { return this->auto_answer_; }

  // Manual answer for incoming call (when auto_answer is OFF)
  void answer_call();
  void decline_call();
  bool is_ringing() const { return this->call_state_.load(std::memory_order_acquire) == CallState::RINGING; }
  bool is_outgoing() const { return this->call_state_.load(std::memory_order_acquire) == CallState::OUTGOING; }
  bool is_idle() const { return this->call_state_.load(std::memory_order_acquire) == CallState::IDLE; }
  bool is_streaming() const { return this->call_state_.load(std::memory_order_acquire) == CallState::STREAMING; }
  // The HA peer is by convention contacts_[0] (the integration sensor always
  // prepends the HA instance name). Selecting it by index instead of by string
  // keeps the YAML conditions stable when location_name changes.
  bool is_ha_destination() const { return this->contact_index_ == 0; }

  // Smart call toggle: ringing → answer, active → hangup, idle → start
  void call_toggle();

  // Ringing timeout configuration (auto-hangup if not answered)
  void set_ringing_timeout(uint32_t timeout_ms) { this->ringing_timeout_ms_ = timeout_ms; }

  // Mic gain control (dB scale: -20 to +20)
  void set_mic_gain_db(float db);
  float get_mic_gain() const { return this->mic_gain_; }

  // State getters
  ConnectionState get_state() const { return this->state_.load(std::memory_order_acquire); }
  const char *get_state_str() const;

  // Mode setting
  void set_full_mode(bool full) { this->full_mode_ = full; }

  // Sensor registration
  void set_state_sensor(text_sensor::TextSensor *sensor) { this->state_sensor_ = sensor; }
  void set_destination_sensor(text_sensor::TextSensor *sensor) { this->destination_sensor_ = sensor; }
  void set_caller_sensor(text_sensor::TextSensor *sensor) { this->caller_sensor_ = sensor; }
  void set_contacts_sensor(text_sensor::TextSensor *sensor) { this->contacts_sensor_ = sensor; }

  // Entity registration (for state sync after boot)
  void register_auto_answer_switch(switch_::Switch *sw) { this->auto_answer_switch_ = sw; }
  void register_volume_number(number::Number *num) { this->volume_number_ = num; }
  void register_mic_gain_number(number::Number *num) { this->mic_gain_number_ = num; }
#ifdef USE_AUDIO_PROCESSOR
  void register_aec_switch(switch_::Switch *sw) { this->aec_switch_ = sw; }
#endif

  // Contacts management (full mode only)
  void set_contacts(const std::string &contacts_csv);
  bool set_contact(const std::string &name);
  void next_contact();
  void prev_contact();
  const std::string &get_current_destination() const;
  std::string get_caller() const { return this->caller_sensor_ ? this->caller_sensor_->state : ""; }
  std::string get_contacts_csv() const;

  // Call state triggers (exposed to YAML)
  Trigger<> *get_ringing_trigger() { return &this->ringing_trigger_; }
  Trigger<> *get_streaming_trigger() { return &this->streaming_trigger_; }
  Trigger<> *get_idle_trigger() { return &this->idle_trigger_; }
  Trigger<> *get_outgoing_call_trigger() { return &this->outgoing_call_trigger_; }
  Trigger<> *get_answered_trigger() { return &this->answered_trigger_; }
  Trigger<std::string> *get_hangup_trigger() { return &this->hangup_trigger_; }
  Trigger<std::string> *get_call_failed_trigger() { return &this->call_failed_trigger_; }

  // Call state getter
  CallState get_call_state() const { return this->call_state_.load(std::memory_order_acquire); }
  const char *get_call_state_str() const { return call_state_to_str(this->call_state_.load(std::memory_order_acquire)); }

 protected:
  // Returns true when intercom_api has its own AEC (processor_id configured on intercom_api component)
  // When false, speaker_task and tx_task are eliminated to save ~34KB internal RAM
  bool has_intercom_processor_() const {
#ifdef USE_AUDIO_PROCESSOR
    return this->aec_ != nullptr;
#else
    return false;
#endif
  }

  // Server task - handles incoming connections and receiving data
  // When !has_intercom_processor_(), also handles TX (mic→network) and direct speaker playback
  static void server_task(void *param);
  void server_task_();

  // TX task - handles mic capture and sending to network (Core 0)
  // Only created when has_intercom_processor_() (AEC processing needs dedicated task)
  static void tx_task(void *param);
  void tx_task_();

  // Build AUDIO message header + payload and send over the active client
  // socket. Noops if not active or no socket. Used by both AEC and pass-through
  // TX paths.
  void send_chunk_(const uint8_t *data, size_t length);

#ifdef USE_AUDIO_PROCESSOR
  // Drain `audio_chunk` (AUDIO_CHUNK_SIZE bytes) into the AEC accumulator,
  // process every complete AEC frame, and send each result via send_chunk_().
  // Updates aec_mic_fill_ across calls.
  void process_aec_chunk_(const uint8_t *audio_chunk);
#endif

  // Speaker task - handles playback from speaker buffer (Core 0)
  // Only created when has_intercom_processor_() (needs to feed AEC reference buffer)
  static void speaker_task(void *param);
  void speaker_task_();

  // Protocol handling
  bool send_message_(int socket, MessageType type, MessageFlags flags = MessageFlags::NONE,
                     const uint8_t *data = nullptr, size_t len = 0);
  bool receive_message_(int socket, MessageHeader &header, uint8_t *buffer, size_t buffer_size);
  void handle_message_(const MessageHeader &header, const uint8_t *data);

  // Socket helpers
  bool setup_server_socket_();
  void close_server_socket_();
  void close_client_socket_();
  void accept_client_();

  // Microphone callback
  void on_microphone_data_(const uint8_t *data, size_t len);

  // State helpers - consolidate duplicated start/stop logic
  void set_active_(bool on);
  void set_streaming_(bool on);

  // Publish sensor values
  void publish_state_();
  void publish_destination_();
  void publish_caller_(const std::string &caller_name);
  void publish_contacts_();

  // Call state FSM
  void set_call_state_(CallState new_state);
  void end_call_(CallEndReason reason);

#ifdef USE_AUDIO_PROCESSOR
  // AEC helper
  void reset_aec_buffers_();
#endif

  // Components
#ifdef USE_MICROPHONE
  microphone::MicrophoneSource *microphone_source_{nullptr};
#endif
#ifdef USE_SPEAKER
  speaker::Speaker *speaker_{nullptr};
#endif

  // Mode and state
  bool full_mode_{false};  // simple (false) or full (true) mode
  std::atomic<bool> active_{false};
  std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
  std::atomic<CallState> call_state_{CallState::IDLE};  // High-level FSM state (source of truth)

  // Sensors (state is always present, others only in full mode)
  text_sensor::TextSensor *state_sensor_{nullptr};
  text_sensor::TextSensor *destination_sensor_{nullptr};  // full: selected contact
  text_sensor::TextSensor *caller_sensor_{nullptr};       // full: who is calling
  text_sensor::TextSensor *contacts_sensor_{nullptr};     // full: contact count (e.g. "3 contacts")

  // Registered entities (for state sync after boot)
  switch_::Switch *auto_answer_switch_{nullptr};
  number::Number *volume_number_{nullptr};
  number::Number *mic_gain_number_{nullptr};
#ifdef USE_AUDIO_PROCESSOR
  switch_::Switch *aec_switch_{nullptr};
#endif

  // Contacts management (full mode only)
  std::vector<std::string> contacts_{"Home Assistant"};  // Default contact
  size_t contact_index_{0};
  std::string device_name_;  // This device's friendly name (to exclude from contacts)

  // Sockets
  int server_socket_{-1};
  ClientInfo client_;
  SemaphoreHandle_t client_mutex_{nullptr};

  // Buffers
  std::unique_ptr<RingBuffer> mic_buffer_;
  std::unique_ptr<RingBuffer> speaker_buffer_;


  // Pre-allocated frame buffers
  uint8_t *tx_buffer_{nullptr};      // Used by server_task for control messages
  uint8_t *rx_buffer_{nullptr};      // Used by server_task for receiving
  SemaphoreHandle_t send_mutex_{nullptr};  // Protects tx_buffer_ during send

  // Task handles and static-task storage.
  // When tasks_stack_in_psram_ is true (default false), the stacks are
  // allocated from PSRAM via xTaskCreateStaticPinnedToCore: saves ~28 KB
  // of internal heap on S3/P4 boards that already have heavy AFE/MWW/LVGL
  // pressure. When false, the tasks are created with the standard dynamic
  // xTaskCreatePinnedToCore (stack on the internal heap), which is the
  // only option on plain ESP32 boards without PSRAM.
  TaskHandle_t server_task_handle_{nullptr};
  TaskHandle_t tx_task_handle_{nullptr};
  TaskHandle_t speaker_task_handle_{nullptr};
  StaticTask_t server_task_tcb_{};
  StaticTask_t tx_task_tcb_{};
  StaticTask_t speaker_task_tcb_{};
  StackType_t *server_task_stack_{nullptr};
  StackType_t *tx_task_stack_{nullptr};
  StackType_t *speaker_task_stack_{nullptr};
  bool tasks_stack_in_psram_{false};

  // Speaker single-owner: only speaker_task_ touches speaker hardware
  // This prevents race conditions between play() and stop()
  std::atomic<bool> speaker_stop_requested_{false};
  SemaphoreHandle_t speaker_stopped_sem_{nullptr};  // Signaled when speaker has stopped

  // Volume: atomic, shared between main/api thread and audio tasks
  std::atomic<float> volume_{1.0f};

  // Diagnostic counters
  uint32_t spk_overflow_count_{0};

  // Auto-answer (default true for backward compatibility)
  bool auto_answer_{true};

  // Call timeout (0 = disabled, otherwise auto-hangup after this many ms)
  uint32_t ringing_timeout_ms_{0};
  uint32_t ringing_start_time_{0};
  uint32_t outgoing_start_time_{0};

  // Mic gain (applied before sending to network)
  // Atomic: shared between main/api thread and mic_task
  std::atomic<float> mic_gain_{1.0f};
  float mic_gain_db_{0.0f};  // UI-friendly value (dB) for persistence, main thread only

  // === Settings persistence (local flash) ===
  static constexpr uint8_t SETTINGS_VERSION = 1;

  struct StoredSettings {
    uint8_t version{SETTINGS_VERSION};
    uint8_t volume_pct{100};   // 0..100
    int8_t mic_gain_db{0};     // -20..+20
    uint8_t reserved{0};       // Padding for alignment
  };

  ESPPreferenceObject settings_pref_{};
  bool suppress_save_{false};
  bool save_scheduled_{false};

  void load_settings_();
  void schedule_save_settings_();
  void save_settings_();

  // Mic configuration
  bool dc_offset_removal_{false}; // Enable for mics with DC bias (SPH0645)
  bool frame_buffers_in_psram_{false};  // Place aec_mic_/ref_/out_ (~3 KB) in PSRAM (default internal)
  int32_t dc_offset_{0};          // Running DC offset value

  // Pre-allocated processing buffers (avoid stack VLAs on FreeRTOS tasks)
  int16_t *mic_converted_{nullptr};     // Mic callback processing (MAX_SAMPLES = 512 samples)
  int16_t *spk_ref_scaled_{nullptr};    // Speaker AEC ref scaling (AUDIO_CHUNK_SIZE*4/2 = 2048 samples)

#ifdef USE_AUDIO_PROCESSOR
  // AEC (Acoustic Echo Cancellation)
  AudioProcessor *aec_{nullptr};
  // Atomic: shared between set_aec_enabled (API thread) and audio tasks
  std::atomic<bool> aec_enabled_{false};
  uint32_t aec_ref_delay_ms_{80};  // Configurable via YAML (default 80ms)

  // Speaker reference buffer for AEC (fed by speaker_task)
  std::unique_ptr<RingBuffer> spk_ref_buffer_;
  SemaphoreHandle_t spk_ref_mutex_{nullptr};

  // AEC frame accumulation (frame_size = 512 samples = 32ms at 16kHz)
  int aec_frame_samples_{0};
  int16_t *aec_mic_{nullptr};   // Accumulated mic samples (frame_size)
  int16_t *aec_ref_{nullptr};   // Speaker reference samples (frame_size)
  int16_t *aec_out_{nullptr};   // AEC output samples (frame_size)
  size_t aec_mic_fill_{0};      // Current fill level in aec_mic_
#endif

  // Call state triggers (exposed to YAML)
  Trigger<> ringing_trigger_;
  Trigger<> streaming_trigger_;
  Trigger<> idle_trigger_;
  Trigger<> outgoing_call_trigger_;
  Trigger<> answered_trigger_;
  Trigger<std::string> hangup_trigger_;
  Trigger<std::string> call_failed_trigger_;
};

// Switch for on/off control (simple - ESPHome handles restore)
class IntercomApiSwitch : public switch_::Switch, public Parented<IntercomApi> {
 public:
  void write_state(bool state) override {
    if (state) {
      this->parent_->start();
    } else {
      this->parent_->stop();
    }
    this->publish_state(state);
  }
};

// Number for volume control (simple - parent syncs after boot)
class IntercomApiVolume : public number::Number, public Parented<IntercomApi> {
 public:
  void control(float value) override {
    this->parent_->set_volume(value / 100.0f);
    this->publish_state(value);
  }
};

// Number for mic gain control (dB scale, simple - parent syncs after boot)
class IntercomApiMicGain : public number::Number, public Parented<IntercomApi> {
 public:
  void control(float value) override {
    this->parent_->set_mic_gain_db(value);
    this->publish_state(value);
  }
};

// Switch for auto-answer control
class IntercomApiAutoAnswer : public switch_::Switch, public Parented<IntercomApi> {
 public:
  void write_state(bool state) override {
    this->parent_->set_auto_answer(state);
    this->publish_state(state);
  }
};

// Note: State and Destination sensors are plain TextSensor* created via new_text_sensor()
// No custom classes needed - the parent IntercomApi just holds pointers to them

// === Actions for YAML automation syntax ===
// Usage: intercom_api.next_contact:, intercom_api.start:, etc.

template<typename... Ts>
class NextContactAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  void play(const Ts &...x) override { this->parent_->next_contact(); }
};

template<typename... Ts>
class PrevContactAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  void play(const Ts &...x) override { this->parent_->prev_contact(); }
};

template<typename... Ts>
class StartAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  void play(const Ts &...x) override { this->parent_->start(); }
};

template<typename... Ts>
class StopAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  void play(const Ts &...x) override { this->parent_->stop(); }
};

template<typename... Ts>
class AnswerCallAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  void play(const Ts &...x) override { this->parent_->answer_call(); }
};

template<typename... Ts>
class DeclineCallAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  void play(const Ts &...x) override { this->parent_->decline_call(); }
};

// === Parameterized actions ===

template<typename... Ts>
class SetVolumeAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  TEMPLATABLE_VALUE(float, volume)
  void play(const Ts &...x) override {
    this->parent_->set_volume(this->volume_.value(x...));
  }
};

template<typename... Ts>
class SetMicGainDbAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  TEMPLATABLE_VALUE(float, gain_db)
  void play(const Ts &...x) override {
    this->parent_->set_mic_gain_db(this->gain_db_.value(x...));
  }
};

template<typename... Ts>
class SetContactsAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  TEMPLATABLE_VALUE(std::string, contacts_csv)
  void play(const Ts &...x) override {
    this->parent_->set_contacts(this->contacts_csv_.value(x...));
  }
};

template<typename... Ts>
class SetContactAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  TEMPLATABLE_VALUE(std::string, contact)
  void play(const Ts &...x) override {
    this->parent_->set_contact(this->contact_.value(x...));
  }
};

template<typename... Ts>
class CallToggleAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  void play(const Ts &...x) override { this->parent_->call_toggle(); }
};

template<typename... Ts>
class PublishEntityStatesAction : public Action<Ts...>, public Parented<IntercomApi> {
 public:
  void play(const Ts &...x) override { this->parent_->publish_entity_states(); }
};

// === Switch platform classes with restore support ===

// AEC switch (only available when USE_AUDIO_PROCESSOR is defined)
#ifdef USE_AUDIO_PROCESSOR
class IntercomAecSwitch : public switch_::Switch, public Parented<IntercomApi> {
 public:
  void write_state(bool state) override {
    this->parent_->set_aec_enabled(state);
    // Publish ACTUAL state (set_aec_enabled may refuse if AEC not initialized)
    this->publish_state(this->parent_->is_aec_enabled());
  }
};
#endif

// === Condition classes for ESPHome automation ===

template<typename... Ts>
class IntercomIsIdleCondition : public Condition<Ts...>, public Parented<IntercomApi> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_idle(); }
};

template<typename... Ts>
class IntercomIsRingingCondition : public Condition<Ts...>, public Parented<IntercomApi> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_ringing(); }
};

template<typename... Ts>
class IntercomIsStreamingCondition : public Condition<Ts...>, public Parented<IntercomApi> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_streaming(); }
};

template<typename... Ts>
class IntercomIsCallingCondition : public Condition<Ts...>, public Parented<IntercomApi> {
 public:
  bool check(const Ts &...x) override { return this->parent_->get_call_state() == CallState::OUTGOING; }
};

template<typename... Ts>
class IntercomIsIncomingCondition : public Condition<Ts...>, public Parented<IntercomApi> {
 public:
  bool check(const Ts &...x) override {
    CallState cs = this->parent_->get_call_state();
    return cs == CallState::INCOMING || cs == CallState::RINGING;
  }
};

template<typename... Ts>
class IntercomIsAnsweringCondition : public Condition<Ts...>, public Parented<IntercomApi> {
 public:
  bool check(const Ts &...x) override {
    return this->parent_->get_call_state() == CallState::ANSWERING;
  }
};

template<typename... Ts>
class IntercomIsInCallCondition : public Condition<Ts...>, public Parented<IntercomApi> {
 public:
  bool check(const Ts &...x) override {
    auto state = this->parent_->get_call_state();
    return state == CallState::STREAMING || state == CallState::ANSWERING;
  }
};

template<typename... Ts>
class IntercomDestinationIsCondition : public Condition<Ts...>, public Parented<IntercomApi> {
 public:
  TEMPLATABLE_VALUE(std::string, destination)
  bool check(const Ts &...x) override {
    return this->parent_->get_current_destination() == this->destination_.value(x...);
  }
};

template<typename... Ts>
class IntercomIsHaDestinationCondition : public Condition<Ts...>, public Parented<IntercomApi> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_ha_destination(); }
};

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32
