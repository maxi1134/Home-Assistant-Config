#include "intercom_api.h"

#ifdef USE_ESP32

#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace intercom_api {

static const char *const TAG = "intercom_api.fsm";

// === Call-identity helpers ===
// Mutex-guarded so the recv task and the main loop never observe a
// half-written std::string assignment to the per-call fields.

void IntercomApi::set_call_identity_(const std::string &call_id,
                                      const std::string &caller_route,
                                      const std::string &caller_name,
                                      const std::string &dest_route,
                                      const std::string &dest_name) {
  LockGuard l(this->call_state_mutex_);
  this->current_call_id_ = call_id;
  this->current_caller_route_id_ = caller_route;
  this->current_caller_name_ = caller_name;
  this->current_dest_route_id_ = dest_route;
  this->current_dest_name_ = dest_name;
}

void IntercomApi::clear_call_identity_() {
  LockGuard l(this->call_state_mutex_);
  this->current_call_id_.clear();
  this->current_caller_route_id_.clear();
  this->current_caller_name_.clear();
  this->current_dest_route_id_.clear();
  this->current_dest_name_.clear();
}

IntercomApi::CallSnapshot IntercomApi::snapshot_call_identity_() const {
  LockGuard l(this->call_state_mutex_);
  return CallSnapshot{this->current_call_id_,
                      this->current_caller_route_id_, this->current_caller_name_,
                      this->current_dest_route_id_, this->current_dest_name_};
}

std::string IntercomApi::get_current_call_id_() const {
  LockGuard l(this->call_state_mutex_);
  return this->current_call_id_;
}

void IntercomApi::set_terminal_decline_(const std::string &call_id, const std::string &reason) {
  LockGuard l(this->call_state_mutex_);
  this->last_terminal_decline_call_id_ = call_id;
  this->last_terminal_decline_reason_ = reason;
  this->last_terminal_decline_ms_ = millis();
}

void IntercomApi::clear_terminal_decline_() {
  LockGuard l(this->call_state_mutex_);
  this->last_terminal_decline_call_id_.clear();
  this->last_terminal_decline_reason_.clear();
  this->last_terminal_decline_ms_ = 0;
}

void IntercomApi::snapshot_terminal_decline_(std::string *call_id, std::string *reason, uint32_t *age_ms) const {
  LockGuard l(this->call_state_mutex_);
  if (call_id) *call_id = this->last_terminal_decline_call_id_;
  if (reason) *reason = this->last_terminal_decline_reason_;
  if (age_ms) *age_ms = this->last_terminal_decline_ms_ == 0
      ? UINT32_MAX
      : millis() - this->last_terminal_decline_ms_;
}

// === PBX-lite outbound helpers ===
// Single source of truth for control-message body framing.

bool IntercomApi::send_pbx_simple_(MessageType type, const std::string &call_id) {
  if (!this->transport_) return false;
  uint8_t buf[INTERCOM_MAX_CALL_ID_LEN + 4];
  size_t off = encode_call_id_prefix(buf, sizeof(buf), call_id);
  if (off == 0) return false;
  return this->transport_->send_control(type, buf, off);
}

bool IntercomApi::send_pbx_decline_(const std::string &call_id, const std::string &reason) {
  if (!this->transport_) return false;
  uint8_t buf[INTERCOM_MAX_CALL_ID_LEN + INTERCOM_MAX_REASON_LEN + 8];
  size_t off = encode_call_id_prefix(buf, sizeof(buf), call_id);
  if (off == 0) return false;
  size_t n = encode_lp_string(buf + off, sizeof(buf) - off, reason, INTERCOM_MAX_REASON_LEN);
  if (n == 0) return false;
  off += n;
  return this->transport_->send_control(MessageType::DECLINE, buf, off);
}

bool IntercomApi::send_pbx_start_(const std::string &call_id,
                                   const std::string &caller_route,
                                   const std::string &caller_name,
                                   const std::string &dest_route,
                                   const std::string &dest_name) {
  if (!this->transport_) return false;
  uint8_t buf[INTERCOM_MAX_CALL_ID_LEN + 4 * (INTERCOM_MAX_NAME_LEN + 1) + 4];
  size_t off = encode_call_id_prefix(buf, sizeof(buf), call_id);
  if (off == 0) return false;
  size_t n;
  n = encode_lp_string(buf + off, sizeof(buf) - off, caller_route, INTERCOM_MAX_ROUTE_ID_LEN);
  if (n == 0) return false;
  off += n;
  n = encode_lp_string(buf + off, sizeof(buf) - off, caller_name, INTERCOM_MAX_NAME_LEN);
  if (n == 0) return false;
  off += n;
  n = encode_lp_string(buf + off, sizeof(buf) - off, dest_route, INTERCOM_MAX_ROUTE_ID_LEN);
  if (n == 0) return false;
  off += n;
  n = encode_lp_string(buf + off, sizeof(buf) - off, dest_name, INTERCOM_MAX_NAME_LEN);
  if (n == 0) return false;
  off += n;
  // Surface the transport result so the FSM can distinguish "leg dialed"
  // from "leg silently dropped" (TCP no socket, UDP no peer address yet).
  return this->transport_->send_control(MessageType::START, buf, off);
}

bool IntercomApi::ignore_if_idle_or_stale_(const char *message_name,
                                           const std::string &call_id) const {
  if (this->call_state_.load(std::memory_order_acquire) == CallState::IDLE) {
    ESP_LOGD(TAG, "ignoring %s while idle (call_id=%s)", message_name, call_id.c_str());
    return true;
  }
  if (!call_id.empty()) {
    const std::string current_cid = this->get_current_call_id_();
    if (!current_cid.empty() && call_id != current_cid) {
      ESP_LOGD(TAG, "ignoring stale %s for call_id=%s (current=%s)",
               message_name, call_id.c_str(), current_cid.c_str());
      return true;
    }
  }
  return false;
}

// === Lifecycle / user actions ===

void IntercomApi::start() {
  if (this->call_state_.load(std::memory_order_acquire) != CallState::IDLE) {
    ESP_LOGW(TAG, "Cannot start call: already %s", this->get_call_state_str());
    return;
  }

  // mode: raw_udp - bypass FSM signaling. Endpoint must be set via
  // set_remote_endpoint or YAML remote_ip_/remote_port_; jump straight
  // to STREAMING (no MSG_START, no peer ack). The downstream consumer
  // (go2rtc / browser RTC) handles its own offer/answer.
  if (this->raw_udp_mode_) {
    if (this->protocol_ != TransportType::UDP) {
      ESP_LOGE(TAG, "raw_udp mode requires protocol: udp (codegen guard should have caught this)");
      return;
    }
    if (this->remote_ip_.empty() || this->remote_port_ == 0) {
      ESP_LOGW(TAG, "raw_udp start: no peer/port set; call "
                    "intercom_api.set_remote_endpoint before intercom_api.start");
      return;
    }
    ESP_LOGI(TAG, "%s -> %s:%u: raw_udp stream open (no signaling)",
             this->device_name_.c_str(), this->remote_ip_.c_str(),
             (unsigned) this->remote_port_);
    this->set_active_(true);
    // arms streaming_ + opens audio path; without it tx_task stays muted.
    this->set_streaming_(true);
    return;
  }

  // device_independent: dial the selected contact. ha_pbx: dial the HA
  // entry from the phonebook instead. dest_name is preserved in both, so
  // the PBX-lite payload always names the real callee for HA to bridge.
  std::string dial_ip = this->get_current_contact_ip();
  uint16_t dial_port = this->get_current_contact_port();
  uint16_t dial_control_port = this->get_current_contact_control_port();
  if (this->routing_mode_ == IntercomRoutingMode::HA_PBX) {
    if (this->ha_peer_name_.empty()) {
      ESP_LOGE(TAG,
               "ha_pbx routing requested but ha_peer_name is unset; "
               "the HA integration must push it (= location_name) via "
               "set_ha_peer_name, or YAML must set it explicitly");
      this->set_active_(false);
      this->set_call_state_(CallState::IDLE);
      return;
    }
    const auto *ha_entry = this->phonebook_.find(this->ha_peer_name_);
    if (ha_entry == nullptr || ha_entry->ip.empty() || ha_entry->port == 0) {
      ESP_LOGE(TAG, "ha_pbx routing requested but phonebook has no '%s' entry with ip+port",
               this->ha_peer_name_.c_str());
      this->set_active_(false);
      this->set_call_state_(CallState::IDLE);
      return;
    }
    dial_ip = ha_entry->ip;
    dial_port = ha_entry->port;
    dial_control_port = ha_entry->control_port;
  }

  // UDP: bind the transport to the resolved endpoint before opening
  // the stream so the next sendto targets the right peer. Empty ip
  // falls back to the YAML-configured remote_ip_/remote_port_.
  if (this->protocol_ == TransportType::UDP) {
    if (!dial_ip.empty() && dial_port > 0) {
      this->set_remote_endpoint(dial_ip, dial_port, dial_control_port);
    }
  }

  const auto &dest = this->get_current_destination();

  // call_id format: "<caller_name><->dest_name>". Empty phonebook falls back
  // to ha_peer_name_ if set, else "(unknown)" so logs stay readable; TCP
  // outbound below rejects the dial when ip/port don't resolve.
  const std::string dest_name = dest.empty()
      ? (this->ha_peer_name_.empty() ? std::string("(unknown)") : this->ha_peer_name_)
      : dest;
  const std::string call_id = this->device_name_ + "<->" + dest_name;
  const std::string caller_route =
      this->device_route_id_.empty() ? this->device_name_ : this->device_route_id_;
  // Phonebook grammar carries endpoint ports, not route_id; HA disambiguates
  // by IP/name at the bridge, so the route field is just human context.
  const std::string &dest_route = dest_name;

  this->set_call_identity_(call_id, caller_route, this->device_name_,
                            dest_route, dest_name);
  this->clear_terminal_decline_();

  ESP_LOGI(TAG, "%s -> %s: calling... (call_id=%s)",
           this->device_name_.c_str(), dest_name.c_str(), call_id.c_str());
  this->set_active_(true);
  this->set_call_state_(CallState::OUTGOING);
  this->outgoing_start_time_ = millis();

  // TCP needs an outbound connect before the START frame can be written.
  // (UDP already pinned the destination via set_remote_endpoint above.)
  if (this->protocol_ == TransportType::TCP && this->transport_ != nullptr) {
    if (dial_ip.empty() || dial_port == 0) {
      ESP_LOGE(TAG, "%s: TCP outgoing needs ip+port in phonebook for '%s' (got '%s':%u)",
               this->device_name_.c_str(), dest_name.c_str(),
               dial_ip.c_str(), (unsigned) dial_port);
      this->end_call_(CallEndReason::UNREACHABLE);
      return;
    }
    if (!this->transport_->originate(dial_ip, dial_port)) {
      ESP_LOGE(TAG, "%s: TCP originate to %s:%u failed",
               this->device_name_.c_str(), dial_ip.c_str(), (unsigned) dial_port);
      this->end_call_(CallEndReason::UNREACHABLE);
      return;
    }
  } else if (this->protocol_ == TransportType::UDP) {
    // YAML compile-time used to require remote_ip; now check at runtime.
    if (this->remote_ip_.empty() || this->remote_port_ == 0) {
      ESP_LOGE(TAG, "%s: UDP outgoing needs a peer (phonebook entry or YAML remote_ip+remote_port)",
               this->device_name_.c_str());
      this->end_call_(CallEndReason::UNREACHABLE);
      return;
    }
  }

  if (this->transport_ != nullptr) {
    if (!this->send_pbx_start_(call_id, caller_route, this->device_name_,
                                dest_route, dest_name)) {
      ESP_LOGE(TAG, "MSG_START encode/send failed");
      this->end_call_(CallEndReason::PROTOCOL_ERROR);
      return;
    }
  }
}

void IntercomApi::stop() {
  if (!this->active_.load(std::memory_order_acquire) && this->call_state_.load(std::memory_order_acquire) == CallState::IDLE) {
    return;
  }

  // mode: raw_udp - local close only, no peer signaling.
  if (this->raw_udp_mode_) {
    ESP_LOGI(TAG, "%s: raw_udp stream close", this->device_name_.c_str());
    this->end_call_(CallEndReason::LOCAL_HANGUP);
    this->set_active_(false);
    // set_streaming_(false) closes the UDP audio_socket_.
    this->set_streaming_(false);
    if (this->transport_) this->transport_->disconnect();
    return;
  }

  const std::string call_id = this->get_current_call_id_();
  const CallState state = this->call_state_.load(std::memory_order_acquire);
  ESP_LOGI(TAG, "%s: hanging up (state=%s call_id=%s)",
           this->device_name_.c_str(),
           call_state_to_str(state),
           call_id.c_str());

  // Teardown semantics:
  //   STREAMING -> MSG_HANGUP        (BYE: established call torn down)
  //   OUTGOING  -> MSG_DECLINE empty (CANCEL: caller withdraws)
  //   RINGING   -> MSG_DECLINE empty (callee silently dismisses)
  // Peer interprets empty DECLINE as remote_hangup so on_hangup fires
  // instead of on_call_failed.
  // Mark the terminal reason before signaling or blocking audio teardown.
  // Otherwise a fast bridge echo can race in and mask a local cancel as remote.
  this->end_call_(CallEndReason::LOCAL_HANGUP);
  if (this->transport_ && this->transport_->is_connected() && !call_id.empty()) {
    if (state == CallState::STREAMING) {
      this->send_pbx_simple_(MessageType::HANGUP, call_id);
    } else {
      this->send_pbx_decline_(call_id, "");
    }
  }
  this->set_active_(false);
  // set_streaming_(false) closes the UDP audio_socket_.
  this->set_streaming_(false);
  if (this->transport_) this->transport_->disconnect();
}

void IntercomApi::answer_call() {
  if (!this->is_ringing()) {
    ESP_LOGW(TAG, "Cannot answer: not ringing (state=%s)", this->get_call_state_str());
    return;
  }

  // UDP: the transport already pinned the caller endpoint from the inbound
  // MSG_START source address. Don't override from the local phonebook - the
  // currently selected contact may be a different peer.
  if (this->protocol_ != TransportType::UDP && !this->is_connected()) {
    ESP_LOGW(TAG, "Cannot answer: no connection");
    return;
  }

  const std::string call_id = this->get_current_call_id_();
  ESP_LOGI(TAG, "%s: answering call (call_id=%s)",
           this->device_name_.c_str(), call_id.c_str());
  if (this->transport_ && !call_id.empty()) {
    this->send_pbx_simple_(MessageType::ANSWER, call_id);
  } else if (this->transport_) {
    this->transport_->send_control(MessageType::ANSWER);
  }
  this->set_active_(true);
  this->set_streaming_(true);  // also publishes STREAMING state
}

void IntercomApi::decline_call(const std::string &reason) {
  // DECLINE is a pre-call rejection. Mid-call termination uses STOP, so a
  // YAML decline_call() during STREAMING falls back to stop() to avoid a
  // misleading "declined" reaching the peer after we'd already accepted.
  if (this->call_state_.load(std::memory_order_acquire) == CallState::STREAMING) {
    ESP_LOGD(TAG, "decline_call() during STREAMING -> falling back to stop()");
    this->stop();
    return;
  }
  if (this->is_ringing()) {
    ESP_LOGI(TAG, "%s: declining incoming call", this->device_name_.c_str());
  } else if (this->is_outgoing()) {
    ESP_LOGI(TAG, "%s: cancelling outgoing call to %s",
             this->device_name_.c_str(), this->get_current_destination().c_str());
  } else {
    ESP_LOGW(TAG, "Cannot decline: no call in progress (state=%s)",
             this->get_call_state_str());
    return;
  }

  // Empty reason => peer treats as remote_hangup; non-empty surfaces as
  // user-visible "Call ended: X".
  const std::string call_id = this->get_current_call_id_();
  // Cached so a retransmitted START with this call_id replays the same
  // DECLINE instead of being seen as a fresh ring.
  this->set_terminal_decline_(call_id, reason);

  this->end_call_(
      reason.empty() ? CallEndReason::LOCAL_HANGUP : CallEndReason::DECLINED,
      reason);
  if (this->transport_ && this->transport_->is_connected() && !call_id.empty()) {
    this->send_pbx_decline_(call_id, reason);
  }
  this->set_active_(false);
  this->streaming_.store(false, std::memory_order_release);

  // Disconnect after end_call_ so on_connection_change_(false) sees IDLE
  // and skips the REMOTE_HANGUP path that would mask our trigger.
  if (this->transport_ && this->transport_->is_connected()) {
    this->transport_->disconnect();
  }
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

// === State helpers ===

const char *IntercomApi::get_state_str() const {
  switch (this->call_state_.load(std::memory_order_acquire)) {
    case CallState::IDLE: return "Idle";
    case CallState::OUTGOING: return "Outgoing";
    case CallState::RINGING: return "Ringing";
    case CallState::STREAMING: return "Streaming";
    default: return "Unknown";
  }
}

void IntercomApi::publish_state_() {
  if (this->state_sensor_ != nullptr) {
    this->state_sensor_->publish_state(this->get_state_str());
  }
}

void IntercomApi::publish_last_reason_(const std::string &reason) {
  if (this->last_reason_ == reason) {
    return;
  }
  this->last_reason_ = reason;
  if (this->last_reason_sensor_ != nullptr) {
    this->last_reason_sensor_->publish_state(reason);
  }
}

void IntercomApi::set_active_(bool on) {
  bool was = this->active_.exchange(on, std::memory_order_acq_rel);
  if (was == on) return;
  this->notify_audio_tasks_();

  if (on) {
    this->first_audio_received_.store(false, std::memory_order_release);  // re-arm ANSWER echo for new call

#ifdef USE_INTERCOM_API_MIC
    if (this->microphone_) {
      this->microphone_->start();
    }
    if (this->microphone_source_) {
      this->microphone_source_->start();
    }
#endif
#ifdef USE_INTERCOM_API_SPEAKER
    if (this->speaker_) {
      this->speaker_->start();
    }
#endif
  } else {
    this->first_audio_received_.store(false, std::memory_order_release);

#ifdef USE_INTERCOM_API_SPEAKER
    if (this->speaker_) {
      this->speaker_->stop();
    }
#endif

#ifdef USE_INTERCOM_API_MIC
    if (this->microphone_) {
      this->microphone_->stop();
    }
    if (this->microphone_source_) {
      this->microphone_source_->stop();
    }
#endif
  }
}

void IntercomApi::set_streaming_(bool on) {
  this->streaming_.store(on, std::memory_order_release);
  this->notify_audio_tasks_();
  if (on) {
    if (this->protocol_ == TransportType::UDP && !this->raw_udp_mode_) {
      this->udp_last_peer_activity_ms_.store(millis(), std::memory_order_release);
      this->udp_last_ping_sent_ms_.store(0, std::memory_order_release);
    }
    // Lazy-bind the UDP audio socket only here so an idle device never
    // listens for incoming audio (TCP is a no-op).
    if (this->transport_) this->transport_->start_audio_path();

    // Drop stale frames from the previous call before audio resumes.
#ifdef USE_INTERCOM_API_MIC
    if (this->mic_buffer_) {
      this->mic_buffer_->reset();
    }
#endif
#ifdef USE_INTERCOM_API_MIC
    this->dc_offset_ = 0;
#endif

    this->set_call_state_(CallState::STREAMING);  // publishes state internally
  } else {
    this->udp_last_peer_activity_ms_.store(0, std::memory_order_release);
    this->udp_last_ping_sent_ms_.store(0, std::memory_order_release);
    // Stop listening for incoming audio so the device drops back to idle.
    if (this->transport_) this->transport_->stop_audio_path();
    this->publish_state_();
  }
}

void IntercomApi::notify_audio_tasks_() {
#ifdef USE_INTERCOM_API_MIC
  if (this->tx_task_handle_ != nullptr) {
    xTaskNotifyGive(this->tx_task_handle_);
  }
#endif
}

void IntercomApi::set_call_state_(CallState new_state) {
  if (new_state != CallState::IDLE) {
    this->enable_loop_soon_any_context();
  }
  // Atomic exchange so concurrent callers (recv-task START vs main-loop
  // ringing_timeout) can't both fire the triggers; the loser sees
  // old==new and returns.
  CallState old_state = this->call_state_.exchange(new_state, std::memory_order_acq_rel);
  if (old_state == new_state) return;

  if (new_state == CallState::STREAMING && this->caller_sensor_ != nullptr &&
      !this->caller_sensor_->state.empty()) {
    ESP_LOGI(TAG, "%s: %s -> %s with %s", this->device_name_.c_str(),
             call_state_to_str(old_state), call_state_to_str(new_state),
             this->caller_sensor_->state.c_str());
  } else {
    ESP_LOGI(TAG, "%s: %s -> %s", this->device_name_.c_str(),
             call_state_to_str(old_state), call_state_to_str(new_state));
  }

  // Defer trigger fires to the main loop: call-state changes come from
  // the intercom_srv task on Core 1, but on_* actions often touch LVGL
  // which must run on the main loop. Running inline trips the WD.
  switch (new_state) {
    case CallState::IDLE:
      this->defer([this]() { this->idle_trigger_.trigger(); });
      break;
    case CallState::OUTGOING:
      this->publish_last_reason_("");
      this->defer([this]() { this->outgoing_call_trigger_.trigger(); });
      break;
    case CallState::RINGING:
      this->publish_last_reason_("");
      this->defer([this]() { this->ringing_trigger_.trigger(); });
      break;
    case CallState::STREAMING:
      this->publish_last_reason_("");
      this->defer([this]() { this->streaming_trigger_.trigger(); });
      break;
  }

  this->publish_state_();
}

void IntercomApi::end_call_(CallEndReason reason, const std::string &detail) {
  if (this->call_state_.load(std::memory_order_acquire) == CallState::IDLE) return;

  std::string reason_str = detail.empty() ? call_end_reason_to_str(reason) : detail;
  const std::string call_id = this->get_current_call_id_();
  ESP_LOGI(TAG, "%s: call ended (%s) call_id=%s",
           this->device_name_.c_str(), reason_str.c_str(),
           call_id.empty() ? "(none)" : call_id.c_str());
  this->publish_last_reason_(reason_str);

  // Fire the matching trigger (deferred to main loop).
  if (reason == CallEndReason::DECLINED ||
      reason == CallEndReason::TIMEOUT ||
      reason == CallEndReason::REMOTE_DEVICE_LOST ||
      reason == CallEndReason::UNREACHABLE ||
      reason == CallEndReason::BUSY ||
      reason == CallEndReason::PROTOCOL_ERROR ||
      reason == CallEndReason::BRIDGE_ERROR) {
    this->defer([this, reason_str]() { this->call_failed_trigger_.trigger(reason_str); });
  } else {
    this->defer([this, reason_str]() { this->hangup_trigger_.trigger(reason_str); });
  }

  // Clear per-call identity. Terminal-decline cache stays so dup-START
  // can replay the same DECLINE; cleared by the next accepted START.
  this->clear_call_identity_();

  this->set_call_state_(CallState::IDLE);
}

// === Transport callbacks ===

void IntercomApi::on_audio_received_(const uint8_t *pcm, size_t bytes) {
  // First inbound audio is the strongest "call established" signal; gates
  // the ANSWER-echo loop in on_control_received_(ANSWER).
  this->first_audio_received_.store(true, std::memory_order_release);
  if (this->protocol_ == TransportType::UDP && !this->raw_udp_mode_) {
    this->udp_last_peer_activity_ms_.store(millis(), std::memory_order_release);
  }
  if (this->audio_debug_) {
    this->debug_log_pcm_level_("rx_network", pcm, bytes,
                               this->audio_debug_last_rx_log_ms_, this->audio_debug_rx_frames_);
  }
#ifdef USE_INTERCOM_API_SPEAKER
  if (this->speaker_) {
    if (this->volume_.load(std::memory_order_relaxed) > 0.001f) {
      this->speaker_->play(pcm, bytes, 0);
    }
  }
#endif

  // Audio arrival also promotes OUTGOING -> STREAMING (the dest answered).
  if (this->call_state_.load(std::memory_order_acquire) == CallState::OUTGOING) {
    ESP_LOGI(TAG, "Dest answered - received audio, transitioning to STREAMING");
    this->set_streaming_(true);
  }
}

bool IntercomApi::decode_control_fields_(MessageType type, const uint8_t *data, size_t len,
                                         ControlMessageFields *out) const {
  if (out == nullptr) {
    return false;
  }

  size_t off = 0;
  if (len > 0) {
    off = decode_call_id_prefix(data, len, &out->call_id);
    if (off == 0) {
      ESP_LOGW(TAG, "control prefix decode failed for MSG 0x%02X (len=%zu)",
               static_cast<unsigned>(type), len);
      return false;
    }
  }

  if (type == MessageType::START) {
    auto decode_start_field = [&](const char *field, std::string *value) -> bool {
      if (len <= off) {
        ESP_LOGW(TAG, "START %s decode failed", field);
        return false;
      }
      const size_t n = decode_lp_string(data + off, len - off, value);
      if (n == 0) {
        ESP_LOGW(TAG, "START %s decode failed", field);
        return false;
      }
      off += n;
      return true;
    };
    return decode_start_field("caller_route", &out->caller_route) &&
           decode_start_field("caller_name", &out->caller_name) &&
           decode_start_field("dest_route", &out->dest_route) &&
           decode_start_field("dest_name", &out->dest_name);
  }

  if (type == MessageType::DECLINE) {
    if (len == off) {
      return true;
    }
    const size_t n = decode_lp_string(data + off, len - off, &out->reason);
    if (n == 0) {
      ESP_LOGW(TAG, "DECLINE reason decode failed (truncated)");
      return false;
    }
    return true;
  }

  if (type == MessageType::ERROR) {
    if (len < off + 1) {
      ESP_LOGW(TAG, "ERROR truncated (no error_code)");
      return false;
    }
    out->error_code = data[off++];
    decode_lp_string(data + off, len - off, &out->error_detail);
  }
  return true;
}

void IntercomApi::on_control_received_(MessageType type,
                                        const uint8_t *data, size_t len) {
  if (this->protocol_ == TransportType::UDP && !this->raw_udp_mode_) {
    this->udp_last_peer_activity_ms_.store(millis(), std::memory_order_release);
  }
  // PBX-lite body shape: call_id_len + call_id bytes, then per-type tail.
  // Empty body = no-call-id; safety net for terminal messages emitted before
  // a call_id was locked in. PING/PONG carry call_id_len = 0.
  ControlMessageFields msg;
  if (!this->decode_control_fields_(type, data, len, &msg)) {
    return;
  }
  const std::string &in_call_id = msg.call_id;
  const std::string &in_caller_route = msg.caller_route;
  const std::string &in_caller_name = msg.caller_name;
  const std::string &in_dest_route = msg.dest_route;
  const std::string &in_dest_name = msg.dest_name;
  const std::string &in_reason = msg.reason;
  const uint8_t in_error_code = msg.error_code;
  const std::string &in_error_detail = msg.error_detail;

  auto send_call_id_only = [this](MessageType t, const std::string &cid) {
    this->send_pbx_simple_(t, cid);
  };
  auto send_decline_busy = [this](const std::string &cid) {
    this->send_pbx_decline_(cid, kReasonBusy);
  };

  switch (type) {
    case MessageType::START: {
      const std::string &incoming_cid = in_call_id;
      const std::string active_cid = this->get_current_call_id_();
      const CallState state = this->call_state_.load(std::memory_order_acquire);

      if (state == CallState::RINGING) {
        if (incoming_cid == active_cid) {
          ESP_LOGD(TAG, "START retransmit while RINGING - re-acking with RING");
          send_call_id_only(MessageType::RING, active_cid);
        } else {
          ESP_LOGW(TAG, "%s: another caller while RINGING - DECLINE busy",
                   this->device_name_.c_str());
          send_decline_busy(incoming_cid);
        }
        break;
      }
      if (state == CallState::STREAMING) {
        if (incoming_cid == active_cid) {
          ESP_LOGD(TAG, "START retransmit while STREAMING - re-acking with ANSWER");
          send_call_id_only(MessageType::ANSWER, active_cid);
        } else {
          ESP_LOGW(TAG, "%s: another caller while STREAMING - DECLINE busy",
                   this->device_name_.c_str());
          send_decline_busy(incoming_cid);
        }
        break;
      }
      if (state == CallState::OUTGOING) {
        if (incoming_cid == active_cid) {
          ESP_LOGD(TAG, "START echo of our own call_id while OUTGOING - ignored");
          break;
        }
        // Glare: both ends dialed each other. Recognised when the
        // inbound caller_name matches our current dest. Anything else is
        // a third-party collision (DECLINE busy).
        const auto active = this->snapshot_call_identity_();
        const bool is_glare = !in_caller_name.empty() &&
                              in_caller_name == active.dest_name;
        if (!is_glare) {
          ESP_LOGW(TAG, "%s: collision (we are OUTGOING) - DECLINE busy",
                   this->device_name_.c_str());
          send_decline_busy(incoming_cid);
          break;
        }
        // Tie-break: lexicographically lower device_name wins. Symmetric
        // on both sides so exactly one peer survives.
        const bool we_win = this->device_name_ < in_caller_name;
        if (we_win) {
          ESP_LOGW(TAG, "%s: glare with %s (we win), DECLINE(glare) inbound",
                   this->device_name_.c_str(), in_caller_name.c_str());
          this->send_pbx_decline_(incoming_cid, "glare");
          break;
        }
        ESP_LOGW(TAG, "%s: glare with %s (we lose), aborting OUTGOING and accepting inbound",
                 this->device_name_.c_str(), in_caller_name.c_str());
        // Retract silently. The peer never sees our ANSWER, so its
        // outgoing leg dies on its own timeout; a DECLINE here would
        // just race the peer's success.
        this->set_active_(false);
        this->set_call_state_(CallState::IDLE);
        goto handle_incoming_start_in_idle;
      }
handle_incoming_start_in_idle:
      // Replay the cached DECLINE for a dup START after ringing_timeout
      // / decline_call so the peer sees the same final response again.
      std::string cached_cid, cached_reason;
      uint32_t cached_age_ms = UINT32_MAX;
      this->snapshot_terminal_decline_(&cached_cid, &cached_reason, &cached_age_ms);
      if (!cached_cid.empty() && incoming_cid == cached_cid &&
          cached_age_ms <= TERMINAL_DECLINE_REPLAY_MS) {
        ESP_LOGD(TAG, "Replaying cached terminal DECLINE for %s", incoming_cid.c_str());
        this->send_pbx_decline_(incoming_cid, cached_reason);
        break;
      }

      // Accept the new call. Latch identity so RING/ANSWER/STOP can route
      // back with the same call_id.
      if (this->protocol_ == TransportType::UDP && !in_caller_name.empty()) {
        const auto *caller_entry = this->phonebook_.find(in_caller_name);
        if (caller_entry != nullptr && !caller_entry->ip.empty() && caller_entry->port != 0) {
          // UDP START arrives on the peer's control socket, so the transport
          // can learn the control port from source address. It cannot infer the
          // peer audio port; use the caller's phonebook entry when available.
          this->set_remote_endpoint(caller_entry->ip, caller_entry->port,
                                    caller_entry->control_port);
        }
      }

      if (this->do_not_disturb_) {
        ESP_LOGI(TAG, "%s: do-not-disturb active - DECLINE(%s) inbound call from %s",
                 this->device_name_.c_str(), kReasonDnd,
                 in_caller_name.empty() ? "(unknown caller)" : in_caller_name.c_str());
        this->set_terminal_decline_(incoming_cid, kReasonDnd);
        this->send_pbx_decline_(incoming_cid, kReasonDnd);
        if (this->transport_ != nullptr) {
          this->transport_->disconnect();
        }
        break;
      }

      const std::string dest_route = in_dest_route.empty()
          ? (this->device_route_id_.empty() ? this->device_name_ : this->device_route_id_)
          : in_dest_route;
      const std::string dest_name = in_dest_name.empty() ? this->device_name_ : in_dest_name;
      this->set_call_identity_(incoming_cid, in_caller_route, in_caller_name,
                                dest_route, dest_name);
      this->clear_terminal_decline_();

      const char *local = this->device_name_.c_str();
      const char *remote = in_caller_name.empty() ? "(unknown caller)" : in_caller_name.c_str();
      ESP_LOGI(TAG, "%s <- %s: incoming call (call_id=%s)",
               local, remote, incoming_cid.c_str());

      this->publish_caller_(in_caller_name);

      if (this->auto_answer_) {
        // ANSWER first so the peer learns the call is up, then bring our
        // audio path online.
        send_call_id_only(MessageType::ANSWER, incoming_cid);
        this->set_active_(true);
        this->set_streaming_(true);
      } else {
        // RING tells the peer we're presenting the call locally; their
        // on_dest_ringing handler updates the outgoing screen.
        send_call_id_only(MessageType::RING, incoming_cid);
        this->ringing_start_time_ = millis();
        this->set_call_state_(CallState::RINGING);
        ESP_LOGI(TAG, "%s: ringing (waiting for local answer)", this->device_name_.c_str());
      }
      break;
    }

    case MessageType::HANGUP: {
      // Stale-cid gate: drop HANGUP for a call_id that doesn't match the
      // current session (peer retransmit after reconnect, UDP reorder).
      // Empty cid is the anonymous case and is allowed through.
      if (this->ignore_if_idle_or_stale_("HANGUP", in_call_id)) break;
      ESP_LOGI(TAG, "%s: remote hung up (call_id=%s)",
               this->device_name_.c_str(),
               in_call_id.c_str());
      this->publish_caller_("");
      // Order matters: publish the terminal reason before disconnect() can
      // fire on_connection_change(false) and mask it as REMOTE_HANGUP.
      this->end_call_(CallEndReason::REMOTE_HANGUP);
      this->set_streaming_(false);
      this->set_active_(false);
      if (this->transport_) this->transport_->disconnect();
      break;
    }

    case MessageType::ANSWER: {
      // OUTGOING: commits us to STREAMING.
      // RINGING:  HA-side forced answer; mirror locally and echo back.
      // STREAMING (no audio yet): peer retry hadn't seen our reply; re-echo
      //   until first_audio_received_ flips.
      if (!in_call_id.empty()) {
        const std::string current_cid = this->get_current_call_id_();
        if (!current_cid.empty() && in_call_id != current_cid) {
          ESP_LOGD(TAG, "ignoring stale ANSWER for call_id=%s (current=%s)",
                   in_call_id.c_str(), current_cid.c_str());
          break;
        }
      }
      const CallState state = this->call_state_.load(std::memory_order_acquire);
      if (state == CallState::OUTGOING) {
        ESP_LOGI(TAG, "%s: destination answered, streaming (call_id=%s)",
                 this->device_name_.c_str(),
                 in_call_id.c_str());
        this->set_streaming_(true);
      } else if (state == CallState::RINGING) {
        ESP_LOGI(TAG, "%s: answered remotely (by HA)", this->device_name_.c_str());
        this->set_active_(true);
        this->set_streaming_(true);
        send_call_id_only(MessageType::ANSWER, this->get_current_call_id_());
      } else if (state == CallState::STREAMING &&
                 !this->first_audio_received_.load(std::memory_order_acquire)) {
        ESP_LOGD(TAG, "ANSWER repeat in STREAMING (no audio yet) - re-echoing ack");
        send_call_id_only(MessageType::ANSWER, this->get_current_call_id_());
      } else {
        const bool audio_seen = this->first_audio_received_.load(std::memory_order_acquire);
        ESP_LOGD(TAG, "ANSWER ignored in state %s (audio_seen=%d)",
                 call_state_to_str(state), audio_seen);
      }
      break;
    }

    case MessageType::PING:
      ESP_LOGD(TAG, "PING received in state %s - replying PONG",
               this->get_call_state_str());
      this->send_pbx_simple_(MessageType::PONG, "");
      break;

    case MessageType::PONG:
      // Socket-level keepalive (call_id_len=0). Logged for diagnostics.
      ESP_LOGD(TAG, "PONG received in state %s (no-op)", this->get_call_state_str());
      break;

    case MessageType::DECLINE: {
      // Stale-cid gate: see HANGUP/ANSWER above.
      if (this->ignore_if_idle_or_stale_("DECLINE", in_call_id)) break;
      // Empty reason: treat as a plain hangup (fires on_hangup).
      // Non-empty: surface verbatim ("Call ended: <reason>" on the UI).
      if (in_reason.empty()) {
        ESP_LOGI(TAG, "%s: remote declined (no reason) - treating as hangup (call_id=%s)",
                 this->device_name_.c_str(),
                 in_call_id.c_str());
        this->end_call_(CallEndReason::REMOTE_HANGUP);
        this->set_active_(false);
        if (this->transport_) this->transport_->disconnect();
      } else {
        ESP_LOGI(TAG, "%s: remote declined call (%s) (call_id=%s)",
                 this->device_name_.c_str(), in_reason.c_str(),
                 in_call_id.c_str());
        this->end_call_(CallEndReason::DECLINED, in_reason);
        this->set_active_(false);
        if (this->transport_) this->transport_->disconnect();
      }
      break;
    }

    case MessageType::ERROR: {
      // Stale-cid gate: see HANGUP/DECLINE/ANSWER above.
      if (this->ignore_if_idle_or_stale_("ERROR", in_call_id)) break;
      // Reserved for technical faults. Busy is signalled as DECLINE("busy").
      ESP_LOGE(TAG, "Received ERROR code=%u detail=%s", in_error_code,
               in_error_detail.empty() ? "(none)" : in_error_detail.c_str());
      this->end_call_(CallEndReason::PROTOCOL_ERROR, in_error_detail);
      this->set_active_(false);
      if (this->transport_) this->transport_->disconnect();
      break;
    }

    case MessageType::RING:
      // Caller side: dest is presenting our call. Stay in OUTGOING and
      // fire dest_ringing so the UI can flip "Calling X..." to "X is
      // ringing..." without forking the public state.
      if (this->call_state_.load(std::memory_order_acquire) == CallState::OUTGOING) {
        ESP_LOGI(TAG, "%s: dest is ringing (call_id=%s)",
                 this->device_name_.c_str(),
                 in_call_id.c_str());
        this->defer([this]() { this->dest_ringing_trigger_.trigger(); });
      } else {
        ESP_LOGD(TAG, "RING ignored in state %s", this->get_call_state_str());
      }
      break;

    default:
      ESP_LOGW(TAG, "Unknown control message type: 0x%02X",
               static_cast<unsigned>(type));
      break;
  }
}

void IntercomApi::on_connection_change_(bool connected) {
  if (connected) {
    // FSM stays IDLE until START arrives; can_accept_session_ has gated.
    return;
  }

  ESP_LOGI(TAG, "Transport disconnected");
  this->streaming_.store(false, std::memory_order_release);
  this->set_active_(false);
  this->publish_caller_("");
  if (this->call_state_.load(std::memory_order_acquire) != CallState::IDLE) {
    this->end_call_(CallEndReason::REMOTE_DEVICE_LOST);
  } else {
    this->publish_state_();
  }
}

bool IntercomApi::can_accept_session_() const {
  // Accept new TCP sessions only when IDLE or OUTGOING. OUTGOING covers
  // the bridged-caller case where the dest connects back.
  CallState cs = this->call_state_.load(std::memory_order_acquire);
  return cs == CallState::IDLE || cs == CallState::OUTGOING;
}

// Synthesize an incoming-call transition from a transport without its own
// signaling (UDP raw). Wired from YAML when the udp: component sees an
// INVITE-equivalent datagram.
void IntercomApi::simulate_incoming_call(const std::string &caller) {
  if (this->call_state_.load(std::memory_order_acquire) != CallState::IDLE) {
    ESP_LOGW(TAG, "simulate_incoming_call ignored: state=%s",
             this->get_call_state_str());
    return;
  }

  ESP_LOGI(TAG, "%s <- %s: incoming UDP call", this->device_name_.c_str(),
           caller.c_str());
  this->publish_caller_(caller);

  if (this->auto_answer_) {
    // Cross RINGING momentarily so answer_call's gate passes; the UDP
    // branch there pins the caller endpoint and opens the stream.
    this->set_call_state_(CallState::RINGING);
    this->answer_call();
  } else {
    this->ringing_start_time_ = millis();
    this->set_call_state_(CallState::RINGING);
  }
}

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32
