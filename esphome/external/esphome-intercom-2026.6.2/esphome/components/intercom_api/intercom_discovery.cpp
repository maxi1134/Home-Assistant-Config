#include "intercom_api.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#ifdef USE_INTERCOM_MDNS_DISCOVERY
#include <mdns.h>
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#endif

#include <cstring>

namespace esphome {
namespace intercom_api {

static const char *const TAG = "intercom_api.discovery";

namespace {

void append_csv(std::string *out, const std::string &entry) {
  if (entry.empty()) return;
  if (!out->empty()) out->push_back(',');
  *out += entry;
}

bool valid_contact_name(const std::string &name) {
  return !name.empty() && name.find('|') == std::string::npos &&
         name.find(',') == std::string::npos;
}

#ifdef USE_INTERCOM_MDNS_DISCOVERY
std::string txt_value(const mdns_result_t *result, const char *key) {
  if (result == nullptr || result->txt == nullptr) return "";
  for (size_t i = 0; i < result->txt_count; i++) {
    const auto &item = result->txt[i];
    if (item.key == nullptr || std::strcmp(item.key, key) != 0) continue;
    if (item.value == nullptr) return "";
    if (result->txt_value_len != nullptr) {
      return std::string(item.value, item.value + result->txt_value_len[i]);
    }
    return std::string(item.value);
  }
  return "";
}

uint16_t txt_port_or(const mdns_result_t *result, const char *key, uint16_t fallback) {
  const std::string raw = txt_value(result, key);
  if (raw.empty()) return fallback;
  uint16_t parsed = 0;
  return Phonebook::parse_u16(Phonebook::trim(raw), &parsed) ? parsed : fallback;
}

std::string first_ipv4(const mdns_result_t *result) {
  if (result == nullptr) return "";
  for (const mdns_ip_addr_t *addr = result->addr; addr != nullptr; addr = addr->next) {
    if (addr->addr.type != ESP_IPADDR_TYPE_V4) continue;
    char ip[IP4ADDR_STRLEN_MAX];
    if (esp_ip4addr_ntoa(&addr->addr.u_addr.ip4, ip, sizeof(ip)) == nullptr) {
      continue;
    }
    return std::string(ip);
  }
  return "";
}

std::string result_name(const mdns_result_t *result) {
  std::string name = txt_value(result, "friendly_name");
  if (name.empty() && result != nullptr && result->instance_name != nullptr) {
    name = result->instance_name;
  }
  return Phonebook::trim(name);
}
#endif

}  // namespace

#ifdef USE_INTERCOM_MDNS_DISCOVERY

void IntercomApi::request_mdns_discovery_scan_() {
  if (!this->mdns_discovery_enabled_ || this->mdns_discovery_task_handle_ == nullptr) return;
  const bool already_requested = this->mdns_discovery_scan_requested_.exchange(true, std::memory_order_acq_rel);
  if (!already_requested) {
    xTaskNotifyGive(this->mdns_discovery_task_handle_);
  }
}

void IntercomApi::process_pending_mdns_discovery_() {
  if (!this->mdns_discovery_pending_.exchange(false, std::memory_order_acq_rel)) return;

  std::string csv;
  {
    LockGuard lock(this->mdns_discovery_mutex_);
    csv.swap(this->mdns_discovery_pending_csv_);
  }
  if (csv.empty()) return;

  ESP_LOGI(TAG, "Applying mDNS discovery contacts: %s", csv.c_str());
  this->set_contacts(csv);
}

void IntercomApi::mdns_discovery_task(void *param) {
  static_cast<IntercomApi *>(param)->mdns_discovery_task_();
}

void IntercomApi::mdns_discovery_task_() {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (!this->mdns_discovery_enabled_) continue;
    if (!this->mdns_discovery_scan_requested_.exchange(false, std::memory_order_acq_rel)) continue;

    std::string csv = this->run_mdns_discovery_query_();
    if (!csv.empty()) {
      LockGuard lock(this->mdns_discovery_mutex_);
      if (!this->mdns_discovery_pending_csv_.empty()) {
        this->mdns_discovery_pending_csv_.push_back(',');
      }
      this->mdns_discovery_pending_csv_ += csv;
      this->mdns_discovery_pending_.store(true, std::memory_order_release);
      this->enable_loop_soon_any_context();
    }

    if (this->mdns_discovery_scan_requested_.load(std::memory_order_acquire)) {
      xTaskNotifyGive(this->mdns_discovery_task_handle_);
    }
  }
}

std::string IntercomApi::run_mdns_discovery_query_() {
  std::string out;
  auto query = [this, &out](const char *service, const char *proto, ContactProtocol contact_protocol) {
    mdns_result_t *results = nullptr;
    const esp_err_t err = mdns_query_ptr(service, proto, this->mdns_discovery_query_timeout_ms_,
                                         this->mdns_discovery_max_results_, &results);
    if (err != ESP_OK) {
      if (err != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "mDNS query %s.%s failed: %s", service, proto, esp_err_to_name(err));
      }
      return;
    }

    size_t accepted = 0;
    for (mdns_result_t *r = results; r != nullptr; r = r->next) {
      const std::string endpoint = Phonebook::trim(txt_value(r, "endpoint"));
      if (!endpoint.empty()) {
        ContactEntry entry;
        if (!Phonebook::parse_entry(endpoint, &entry)) continue;
        // HA announces the same bridge endpoint on both _intercom-tcp and
        // _intercom-udp. Keep that HA row so local transport normalization can
        // route cross-protocol contacts through HA when present.
        if (entry.protocol != ContactProtocol::HA && entry.protocol != contact_protocol) continue;
        if (!valid_contact_name(entry.name)) continue;
        if (entry.name == this->device_name_ || entry.name == this->device_route_id_) continue;
        append_csv(&out, endpoint);
        accepted++;
        continue;
      }

      const std::string name = result_name(r);
      if (!valid_contact_name(name)) continue;
      if (name == this->device_name_ || name == this->device_route_id_) continue;

      const std::string ip = first_ipv4(r);
      if (ip.empty()) continue;

      if (contact_protocol == ContactProtocol::TCP) {
        const uint16_t port = txt_port_or(r, "tcp_port", r->port);
        if (port == 0) continue;
        append_csv(&out, name + "|tcp|" + ip + "|" + std::to_string(port));
        accepted++;
      } else if (contact_protocol == ContactProtocol::UDP) {
        const uint16_t audio_port = txt_port_or(r, "audio_port", r->port);
        const uint16_t control_port = txt_port_or(r, "control_port", this->control_port_);
        if (audio_port == 0 || control_port == 0) continue;
        append_csv(&out, name + "|udp|" + ip + "|" + std::to_string(audio_port) + "|" +
                            std::to_string(control_port));
        accepted++;
      }
    }

    mdns_query_results_free(results);
    ESP_LOGD(TAG, "mDNS query %s.%s accepted %u contacts", service, proto,
             (unsigned) accepted);
  };

  if (this->mdns_discovery_scan_tcp_) query("_intercom-tcp", "_tcp", ContactProtocol::TCP);
  if (this->mdns_discovery_scan_udp_) query("_intercom-udp", "_udp", ContactProtocol::UDP);
  return out;
}

#endif

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32
