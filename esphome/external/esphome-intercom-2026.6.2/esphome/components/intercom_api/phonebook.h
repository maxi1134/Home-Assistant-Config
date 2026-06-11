#pragma once

#ifdef USE_ESP32

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

namespace esphome {
namespace intercom_api {

enum class ContactProtocol : uint8_t {
  UNKNOWN,
  TCP,
  UDP,
  HA,
};

inline const char *contact_protocol_to_str(ContactProtocol protocol) {
  switch (protocol) {
    case ContactProtocol::TCP:
      return "tcp";
    case ContactProtocol::UDP:
      return "udp";
    case ContactProtocol::HA:
      return "ha";
    case ContactProtocol::UNKNOWN:
    default:
      return "unknown";
  }
}

/// One phonebook slot. Empty ip/port = no endpoint yet (name-only placeholder).
/// Legacy entries are still accepted:
///   Name
///   Name|ip
///   Name|ip|port
///   Name|ip|audio_port|control_port
/// New protocol-aware entries are:
///   Name|tcp|ip|port[|audio_capability]
///   Name|udp|ip|audio_port|control_port[|audio_capability]
///   Name|ha|ip|port[|control_port]
///
/// The HA-wide unified roster can also publish HA as:
///   Name|ha|ip|tcp_port|udp_audio_port|udp_control_port
/// IntercomApi normalizes that six-field HA row into a local TCP/UDP endpoint
/// before it reaches Phonebook, because Phonebook stores one dialable endpoint
/// per slot.
/// missing_count tracks consecutive update cycles where this slot was not seen
/// by any source; commit_cycle() advances/resets it and prunes once the
/// configured threshold is hit. Default 0 keeps pruning disabled.
struct ContactEntry {
  std::string name;
  std::string ip;
  ContactProtocol protocol{ContactProtocol::UNKNOWN};
  uint16_t port{0};
  uint16_t control_port{0};
  std::string audio_capability;
  uint8_t missing_count{0};
};

/// add_one outcome - lets a batch upload defer publish_* side-effects.
enum class AddResult : uint8_t {
  Noop,
  Added,
  Upgraded,          // name existed without endpoint, now has ip+port
  EndpointReplaced,  // existing endpoint overwritten
  Rejected,          // empty name or self-name
};

/// Local contact list with idempotent merge semantics. Slot order is
/// stable across re-adds so next/prev never jump on republish.
class Phonebook {
 public:
  /// Skip add_one() for this name (the device's own name).
  void set_self_name(const std::string &n) { this->self_name_ = n; }

  /// `entry` grammar:
  ///   "Name"
  ///   "Name|ip|port"
  ///   "Name|ip|audio_port|control_port" (UDP complete endpoint)
  ///   "Name|tcp|ip|port[|audio_capability]"
  ///   "Name|udp|ip|audio_port|control_port[|audio_capability]"
  ///   "Name|ha|ip|port[|control_port]"
  AddResult add_one(const std::string &entry) {
    ContactEntry incoming;
    if (!parse_entry_(entry, &incoming)) return AddResult::Rejected;
    if (!this->self_name_.empty() && incoming.name == this->self_name_) {
      return AddResult::Rejected;
    }
    return this->merge_(incoming);
  }

  /// Parse one CSV slot without mutating the phonebook. Used by IntercomApi to
  /// normalize the protocol-aware HA phonebook into a transport-local dial plan
  /// before merge.
  static bool parse_entry(const std::string &entry, ContactEntry *out) {
    return parse_entry_(entry, out);
  }
  static bool parse_protocol(const std::string &s, ContactProtocol *out) {
    return parse_protocol_(s, out);
  }
  static bool parse_u16(const std::string &s, uint16_t *out) {
    return parse_u16_(s, out);
  }
  static std::string trim(const std::string &raw) { return trim_(raw); }
  static std::vector<std::string> split(const std::string &s, char sep) {
    return split_(s, sep);
  }

  /// Batch add. True if at least one slot mutated.
  bool add_batch(const std::string &csv) {
    bool changed = false;
    size_t start = 0;
    while (start <= csv.size()) {
      const size_t comma = csv.find(',', start);
      const size_t end = (comma == std::string::npos) ? csv.size() : comma;
      const AddResult r = this->add_one(csv.substr(start, end - start));
      if (r == AddResult::Added || r == AddResult::Upgraded ||
          r == AddResult::EndpointReplaced) {
        changed = true;
      }
      if (comma == std::string::npos) break;
      start = comma + 1;
    }
    return changed;
  }

  /// Drop every entry, reset the cursor.
  void clear() {
    this->entries_.clear();
    this->index_ = 0;
  }

  /// Apply one update-cycle outcome. For each slot: reset missing_count to 0
  /// if its name appears in `seen`, otherwise increment (saturated at 255).
  /// After accounting, prune slots whose missing_count >= threshold.
  /// threshold=0 disables pruning entirely (counters still advance, no prune).
  /// Cursor is preserved via remove_one() which shifts/wraps as needed.
  /// Returns true if at least one slot was removed (caller should re-publish).
  bool commit_cycle(const std::unordered_set<std::string> &seen, uint8_t threshold) {
    for (auto &entry : this->entries_) {
      if (seen.count(entry.name)) {
        entry.missing_count = 0;
      } else if (entry.missing_count < 255) {
        entry.missing_count++;
      }
    }
    if (threshold == 0) return false;
    std::vector<std::string> to_remove;
    for (const auto &entry : this->entries_) {
      if (entry.missing_count >= threshold) to_remove.push_back(entry.name);
    }
    bool changed = false;
    for (const auto &name : to_remove) {
      if (this->remove_one(name)) changed = true;
    }
    return changed;
  }

  /// Cursor follows: removing before the cursor shifts it down; removing
  /// the cursor itself keeps it on the next slot (or wraps to 0).
  bool remove_one(const std::string &name) {
    for (size_t i = 0; i < this->entries_.size(); i++) {
      if (this->entries_[i].name != name) continue;
      this->entries_.erase(this->entries_.begin() + i);
      if (this->entries_.empty()) {
        this->index_ = 0;
      } else if (i < this->index_) {
        this->index_--;
      } else if (i == this->index_ && this->index_ >= this->entries_.size()) {
        this->index_ = 0;
      }
      return true;
    }
    return false;
  }

  /// Move the cursor to the named contact. Returns false if not present.
  bool select(const std::string &name) {
    for (size_t i = 0; i < this->entries_.size(); i++) {
      if (this->entries_[i].name == name) {
        this->index_ = i;
        return true;
      }
    }
    return false;
  }

  /// Step through positions in order. No-op on empty phonebook.
  void next() {
    if (this->entries_.empty()) return;
    this->index_ = (this->index_ + 1) % this->entries_.size();
  }
  void prev() {
    if (this->entries_.empty()) return;
    this->index_ = (this->index_ + this->entries_.size() - 1) % this->entries_.size();
  }

  const ContactEntry *current() const {
    if (this->entries_.empty()) return nullptr;
    return &this->entries_[this->index_ % this->entries_.size()];
  }

  /// Lookup without moving the cursor (safe on the FSM hot path).
  const ContactEntry *find(const std::string &name) const {
    for (const auto &e : this->entries_) {
      if (e.name == name) return &e;
    }
    return nullptr;
  }
  const std::string &current_name() const {
    static const std::string kEmpty;
    const auto *c = this->current();
    return c ? c->name : kEmpty;
  }
  const std::string &current_ip() const {
    static const std::string kEmpty;
    const auto *c = this->current();
    return c ? c->ip : kEmpty;
  }
  uint16_t current_port() const {
    const auto *c = this->current();
    return c ? c->port : 0;
  }
  ContactProtocol current_protocol() const {
    const auto *c = this->current();
    return c ? c->protocol : ContactProtocol::UNKNOWN;
  }

  /// Comma-separated list of names (no endpoints) for diagnostic publishing.
  std::string names_csv() const {
    std::string out;
    for (size_t i = 0; i < this->entries_.size(); i++) {
      if (i > 0) out.push_back(',');
      out += this->entries_[i].name;
    }
    return out;
  }

  size_t size() const { return this->entries_.size(); }
  bool empty() const { return this->entries_.empty(); }
  size_t cursor() const { return this->index_; }

 protected:
  /// Dedup by name; on endpoint conflict the last writer wins. Same-transport
  /// peers can arrive from HA sensor and mDNS with the same endpoint;
  /// cross-transport peers arrive target-shaped from HA because local mDNS
  /// never crosses protocols. DHCP IP changes resolve on the next batch.
  AddResult merge_(const ContactEntry &incoming) {
    for (auto &existing : this->entries_) {
      if (existing.name != incoming.name) continue;
      if (incoming.ip.empty() && incoming.port == 0 && incoming.control_port == 0) {
        return AddResult::Noop;
      }
      if (existing.ip == incoming.ip && existing.port == incoming.port &&
          existing.control_port == incoming.control_port &&
          existing.protocol == incoming.protocol &&
          existing.audio_capability == incoming.audio_capability) {
        return AddResult::Noop;
      }
      const bool was_unset = existing.ip.empty() && existing.port == 0 &&
                             existing.control_port == 0;
      existing.ip = incoming.ip;
      existing.protocol = incoming.protocol;
      existing.port = incoming.port;
      existing.control_port = incoming.control_port;
      existing.audio_capability = incoming.audio_capability;
      return was_unset ? AddResult::Upgraded : AddResult::EndpointReplaced;
    }
    // New name: append; cursor unchanged so the UI doesn't jump.
    this->entries_.push_back(incoming);
    return AddResult::Added;
  }

  static bool parse_entry_(const std::string &raw, ContactEntry *out) {
    const std::string s = trim_(raw);
    if (s.empty()) return false;

    const std::vector<std::string> parts = split_(s, '|');
    if (parts.empty()) return false;

    ContactEntry c;
    c.name = trim_(parts[0]);
    if (c.name.empty()) return false;

    ContactProtocol protocol = ContactProtocol::UNKNOWN;
    if (parts.size() >= 2 && parse_protocol_(trim_(parts[1]), &protocol)) {
      if (!parse_typed_entry_(parts, protocol, &c)) return false;
    } else {
      if (!parse_short_entry_(parts, &c)) return false;
    }

    *out = std::move(c);
    return true;
  }

  static bool parse_typed_entry_(const std::vector<std::string> &parts,
                                 ContactProtocol protocol, ContactEntry *out) {
    if (parts.size() < 4) return false;
    out->protocol = protocol;
    out->ip = trim_(parts[2]);
    if (out->ip.empty()) return false;

    if (protocol == ContactProtocol::TCP) {
      if (parts.size() != 4 && parts.size() != 5) return false;
      if (!parse_u16_(trim_(parts[3]), &out->port)) return false;
      if (parts.size() == 5) out->audio_capability = trim_(parts[4]);
      return true;
    }

    if (protocol == ContactProtocol::UDP) {
      if (parts.size() != 5 && parts.size() != 6) return false;
      if (!parse_u16_(trim_(parts[3]), &out->port) ||
          !parse_u16_(trim_(parts[4]), &out->control_port)) {
        return false;
      }
      if (parts.size() == 6) out->audio_capability = trim_(parts[5]);
      return true;
    }

    if (protocol == ContactProtocol::HA) {
      if (parts.size() != 4 && parts.size() != 5 && parts.size() != 6) return false;
      if (!parse_u16_(trim_(parts[3]), &out->port)) return false;
      if (parts.size() == 5 && !parse_u16_(trim_(parts[4]), &out->control_port)) {
        return false;
      }
      if (parts.size() == 6 && !parse_u16_(trim_(parts[5]), &out->control_port)) {
        return false;
      }
      return true;
    }

    return false;
  }

  static bool parse_short_entry_(const std::vector<std::string> &parts, ContactEntry *out) {
    if (parts.size() == 1) return true;
    if (parts.size() < 2 || parts.size() > 4) return false;

    out->ip = trim_(parts[1]);
    if (parts.size() == 2) return true;
    if (!parse_u16_(trim_(parts[2]), &out->port)) return false;
    if (parts.size() == 4) {
      if (!parse_u16_(trim_(parts[3]), &out->control_port)) return false;
      out->protocol = ContactProtocol::UDP;
    }
    return true;
  }

  static bool parse_protocol_(const std::string &s, ContactProtocol *out) {
    if (s == "tcp" || s == "TCP") {
      *out = ContactProtocol::TCP;
      return true;
    }
    if (s == "udp" || s == "UDP") {
      *out = ContactProtocol::UDP;
      return true;
    }
    if (s == "ha" || s == "HA") {
      *out = ContactProtocol::HA;
      return true;
    }
    return false;
  }

  static std::string trim_(const std::string &raw) {
    size_t begin = 0;
    while (begin < raw.size() && (raw[begin] == ' ' || raw[begin] == '\t')) begin++;
    size_t end = raw.size();
    while (end > begin && (raw[end - 1] == ' ' || raw[end - 1] == '\t')) end--;
    return raw.substr(begin, end - begin);
  }

  static std::vector<std::string> split_(const std::string &s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
      const size_t pos = s.find(sep, start);
      if (pos == std::string::npos) {
        out.push_back(s.substr(start));
        break;
      }
      out.push_back(s.substr(start, pos - start));
      start = pos + 1;
    }
    return out;
  }

  static bool parse_u16_(const std::string &s, uint16_t *out) {
    if (s.empty()) return false;
    char *end = nullptr;
    errno = 0;
    unsigned long value = std::strtoul(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str() || *end != '\0' || value > 65535UL) {
      return false;
    }
    *out = static_cast<uint16_t>(value);
    return true;
  }

  std::vector<ContactEntry> entries_;
  size_t index_{0};
  std::string self_name_;
};

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32
