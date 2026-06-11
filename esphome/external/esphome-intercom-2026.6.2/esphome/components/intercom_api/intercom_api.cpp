#include "intercom_api.h"

#ifdef USE_ESP32

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../audio_processor/ring_buffer_caps.h"
#include "../audio_processor/task_utils.h"
#ifdef USE_INTERCOM_TCP_TRANSPORT
#include "tcp_transport.h"
#endif
#ifdef USE_INTERCOM_UDP_TRANSPORT
#include "udp_transport.h"
#endif

#include "esp_netif.h"
#ifdef USE_INTERCOM_MDNS_ANNOUNCE
#include "esp_event.h"
#include "mdns.h"
#endif

namespace esphome {
namespace intercom_api {

static const char *const TAG = "intercom_api";

bool IntercomApi::ensure_mic_processing_buffer_() {
#ifdef USE_INTERCOM_API_MIC
  if (this->mic_converted_.load(std::memory_order_acquire) != nullptr)
    return true;

  RAMAllocator<int16_t> alloc = this->buffers_in_psram_
      ? RAMAllocator<int16_t>()
      : RAMAllocator<int16_t>(RAMAllocator<int16_t>::ALLOC_INTERNAL);
  int16_t *buf = alloc.allocate(kMicConvertedSamples);
  if (buf == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate mic processing buffer");
    return false;
  }
  int16_t *expected = nullptr;
  if (!this->mic_converted_.compare_exchange_strong(
          expected, buf, std::memory_order_release, std::memory_order_acquire)) {
    alloc.deallocate(buf, kMicConvertedSamples);
  }
  return true;
#else
  ESP_LOGW(TAG, "Ignoring mic processing request: this intercom endpoint has no microphone");
  return false;
#endif
}

void IntercomApi::cleanup_partial_setup_() {
  // Transactional setup cleanup. force_delete is safe here only because tasks
  // were just spawned and have not entered a blocking upstream call yet.
#ifdef USE_INTERCOM_API_MIC
  audio_processor::force_delete_pinned_task(&this->tx_task_handle_, &this->tx_task_stack_,
                                             IntercomApi::kTxTaskStackBytes);

  RAMAllocator<int16_t> i16_alloc;
  if (int16_t *mic_converted = this->mic_converted_.exchange(nullptr, std::memory_order_acq_rel)) {
    i16_alloc.deallocate(mic_converted, kMicConvertedSamples);
  }

  RAMAllocator<uint8_t> u8_alloc;
  if (this->tx_audio_chunk_ != nullptr) {
    u8_alloc.deallocate(this->tx_audio_chunk_, IntercomApi::kTxAudioChunkBytes);
    this->tx_audio_chunk_ = nullptr;
  }

  this->mic_buffer_.reset();
#endif
  this->transport_.reset();
}

bool IntercomApi::allocate_setup_buffers_() {
#ifdef USE_INTERCOM_API_MIC
  if (this->has_microphone_()) {
    this->mic_buffer_ = this->buffers_in_psram_
        ? audio_processor::create_prefer_psram(TX_BUFFER_SIZE, "intercom.mic")
        : audio_processor::create_internal(TX_BUFFER_SIZE, "intercom.mic");
    if (!this->mic_buffer_) {
      ESP_LOGE(TAG, "Failed to allocate mic ring buffer");
      return false;
    }
  }

  if (this->has_microphone_() && this->dc_offset_removal_ && !this->ensure_mic_processing_buffer_()) {
    return false;
  }

  // Per-iteration drain buffers; same placement policy as above.
  RAMAllocator<uint8_t> psram_u8 = this->buffers_in_psram_
      ? RAMAllocator<uint8_t>()
      : RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::ALLOC_INTERNAL);
  if (this->has_microphone_()) {
    this->tx_audio_chunk_ = psram_u8.allocate(IntercomApi::kTxAudioChunkBytes);
    if (!this->tx_audio_chunk_) {
      ESP_LOGE(TAG, "Failed to allocate tx audio chunk buffer");
      return false;
    }
  }
#endif

  return true;
}

bool IntercomApi::setup_audio_processor_() {
#ifdef USE_INTERCOM_API_MIC
  if (this->microphone_ != nullptr) {
    this->microphone_->add_data_callback([this](const std::vector<uint8_t> &data) {
      this->on_microphone_data_(data.data(), data.size());
    });
  }
  if (this->microphone_source_ != nullptr) {
    this->microphone_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
      this->on_microphone_data_(data.data(), data.size());
    });
  }
#endif
  return true;
}

bool IntercomApi::setup_transport_() {
#ifdef USE_INTERCOM_UDP_TRANSPORT
  if (this->protocol_ == TransportType::UDP) {
    this->transport_ = std::make_unique<UdpTransport>(
        this->listen_port_, this->remote_ip_, this->remote_port_,
        this->control_port_, this->remote_control_port_,
        this->task_stacks_in_psram_);
  } else
#endif
#ifdef USE_INTERCOM_TCP_TRANSPORT
  if (this->protocol_ == TransportType::TCP) {
    this->transport_ = std::make_unique<TcpTransport>(this->tcp_port_, this->task_stacks_in_psram_);
  } else
#endif
  {
    ESP_LOGE(TAG, "Configured intercom transport was not compiled into this firmware");
    return false;
  }
  if (!this->transport_) {
    ESP_LOGE(TAG, "Failed to allocate transport");
    return false;
  }

  // Wire callbacks before start() so the transport task never fires into null.
  this->transport_->on_audio_frame = [this](const uint8_t *pcm, size_t bytes) {
    this->on_audio_received_(pcm, bytes);
  };
  this->transport_->on_control = [this](MessageType type,
                                        const uint8_t *payload, size_t len) {
    this->on_control_received_(type, payload, len);
  };
  this->transport_->on_connection_change = [this](bool connected) {
    this->on_connection_change_(connected);
  };
  this->transport_->should_accept_session = [this]() -> bool {
    return this->can_accept_session_();
  };

  if (!this->transport_->start()) {
    ESP_LOGE(TAG, "Transport failed to start");
    return false;
  }
  return true;
}

bool IntercomApi::start_runtime_tasks_() {
#ifdef USE_INTERCOM_API_MIC
  // TX task exists only when a microphone is configured. Speaker-only peers
  // still accept calls and play incoming audio through the transport recv task.
  if (this->has_microphone_()) {
    if (!audio_processor::start_pinned_task(IntercomApi::tx_task, "intercom_tx",
                                             IntercomApi::kTxTaskStackBytes, this, 5, 0,
                                             this->task_stacks_in_psram_, TAG,
                                             &this->tx_task_handle_, &this->tx_task_tcb_,
                                             &this->tx_task_stack_)) {
      return false;
    }
  }
#endif
  return true;
}

#ifdef USE_INTERCOM_MDNS_DISCOVERY
void IntercomApi::start_mdns_discovery_() {
  if (this->mdns_discovery_enabled_) {
    if (!audio_processor::start_pinned_task(IntercomApi::mdns_discovery_task, "intercom_mdns",
                                             IntercomApi::kMdnsDiscoveryTaskStackBytes, this, 3, 0,
                                             this->task_stacks_in_psram_, TAG,
                                             &this->mdns_discovery_task_handle_,
                                             &this->mdns_discovery_task_tcb_,
                                             &this->mdns_discovery_task_stack_)) {
      ESP_LOGW(TAG, "mDNS discovery disabled: failed to create discovery task");
      this->mdns_discovery_enabled_ = false;
    } else {
      if (this->mdns_discovery_startup_scan_) {
        this->set_timeout(SCHED_MDNS_STARTUP_SCAN, kMdnsDiscoveryStartupDelayMs, [this]() {
          this->request_mdns_discovery_scan_();
        });
      }
      if (this->mdns_discovery_interval_ms_ > 0) {
        this->set_interval(SCHED_MDNS_PERIODIC_SCAN, this->mdns_discovery_interval_ms_, [this]() {
          this->request_mdns_discovery_scan_();
        });
      }
    }
  }
}
#endif

void IntercomApi::publish_initial_state_later_() {
  // Deferred so sensors are fully wired before the first publish.
  this->set_timeout(SCHED_PUBLISH_INITIAL_STATE, 250, [this]() {
    this->publish_state_();
    this->publish_destination_();
    this->publish_transport_();
    this->publish_endpoint_();
  });
}

void IntercomApi::fail_setup_() {
  this->cleanup_partial_setup_();
  this->mark_failed();
}

void IntercomApi::setup() {
  ESP_LOGI(TAG, "Setting up Intercom API...");

  ESP_LOGI(TAG, "Audio capability: %s (transport: %s, tasks: %s)",
           this->audio_capability_(),
           this->protocol_ == TransportType::UDP ? "udp" : "tcp",
           this->has_microphone_() ? "tx+rx/control" : "rx/control");

  if (!this->allocate_setup_buffers_()) {
    this->fail_setup_();
    return;
  }
  if (!this->setup_audio_processor_()) {
    this->fail_setup_();
    return;
  }
  if (!this->setup_transport_()) {
    this->fail_setup_();
    return;
  }
  if (!this->start_runtime_tasks_()) {
    this->fail_setup_();
    return;
  }

  this->load_settings_();
#ifdef USE_INTERCOM_MDNS_ANNOUNCE
  if (this->mdns_announce_enabled_) {
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                        &IntercomApi::ip_event_handler_,
                                        this, nullptr);
  }
#endif
#ifdef USE_INTERCOM_MDNS_DISCOVERY
  this->start_mdns_discovery_();
#endif
  this->publish_initial_state_later_();

  if (this->protocol_ == TransportType::UDP) {
    ESP_LOGI(TAG, "Intercom API ready on UDP port %u (peer %s:%u)",
             (unsigned) this->listen_port_, this->remote_ip_.c_str(),
             (unsigned) this->remote_port_);
  } else {
    ESP_LOGI(TAG, "Intercom API ready on TCP port %u", (unsigned) this->tcp_port_);
  }
}

void IntercomApi::handle_call_timeouts_(uint32_t now_ms, uint32_t calling_timeout_ms) {
  const CallState state = this->call_state_.load(std::memory_order_acquire);
  if (this->ringing_timeout_ms_ > 0 && state == CallState::RINGING &&
      now_ms - this->ringing_start_time_ >= this->ringing_timeout_ms_) {
    const std::string cid = this->get_current_call_id_();
    ESP_LOGI(TAG, "Ringing timeout after %u ms - declining caller (call_id=%s)",
             this->ringing_timeout_ms_, cid.c_str());
    this->fire_timeout_decline_();
    return;
  }

  if (calling_timeout_ms > 0 && state == CallState::OUTGOING &&
      now_ms - this->outgoing_start_time_ >= calling_timeout_ms) {
    const std::string cid = this->get_current_call_id_();
    ESP_LOGI(TAG, "Calling timeout after %u ms - cancelling outgoing (call_id=%s)",
             calling_timeout_ms, cid.c_str());
    this->fire_timeout_decline_();
  }
}

void IntercomApi::handle_udp_keepalive_(uint32_t now_ms) {
  if (this->protocol_ != TransportType::UDP || this->raw_udp_mode_ ||
      this->call_state_.load(std::memory_order_acquire) != CallState::STREAMING ||
      this->transport_ == nullptr || !this->transport_->is_connected()) {
    return;
  }

  const uint32_t last_activity = this->udp_last_peer_activity_ms_.load(std::memory_order_acquire);
  if (last_activity == 0) {
    this->udp_last_peer_activity_ms_.store(now_ms, std::memory_order_release);
    return;
  }

  uint32_t effective_now = now_ms;
  const uint32_t silence = now_ms - last_activity;
  if (silence > KEEPALIVE_DEADLINE_MS) {
    // Re-read before tearing down: UDP audio/control callbacks update this
    // from transport tasks, and using a stale sample can kill a live call.
    const uint32_t fresh_activity = this->udp_last_peer_activity_ms_.load(std::memory_order_acquire);
    const uint32_t fresh_now = millis();
    const uint32_t fresh_silence = fresh_now - fresh_activity;
    if (fresh_activity != 0 && fresh_silence <= KEEPALIVE_DEADLINE_MS) {
      effective_now = fresh_now;
    } else {
      const std::string call_id = this->get_current_call_id_();
      ESP_LOGW(TAG, "UDP peer keepalive timeout after %u ms",
               (unsigned) (fresh_activity != 0 ? fresh_silence : silence));
      this->end_call_(CallEndReason::REMOTE_DEVICE_LOST);
      if (!call_id.empty()) {
        this->send_pbx_simple_(MessageType::HANGUP, call_id);
      }
      this->set_active_(false);
      this->set_streaming_(false);
      this->publish_caller_("");
      this->transport_->disconnect();
      return;
    }
  }

  const uint32_t last_ping = this->udp_last_ping_sent_ms_.load(std::memory_order_acquire);
  if (last_ping == 0 || effective_now - last_ping >= PING_INTERVAL_MS) {
    if (this->send_pbx_simple_(MessageType::PING, "")) {
      this->udp_last_ping_sent_ms_.store(effective_now, std::memory_order_release);
    }
  }
}

void IntercomApi::loop() {
#ifdef USE_INTERCOM_MDNS_ANNOUNCE
  if (this->endpoint_publish_requested_.exchange(false, std::memory_order_acq_rel)) {
    this->publish_endpoint_();
  }
#endif
#ifdef USE_INTERCOM_MDNS_DISCOVERY
  this->process_pending_mdns_discovery_();
#endif

  // Phonebook cycle timeout safeguard: a stuck on_update_contacts chain (e.g.
  // an external update source never completes) would otherwise leave the cycle open forever
  // and block subsequent counter advances. CYCLE_TIMEOUT_MS commits forcibly.
  if (this->cycle_active_ &&
      (millis() - this->cycle_started_at_) > CYCLE_TIMEOUT_MS) {
    ESP_LOGD("intercom_api", "Phonebook update cycle auto-commit after %u ms",
             (unsigned) CYCLE_TIMEOUT_MS);
    this->commit_cycle_();
  }

  // Auto-decline timeouts (0 = disabled). OUTGOING falls back to
  // ringing_timeout when calling_timeout is unset.
  uint32_t now = millis();
  const uint32_t calling_to = this->calling_timeout_ms_ > 0
                                ? this->calling_timeout_ms_
                                : this->ringing_timeout_ms_;

  this->handle_call_timeouts_(now, calling_to);
  this->handle_udp_keepalive_(now);

  bool keep_loop = this->cycle_active_ ||
                   this->call_state_.load(std::memory_order_acquire) != CallState::IDLE;
#ifdef USE_INTERCOM_MDNS_ANNOUNCE
  keep_loop = keep_loop || this->endpoint_publish_requested_.load(std::memory_order_acquire);
#endif
#ifdef USE_INTERCOM_MDNS_DISCOVERY
  keep_loop = keep_loop || this->mdns_discovery_pending_.load(std::memory_order_acquire);
#endif
  if (!keep_loop) {
    this->disable_loop();
  }
}

void IntercomApi::fire_timeout_decline_() {
  // DECLINE("timeout") + cache it so dup START replays the same response.
  const std::string call_id = this->get_current_call_id_();
  if (this->transport_ && this->transport_->is_connected() && !call_id.empty()) {
    this->send_pbx_decline_(call_id, kReasonTimeout);
  }
  this->set_terminal_decline_(call_id, kReasonTimeout);
  this->set_active_(false);
  this->streaming_.store(false, std::memory_order_release);
  this->publish_caller_("");
  this->end_call_(CallEndReason::TIMEOUT, kReasonTimeout);
  if (this->transport_) this->transport_->disconnect();
}

void IntercomApi::dump_config() {
  ESP_LOGCONFIG(TAG, "Intercom API:");
  if (this->transport_) {
    ESP_LOGCONFIG(TAG, "  Transport: %s", this->transport_->transport_name());
  } else {
    ESP_LOGCONFIG(TAG, "  Transport: (not initialised)");
  }
  if (this->protocol_ == TransportType::UDP) {
    ESP_LOGCONFIG(TAG, "  Audio listen port: %u", (unsigned) this->listen_port_);
    ESP_LOGCONFIG(TAG, "  Control port: %u", (unsigned) this->control_port_);
    ESP_LOGCONFIG(TAG, "  Remote: %s:%u control=%u", this->remote_ip_.c_str(),
                  (unsigned) this->remote_port_,
                  (unsigned) (this->remote_control_port_ != 0
                                  ? this->remote_control_port_
                                  : this->control_port_));
  } else {
    ESP_LOGCONFIG(TAG, "  Port: %u", (unsigned) this->tcp_port_);
  }
  ESP_LOGCONFIG(TAG, "  Routing mode: %s",
                this->routing_mode_ == IntercomRoutingMode::HA_PBX
                    ? "HA_PBX"
                    : "DEVICE_INDEPENDENT");
  ESP_LOGCONFIG(TAG, "  HA peer name: %s", this->ha_peer_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Audio capability: %s", this->audio_capability_());
  ESP_LOGCONFIG(TAG, "  HA as first contact: %s", YESNO(this->use_ha_as_first_contact_));
#ifdef USE_INTERCOM_MDNS_DISCOVERY
  ESP_LOGCONFIG(TAG, "  mDNS discovery: %s%s%s interval=%u ms timeout=%u ms max=%u",
                YESNO(this->mdns_discovery_enabled_),
                this->mdns_discovery_scan_tcp_ ? " tcp" : "",
                this->mdns_discovery_scan_udp_ ? " udp" : "",
                (unsigned) this->mdns_discovery_interval_ms_,
                (unsigned) this->mdns_discovery_query_timeout_ms_,
                (unsigned) this->mdns_discovery_max_results_);
#else
  ESP_LOGCONFIG(TAG, "  mDNS discovery: NO");
#endif
#ifdef USE_INTERCOM_API_MIC
  ESP_LOGCONFIG(TAG, "  Microphone: %s", this->microphone_ ? "direct" : (this->microphone_source_ ? "source" : "none"));
#endif
#ifdef USE_INTERCOM_API_SPEAKER
  ESP_LOGCONFIG(TAG, "  Speaker: %s", this->speaker_ ? "configured" : "none");
#endif
  ESP_LOGCONFIG(TAG, "  Tasks: %s", this->has_microphone_() ? "tx+rx/control" : "rx/control only");
  ESP_LOGCONFIG(TAG, "  Device Name: %s",
                this->device_name_.empty() ? "(unset)" : this->device_name_.c_str());
  if (this->ringing_timeout_ms_ > 0) {
    ESP_LOGCONFIG(TAG, "  Ringing Timeout: %u ms", this->ringing_timeout_ms_);
  } else {
    ESP_LOGCONFIG(TAG, "  Ringing Timeout: disabled");
  }
  if (this->calling_timeout_ms_ > 0) {
    ESP_LOGCONFIG(TAG, "  Calling Timeout: %u ms", this->calling_timeout_ms_);
  } else {
    ESP_LOGCONFIG(TAG, "  Calling Timeout: disabled");
  }
  ESP_LOGCONFIG(TAG, "  Contacts: %zu configured", this->phonebook_.size());
}

void IntercomApi::set_remote_endpoint(const std::string &ip, uint16_t port, uint16_t control_port) {
  this->remote_ip_ = ip;
  this->remote_port_ = port;
  this->remote_control_port_ = control_port;
  // No-op on TCP.
  if (this->transport_ != nullptr) {
    this->transport_->set_remote(ip, port, control_port);
  }
  ESP_LOGI(TAG, "Remote endpoint updated to %s:%u control=%u", ip.c_str(), (unsigned) port,
           (unsigned) (control_port != 0 ? control_port : this->control_port_));
}

void IntercomApi::publish_transport_() {
  if (this->transport_sensor_ != nullptr) {
    const char *t = (this->protocol_ == TransportType::UDP) ? "udp" : "tcp";
    this->transport_sensor_->publish_state(t);
  }
}

std::string IntercomApi::local_ip_string_() const {
  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (sta_netif == nullptr) {
    return "";
  }
  esp_netif_ip_info_t ip_info{};
  if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
    return "";
  }
  char ip[16];
  snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ip_info.ip));
  return ip;
}

std::string IntercomApi::build_endpoint_string_() const {
  const std::string name = !this->device_name_.empty()
                               ? this->device_name_
                               : App.get_friendly_name().str();
  const std::string ip = this->local_ip_string_();
  if (name.empty() || ip.empty()) {
    return "";
  }

  char buf[192];
  if (this->protocol_ == TransportType::UDP) {
    snprintf(buf, sizeof(buf), "%s|udp|%s|%u|%u|%s", name.c_str(), ip.c_str(),
             (unsigned) this->listen_port_, (unsigned) this->control_port_,
             this->audio_capability_());
  } else {
    snprintf(buf, sizeof(buf), "%s|tcp|%s|%u|%s", name.c_str(), ip.c_str(),
             (unsigned) this->tcp_port_, this->audio_capability_());
  }
  return buf;
}

void IntercomApi::publish_endpoint_() {
  std::string endpoint = this->build_endpoint_string_();
  if (this->endpoint_sensor_ != nullptr && endpoint != this->last_endpoint_) {
    this->last_endpoint_ = endpoint;
    this->endpoint_sensor_->publish_state(endpoint);
  }
  this->publish_mdns_endpoint_(endpoint);
}

#ifdef USE_INTERCOM_MDNS_ANNOUNCE
void IntercomApi::request_endpoint_publish_() {
  this->endpoint_publish_requested_.store(true, std::memory_order_release);
  this->enable_loop_soon_any_context();
}

void IntercomApi::ip_event_handler_(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data) {
  if (event_base != IP_EVENT) return;
  if (event_id != IP_EVENT_STA_GOT_IP && event_id != IP_EVENT_ETH_GOT_IP) return;
  static_cast<IntercomApi *>(arg)->request_endpoint_publish_();
}

bool IntercomApi::ensure_mdns_announce_registered_(const std::string &endpoint) {
  if (this->mdns_announce_registered_) return true;

  const esp_err_t init_err = mdns_init();
  if (init_err != ESP_OK) {
    if (!this->mdns_endpoint_warning_logged_) {
      ESP_LOGW(TAG, "mDNS announce init failed: %s", esp_err_to_name(init_err));
      this->mdns_endpoint_warning_logged_ = true;
    }
    return false;
  }

  mdns_hostname_set(App.get_name().c_str());
  const std::string instance = !this->device_name_.empty()
                                   ? this->device_name_
                                   : App.get_friendly_name().str();
  const char *service = this->protocol_ == TransportType::UDP ? "_intercom-udp" : "_intercom-tcp";
  const char *proto = this->protocol_ == TransportType::UDP ? "_udp" : "_tcp";
  const uint16_t port = this->protocol_ == TransportType::UDP ? this->listen_port_ : this->tcp_port_;
  mdns_txt_item_t txt[] = {
      {"endpoint", endpoint.c_str()},
      {"friendly_name", instance.c_str()},
  };

  esp_err_t err = mdns_service_add(instance.c_str(), service, proto, port, txt, 2);
  if (err == ESP_ERR_INVALID_ARG) {
    // Service may already exist; refresh its mutable parts below.
    mdns_service_port_set(service, proto, port);
    err = mdns_service_txt_item_set(service, proto, "friendly_name", instance.c_str());
  }
  if (err != ESP_OK) {
    if (!this->mdns_endpoint_warning_logged_) {
      ESP_LOGW(TAG, "mDNS announce service %s.%s failed: %s", service, proto,
               esp_err_to_name(err));
      this->mdns_endpoint_warning_logged_ = true;
    }
    return false;
  }
  this->mdns_announce_registered_ = true;
  this->mdns_endpoint_warning_logged_ = false;
  return true;
}

void IntercomApi::publish_mdns_endpoint_(const std::string &endpoint) {
  if (!this->mdns_announce_enabled_ || endpoint.empty()) return;
  if (!this->ensure_mdns_announce_registered_(endpoint)) return;
  if (endpoint == this->last_mdns_endpoint_) return;

  const char *service = this->protocol_ == TransportType::UDP ? "_intercom-udp" : "_intercom-tcp";
  const char *proto = this->protocol_ == TransportType::UDP ? "_udp" : "_tcp";
  const esp_err_t err = mdns_service_txt_item_set(service, proto, "endpoint", endpoint.c_str());
  if (err != ESP_OK) {
    this->mdns_announce_registered_ = false;
    if (!this->mdns_endpoint_warning_logged_) {
      ESP_LOGW(TAG, "mDNS endpoint update failed: %s", esp_err_to_name(err));
      this->mdns_endpoint_warning_logged_ = true;
    }
    return;
  }
  this->last_mdns_endpoint_ = endpoint;
  this->mdns_endpoint_warning_logged_ = false;
  ESP_LOGD(TAG, "mDNS endpoint announced: %s", endpoint.c_str());
}
#endif

// Wired from YAML via `api.on_client_connected:`.
void IntercomApi::publish_entity_states() {
  // Re-publish on every HA reconnect so intercom_native sees it without
  // depending on HA restart timing. Restore-backed switches are applied only
  // once; a reconnect must not roll runtime state back to the boot preference.
  const bool apply_restore = !this->entity_restore_applied_;
  this->entity_restore_applied_ = true;

  this->publish_transport_();
  this->publish_endpoint_();
  if (this->last_reason_sensor_ != nullptr) {
    this->last_reason_sensor_->publish_state(this->last_reason_);
  }

  if (this->auto_answer_switch_ != nullptr) {
    if (apply_restore) {
      auto initial = this->auto_answer_switch_->get_initial_state_with_restore_mode();
      if (initial.has_value()) {
        this->auto_answer_ = *initial;
      }
    }
    this->auto_answer_switch_->publish_state(this->auto_answer_);
  }

  if (this->dnd_switch_ != nullptr) {
    if (apply_restore) {
      auto initial = this->dnd_switch_->get_initial_state_with_restore_mode();
      if (initial.has_value()) {
        this->do_not_disturb_ = *initial;
      }
    }
    this->dnd_switch_->publish_state(this->do_not_disturb_);
  }

  if (this->routing_mode_switch_ != nullptr) {
    if (apply_restore) {
      auto initial = this->routing_mode_switch_->get_initial_state_with_restore_mode();
      if (initial.has_value()) {
        this->routing_mode_ = *initial ? IntercomRoutingMode::HA_PBX
                                       : IntercomRoutingMode::DEVICE_INDEPENDENT;
      }
    }
    const bool ha_pbx_now = this->routing_mode_ == IntercomRoutingMode::HA_PBX;
    this->routing_mode_switch_->publish_state(ha_pbx_now);
  }

  ESP_LOGD(TAG, "Entity states synced (vol=%.0f%%, mic=%.1fdB, auto=%s, dnd=%s)",
           this->volume_.load(std::memory_order_relaxed) * 100.0f, this->mic_gain_db_,
           this->auto_answer_ ? "ON" : "OFF", this->do_not_disturb_ ? "ON" : "OFF");

  if (this->volume_number_ != nullptr) {
    this->volume_number_->publish_state(this->volume_.load(std::memory_order_relaxed) * 100.0f);
  }
  if (this->mic_gain_number_ != nullptr) {
    this->mic_gain_number_->publish_state(this->mic_gain_db_);
  }
}

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32
