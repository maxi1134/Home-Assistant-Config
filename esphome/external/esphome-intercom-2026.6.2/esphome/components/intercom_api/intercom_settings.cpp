#include "intercom_api.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cmath>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace intercom_api {

static const char *const TAG = "intercom_api.settings";

namespace {

struct ParsedPhonebookSlot {
  ContactEntry entry;
  uint16_t ha_udp_audio_port{0};
};

float db_to_linear(float db) {
  return std::pow(10.0f, db / 20.0f);
}

float clamp_volume_(float volume) {
  if (!std::isfinite(volume)) return 0.0f;
  return std::max(0.0f, std::min(1.0f, volume));
}

float clamp_mic_gain_db_(float db) {
  if (!std::isfinite(db)) return 0.0f;
  return std::max(-20.0f, std::min(20.0f, db));
}

bool parse_slot_for_normalize(const std::string &raw, ParsedPhonebookSlot *slot) {
  const auto parts = Phonebook::split(raw, '|');
  ContactProtocol protocol = ContactProtocol::UNKNOWN;

  // Unified HA rows carry every HA transport port:
  //   HA Name|ha|ip|tcp_port|udp_audio_port|udp_control_port
  // Phonebook::ContactEntry only stores one primary port, so keep the UDP
  // audio port beside the parsed slot for local transport shaping.
  if (parts.size() >= 2 &&
      Phonebook::parse_protocol(Phonebook::trim(parts[1]), &protocol) &&
      protocol == ContactProtocol::HA && parts.size() == 6) {
    ContactEntry entry;
    entry.name = Phonebook::trim(parts[0]);
    entry.protocol = ContactProtocol::HA;
    entry.ip = Phonebook::trim(parts[2]);
    if (entry.name.empty() || entry.ip.empty()) return false;
    if (!Phonebook::parse_u16(Phonebook::trim(parts[3]), &entry.port)) return false;
    if (!Phonebook::parse_u16(Phonebook::trim(parts[4]), &slot->ha_udp_audio_port)) return false;
    if (!Phonebook::parse_u16(Phonebook::trim(parts[5]), &entry.control_port)) return false;
    slot->entry = entry;
    return true;
  }

  if (!Phonebook::parse_entry(raw, &slot->entry)) return false;
  if (slot->entry.protocol == ContactProtocol::HA) {
    // Short HA row: use the primary port for both TCP and UDP audio. This is
    // correct for the default 6054/6055 config; canonical HA rows can carry
    // distinct TCP, UDP audio and UDP control ports.
    slot->ha_udp_audio_port = slot->entry.port;
  }
  return true;
}

ContactProtocol local_contact_protocol(TransportType transport) {
  return transport == TransportType::UDP ? ContactProtocol::UDP : ContactProtocol::TCP;
}

void append_csv(std::string *out, const std::string &entry) {
  if (entry.empty()) return;
  if (!out->empty()) out->push_back(',');
  *out += entry;
}

std::string serialize_endpoint(const std::string &name, ContactProtocol protocol,
                               const std::string &ip, uint16_t port,
                               uint16_t control_port) {
  if (name.empty()) return "";
  if (ip.empty() || port == 0) return name;
  if (protocol == ContactProtocol::TCP) {
    return name + "|tcp|" + ip + "|" + std::to_string(port);
  }
  if (protocol == ContactProtocol::UDP) {
    return name + "|udp|" + ip + "|" + std::to_string(port) + "|" +
           std::to_string(control_port);
  }
  if (protocol == ContactProtocol::HA) {
    std::string out = name + "|ha|" + ip + "|" + std::to_string(port);
    if (control_port != 0) out += "|" + std::to_string(control_port);
    return out;
  }

  std::string out = name;
  if (!ip.empty()) out += "|" + ip;
  if (port != 0) out += "|" + std::to_string(port);
  if (control_port != 0) out += "|" + std::to_string(control_port);
  return out;
}

}  // namespace

// === Settings persistence ===

void IntercomApi::load_settings_() {
  this->settings_pref_ = global_preferences->make_preference<StoredSettings>(fnv1_hash("intercom_api_settings"));

  StoredSettings stored;
  if (this->settings_pref_.load(&stored) && stored.version == SETTINGS_VERSION) {
    this->suppress_save_ = true;

    // Volume only when intercom_api owns the master_volume number;
    // template-number setups own their own DAC + AEC sync.
    const float volume = clamp_volume_(stored.volume_pct / 100.0f);
    this->volume_.store(volume, std::memory_order_relaxed);
    if (this->volume_number_ != nullptr) {
#ifdef USE_INTERCOM_API_SPEAKER
      if (this->speaker_ != nullptr) {
        this->speaker_->set_volume(volume);
      }
#endif
      ESP_LOGD(TAG, "Loaded volume: %.0f%%", volume * 100.0f);
    }

    // Skip mic_gain when esp_audio_stack owns it (its own persistence).
    this->mic_gain_db_ = clamp_mic_gain_db_(stored.mic_gain_db);
    if (this->mic_gain_number_ != nullptr && this->has_microphone_()) {
      if (this->mic_gain_db_ != 0.0f && !this->ensure_mic_processing_buffer_()) {
        ESP_LOGE(TAG, "Stored mic_gain %.1fdB ignored: processing buffer unavailable", this->mic_gain_db_);
        this->mic_gain_db_ = 0.0f;
      }
      this->mic_gain_.store(db_to_linear(this->mic_gain_db_), std::memory_order_relaxed);
      ESP_LOGD(TAG, "Loaded mic_gain: %.1fdB", this->mic_gain_db_);
    }

    // auto_answer / AEC use switch restore_mode, not this struct.
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
  // 250 ms debounce against slider drag.
  this->set_timeout(SCHED_SAVE_SETTINGS, 250, [this]() {
    this->save_scheduled_ = false;
    this->save_settings_();
  });
}

void IntercomApi::save_settings_() {
  StoredSettings stored;
  stored.version = SETTINGS_VERSION;
  const float volume = clamp_volume_(this->volume_.load(std::memory_order_relaxed));
  stored.volume_pct = static_cast<uint8_t>(
      std::lround(volume * 100.0f));
  stored.mic_gain_db = static_cast<int8_t>(std::lround(clamp_mic_gain_db_(this->mic_gain_db_)));

  this->settings_pref_.save(&stored);
  ESP_LOGD(TAG, "Saved settings: vol=%d%%, mic=%ddB",
           stored.volume_pct, stored.mic_gain_db);
}

// === User-facing setters ===

void IntercomApi::set_volume(float volume) {
  volume = clamp_volume_(volume);
  this->volume_.store(volume, std::memory_order_relaxed);
#ifdef USE_INTERCOM_API_SPEAKER
  if (this->speaker_ != nullptr) {
    this->speaker_->set_volume(volume);
  }
#endif
  this->schedule_save_settings_();
}

void IntercomApi::set_auto_answer(bool enabled) {
  this->auto_answer_ = enabled;
  ESP_LOGI(TAG, "Auto-answer set to %s", enabled ? "ON" : "OFF");
  // Persisted via switch restore_mode, not save_settings_.
}

void IntercomApi::set_do_not_disturb(bool enabled) {
  this->do_not_disturb_ = enabled;
  ESP_LOGI(TAG, "Do-not-disturb set to %s", enabled ? "ON" : "OFF");
  // Persisted via switch restore_mode, not save_settings_.
}

void IntercomApi::set_mic_gain_db(float db) {
  if (!this->has_microphone_()) {
    ESP_LOGW(TAG, "Ignoring mic_gain: this intercom endpoint has no microphone");
    return;
  }
  // gain = 10^(dB/20); clamp to -20..+20 dB.
  db = clamp_mic_gain_db_(db);
  if (db != 0.0f && !this->ensure_mic_processing_buffer_()) {
    ESP_LOGE(TAG, "Mic gain %.1f dB ignored: processing buffer unavailable", db);
    return;
  }
  this->mic_gain_db_ = db;
  const float gain = db_to_linear(db);
  this->mic_gain_.store(gain, std::memory_order_relaxed);
  ESP_LOGD(TAG, "Mic gain set to %.1f dB (%.2fx)", db, gain);
  this->schedule_save_settings_();
}

// === Contacts ===
// Phonebook owns merge logic. add/set route through the same idempotent
// merge: same shape = noop, missing endpoint = upgrade, mismatch = replace.
// Slot order is stable; only flush_contacts wipes.

void IntercomApi::add_contact(const std::string &entry) {
  this->phonebook_.set_self_name(this->device_name_);
  const std::string before = this->phonebook_.current_name();
  const std::string normalized_entry = this->normalize_phonebook_for_transport_(entry);
  const AddResult r = this->phonebook_.add_one(normalized_entry);
  switch (r) {
    case AddResult::Added:
      ESP_LOGI(TAG, "Contact added: %s", normalized_entry.c_str());
      this->publish_contacts_();
      if (this->phonebook_.current_name() != before) this->publish_destination_();
      break;
    case AddResult::Upgraded:
    case AddResult::EndpointReplaced:
      ESP_LOGI(TAG, "Contact endpoint updated: %s", normalized_entry.c_str());
      this->publish_contacts_();
      break;
    case AddResult::Noop:
    case AddResult::Rejected:
      break;
  }
}

void IntercomApi::remove_contact(const std::string &name) {
  const std::string before = this->phonebook_.current_name();
  if (!this->phonebook_.remove_one(name)) {
    return;
  }
  ESP_LOGI(TAG, "Contact removed: %s", name.c_str());
  this->publish_contacts_();
  if (this->phonebook_.current_name() != before) this->publish_destination_();
}

void IntercomApi::set_contacts(const std::string &contacts_csv) {
  this->phonebook_.set_self_name(this->device_name_);
  const std::string normalized_csv = this->normalize_phonebook_for_transport_(contacts_csv);
  // Inside an open update cycle (started by update_contacts()), record names
  // so commit_cycle_() can reset their counters. Outside a cycle this is a
  // no-op - pruning only applies to the official update_contacts() flow.
  if (this->cycle_active_) this->track_csv_(normalized_csv);
  // Just add_one per entry; for a clean slate call flush_contacts() first.
  const bool changed = this->phonebook_.add_batch(normalized_csv);
  const bool selected = this->maybe_auto_select_ha_first_();
  if (changed || selected) {
    ESP_LOGI(TAG, "Contacts batch applied: %zu total", this->phonebook_.size());
    this->publish_destination_();
    if (changed) this->publish_contacts_();
  }
}

std::string IntercomApi::normalize_phonebook_for_transport_(const std::string &contacts_csv) {
  const auto raw_entries = Phonebook::split(contacts_csv, ',');
  std::vector<ParsedPhonebookSlot> slots;
  slots.reserve(raw_entries.size());

  ParsedPhonebookSlot ha_slot;
  bool has_ha = false;

  for (const auto &raw : raw_entries) {
    ParsedPhonebookSlot slot;
    if (!parse_slot_for_normalize(raw, &slot)) {
      continue;
    }
    if (slot.entry.protocol == ContactProtocol::HA && !slot.entry.ip.empty() &&
        slot.entry.port != 0) {
      ha_slot = slot;
      has_ha = true;
    }
    slots.push_back(slot);
  }

  if (slots.empty()) return contacts_csv;

  if (has_ha && this->ha_peer_name_ != ha_slot.entry.name) {
    this->ha_peer_name_ = ha_slot.entry.name;
    ESP_LOGI(TAG, "HA peer name learned from phonebook: %s", this->ha_peer_name_.c_str());
  }

  const ContactProtocol local_protocol = local_contact_protocol(this->protocol_);
  const uint16_t ha_local_port = local_protocol == ContactProtocol::UDP
                                     ? (ha_slot.ha_udp_audio_port != 0
                                            ? ha_slot.ha_udp_audio_port
                                            : ha_slot.entry.port)
                                     : ha_slot.entry.port;
  const uint16_t ha_local_control = local_protocol == ContactProtocol::UDP
                                        ? ha_slot.entry.control_port
                                        : 0;

  std::string out;
  for (const auto &slot : slots) {
    const auto &entry = slot.entry;

    if (entry.protocol == ContactProtocol::HA) {
      const uint16_t local_port = local_protocol == ContactProtocol::UDP
                                      ? (slot.ha_udp_audio_port != 0
                                             ? slot.ha_udp_audio_port
                                             : entry.port)
                                      : entry.port;
      const uint16_t local_control = local_protocol == ContactProtocol::UDP
                                         ? entry.control_port
                                         : 0;
      append_csv(&out, serialize_endpoint(entry.name, local_protocol, entry.ip,
                                          local_port, local_control));
      continue;
    }

    if (entry.protocol == local_protocol || entry.protocol == ContactProtocol::UNKNOWN) {
      append_csv(&out, serialize_endpoint(entry.name, entry.protocol, entry.ip,
                                          entry.port, entry.control_port));
      continue;
    }

    if ((entry.protocol == ContactProtocol::TCP || entry.protocol == ContactProtocol::UDP) &&
        has_ha) {
      append_csv(&out, serialize_endpoint(entry.name, local_protocol, ha_slot.entry.ip,
                                          ha_local_port, ha_local_control));
      continue;
    }

    // Cross-protocol contact without an HA row: keep the name visible but avoid
    // writing a wrong direct endpoint into the phonebook.
    append_csv(&out, entry.name);
  }

  return out;
}

void IntercomApi::update_contacts() {
  // Re-entry: commit any cycle still open (e.g. previous trigger chain didn't
  // wake the timeout). Counter advance + prune happen here, never inside the
  // cycle itself.
  if (this->cycle_active_) this->commit_cycle_();

  this->phonebook_.set_self_name(this->device_name_);
  this->cycle_active_ = true;
  this->cycle_started_at_ = millis();
  this->seen_in_cycle_.clear();
  this->enable_loop_soon_any_context();

  // HA path: read the codegen-internal homeassistant text_sensor. When ESP-side
  // mDNS discovery is enabled, this device is in ESP-only phonebook mode and
  // deliberately ignores HA's central roster even if the package is present.
  if (this->ha_phonebook_sensor_ != nullptr
#ifdef USE_INTERCOM_MDNS_DISCOVERY
      && !this->mdns_discovery_enabled_
#endif
  ) {
    const std::string &csv = this->ha_phonebook_sensor_->state;
    if (!csv.empty()) {
      this->set_contacts(csv);  // tracks CSV via cycle_active_ guard above
    }
  }

  // Fire trigger for downstream sources (mDNS scan etc). If nothing is wired,
  // the loop() timeout will commit the cycle in CYCLE_TIMEOUT_MS.
  this->update_contacts_trigger_.trigger();
#ifdef USE_INTERCOM_MDNS_DISCOVERY
  this->request_mdns_discovery_scan_();
#endif
}

void IntercomApi::track_csv_(const std::string &csv) {
  // Parse names only - endpoints are merged via Phonebook::add_batch already.
  const auto entries = Phonebook::split(csv, ',');
  for (const auto &entry : entries) {
    const size_t bar = entry.find('|');
    std::string name = Phonebook::trim((bar == std::string::npos) ? entry : entry.substr(0, bar));
    if (!name.empty()) this->seen_in_cycle_.insert(std::move(name));
  }
}

void IntercomApi::commit_cycle_() {
  const bool pruned = this->phonebook_.commit_cycle(this->seen_in_cycle_, this->prune_threshold_);
  if (pruned) {
    ESP_LOGI(TAG, "Phonebook cycle: pruned stale contacts, %zu remain", this->phonebook_.size());
    this->maybe_auto_select_ha_first_();
    this->publish_destination_();
    this->publish_contacts_();
  }
  this->seen_in_cycle_.clear();
  this->cycle_active_ = false;
}

bool IntercomApi::maybe_auto_select_ha_first_() {
  if (!this->use_ha_as_first_contact_) return false;
  if (this->first_contacts_batch_committed_) return false;
  if (this->ha_peer_name_.empty()) return false;
  if (this->call_state_.load(std::memory_order_acquire) != CallState::IDLE) return false;
  if (this->phonebook_.find(this->ha_peer_name_) == nullptr) return false;
  if (!this->phonebook_.select(this->ha_peer_name_)) return false;

  this->first_contacts_batch_committed_ = true;
  ESP_LOGI(TAG, "Selected HA peer '%s' as initial destination", this->ha_peer_name_.c_str());
  return true;
}

void IntercomApi::flush_contacts() {
  this->phonebook_.clear();
  this->publish_destination_();
  this->publish_contacts_();
  ESP_LOGI(TAG, "Contacts flushed (empty list)");
}

bool IntercomApi::set_contact(const std::string &name) {
  if (this->phonebook_.empty()) {
    ESP_LOGW(TAG, "set_contact('%s') failed: contacts list is empty", name.c_str());
    std::string msg = std::string("Contact not found: ") + name;
    this->defer([this, msg]() { this->call_failed_trigger_.trigger(msg); });
    return false;
  }
  if (this->phonebook_.select(name)) {
    this->publish_destination_();
    ESP_LOGI(TAG, "Selected contact: %s", name.c_str());
    return true;
  }
  ESP_LOGW(TAG, "set_contact('%s') failed: not found in %zu contacts",
           name.c_str(), this->phonebook_.size());
  std::string msg = std::string("Contact not found: ") + name;
  this->defer([this, msg]() { this->call_failed_trigger_.trigger(msg); });
  return false;
}

void IntercomApi::call_contact(const std::string &name) {
  if (!this->set_contact(name)) {
    return;
  }
  this->start();
}

void IntercomApi::next_contact() {
  if (this->phonebook_.empty()) return;
  this->phonebook_.next();
  this->publish_destination_();
  ESP_LOGD(TAG, "Selected contact: %s", this->get_current_destination().c_str());
}

void IntercomApi::prev_contact() {
  if (this->phonebook_.empty()) return;
  this->phonebook_.prev();
  this->publish_destination_();
  ESP_LOGD(TAG, "Selected contact: %s", this->get_current_destination().c_str());
}

const std::string &IntercomApi::get_current_destination() const {
  return this->phonebook_.current_name();
}

const std::string &IntercomApi::get_current_contact_ip() const {
  return this->phonebook_.current_ip();
}

uint16_t IntercomApi::get_current_contact_port() const {
  return this->phonebook_.current_port();
}

uint16_t IntercomApi::get_current_contact_control_port() const {
  const auto *c = this->phonebook_.current();
  return c ? c->control_port : 0;
}

void IntercomApi::publish_destination_() {
  const std::string current = this->get_current_destination();
  if (this->destination_sensor_ != nullptr) {
    this->destination_sensor_->publish_state(current);
  }
  if (current != this->last_published_destination_) {
    this->last_published_destination_ = current;
    this->destination_changed_trigger_.trigger();
  }
}

void IntercomApi::publish_caller_(const std::string &caller_name) {
  if (this->caller_sensor_ != nullptr) {
    this->caller_sensor_->publish_state(caller_name);
  }
}

void IntercomApi::publish_contacts_() {
  if (this->contacts_sensor_ != nullptr) {
    // Count only ("3 contacts"); the CSV is on demand via get_contacts_csv().
    char buf[32];
    snprintf(buf, sizeof(buf), "%zu contact%s",
             this->phonebook_.size(),
             this->phonebook_.size() == 1 ? "" : "s");
    this->contacts_sensor_->publish_state(buf);
  }
}

std::string IntercomApi::get_contacts_csv() const {
  // Names only (no endpoints) - for UI / diagnostic display.
  return this->phonebook_.names_csv();
}

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32
