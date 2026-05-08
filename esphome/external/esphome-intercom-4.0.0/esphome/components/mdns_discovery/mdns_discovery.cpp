#include "mdns_discovery.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <mdns.h>
#include <algorithm>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

namespace esphome {
namespace mdns_discovery {

static const char *TAG = "mdns_discovery";

void MdnsDiscovery::setup() {
  // Config logged via dump_config()
}

void MdnsDiscovery::loop() {
  uint32_t now = millis();

  // Periodic scan
  if (now - this->last_scan_ > this->scan_interval_) {
    this->query_peers_();
    this->cleanup_stale_peers_();
    this->last_scan_ = now;
  }
}

void MdnsDiscovery::dump_config() {
  ESP_LOGCONFIG(TAG, "mDNS Discovery:");
  ESP_LOGCONFIG(TAG, "  Service Type: %s", this->service_type_.c_str());
  ESP_LOGCONFIG(TAG, "  Scan Interval: %d ms", this->scan_interval_);
  ESP_LOGCONFIG(TAG, "  Peer Timeout: %d ms", this->peer_timeout_);
}

void MdnsDiscovery::scan_now() {
  this->query_peers_();
  this->cleanup_stale_peers_();
  this->last_scan_ = millis();
}

void MdnsDiscovery::query_peers_() {
  mdns_result_t *results = nullptr;

  // Parse service type - add underscore prefix if not present
  std::string service = this->service_type_;
  std::string protocol = "_udp";

  // Check if it contains a dot (e.g., "_intercom._udp")
  size_t dot_pos = service.find('.');
  if (dot_pos != std::string::npos) {
    protocol = service.substr(dot_pos + 1);
    service = service.substr(0, dot_pos);
  }

  // Ensure underscore prefix
  if (!service.empty() && service[0] != '_') {
    service = "_" + service;
  }
  if (!protocol.empty() && protocol[0] != '_') {
    protocol = "_" + protocol;
  }

  ESP_LOGD(TAG, "mDNS query: service=%s, protocol=%s", service.c_str(), protocol.c_str());

  esp_err_t err = mdns_query_ptr(service.c_str(), protocol.c_str(), 1000, 10, &results);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mDNS query failed: %s", esp_err_to_name(err));
    this->scan_complete_callbacks_.call(this->peers_.size());
    return;
  }

  if (results == nullptr) {
    ESP_LOGD(TAG, "mDNS query: no results");
    this->scan_complete_callbacks_.call(this->peers_.size());
    return;
  }

  std::string my_name = App.get_name();

  mdns_result_t *r = results;
  while (r) {
    if (r->hostname && r->addr) {
      std::string peer_name = r->hostname;

      // Skip ourselves
      if (peer_name != my_name) {
        char ip_str[16];
        inet_ntoa_r(r->addr->addr.u_addr.ip4, ip_str, sizeof(ip_str));

        // Check if we already know this peer
        bool found = false;
        for (auto &peer : this->peers_) {
          if (peer.name == peer_name) {
            peer.last_seen = millis();
            peer.ip = ip_str;
            peer.port = r->port;
            peer.active = true;
            found = true;
            break;
          }
        }

        if (!found) {
          PeerInfo new_peer;
          new_peer.name = peer_name;
          new_peer.ip = ip_str;
          new_peer.port = r->port;
          new_peer.last_seen = millis();
          new_peer.active = true;
          this->peers_.push_back(new_peer);

          ESP_LOGI(TAG, "Peer found: %s (%s:%d)", peer_name.c_str(), ip_str, r->port);
          this->peer_found_callbacks_.call(peer_name, std::string(ip_str), r->port);
        }
      }
    }
    r = r->next;
  }

  mdns_query_results_free(results);
  this->scan_complete_callbacks_.call(this->peers_.size());
}

void MdnsDiscovery::cleanup_stale_peers_() {
  uint32_t now = millis();

  for (auto it = this->peers_.begin(); it != this->peers_.end();) {
    if (now - it->last_seen > this->peer_timeout_) {
      ESP_LOGI(TAG, "Peer lost: %s", it->name.c_str());
      this->peer_lost_callbacks_.call(it->name);
      it = this->peers_.erase(it);
    } else {
      ++it;
    }
  }
}

std::string MdnsDiscovery::get_peer_ip(int index) const {
  if (index >= 0 && index < (int)this->peers_.size()) {
    return this->peers_[index].ip;
  }
  return "";
}

std::string MdnsDiscovery::get_peer_name(int index) const {
  if (index >= 0 && index < (int)this->peers_.size()) {
    return this->peers_[index].name;
  }
  return "";
}

uint16_t MdnsDiscovery::get_peer_port(int index) const {
  if (index >= 0 && index < (int)this->peers_.size()) {
    return this->peers_[index].port;
  }
  return 0;
}

std::string MdnsDiscovery::get_peer_ip_by_name(const std::string &name) const {
  for (const auto &peer : this->peers_) {
    if (peer.name == name) {
      return peer.ip;
    }
  }
  return "";
}

std::string MdnsDiscovery::get_peers_list() const {
  std::string result;
  for (size_t i = 0; i < this->peers_.size(); i++) {
    if (i > 0) result += ", ";
    result += this->peers_[i].name;
    result += " (";
    result += this->peers_[i].ip;
    result += ")";
  }
  return result.empty() ? "No peers found" : result;
}

}  // namespace mdns_discovery
}  // namespace esphome
