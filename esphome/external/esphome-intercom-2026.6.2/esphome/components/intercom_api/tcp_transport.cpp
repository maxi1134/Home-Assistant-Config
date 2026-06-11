#include "tcp_transport.h"

#if defined(USE_ESP32) && defined(USE_INTERCOM_TCP_TRANSPORT)

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../audio_processor/log_utils.h"
#include "../audio_processor/scoped_lock.h"
#include "../audio_processor/task_utils.h"

namespace esphome {
namespace intercom_api {

static const char *const TAG = "intercom_api.tcp";

namespace {
constexpr uint32_t kSocketIoTimeoutMs = 500;
constexpr uint32_t kFrameProgressTimeoutMs = KEEPALIVE_DEADLINE_MS;
constexpr UBaseType_t kServerTaskPriority = 12;

void set_blocking_mode(int socket, bool blocking) {
  int flags = fcntl(socket, F_GETFL, 0);
  if (flags < 0) return;
  if (blocking) {
    flags &= ~O_NONBLOCK;
  } else {
    flags |= O_NONBLOCK;
  }
  fcntl(socket, F_SETFL, flags);
}

void configure_client_socket(int socket) {
  int opt = 1;
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
  struct timeval tv{};
  tv.tv_sec = kSocketIoTimeoutMs / 1000;
  tv.tv_usec = (kSocketIoTimeoutMs % 1000) * 1000;
  setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  // The server task uses select() before recv(), and intercom_tx is not the
  // realtime I2S task. Blocking send with a bounded timeout avoids short TCP
  // writes splitting a framed audio packet under Wi-Fi/media backpressure.
  set_blocking_mode(socket, true);
}

bool wait_socket_writable(int socket, uint32_t wait_ms) {
  fd_set wfds;
  FD_ZERO(&wfds);
  FD_SET(socket, &wfds);
  struct timeval tv{};
  tv.tv_sec = wait_ms / 1000;
  tv.tv_usec = (wait_ms % 1000) * 1000;
  int ready = select(socket + 1, nullptr, &wfds, nullptr, &tv);
  return ready > 0 && FD_ISSET(socket, &wfds);
}

}  // namespace

TcpTransport::TcpTransport(uint16_t port, bool task_stacks_in_psram)
    : tcp_port_(port), task_stacks_in_psram_(task_stacks_in_psram) {}

TcpTransport::~TcpTransport() {
  this->stop();
  if (this->client_mutex_ != nullptr) {
    vSemaphoreDelete(this->client_mutex_);
    this->client_mutex_ = nullptr;
  }
  if (this->send_mutex_ != nullptr) {
    vSemaphoreDelete(this->send_mutex_);
    this->send_mutex_ = nullptr;
  }
}

bool TcpTransport::start() {
  if (this->running_.load(std::memory_order_acquire)) {
    return true;
  }

  const bool created_client_mutex = this->client_mutex_ == nullptr;
  if (created_client_mutex) {
    this->client_mutex_ = xSemaphoreCreateMutex();
  }
  const bool created_send_mutex = this->send_mutex_ == nullptr;
  if (created_send_mutex) {
    this->send_mutex_ = xSemaphoreCreateMutex();
  }
  if (this->client_mutex_ == nullptr || this->send_mutex_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create transport mutexes");
    if (created_client_mutex && this->client_mutex_ != nullptr) {
      vSemaphoreDelete(this->client_mutex_);
      this->client_mutex_ = nullptr;
    }
    if (created_send_mutex && this->send_mutex_ != nullptr) {
      vSemaphoreDelete(this->send_mutex_);
      this->send_mutex_ = nullptr;
    }
    return false;
  }

  RAMAllocator<uint8_t> psram_u8;
  if (this->tx_buffer_ == nullptr) {
    this->tx_buffer_ = psram_u8.allocate(MAX_MESSAGE_SIZE);
  }
  if (this->rx_buffer_ == nullptr) {
    this->rx_buffer_ = psram_u8.allocate(MAX_MESSAGE_SIZE);
  }
  if (this->tx_buffer_ == nullptr || this->rx_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate transport buffers");
    if (this->tx_buffer_ != nullptr) {
      psram_u8.deallocate(this->tx_buffer_, MAX_MESSAGE_SIZE);
      this->tx_buffer_ = nullptr;
    }
    if (this->rx_buffer_ != nullptr) {
      psram_u8.deallocate(this->rx_buffer_, MAX_MESSAGE_SIZE);
      this->rx_buffer_ = nullptr;
    }
    return false;
  }

  this->running_.store(true, std::memory_order_release);

  if (!audio_processor::start_pinned_task(TcpTransport::server_task_trampoline_, "intercom_srv",
                                           kServerTaskStackBytes, this, kServerTaskPriority, 1,
                                           this->task_stacks_in_psram_, TAG,
                                           &this->server_task_handle_, &this->server_task_tcb_,
                                           &this->server_task_stack_)) {
    this->running_.store(false, std::memory_order_release);
    psram_u8.deallocate(this->tx_buffer_, MAX_MESSAGE_SIZE);
    psram_u8.deallocate(this->rx_buffer_, MAX_MESSAGE_SIZE);
    this->tx_buffer_ = nullptr;
    this->rx_buffer_ = nullptr;
    return false;
  }

  return true;
}

void TcpTransport::stop() {
  if (!this->running_.exchange(false, std::memory_order_acq_rel)) {
    return;
  }

  // Server task polls running_; wake it if it's parked in notify-wait, then
  // give it ~300 ms to exit cleanly before cleanup_pinned_task force-deletes.
  if (this->server_task_handle_ != nullptr) {
    xTaskNotifyGive(this->server_task_handle_);
    for (int i = 0; i < 30; i++) {
      if (eTaskGetState(this->server_task_handle_) == eDeleted) break;
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  audio_processor::cleanup_pinned_task(&this->server_task_handle_,
                                        &this->server_task_stack_, kServerTaskStackBytes);

  this->close_client_socket_();
  this->close_server_socket_();

  RAMAllocator<uint8_t> psram_u8;
  if (this->tx_buffer_ != nullptr) {
    psram_u8.deallocate(this->tx_buffer_, MAX_MESSAGE_SIZE);
    this->tx_buffer_ = nullptr;
  }
  if (this->rx_buffer_ != nullptr) {
    psram_u8.deallocate(this->rx_buffer_, MAX_MESSAGE_SIZE);
    this->rx_buffer_ = nullptr;
  }
}

bool TcpTransport::is_connected() const {
  return this->client_.socket.load(std::memory_order_acquire) >= 0;
}

void TcpTransport::send_audio_frame(const uint8_t *pcm, size_t bytes) {
  int socket = this->client_.socket.load(std::memory_order_acquire);
  if (socket < 0) return;
  this->send_audio_chunk_(socket, pcm, bytes);
}

bool TcpTransport::send_control(MessageType type,
                                 const uint8_t *payload, size_t len) {
  int socket = this->client_.socket.load(std::memory_order_acquire);
  if (socket < 0) return false;
  return this->send_message_(socket, type, payload, len);
}

void TcpTransport::disconnect() {
  this->close_client_socket_and_notify_();
}

// === Server Task ===

void TcpTransport::server_task_trampoline_(void *param) {
  static_cast<TcpTransport *>(param)->server_task_();
}

void TcpTransport::server_task_() {
  ESP_LOGD(TAG, "Server task started");

  if (!this->setup_server_socket_()) {
    ESP_LOGE(TAG, "Failed to setup server socket on startup");
  }

  while (this->running_.load(std::memory_order_acquire)) {
    // Streaming: poll non-blocking. Idle: park up to 100 ms for a wake.
    if (this->client_.streaming.load(std::memory_order_acquire)) {
      ulTaskNotifyTake(pdTRUE, 0);
    } else {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
    }

    if (!this->running_.load(std::memory_order_acquire)) break;

    if (this->server_socket_ < 0) {
      if (!this->setup_server_socket_()) {
        delay(1000);
        continue;
      }
    }

    // Always drain pending inbound connections. Even while this node already
    // owns an active call leg, a new caller deserves a protocol-level
    // DECLINE("busy") instead of connecting successfully and waiting forever
    // in OUTGOING.
    this->accept_client_();

    int client_fd = this->client_.socket.load(std::memory_order_acquire);
    if (client_fd >= 0) {
      fd_set read_fds;
      FD_ZERO(&read_fds);
      FD_SET(client_fd, &read_fds);
      struct timeval tv = {.tv_sec = 0, .tv_usec = 10000};  // 10 ms

      int ret = ::select(client_fd + 1, &read_fds, nullptr, nullptr, &tv);
      if (ret > 0 && FD_ISSET(client_fd, &read_fds)) {
        MessageHeader header;
        if (this->receive_message_(client_fd, header, this->rx_buffer_, MAX_MESSAGE_SIZE)) {
          this->dispatch_message_(header, this->rx_buffer_ + HEADER_SIZE);
        } else {
          ESP_LOGI(TAG, "Client disconnected");
          this->close_client_socket_and_notify_();
        }
      }

      // Catch half-open sockets (NAT timeout, silent peer crash) that
      // recv() never errors on.
      uint32_t silence = millis() - this->client_.last_inbound_ms;
      if (silence > KEEPALIVE_DEADLINE_MS) {
        ESP_LOGW(TAG, "TCP peer silent for %u ms, closing dead connection",
                 (unsigned) silence);
        this->close_client_socket_and_notify_();
      } else if (silence > PING_INTERVAL_MS &&
                 millis() - this->client_.last_ping_sent_ms > PING_INTERVAL_MS) {
        // Keep-alive PING is control-plane and must stay active even while
        // streaming; VAD/mute can make audio an invalid liveness signal.
        // last_ping_sent_ms prevents back-to-back PINGs while waiting for PONG.
        this->send_message_(this->client_.socket.load(std::memory_order_acquire), MessageType::PING,
                            nullptr, 0);
        this->client_.last_ping_sent_ms = millis();
      }
    }

    delay(1);
  }

  ESP_LOGD(TAG, "Server task exiting");
  vTaskDelete(nullptr);
}

// === Socket Lifecycle ===

bool TcpTransport::setup_server_socket_() {
  this->server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (this->server_socket_ < 0) {
    ESP_LOGE(TAG, "Failed to create server socket: %d", errno);
    return false;
  }

  int opt = 1;
  setsockopt(this->server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  int flags = fcntl(this->server_socket_, F_GETFL, 0);
  fcntl(this->server_socket_, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(this->tcp_port_);

  if (bind(this->server_socket_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
    ESP_LOGE(TAG, "Bind on port %u failed: %d", (unsigned) this->tcp_port_, errno);
    close(this->server_socket_);
    this->server_socket_ = -1;
    return false;
  }

  if (listen(this->server_socket_, 4) < 0) {
    ESP_LOGE(TAG, "Listen failed: %d", errno);
    close(this->server_socket_);
    this->server_socket_ = -1;
    return false;
  }

  ESP_LOGI(TAG, "Server listening on port %u", (unsigned) this->tcp_port_);
  return true;
}

void TcpTransport::close_server_socket_() {
  if (this->server_socket_ >= 0) {
    close(this->server_socket_);
    this->server_socket_ = -1;
  }
}

void TcpTransport::close_client_socket_() {
  // Streaming flag off before fd invalidation so concurrent senders see
  // "not streaming" before they see -1.
  this->client_.streaming.store(false, std::memory_order_release);

  int sock = this->client_.socket.exchange(-1, std::memory_order_acq_rel);
  if (sock >= 0) {
    shutdown(sock, SHUT_RDWR);
    close(sock);
  }
}

void TcpTransport::close_client_socket_and_notify_() {
  const bool was_connected = this->client_.socket.load(std::memory_order_acquire) >= 0;
  this->close_client_socket_();
  if (was_connected && this->on_connection_change) {
    this->on_connection_change(false);
  }
}

void TcpTransport::accept_client_() {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);

  int client_sock = accept(this->server_socket_,
                           reinterpret_cast<struct sockaddr *>(&client_addr),
                           &client_len);
  if (client_sock < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGW(TAG, "Accept error: %d", errno);
    }
    return;
  }

  configure_client_socket(client_sock);

  // Read MSG_START before deciding accept/reject: a wire-correct DECLINE
  // must echo the caller's call_id back.
  MessageHeader hello_header;
  if (!this->receive_message_(client_sock, hello_header,
                              this->rx_buffer_, MAX_MESSAGE_SIZE)) {
    ESP_LOGD(TAG, "Client closed before MSG_START");
    close(client_sock);
    return;
  }

  if (hello_header.type != static_cast<uint8_t>(MessageType::START)) {
    ESP_LOGW(TAG, "Expected MSG_START, got type=0x%02x", hello_header.type);
    close(client_sock);
    return;
  }

  std::string incoming_cid;
  if (decode_call_id_prefix(this->rx_buffer_ + HEADER_SIZE,
                            hello_header.length, &incoming_cid) == 0) {
    ESP_LOGW(TAG, "Malformed MSG_START body (no call_id prefix)");
    close(client_sock);
    return;
  }

  auto reject = [&](const char *what) {
    ESP_LOGW(TAG, "Rejecting connection - %s (cid='%s')", what, incoming_cid.c_str());

    // DECLINE body: [call_id_len:u8][call_id][reason_len:u8][reason].
    static constexpr char kBusyReason[] = "busy";
    constexpr size_t kReasonLen = sizeof(kBusyReason) - 1;
    const std::string busy_reason(kBusyReason, kReasonLen);

    uint8_t body[1 + INTERCOM_MAX_CALL_ID_LEN + 1 + INTERCOM_MAX_REASON_LEN];
    size_t off = encode_call_id_prefix(body, sizeof(body), incoming_cid);
    if (off == 0) { close(client_sock); return; }
    size_t n = encode_lp_string(body + off, sizeof(body) - off,
                                busy_reason, INTERCOM_MAX_REASON_LEN);
    if (n == 0) { close(client_sock); return; }
    off += n;

    this->send_message_(client_sock, MessageType::DECLINE, body, off);
    close(client_sock);
  };

  if (this->client_.socket.load(std::memory_order_acquire) >= 0) {
    reject("already have client");
    return;
  }

  if (this->should_accept_session && !this->should_accept_session()) {
    reject("upper layer rejected (FSM busy)");
    return;
  }

  char ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
  ESP_LOGI(TAG, "Client connected from %s", ip_str);

  // Re-check under lock: originate() on Core 1 races with accept here.
  bool adopted = false;
  {
    audio_processor::ScopedLock lock(this->client_mutex_);
    if (this->client_.socket.load(std::memory_order_acquire) < 0) {
      this->client_.socket.store(client_sock, std::memory_order_release);
      this->client_.addr = client_addr;
      this->client_.last_inbound_ms = millis();
      this->client_.last_ping_sent_ms = millis();
      this->client_.streaming.store(false, std::memory_order_release);
      adopted = true;
    }
  }
  if (!adopted) {
    reject("active leg appeared while accepting");
    return;
  }

  if (this->on_connection_change) {
    this->on_connection_change(true);
  }

  // Forward the MSG_START we already consumed; on_connection_change(true)
  // above ran first so the FSM sees "connected" before processing START.
  this->dispatch_message_(hello_header, this->rx_buffer_ + HEADER_SIZE);
}

bool TcpTransport::originate(const std::string &host, uint16_t port) {
  if (!this->running_.load(std::memory_order_acquire)) {
    ESP_LOGW(TAG, "originate(%s:%u) refused: transport not started",
             host.c_str(), (unsigned) port);
    return false;
  }
  if (this->client_.socket.load(std::memory_order_acquire) >= 0) {
    ESP_LOGW(TAG, "originate(%s:%u) refused: active leg already busy",
             host.c_str(), (unsigned) port);
    return false;
  }

  struct sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &dst.sin_addr) != 1) {
    ESP_LOGW(TAG, "originate: invalid IPv4 host '%s'", host.c_str());
    return false;
  }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    ESP_LOGW(TAG, "originate(%s:%u): socket() failed errno=%d",
             host.c_str(), (unsigned) port, errno);
    return false;
  }

  // Never let an outbound dial block ESPHome's loop task. lwIP may ignore
  // SO_SNDTIMEO for connect(), so use a non-blocking connect and poll with a
  // short LAN timeout instead.
  set_blocking_mode(sock, false);

  bool connected = false;
  if (connect(sock, reinterpret_cast<struct sockaddr *>(&dst), sizeof(dst)) == 0) {
    connected = true;
  } else if (errno == EINPROGRESS || errno == EALREADY || errno == EWOULDBLOCK) {
    constexpr uint32_t kConnectTimeoutMs = 500;
    const uint32_t start_ms = millis();
    while (millis() - start_ms < kConnectTimeoutMs) {
      fd_set wfds;
      FD_ZERO(&wfds);
      FD_SET(sock, &wfds);
      struct timeval tv{};
      tv.tv_sec = 0;
      tv.tv_usec = 10000;
      int ready = select(sock + 1, nullptr, &wfds, nullptr, &tv);
      if (ready > 0 && FD_ISSET(sock, &wfds)) {
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) == 0 &&
            so_error == 0) {
          connected = true;
        } else {
          errno = so_error;
        }
        break;
      }
      if (ready < 0 && errno != EINTR) break;
      delay(1);
    }
    if (!connected && errno == EINPROGRESS) errno = ETIMEDOUT;
  }

  if (!connected) {
    ESP_LOGW(TAG, "originate(%s:%u): connect() failed/timeout errno=%d",
             host.c_str(), (unsigned) port, errno);
    close(sock);
    return false;
  }

  // Match accept_client_(): NODELAY + bounded blocking I/O after connect.
  configure_client_socket(sock);

  ESP_LOGI(TAG, "Originated TCP leg to %s:%u", host.c_str(), (unsigned) port);

  bool adopted = false;
  {
    audio_processor::ScopedLock lock(this->client_mutex_);
    // An accept may have landed while we were blocked in connect().
    if (this->client_.socket.load(std::memory_order_acquire) < 0) {
      this->client_.socket.store(sock, std::memory_order_release);
      this->client_.addr = dst;
      this->client_.last_inbound_ms = millis();
      this->client_.last_ping_sent_ms = millis();
      this->client_.streaming.store(false, std::memory_order_release);
      adopted = true;
    }
  }
  if (!adopted) {
    ESP_LOGW(TAG, "originate: active leg appeared while connecting, dropping");
    close(sock);
    return false;
  }

  if (this->server_task_handle_ != nullptr) {
    xTaskNotifyGive(this->server_task_handle_);
  }

  if (this->on_connection_change) {
    this->on_connection_change(true);
  }
  return true;
}

// === Wire protocol ===

bool TcpTransport::send_message_(int socket, MessageType type,
                                  const uint8_t *data, size_t len) {
  if (socket < 0) return false;

  // Bounded so we never starve server_task on a busy send path.
  audio_processor::ScopedLock lock(this->send_mutex_, pdMS_TO_TICKS(10));
  if (!lock) return false;

  MessageHeader header;
  header.type = static_cast<uint8_t>(type);
  header.length = static_cast<uint16_t>(len);

  encode_header(this->tx_buffer_, header);
  if (data && len > 0) {
    memcpy(this->tx_buffer_ + HEADER_SIZE, data, len);
  }

  size_t total = HEADER_SIZE + len;
  size_t offset = 0;
  uint32_t start_ms = millis();

  while (offset < total) {
    ssize_t sent = send(socket, this->tx_buffer_ + offset, total - offset, 0);

    if (sent > 0) {
      offset += static_cast<size_t>(sent);
      continue;
    }

    if (sent == 0) return false;  // peer closed

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      if (millis() - start_ms > kSocketIoTimeoutMs) return false;
      wait_socket_writable(socket, 5);
      continue;
    }

    return false;
  }

  return true;
}

void TcpTransport::send_audio_chunk_(int socket, const uint8_t *data, size_t length) {
  // Try-lock: control owns serialisation, audio is best-effort.
  audio_processor::ScopedLock lock(this->send_mutex_, 0);
  if (!lock) return;

  uint8_t hdr_buf[HEADER_SIZE];
  MessageHeader header;
  header.type = static_cast<uint8_t>(MessageType::AUDIO);
  header.length = static_cast<uint16_t>(length);
  encode_header(hdr_buf, header);

  struct iovec iov[2];
  iov[0].iov_base = hdr_buf;
  iov[0].iov_len = HEADER_SIZE;
  iov[1].iov_base = const_cast<uint8_t *>(data);
  iov[1].iov_len = length;
  struct msghdr msg{};
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;

  ssize_t sent = sendmsg(socket, &msg, 0);
  const size_t total = HEADER_SIZE + length;
  if (sent < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK &&
        this->client_.streaming.load(std::memory_order_acquire)) {
      LOG_W_THROTTLED("TX send error: %d", errno);
    }
    return;  // nothing on the wire, peer never saw a partial header
  }
  size_t offset = static_cast<size_t>(sent);
  if (offset == 0 || offset == total) return;

  // Partial send must complete or the peer's stream is desynced. This runs
  // from intercom_tx, not from the I2S realtime callback, so use a longer
  // bounded drain window for lwIP/Wi-Fi backpressure during concurrent
  // media/TTS playback. On failure, half-close so server_task's recv sees EOF.
  uint32_t start_ms = millis();
  while (offset < total) {
    const uint8_t *p;
    size_t remaining;
    if (offset < HEADER_SIZE) {
      p = hdr_buf + offset;
      remaining = HEADER_SIZE - offset;
    } else {
      p = data + (offset - HEADER_SIZE);
      remaining = length - (offset - HEADER_SIZE);
    }
    ssize_t n = send(socket, p, remaining, 0);
    if (n > 0) { offset += static_cast<size_t>(n); continue; }
    if (n == 0) break;
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      if (millis() - start_ms > kSocketIoTimeoutMs) break;
      wait_socket_writable(socket, 5);
      continue;
    }
    break;
  }
  if (offset < total) {
    ESP_LOGE(TAG, "TX partial after %u/%u bytes; half-closing to resync peer",
             (unsigned) offset, (unsigned) total);
    shutdown(socket, SHUT_RDWR);
  }
}

bool TcpTransport::receive_message_(int socket, MessageHeader &header,
                                     uint8_t *buffer, size_t buffer_size) {
  size_t header_read = 0;
  uint32_t last_progress_ms = millis();

  while (header_read < HEADER_SIZE && millis() - last_progress_ms < kFrameProgressTimeoutMs) {
    ssize_t received = recv(socket, buffer + header_read, HEADER_SIZE - header_read, 0);
    if (received > 0) {
      header_read += received;
      last_progress_ms = millis();
      continue;
    }
    if (received == 0) return false;
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      delay(1);
      continue;
    }
    return false;
  }

  if (header_read != HEADER_SIZE) {
    if (header_read > 0) {
      ESP_LOGW(TAG, "TCP frame stalled while reading header: %zu/%zu", header_read, HEADER_SIZE);
    }
    return false;
  }

  header = decode_header(buffer);

  if (header.length > buffer_size - HEADER_SIZE) {
    ESP_LOGW(TAG, "Message too large: %d", header.length);
    return false;
  }

  if (header.length > 0) {
    size_t payload_read = 0;
    last_progress_ms = millis();
    while (payload_read < header.length && millis() - last_progress_ms < kFrameProgressTimeoutMs) {
      ssize_t received = recv(socket, buffer + HEADER_SIZE + payload_read,
                              header.length - payload_read, 0);
      if (received > 0) {
        payload_read += received;
        last_progress_ms = millis();
        continue;
      }
      if (received == 0) return false;
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        delay(1);
        continue;
      }
      return false;
    }

    if (payload_read != header.length) {
      ESP_LOGW(TAG, "TCP frame stalled while reading payload: %zu/%d", payload_read, header.length);
      return false;
    }
  }

  return true;
}

void TcpTransport::dispatch_message_(const MessageHeader &header, const uint8_t *data) {
  MessageType type = static_cast<MessageType>(header.type);

  switch (type) {
    case MessageType::AUDIO:
      // Mark streaming so PING gating + error throttling know we're in audio.
      if (!this->client_.streaming.load(std::memory_order_acquire)) {
        this->client_.streaming.store(true, std::memory_order_release);
      }
      this->client_.last_inbound_ms = millis();
      if (this->on_audio_frame) {
        this->on_audio_frame(data, header.length);
      }
      break;

    case MessageType::PING:
      this->client_.last_inbound_ms = millis();
      this->send_message_(this->client_.socket.load(std::memory_order_acquire), MessageType::PONG,
                          nullptr, 0);
      break;

    case MessageType::PONG:
      this->client_.last_inbound_ms = millis();
      break;

    default:
      // START / STOP / RING / ANSWER / ERROR / etc: forward upstream.
      if (this->on_control) {
        this->on_control(type, data, header.length);
      }
      break;
  }
}

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32 && USE_INTERCOM_TCP_TRANSPORT
