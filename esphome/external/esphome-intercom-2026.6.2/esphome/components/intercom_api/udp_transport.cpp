#include "udp_transport.h"

#if defined(USE_ESP32) && defined(USE_INTERCOM_UDP_TRANSPORT)

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../audio_processor/log_utils.h"
#include "../audio_processor/task_utils.h"

namespace esphome {
namespace intercom_api {

static const char *const TAG = "intercom_api.udp";

static bool wait_socket_readable_(int socket, uint32_t timeout_ms) {
  if (socket < 0) return false;

  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(socket, &read_fds);

  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  const int ret = select(socket + 1, &read_fds, nullptr, nullptr, &tv);
  if (ret > 0) {
    return FD_ISSET(socket, &read_fds);
  }
  if (ret < 0 && errno != EINTR) {
    ESP_LOGW(TAG, "UDP select error on socket %d: %d", socket, errno);
  }
  return false;
}

static void release_self_deleted_task_(TaskHandle_t *handle, StackType_t **stack,
                                       uint32_t stack_bytes, const char *task_name) {
  if (handle == nullptr || *handle == nullptr) return;

  bool deleted = false;
  for (int i = 0; i < 30; i++) {
    if (eTaskGetState(*handle) == eDeleted) {
      deleted = true;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (!deleted) {
    ESP_LOGE(TAG, "%s did not self-delete within 300ms; leaking stack to avoid UAF", task_name);
    return;
  }

  if (stack != nullptr && *stack != nullptr) {
    const uint32_t stack_words = (stack_bytes + sizeof(StackType_t) - 1) / sizeof(StackType_t);
    auto alloc = audio_processor::psram_allocator<StackType_t>();
    alloc.deallocate(*stack, stack_words);
    *stack = nullptr;
  }
  *handle = nullptr;
}

UdpTransport::UdpTransport(uint16_t listen_port, std::string remote_ip,
                            uint16_t remote_port, uint16_t control_port,
                            uint16_t remote_control_port,
                            bool task_stacks_in_psram)
    : listen_port_(listen_port),
      control_port_(control_port),
      task_stacks_in_psram_(task_stacks_in_psram) {
  this->remote_port_.store(remote_port, std::memory_order_release);
  this->remote_control_port_.store(
      remote_control_port != 0 ? remote_control_port : control_port,
      std::memory_order_release);
  // 0 = invalid; send paths skip until set.
  if (!remote_ip.empty()) {
    struct in_addr a{};
    if (inet_aton(remote_ip.c_str(), &a) != 0) {
      this->remote_ip_v4_.store(ntohl(a.s_addr), std::memory_order_release);
    }
  }
}

UdpTransport::~UdpTransport() { this->stop(); }

void UdpTransport::set_remote(const std::string &ip, uint16_t port, uint16_t control_port) {
  // Atomic store + str parsing on caller side keeps the cross-task swap safe.
  uint32_t new_host = 0;
  if (!ip.empty()) {
    struct in_addr a{};
    if (inet_aton(ip.c_str(), &a) != 0) {
      new_host = ntohl(a.s_addr);
    }
  }
  const uint32_t old_host = this->remote_ip_v4_.load(std::memory_order_acquire);
  if (new_host != 0) {
    this->remote_ip_v4_.store(new_host, std::memory_order_release);
  }
  this->remote_port_.store(port, std::memory_order_release);
  if (control_port != 0) {
    this->remote_control_port_.store(control_port, std::memory_order_release);
  } else if (new_host != 0 && old_host != new_host) {
    this->remote_control_port_.store(this->control_port_, std::memory_order_release);
  }
}

bool UdpTransport::start() {
  if (this->running_.load(std::memory_order_acquire)) return true;

  // Control socket bound from setup so MSG_START always reaches the FSM.
  // Audio socket is bound lazily by start_audio_path() so an idle device
  // is not a passive PCM listener.
  this->control_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (this->control_socket_ < 0) {
    ESP_LOGE(TAG, "Failed to create UDP control socket: %d", errno);
    return false;
  }
  int ctrl_sockbuf = 4 * 1024;
  setsockopt(this->control_socket_, SOL_SOCKET, SO_RCVBUF, &ctrl_sockbuf, sizeof(ctrl_sockbuf));
  setsockopt(this->control_socket_, SOL_SOCKET, SO_SNDBUF, &ctrl_sockbuf, sizeof(ctrl_sockbuf));

  int cflags = fcntl(this->control_socket_, F_GETFL, 0);
  fcntl(this->control_socket_, F_SETFL, cflags | O_NONBLOCK);

  struct sockaddr_in caddr{};
  caddr.sin_family = AF_INET;
  caddr.sin_addr.s_addr = INADDR_ANY;
  caddr.sin_port = htons(this->control_port_);
  if (bind(this->control_socket_, reinterpret_cast<struct sockaddr *>(&caddr), sizeof(caddr)) < 0) {
    ESP_LOGE(TAG, "UDP control bind on port %u failed: %d", (unsigned) this->control_port_, errno);
    close(this->control_socket_); this->control_socket_ = -1;
    return false;
  }

  this->running_.store(true, std::memory_order_release);

  if (!audio_processor::start_pinned_task(UdpTransport::ctrl_task_trampoline_, "intercom_udp_c",
                                           kCtrlTaskStackBytes, this, 4, 1,
                                           this->task_stacks_in_psram_, TAG,
                                           &this->ctrl_task_handle_, &this->ctrl_task_tcb_,
                                           &this->ctrl_task_stack_)) {
    this->running_.store(false, std::memory_order_release);
    close(this->control_socket_); this->control_socket_ = -1;
    return false;
  }

  this->active_.store(true, std::memory_order_release);
  uint32_t rip = this->remote_ip_v4_.load(std::memory_order_acquire);
  char ip_str[16];
  struct in_addr a; a.s_addr = htonl(rip);
  inet_ntoa_r(a, ip_str, sizeof(ip_str));
  ESP_LOGI(TAG, "UDP control listening on %u (audio lazy on call), remote %s:%u ctrl=%u",
           (unsigned) this->control_port_,
           rip ? ip_str : "(unset)",
           (unsigned) this->remote_port_.load(std::memory_order_acquire),
           (unsigned) this->remote_control_port_.load(std::memory_order_acquire));

  if (this->on_connection_change) {
    this->on_connection_change(true);
  }
  return true;
}

bool UdpTransport::start_audio_path() {
  // Idempotent. False if the transport isn't running yet.
  if (!this->running_.load(std::memory_order_acquire)) {
    ESP_LOGW(TAG, "start_audio_path before start(); ignored");
    return false;
  }
  if (this->audio_active_.exchange(true, std::memory_order_acq_rel)) return true;

  this->audio_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (this->audio_socket_ < 0) {
    ESP_LOGE(TAG, "audio socket create: %d", errno);
    this->audio_active_.store(false, std::memory_order_release);
    return false;
  }
  int sockbuf = 32 * 1024;
  setsockopt(this->audio_socket_, SOL_SOCKET, SO_RCVBUF, &sockbuf, sizeof(sockbuf));
  setsockopt(this->audio_socket_, SOL_SOCKET, SO_SNDBUF, &sockbuf, sizeof(sockbuf));
  int flags = fcntl(this->audio_socket_, F_GETFL, 0);
  fcntl(this->audio_socket_, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(this->listen_port_);
  if (bind(this->audio_socket_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
    ESP_LOGE(TAG, "audio bind on %u: %d", (unsigned) this->listen_port_, errno);
    close(this->audio_socket_); this->audio_socket_ = -1;
    this->audio_active_.store(false, std::memory_order_release);
    return false;
  }

  if (!audio_processor::start_pinned_task(UdpTransport::recv_task_trampoline_, "intercom_udp_a",
                                           kRecvTaskStackBytes, this, 5, 1,
                                           this->task_stacks_in_psram_, TAG,
                                           &this->recv_task_handle_, &this->recv_task_tcb_,
                                           &this->recv_task_stack_)) {
    close(this->audio_socket_); this->audio_socket_ = -1;
    this->audio_active_.store(false, std::memory_order_release);
    return false;
  }
  ESP_LOGI(TAG, "UDP audio path opened on port %u", (unsigned) this->listen_port_);
  return true;
}

void UdpTransport::stop_audio_path() {
  if (!this->audio_active_.exchange(false, std::memory_order_acq_rel)) return;

  // recv_task_() self-deletes via vTaskDelete(nullptr); calling
  // vTaskDelete(handle) again would panic on the freed TCB. Wait for
  // eDeleted, then free the stack. On timeout we leak the ~8 KB stack
  // rather than risk UAF.
  release_self_deleted_task_(&this->recv_task_handle_, &this->recv_task_stack_,
                             kRecvTaskStackBytes, "recv_task");
  if (this->audio_socket_ >= 0) {
    close(this->audio_socket_);
    this->audio_socket_ = -1;
  }
  ESP_LOGI(TAG, "UDP audio path closed");
}

void UdpTransport::stop() {
  this->stop_audio_path();

  if (!this->running_.exchange(false, std::memory_order_acq_rel)) return;

  this->active_.store(false, std::memory_order_release);

  // Same self-delete pattern as stop_audio_path.
  release_self_deleted_task_(&this->ctrl_task_handle_, &this->ctrl_task_stack_,
                             kCtrlTaskStackBytes, "ctrl_task");

  if (this->control_socket_ >= 0) {
    close(this->control_socket_);
    this->control_socket_ = -1;
  }

  if (this->on_connection_change) {
    this->on_connection_change(false);
  }
}

bool UdpTransport::is_connected() const {
  return this->active_.load(std::memory_order_acquire);
}

bool UdpTransport::resolve_remote_(uint16_t port, struct sockaddr_in &out) const {
  uint32_t ipv4_host = this->remote_ip_v4_.load(std::memory_order_acquire);
  if (ipv4_host == 0) return false;
  std::memset(&out, 0, sizeof(out));
  out.sin_family = AF_INET;
  out.sin_port = htons(port);
  out.sin_addr.s_addr = htonl(ipv4_host);
  return true;
}

bool UdpTransport::source_matches_remote_(const struct sockaddr_in &src) const {
  const uint32_t remote = this->remote_ip_v4_.load(std::memory_order_acquire);
  if (remote == 0) return true;
  return ntohl(src.sin_addr.s_addr) == remote;
}

bool UdpTransport::send_decline_to_(const struct sockaddr_in &dst,
                                    const std::string &call_id,
                                    const char *reason) {
  uint8_t body[1 + INTERCOM_MAX_CALL_ID_LEN + 1 + INTERCOM_MAX_REASON_LEN];
  size_t off = encode_call_id_prefix(body, sizeof(body), call_id);
  if (off == 0) return false;
  const std::string reason_str(reason ? reason : "");
  size_t n = encode_lp_string(body + off, sizeof(body) - off,
                              reason_str, INTERCOM_MAX_REASON_LEN);
  if (n == 0) return false;
  off += n;

  uint8_t pkt[HEADER_SIZE + sizeof(body)];
  MessageHeader hdr;
  hdr.type = static_cast<uint8_t>(MessageType::DECLINE);
  hdr.length = static_cast<uint16_t>(off);
  encode_header(pkt, hdr);
  std::memcpy(pkt + HEADER_SIZE, body, off);
  ssize_t sent = sendto(this->control_socket_, pkt, HEADER_SIZE + off, MSG_DONTWAIT,
                        reinterpret_cast<const struct sockaddr *>(&dst), sizeof(dst));
  return sent == static_cast<ssize_t>(HEADER_SIZE + off);
}

void UdpTransport::send_audio_frame(const uint8_t *pcm, size_t bytes) {
  if (!this->active_.load(std::memory_order_acquire) || this->audio_socket_ < 0) return;

  struct sockaddr_in dst;
  uint16_t port = this->remote_port_.load(std::memory_order_acquire);
  if (!this->resolve_remote_(port, dst)) return;

  ssize_t sent = sendto(this->audio_socket_, pcm, bytes, MSG_DONTWAIT,
                        reinterpret_cast<struct sockaddr *>(&dst), sizeof(dst));
  if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    LOG_W_THROTTLED("UDP audio send error: %d", errno);
  }
}

bool UdpTransport::send_control(MessageType type,
                                 const uint8_t *payload, size_t len) {
  if (!this->active_.load(std::memory_order_acquire) || this->control_socket_ < 0) return false;
  if (len > MAX_AUDIO_CHUNK + 64) {
    ESP_LOGW(TAG, "UDP control payload too large (%zu), dropping", len);
    return false;
  }

  struct sockaddr_in dst;
  const uint16_t control_port = this->remote_control_port_.load(std::memory_order_acquire);
  if (!this->resolve_remote_(control_port != 0 ? control_port : this->control_port_, dst)) return false;

  // Single contiguous buffer so the datagram arrives atomic on the peer.
  uint8_t buf[HEADER_SIZE + MAX_AUDIO_CHUNK + 64];
  MessageHeader hdr;
  hdr.type = static_cast<uint8_t>(type);
  hdr.length = static_cast<uint16_t>(len);
  encode_header(buf, hdr);
  if (len > 0 && payload != nullptr) {
    std::memcpy(buf + HEADER_SIZE, payload, len);
  }

  size_t total = HEADER_SIZE + len;
  ssize_t sent = sendto(this->control_socket_, buf, total, MSG_DONTWAIT,
                        reinterpret_cast<struct sockaddr *>(&dst), sizeof(dst));
  if (sent < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGW(TAG, "UDP control send error: %d", errno);
    }
    return false;
  }
  ESP_LOGD(TAG, "UDP control sent type=0x%02X len=%u port=%u",
           static_cast<unsigned>(type),
           (unsigned) len, (unsigned) ntohs(dst.sin_port));
  return true;
}

void UdpTransport::recv_task_trampoline_(void *param) {
  static_cast<UdpTransport *>(param)->recv_task_();
}

void UdpTransport::ctrl_task_trampoline_(void *param) {
  static_cast<UdpTransport *>(param)->ctrl_task_();
}

void UdpTransport::recv_task_() {
  ESP_LOGD(TAG, "UDP audio recv task started on port %u", (unsigned) this->listen_port_);

  // One frame per datagram. 2x leaves headroom for non-default chunk sizes.
  uint8_t rx[AUDIO_CHUNK_BYTES * 2];

  while (this->running_.load(std::memory_order_acquire) &&
         this->audio_active_.load(std::memory_order_acquire)) {
    if (!wait_socket_readable_(this->audio_socket_, 20)) {
      continue;
    }

    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);
    ssize_t n = recvfrom(this->audio_socket_, rx, sizeof(rx), 0,
                         reinterpret_cast<struct sockaddr *>(&src), &src_len);
    if (n > 0) {
      if (!this->source_matches_remote_(src)) {
        char src_ip[16];
        inet_ntoa_r(src.sin_addr, src_ip, sizeof(src_ip));
        ESP_LOGD(TAG, "Dropping UDP audio from non-current peer %s", src_ip);
        continue;
      }
      if (this->on_audio_frame) {
        this->on_audio_frame(rx, static_cast<size_t>(n));
      }
      continue;
    }
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGW(TAG, "UDP audio recv error: %d", errno);
    }
  }

  ESP_LOGD(TAG, "UDP audio recv task exiting");
  vTaskDelete(nullptr);
}

void UdpTransport::ctrl_task_() {
  // LAN-only threat model: source IP is trusted, no HMAC. Do NOT expose
  // this port to the internet.
  ESP_LOGD(TAG, "UDP control recv task started on port %u", (unsigned) this->control_port_);

  uint8_t rx[HEADER_SIZE + MAX_AUDIO_CHUNK + 64];

  while (this->running_.load(std::memory_order_acquire)) {
    if (!wait_socket_readable_(this->control_socket_, 100)) {
      continue;
    }

    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);
    ssize_t n = recvfrom(this->control_socket_, rx, sizeof(rx), 0,
                         reinterpret_cast<struct sockaddr *>(&src), &src_len);
    if (n >= static_cast<ssize_t>(HEADER_SIZE)) {
      MessageHeader hdr;
      hdr = decode_header(rx);
      size_t payload_len = hdr.length;
      if (HEADER_SIZE + payload_len > static_cast<size_t>(n)) {
        ESP_LOGW(TAG, "UDP control truncated: hdr.length=%u datagram=%zd",
                 (unsigned) payload_len, n);
        continue;
      }
      // Learn the caller endpoint from inbound START/ANSWER: the YAML
      // remote_ip may be offline or wrong, so PONGs / outgoing audio
      // would otherwise go to the wrong host. Atomic store because
      // tx_task on Core 0 reads remote_ip_v4_ concurrently.
      auto type = static_cast<MessageType>(hdr.type);
      const uint32_t src_host = ntohl(src.sin_addr.s_addr);
      const uint32_t cur = this->remote_ip_v4_.load(std::memory_order_acquire);
      const bool from_current = cur == 0 || src_host == cur;
      if (!from_current && type != MessageType::START) {
        char src_ip[16];
        inet_ntoa_r(src.sin_addr, src_ip, sizeof(src_ip));
        ESP_LOGD(TAG, "Dropping UDP control 0x%02X from non-current peer %s",
                 static_cast<unsigned>(hdr.type), src_ip);
        continue;
      }
      if (!from_current && type == MessageType::START &&
          this->audio_active_.load(std::memory_order_acquire)) {
        std::string incoming_cid;
        decode_call_id_prefix(rx + HEADER_SIZE, payload_len, &incoming_cid);
        this->send_decline_to_(src, incoming_cid, "busy");
        continue;
      }
      if ((type == MessageType::START || type == MessageType::ANSWER) && from_current) {
        const uint16_t src_control_port = ntohs(src.sin_port);
        if (cur == 0 && src_host != 0) {
          char src_ip[16];
          inet_ntoa_r(src.sin_addr, src_ip, sizeof(src_ip));
          ESP_LOGI(TAG, "Learned caller endpoint %s control=%u from inbound 0x%02X",
                   src_ip, (unsigned) src_control_port, static_cast<unsigned>(hdr.type));
          this->remote_ip_v4_.store(src_host, std::memory_order_release);
        }
        if (src_control_port != 0) {
          this->remote_control_port_.store(src_control_port, std::memory_order_release);
        }
      }
      if (this->on_control) {
        this->on_control(type,
                         payload_len > 0 ? rx + HEADER_SIZE : nullptr,
                         payload_len);
      }
      continue;
    }
    if (n >= 0 && n < static_cast<ssize_t>(HEADER_SIZE)) {
      ESP_LOGW(TAG, "UDP control runt datagram (%zd bytes), dropping", n);
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGW(TAG, "UDP control recv error: %d", errno);
    }
  }

  ESP_LOGD(TAG, "UDP control recv task exiting");
  vTaskDelete(nullptr);
}

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32 && USE_INTERCOM_UDP_TRANSPORT
