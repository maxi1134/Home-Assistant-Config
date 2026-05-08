#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <string>
#include <vector>

namespace esphome {
namespace mdns_discovery {

struct PeerInfo {
  std::string name;
  std::string ip;
  uint16_t port;
  uint32_t last_seen;
  bool active;
};

class MdnsDiscovery : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Configuration
  void set_service_type(const std::string &type) { this->service_type_ = type; }
  void set_scan_interval(uint32_t interval) { this->scan_interval_ = interval; }
  void set_peer_timeout(uint32_t timeout) { this->peer_timeout_ = timeout; }

  // Public methods for lambda access
  void scan_now();
  int get_peer_count() const { return this->peers_.size(); }
  std::string get_peer_ip(int index) const;
  std::string get_peer_name(int index) const;
  uint16_t get_peer_port(int index) const;
  std::string get_peer_ip_by_name(const std::string &name) const;
  std::string get_peers_list() const;
  const std::vector<PeerInfo>& get_peers() const { return this->peers_; }

  // Callbacks
  void add_on_peer_found_callback(std::function<void(std::string, std::string, uint16_t)> callback) {
    this->peer_found_callbacks_.add(std::move(callback));
  }
  void add_on_peer_lost_callback(std::function<void(std::string)> callback) {
    this->peer_lost_callbacks_.add(std::move(callback));
  }
  void add_on_scan_complete_callback(std::function<void(int)> callback) {
    this->scan_complete_callbacks_.add(std::move(callback));
  }

 protected:
  void query_peers_();
  void cleanup_stale_peers_();

  std::string service_type_;
  uint32_t scan_interval_{10000};
  uint32_t peer_timeout_{60000};
  uint32_t last_scan_{0};

  std::vector<PeerInfo> peers_;

  CallbackManager<void(std::string, std::string, uint16_t)> peer_found_callbacks_;
  CallbackManager<void(std::string)> peer_lost_callbacks_;
  CallbackManager<void(int)> scan_complete_callbacks_;
};

// Triggers
class PeerFoundTrigger : public Trigger<std::string, std::string, uint16_t> {
 public:
  explicit PeerFoundTrigger(MdnsDiscovery *parent) {
    parent->add_on_peer_found_callback([this](std::string name, std::string ip, uint16_t port) {
      this->trigger(name, ip, port);
    });
  }
};

class PeerLostTrigger : public Trigger<std::string> {
 public:
  explicit PeerLostTrigger(MdnsDiscovery *parent) {
    parent->add_on_peer_lost_callback([this](std::string name) {
      this->trigger(name);
    });
  }
};

class ScanCompleteTrigger : public Trigger<int> {
 public:
  explicit ScanCompleteTrigger(MdnsDiscovery *parent) {
    parent->add_on_scan_complete_callback([this](int count) {
      this->trigger(count);
    });
  }
};

// Action
template<typename... Ts>
class ScanAction : public Action<Ts...>, public Parented<MdnsDiscovery> {
 public:
  void play(Ts... x) override { this->parent_->scan_now(); }
};

// Sensor for peer count
class MdnsDiscoverySensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(MdnsDiscovery *parent) { this->parent_ = parent; }
  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_peer_count());
    }
  }

 protected:
  MdnsDiscovery *parent_{nullptr};
};

// Text sensor for peers list
class MdnsDiscoveryTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void set_parent(MdnsDiscovery *parent) { this->parent_ = parent; }
  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_peers_list());
    }
  }

 protected:
  MdnsDiscovery *parent_{nullptr};
};

}  // namespace mdns_discovery
}  // namespace esphome
