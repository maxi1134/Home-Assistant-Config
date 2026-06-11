#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "../audio_processor/ring_buffer_caps.h"

#ifdef USE_INTERCOM_API_MIC
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/microphone/microphone_source.h"
#endif
#ifdef USE_INTERCOM_API_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif

#include "esphome/components/switch/switch.h"
#include "esphome/components/button/button.h"
#include "esphome/components/number/number.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "intercom_fsm.h"
#include "intercom_protocol.h"
#include "phonebook.h"
#include "transport.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#ifdef USE_INTERCOM_MDNS_ANNOUNCE
#include <esp_event.h>
#endif

#include <atomic>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace esphome {
namespace intercom_api {

// Network transport selected via YAML `protocol:` (default tcp).
enum class TransportType : uint8_t {
  TCP,  // Framed binary protocol on port 6054 (audio + signaling on a single connection).
  UDP,  // Raw PCM datagram audio + framed signaling, ESP <-> ESP and go2rtc-compatible.
};

// Connection state, derived from transport_->is_connected() and streaming_.
// Kept as a public type for back-compat with users that read get_state().
enum class ConnectionState : uint8_t {
  DISCONNECTED,
  CONNECTED,
  STREAMING,
};

/// Transport-agnostic intercom peer.
///
/// Bi-directional audio between devices on the same LAN, complementing
/// voice_assistant and micro_wake_word. Network protocol is delegated to
/// an IntercomTransport implementation: `TcpTransport` (framed PBX-lite
/// on port 6054) or `UdpTransport` (raw PCM datagrams + framed control,
/// opt-in via `protocol: udp`).
///
/// intercom_api is transport/FSM glue. It accepts any ESPHome-compatible
/// microphone/speaker pair: native ESPHome components, hardware-processed
/// devices such as XMOS, or the microphone/speaker facade exposed by
/// esp_audio_stack when software AEC/AFE is needed.
///
/// The recv task is owned by the transport. A TX FreeRTOS task is created only
/// when a microphone is configured; speaker-only devices play RX audio directly
/// from the transport callback.
/// See README.md.
class IntercomApi : public Component {
 public:
  // FreeRTOS task stack sizes in bytes. Kept at class level so xTaskCreate
  // sites stay free of magic numbers. The transport task declares its own
  // stack size inside the transport class.
  static constexpr uint32_t kTxTaskStackBytes = 12288;

  enum SchedulerId : uint32_t {
    SCHED_MDNS_STARTUP_SCAN = 1,
    SCHED_MDNS_PERIODIC_SCAN = 2,
    SCHED_PUBLISH_INITIAL_STATE = 3,
    SCHED_SAVE_SETTINGS = 4,
  };

  void setup() override;
  void loop() override;
  void dump_config() override;
  // AFTER_CONNECTION: bind sockets and publish initial state only after
  // the HA API is up (AFTER_WIFI would race the API connection).
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  // Call from YAML api: on_client_connected: to publish restored entity states to HA
  void publish_entity_states();

  // Configuration
#ifdef USE_INTERCOM_API_MIC
  void set_microphone(microphone::Microphone *mic) { this->microphone_ = mic; }
  void set_microphone_source(microphone::MicrophoneSource *source) { this->microphone_source_ = source; }
#endif
#ifdef USE_INTERCOM_API_SPEAKER
  void set_speaker(speaker::Speaker *spk) { this->speaker_ = spk; }
#endif

  void set_dc_offset_removal(bool enabled) { this->dc_offset_removal_ = enabled; }
  void set_task_stacks_in_psram(bool enabled) { this->task_stacks_in_psram_ = enabled; }
  void set_buffers_in_psram(bool enabled) { this->buffers_in_psram_ = enabled; }
  void set_device_name(const std::string &name) { this->device_name_ = name; }
  // Stable routing key (yaml `name:` slug, e.g. "spotpear-ball-v2").
  // Matches the slug HA uses for the esphome.{slug}_start_call action.
  void set_device_route_id(const std::string &id) { this->device_route_id_ = id; }
  // OUTGOING / RINGING auto-decline timeouts (0 disables). Both fire
  // DECLINE("timeout") and tear down locally.
  void set_calling_timeout(uint32_t ms) { this->calling_timeout_ms_ = ms; }
  void set_ringing_timeout(uint32_t ms) { this->ringing_timeout_ms_ = ms; }

  // Transport configuration (set by codegen from YAML before setup()).
  void set_protocol(TransportType type) { this->protocol_ = type; }
  void set_remote_ip(const std::string &ip) { this->remote_ip_ = ip; }
  void set_remote_port(uint16_t port) { this->remote_port_ = port; }
  void set_listen_port(uint16_t port) { this->listen_port_ = port; }
  void set_control_port(uint16_t port) { this->control_port_ = port; }
  void set_tcp_port(uint16_t port) { this->tcp_port_ = port; }
  uint16_t get_tcp_port() const { return this->tcp_port_; }
  std::string get_endpoint() const { return this->build_endpoint_string_(); }
  const char *get_audio_capability() const { return this->audio_capability_(); }

  // device_independent (default): true peer-to-peer, ESP dials the phonebook
  // entry. ha_pbx: every outbound call dials the HA entry instead, HA
  // bridges to the real dest_name (lets HA log calls and keep
  // intercom_native.forward functional even with direct reachability).
  enum class IntercomRoutingMode : uint8_t {
    DEVICE_INDEPENDENT = 0,
    HA_PBX = 1,
  };
  void set_routing_mode(IntercomRoutingMode mode) { this->routing_mode_ = mode; }
  IntercomRoutingMode routing_mode() const { return this->routing_mode_; }
  void set_use_ha_as_first_contact(bool enabled) { this->use_ha_as_first_contact_ = enabled; }
  void set_audio_debug(bool enabled) { this->audio_debug_ = enabled; }
  void set_mdns_announce_enabled(bool enabled) { this->mdns_announce_enabled_ = enabled; }

#ifdef USE_INTERCOM_MDNS_DISCOVERY
  void set_mdns_discovery_enabled(bool enabled) { this->mdns_discovery_enabled_ = enabled; }
  void set_mdns_discovery_scan_tcp(bool enabled) { this->mdns_discovery_scan_tcp_ = enabled; }
  void set_mdns_discovery_scan_udp(bool enabled) { this->mdns_discovery_scan_udp_ = enabled; }
  void set_mdns_discovery_startup_scan(bool enabled) { this->mdns_discovery_startup_scan_ = enabled; }
  void set_mdns_discovery_interval_ms(uint32_t ms) { this->mdns_discovery_interval_ms_ = ms; }
  void set_mdns_discovery_query_timeout_ms(uint32_t ms) { this->mdns_discovery_query_timeout_ms_ = ms; }
  void set_mdns_discovery_max_results(uint8_t max_results) { this->mdns_discovery_max_results_ = max_results; }
#else
  void set_mdns_discovery_enabled(bool enabled) { (void) enabled; }
  void set_mdns_discovery_scan_tcp(bool enabled) { (void) enabled; }
  void set_mdns_discovery_scan_udp(bool enabled) { (void) enabled; }
  void set_mdns_discovery_startup_scan(bool enabled) { (void) enabled; }
  void set_mdns_discovery_interval_ms(uint32_t ms) { (void) ms; }
  void set_mdns_discovery_query_timeout_ms(uint32_t ms) { (void) ms; }
  void set_mdns_discovery_max_results(uint8_t max_results) { (void) max_results; }
#endif

  /// Update the UDP peer endpoint at runtime; propagates to the live
  /// transport so the next datagram targets the new peer. `port` is audio;
  /// `control_port` is optional for short contact rows. TCP no-op.
  void set_remote_endpoint(const std::string &ip, uint16_t port, uint16_t control_port = 0);

  // Runtime control
  void start();
  void stop();
  bool is_active() const {
    CallState cs = this->call_state_.load(std::memory_order_acquire);
    return cs == CallState::STREAMING || cs == CallState::OUTGOING;
  }
  bool is_connected() const {
    return this->transport_ != nullptr && this->transport_->is_connected();
  }

  // Volume control
  void set_volume(float volume);
  float get_volume() const { return this->volume_.load(std::memory_order_relaxed); }

  // Auto-answer control (for incoming calls)
  void set_auto_answer(bool enabled);
  bool is_auto_answer() const { return this->auto_answer_; }
  void set_do_not_disturb(bool enabled);
  bool is_do_not_disturb() const { return this->do_not_disturb_; }

  // Manual answer for incoming call (when auto_answer is OFF)
  void answer_call();
  void decline_call(const std::string &reason = "");
  bool is_ringing() const { return this->call_state_.load(std::memory_order_acquire) == CallState::RINGING; }
  bool is_outgoing() const { return this->call_state_.load(std::memory_order_acquire) == CallState::OUTGOING; }
  bool is_idle() const { return this->call_state_.load(std::memory_order_acquire) == CallState::IDLE; }
  bool is_streaming() const { return this->call_state_.load(std::memory_order_acquire) == CallState::STREAMING; }
  // True when the selected contact name matches the configured HA peer.
  // Empty ha_peer_name_ disables the check (treated as "no HA configured").
  bool is_ha_destination() const {
    return !this->ha_peer_name_.empty() &&
           this->phonebook_.current_name() == this->ha_peer_name_;
  }
  void set_ha_peer_name(const std::string &name) { this->ha_peer_name_ = name; }

  // Smart call toggle: ringing → answer, active → hangup, idle → start
  void call_toggle();

  // Mic gain control (dB scale: -20 to +20)
  void set_mic_gain_db(float db);
  float get_mic_gain() const { return this->mic_gain_.load(std::memory_order_relaxed); }

  // ConnectionState is derived: DISCONNECTED if no transport / peer,
  // STREAMING when audio is flowing, CONNECTED otherwise.
  ConnectionState get_state() const {
    if (this->transport_ == nullptr || !this->transport_->is_connected())
      return ConnectionState::DISCONNECTED;
    return this->streaming_.load(std::memory_order_acquire)
               ? ConnectionState::STREAMING
               : ConnectionState::CONNECTED;
  }
  const char *get_state_str() const;

  // mode: raw_udp - UDP-only raw audio path. start() opens the audio path
  // to peer+port (set at runtime via the start action) and skips all
  // signaling (no MSG_START/RING/ANSWER/DECLINE/STOP). Use case: go2rtc
  // / browser RTC consumer that handles offer/answer externally.
  void set_raw_udp_mode(bool on) { this->raw_udp_mode_ = on; }

  // Sensor registration
  void set_state_sensor(text_sensor::TextSensor *sensor) { this->state_sensor_ = sensor; }
  void set_destination_sensor(text_sensor::TextSensor *sensor) { this->destination_sensor_ = sensor; }
  void set_caller_sensor(text_sensor::TextSensor *sensor) { this->caller_sensor_ = sensor; }
  void set_contacts_sensor(text_sensor::TextSensor *sensor) { this->contacts_sensor_ = sensor; }
  void set_transport_sensor(text_sensor::TextSensor *sensor) { this->transport_sensor_ = sensor; }
  void set_endpoint_sensor(text_sensor::TextSensor *sensor) { this->endpoint_sensor_ = sensor; }
  void set_last_reason_sensor(text_sensor::TextSensor *sensor) { this->last_reason_sensor_ = sensor; }
  // Phonebook source from HA: shipped YAMLs wire a homeassistant text_sensor
  // through ha_phonebook_text_sensor_id. Current firmware consumes the unified
  // protocol-aware sensor.intercom_phonebook and normalizes it locally.
  void set_ha_phonebook_sensor(text_sensor::TextSensor *sensor) { this->ha_phonebook_sensor_ = sensor; }
  // Prune threshold (1..10). 0 disables pruning entirely.
  void set_prune_threshold(uint8_t t) { this->prune_threshold_ = t; }

  // Entity registration (for state sync after boot)
  void register_auto_answer_switch(switch_::Switch *sw) { this->auto_answer_switch_ = sw; }
  void register_dnd_switch(switch_::Switch *sw) { this->dnd_switch_ = sw; }
  void register_routing_mode_switch(switch_::Switch *sw) { this->routing_mode_switch_ = sw; }
  void register_volume_number(number::Number *num) { this->volume_number_ = num; }
  void register_mic_gain_number(number::Number *num) { this->mic_gain_number_ = num; }
  // Contacts management. No-op in raw_udp mode.
  // Entry grammar:
  //   "Name"                       bare name placeholder
  //   "Name|ip|port"               TCP port or short UDP audio port
  //   "Name|ip|audio|control"      complete UDP endpoint
  // add_contact and set_contacts run the same idempotent merge: same shape
  // = no-op, missing endpoint upgraded in place, mismatched endpoint
  // replaced. Slot order is stable; only flush_contacts() trims.
  void add_contact(const std::string &entry);
  void remove_contact(const std::string &name);
  void set_contacts(const std::string &contacts_csv);  // batch wrapper, same merge rules per entry
  void flush_contacts();
  // Open a phonebook update cycle: commit any previously-open cycle (counter
  // advance + prune), then read the HA sensor (if configured + non-empty)
  // and fire the on_update_contacts trigger for downstream sources (mDNS).
  // The cycle remains open until the next update_contacts() call or
  // CYCLE_TIMEOUT_MS elapses (loop() safety net).
  void update_contacts();
  bool set_contact(const std::string &name);
  void call_contact(const std::string &name);
  void next_contact();
  void prev_contact();
  const std::string &get_current_destination() const;
  // Endpoint of the selected contact. Empty/zero means start() falls back
  // to the YAML remote_ip_/remote_port_.
  const std::string &get_current_contact_ip() const;
  uint16_t get_current_contact_port() const;
  uint16_t get_current_contact_control_port() const;
  std::string get_caller() const { return this->caller_sensor_ ? this->caller_sensor_->state : ""; }
  std::string get_contacts_csv() const;

  // Synthetic incoming-call trigger for transports without native signaling
  // (raw_udp). Drives the FSM into RINGING and starts the ringing timeout.
  void simulate_incoming_call(const std::string &caller);

  // Call state triggers (exposed to YAML)
  Trigger<> *get_ringing_trigger() { return &this->ringing_trigger_; }
  Trigger<> *get_streaming_trigger() { return &this->streaming_trigger_; }
  Trigger<> *get_idle_trigger() { return &this->idle_trigger_; }
  Trigger<> *get_outgoing_call_trigger() { return &this->outgoing_call_trigger_; }
  Trigger<> *get_dest_ringing_trigger() { return &this->dest_ringing_trigger_; }
  Trigger<std::string> *get_hangup_trigger() { return &this->hangup_trigger_; }
  Trigger<std::string> *get_call_failed_trigger() { return &this->call_failed_trigger_; }
  Trigger<> *get_destination_changed_trigger() { return &this->destination_changed_trigger_; }
  // Fires once per update_contacts() call after the HA-side merge completes
  // (or immediately if HA sensor is empty/absent). YAML hooks here to chain
  // additional sources (e.g. mDNS scan) that feed the same open cycle.
  Trigger<> *get_update_contacts_trigger() { return &this->update_contacts_trigger_; }

  // Call state getter
  CallState get_call_state() const { return this->call_state_.load(std::memory_order_acquire); }
  const char *get_call_state_str() const { return call_state_to_str(this->call_state_.load(std::memory_order_acquire)); }

 protected:
  // Phonebook cycle helpers (see intercom_settings.cpp).
  void track_csv_(const std::string &csv);  // populate seen_in_cycle_ with CSV names
  void commit_cycle_();                      // advance counters + prune via Phonebook::commit_cycle
  std::string normalize_phonebook_for_transport_(const std::string &contacts_csv);
  bool maybe_auto_select_ha_first_();
#ifdef USE_INTERCOM_MDNS_DISCOVERY
  void request_mdns_discovery_scan_();
  void process_pending_mdns_discovery_();
  std::string run_mdns_discovery_query_();
  static void mdns_discovery_task(void *param);
  void mdns_discovery_task_();
#endif

  bool has_microphone_() const {
#ifdef USE_INTERCOM_API_MIC
    return this->microphone_ != nullptr || this->microphone_source_ != nullptr;
#else
    return false;
#endif
  }
  bool has_speaker_() const {
#ifdef USE_INTERCOM_API_SPEAKER
    return this->speaker_ != nullptr;
#else
    return false;
#endif
  }
  const char *audio_capability_() const {
    const bool mic = this->has_microphone_();
    const bool spk = this->has_speaker_();
    if (mic && spk) return "full_duplex";
    if (mic) return "mic_only";
    if (spk) return "speaker_only";
    return "control_only";
  }

  // setup() phases. Kept separate so setup() reads as a transaction and the
  // failure cleanup stays in one place.
  void cleanup_partial_setup_();
  bool allocate_setup_buffers_();
  bool setup_audio_processor_();
  bool setup_transport_();
  bool start_runtime_tasks_();
#ifdef USE_INTERCOM_MDNS_DISCOVERY
  void start_mdns_discovery_();
#endif
  void publish_initial_state_later_();
  void fail_setup_();
  void handle_call_timeouts_(uint32_t now_ms, uint32_t calling_timeout_ms);
  void handle_udp_keepalive_(uint32_t now_ms);

#ifdef USE_INTERCOM_API_MIC
  // TX task: mic capture + send (Core 0). Created only when a microphone is
  // configured; recv/control live in the transport task.
  static void tx_task(void *param);
  void tx_task_();

  bool is_tx_stream_ready_() const;
  void send_chunk_(const uint8_t *data, size_t length);
  void process_tx_chunk_(const uint8_t *audio_chunk);
#endif
  void debug_log_pcm_level_(const char *label, const uint8_t *pcm, size_t bytes,
                            uint32_t &last_log_ms, uint32_t &frame_count);

  // Transport callbacks (registered in setup()).
  void on_audio_received_(const uint8_t *pcm, size_t bytes);
  void on_control_received_(MessageType type,
                            const uint8_t *payload, size_t len);
  struct ControlMessageFields {
    std::string call_id;
    std::string caller_route;
    std::string caller_name;
    std::string dest_route;
    std::string dest_name;
    std::string reason;
    uint8_t error_code{0};
    std::string error_detail;
  };
  bool decode_control_fields_(MessageType type, const uint8_t *data, size_t len,
                              ControlMessageFields *out) const;
  bool ignore_if_idle_or_stale_(const char *message_name, const std::string &call_id) const;
  void on_connection_change_(bool connected);
  bool can_accept_session_() const;

#ifdef USE_INTERCOM_API_MIC
  void on_microphone_data_(const uint8_t *data, size_t len);
#endif

  void set_active_(bool on);
  void set_streaming_(bool on);
  void notify_audio_tasks_();

  void publish_state_();
  void publish_last_reason_(const std::string &reason);
  void publish_destination_();
  void publish_caller_(const std::string &caller_name);
  void publish_contacts_();
  void publish_transport_();
  void publish_endpoint_();
#ifdef USE_INTERCOM_MDNS_ANNOUNCE
  void publish_mdns_endpoint_(const std::string &endpoint);
  void request_endpoint_publish_();
  static void ip_event_handler_(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data);
  bool ensure_mdns_announce_registered_(const std::string &endpoint);
#else
  void publish_mdns_endpoint_(const std::string &) {}
  void request_endpoint_publish_() {}
#endif
  std::string local_ip_string_() const;
  std::string build_endpoint_string_() const;

  void set_call_state_(CallState new_state);
  void end_call_(CallEndReason reason, const std::string &detail = "");
  // Shared teardown for calling/ringing timeouts: DECLINE("timeout"),
  // cache it as terminal, end locally.
  void fire_timeout_decline_();

  // === PBX-lite outbound helpers ===
  // Build body (call_id prefix + per-type tail) and dispatch to
  // transport_->send_control. False on missing transport / overflow.
  bool send_pbx_simple_(MessageType type, const std::string &call_id);
  bool send_pbx_decline_(const std::string &call_id, const std::string &reason);
  bool send_pbx_start_(const std::string &call_id,
                       const std::string &caller_route, const std::string &caller_name,
                       const std::string &dest_route, const std::string &dest_name);

  // Components
#ifdef USE_INTERCOM_API_MIC
  microphone::Microphone *microphone_{nullptr};
  microphone::MicrophoneSource *microphone_source_{nullptr};
#endif
#ifdef USE_INTERCOM_API_SPEAKER
  speaker::Speaker *speaker_{nullptr};
#endif

  // Mode and state
  bool raw_udp_mode_{false};
  std::atomic<bool> active_{false};
  std::atomic<bool> streaming_{false};
  std::atomic<CallState> call_state_{CallState::IDLE};  // FSM source of truth

  // Transport configuration (set from YAML before setup()). Defaults:
  // 6054 data plane (TCP and UDP share the number on different stacks),
  // 6055 UDP control plane. TCP carries signaling on the framed socket.
  TransportType protocol_{TransportType::TCP};
  std::string remote_ip_;          // UDP: peer endpoint IP
  uint16_t remote_port_{6054};     // UDP: peer audio port
  uint16_t remote_control_port_{0};  // UDP: peer control port, 0 = local control_port_
  uint16_t listen_port_{6054};     // UDP: local audio listen port
  uint16_t control_port_{6055};    // UDP: local control listen port
  uint16_t tcp_port_{6054};        // TCP: server listen + outbound originate
  IntercomRoutingMode routing_mode_{IntercomRoutingMode::DEVICE_INDEPENDENT};

  // Active network transport (created in setup() based on protocol_).
  std::unique_ptr<IntercomTransport> transport_;

  // Sensors
  text_sensor::TextSensor *state_sensor_{nullptr};
  text_sensor::TextSensor *destination_sensor_{nullptr};  // full: selected contact
  text_sensor::TextSensor *caller_sensor_{nullptr};       // full: who is calling
  text_sensor::TextSensor *contacts_sensor_{nullptr};     // full: contact count (e.g. "3 contacts")
  text_sensor::TextSensor *transport_sensor_{nullptr};    // diagnostic: "udp" or "tcp"
  text_sensor::TextSensor *endpoint_sensor_{nullptr};     // route source: Name|protocol|ip|ports
  text_sensor::TextSensor *last_reason_sensor_{nullptr};  // terminal reason for HA/card mirroring
  std::string last_reason_;
  std::string last_endpoint_;
#ifdef USE_INTERCOM_MDNS_ANNOUNCE
  std::string last_mdns_endpoint_;
  bool mdns_endpoint_warning_logged_{false};
  bool mdns_announce_enabled_{false};
  bool mdns_announce_registered_{false};
  std::atomic<bool> endpoint_publish_requested_{false};
#else
  bool mdns_announce_enabled_{false};
#endif

  // Registered entities (for state sync after boot)
  switch_::Switch *auto_answer_switch_{nullptr};
  switch_::Switch *dnd_switch_{nullptr};
  switch_::Switch *routing_mode_switch_{nullptr};
  bool entity_restore_applied_{false};
  number::Number *volume_number_{nullptr};
  number::Number *mic_gain_number_{nullptr};
  // Contacts management. Empty at boot; fed by the optional HA text_sensor
  // subscription unless ESP-side mDNS discovery is enabled, plus any YAML
  // sources wired on the on_update_contacts trigger.
  // Slot order is stable:
  // re-add keeps the slot, only the endpoint may upgrade or replace. The
  // cycle counter on each ContactEntry advances/resets in commit_cycle()
  // and prunes once delete_contact_missing_from.updates_number is hit.
  Phonebook phonebook_;
  // Phonebook update cycle state. update_contacts() opens a cycle, set_contacts()
  // calls within the open cycle add their CSV names to seen_in_cycle_, and the
  // next update_contacts() (or the loop() timeout safeguard) commits the cycle.
  text_sensor::TextSensor *ha_phonebook_sensor_{nullptr};
  std::unordered_set<std::string> seen_in_cycle_;
  uint32_t cycle_started_at_{0};
  bool cycle_active_{false};
  uint8_t prune_threshold_{0};  // 0 = pruning disabled (default)
  static constexpr uint32_t CYCLE_TIMEOUT_MS = 10000;  // safety net for stuck cycles
  // Empty by default. HA-facing YAML should set this to hass.config.location_name
  // through intercom_api.set_ha_peer_name so HA_PBX routing can locate the HA
  // phonebook entry. Direct P2P usage can leave it empty.
  std::string ha_peer_name_;
  bool use_ha_as_first_contact_{false};
  bool first_contacts_batch_committed_{false};
  std::string device_name_;  // This device's friendly name (to exclude from contacts)
  std::string last_published_destination_;
  std::string device_route_id_;  // routing key (yaml node name slug)

  // Optional ESP-side peer discovery. It is intentionally narrow: query only
  // _intercom-tcp/_intercom-udp, parse existing TXT keys, and merge into the
  // normal phonebook on the ESPHome loop thread.
#ifdef USE_INTERCOM_MDNS_DISCOVERY
  bool mdns_discovery_enabled_{false};
  bool mdns_discovery_scan_tcp_{false};
  bool mdns_discovery_scan_udp_{false};
  bool mdns_discovery_startup_scan_{true};
  uint32_t mdns_discovery_interval_ms_{60000};
  uint32_t mdns_discovery_query_timeout_ms_{1000};
  uint8_t mdns_discovery_max_results_{8};
  TaskHandle_t mdns_discovery_task_handle_{nullptr};
  StaticTask_t mdns_discovery_task_tcb_{};
  StackType_t *mdns_discovery_task_stack_{nullptr};
  std::atomic<bool> mdns_discovery_scan_requested_{false};
  std::atomic<bool> mdns_discovery_pending_{false};
  Mutex mdns_discovery_mutex_;
  std::string mdns_discovery_pending_csv_;
#endif

#ifdef USE_INTERCOM_API_MIC
  // Audio buffers
  audio_processor::RingBufferPtr mic_buffer_;

  // Per-iteration drain buffers, heap-allocated at setup() so the audio
  // tasks don't carry 4 KB VLAs on top of an 8 KB stack.
  static constexpr size_t kTxAudioChunkBytes = AUDIO_CHUNK_BYTES;       // 1024
  static constexpr size_t kMicConvertedSamples = AUDIO_CHUNK_BYTES / sizeof(int16_t);
#endif
#ifdef USE_INTERCOM_MDNS_DISCOVERY
  static constexpr uint32_t kMdnsDiscoveryTaskStackBytes = 6144;
  static constexpr uint32_t kMdnsDiscoveryStartupDelayMs = 3000;
#endif
#ifdef USE_INTERCOM_API_MIC
  uint8_t *tx_audio_chunk_{nullptr};
  bool read_tx_chunk_(uint8_t *audio_chunk, const uint8_t **chunk_data, void **release_item);

  // task_stacks_in_psram_: true puts task stacks in PSRAM (saves internal
  // heap on S3/P4 with heavy AFE/MWW/LVGL load); false keeps them in
  // internal RAM (the only option on plain ESP32). Honoured by the
  // transport's own task too.
  TaskHandle_t tx_task_handle_{nullptr};
  StaticTask_t tx_task_tcb_{};
  StackType_t *tx_task_stack_{nullptr};
#endif
  bool task_stacks_in_psram_{false};

  std::atomic<float> volume_{1.0f};
  bool audio_debug_{false};
  uint32_t audio_debug_last_tx_log_ms_{0};
  uint32_t audio_debug_last_rx_log_ms_{0};
  uint32_t audio_debug_tx_frames_{0};
  uint32_t audio_debug_rx_frames_{0};

  // First peer audio frame closes the ANSWER-echo loop (avoids A->B->A storms).
  std::atomic<bool> first_audio_received_{false};

  // UDP control keepalive. TCP owns its keepalive in tcp_transport.
  std::atomic<uint32_t> udp_last_peer_activity_ms_{0};
  std::atomic<uint32_t> udp_last_ping_sent_ms_{0};

  bool auto_answer_{true};
  bool do_not_disturb_{false};

  // Auto-decline timeouts (0 = disabled). Both fire DECLINE("timeout").
  uint32_t calling_timeout_ms_{0};
  uint32_t ringing_timeout_ms_{0};
  uint32_t ringing_start_time_{0};
  uint32_t outgoing_start_time_{0};

  // === PBX-lite call state ===
  // call_id format: "<caller_name><->dest_name>", echoed in every packet.
  // All fields are accessed from main loop AND transport recv task; only
  // the helpers below (set/snapshot/clear) are allowed to touch them.
  mutable Mutex call_state_mutex_;
  std::string current_call_id_;
  std::string current_caller_route_id_;
  std::string current_caller_name_;
  std::string current_dest_route_id_;
  std::string current_dest_name_;
  // Last terminal DECLINE: replayed briefly when a START with the same
  // call_id arrives again. The call_id is intentionally human readable
  // ("caller<->dest"), so this cache must be time-limited or a later real
  // call between the same two devices would inherit the previous decline reason.
  static constexpr uint32_t TERMINAL_DECLINE_REPLAY_MS = 1500;
  std::string last_terminal_decline_call_id_;
  std::string last_terminal_decline_reason_;
  uint32_t last_terminal_decline_ms_{0};

  struct CallSnapshot {
    std::string call_id;
    std::string caller_route;
    std::string caller_name;
    std::string dest_route;
    std::string dest_name;
  };

  // === Call-identity helpers (mutex-protected) ===
  void set_call_identity_(const std::string &call_id,
                          const std::string &caller_route, const std::string &caller_name,
                          const std::string &dest_route, const std::string &dest_name);
  void clear_call_identity_();
  CallSnapshot snapshot_call_identity_() const;
  std::string get_current_call_id_() const;
  void set_terminal_decline_(const std::string &call_id, const std::string &reason);
  void clear_terminal_decline_();
  void snapshot_terminal_decline_(std::string *call_id, std::string *reason, uint32_t *age_ms) const;
  bool ensure_mic_processing_buffer_();

  std::atomic<float> mic_gain_{1.0f};
  float mic_gain_db_{0.0f};  // UI-friendly dB value (persisted)

  // === Settings persistence (local flash) ===
  static constexpr uint8_t SETTINGS_VERSION = 1;

  struct StoredSettings {
    uint8_t version{SETTINGS_VERSION};
    uint8_t volume_pct{100};
    int8_t mic_gain_db{0};
    uint8_t reserved{0};
  };

  ESPPreferenceObject settings_pref_{};
  bool suppress_save_{false};
  bool save_scheduled_{false};

  void load_settings_();
  void schedule_save_settings_();
  void save_settings_();

  bool dc_offset_removal_{false};       // for mics with DC bias (SPH0645)
  bool buffers_in_psram_{false};
#ifdef USE_INTERCOM_API_MIC
  int32_t dc_offset_{0};

  // Pre-allocated to avoid task-stack VLAs.
  std::atomic<int16_t *> mic_converted_{nullptr};  // 512 samples, lazy for optional mic processing
#endif

  Trigger<> ringing_trigger_;
  Trigger<> streaming_trigger_;
  Trigger<> idle_trigger_;
  Trigger<> outgoing_call_trigger_;
  Trigger<> dest_ringing_trigger_;
  Trigger<std::string> hangup_trigger_;
  Trigger<std::string> call_failed_trigger_;
  Trigger<> destination_changed_trigger_;
  Trigger<> update_contacts_trigger_;
};

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

class IntercomApiVolume : public number::Number, public Parented<IntercomApi> {
 public:
  void control(float value) override {
    if (value != value) value = 0.0f;
    if (value < 0.0f) value = 0.0f;
    if (value > 100.0f) value = 100.0f;
    this->parent_->set_volume(value / 100.0f);
    this->publish_state(value);
  }
};

class IntercomApiMicGain : public number::Number, public Parented<IntercomApi> {
 public:
  void control(float value) override {
    if (value != value) value = 0.0f;
    if (value < -20.0f) value = -20.0f;
    if (value > 20.0f) value = 20.0f;
    this->parent_->set_mic_gain_db(value);
    this->publish_state(value);
  }
};

class IntercomApiAutoAnswer : public switch_::Switch, public Parented<IntercomApi> {
 public:
  void write_state(bool state) override {
    this->parent_->set_auto_answer(state);
    this->publish_state(state);
  }
};

class IntercomApiDndSwitch : public switch_::Switch, public Parented<IntercomApi> {
 public:
  void write_state(bool state) override {
    this->parent_->set_do_not_disturb(state);
    this->publish_state(state);
  }
};

class IntercomRoutingModeSwitch : public switch_::Switch, public Parented<IntercomApi> {
 public:
  void write_state(bool state) override {
    this->parent_->set_routing_mode(
        state ? IntercomApi::IntercomRoutingMode::HA_PBX
              : IntercomApi::IntercomRoutingMode::DEVICE_INDEPENDENT);
    this->publish_state(state);
  }
};

class IntercomCallButton : public button::Button, public Parented<IntercomApi> {
 protected:
  void press_action() override { this->parent_->call_toggle(); }
};

class IntercomNextContactButton : public button::Button, public Parented<IntercomApi> {
 protected:
  void press_action() override { this->parent_->next_contact(); }
};

class IntercomPreviousContactButton : public button::Button, public Parented<IntercomApi> {
 protected:
  void press_action() override { this->parent_->prev_contact(); }
};

class IntercomDeclineButton : public button::Button, public Parented<IntercomApi> {
 protected:
  void press_action() override { this->parent_->decline_call(); }
};

}  // namespace intercom_api
}  // namespace esphome

// Action / Condition templates. Included after the namespace closes
// because actions.h re-opens it and needs the full IntercomApi type.
#include "actions.h"

#endif  // USE_ESP32
