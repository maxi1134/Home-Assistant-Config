#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_INTERCOM_TCP_TRANSPORT)

#include "transport.h"

#include <lwip/sockets.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstdint>

namespace esphome {
namespace intercom_api {

/// TCP server: framed binary on the configurable port, single accepted
/// client at a time, also the outbound originate() target.
///
/// Threading: one server_task_ owns accept/select/recv. send_*() are
/// safe from any task (serialised on send_mutex_). Handlers fire from
/// server_task_ and must never block.
///
/// task_stacks_in_psram (ctor): true on S3/P4 to free internal heap;
/// false on plain ESP32.
class TcpTransport : public IntercomTransport {
 public:
  static constexpr uint32_t kServerTaskStackBytes = 8192;

  TcpTransport(uint16_t port, bool task_stacks_in_psram);
  ~TcpTransport() override;

  // IntercomTransport
  bool start() override;
  void stop() override;
  bool is_connected() const override;
  void send_audio_frame(const uint8_t *pcm, size_t bytes) override;
  bool send_control(MessageType type,
                    const uint8_t *payload = nullptr, size_t len = 0) override;
  const char *transport_name() const override { return "tcp"; }

  void disconnect() override;
  bool originate(const std::string &host, uint16_t port) override;

 protected:
  // last_inbound_ms drives the read-side deadline; last_ping_sent_ms
  // throttles outbound PINGs while we wait for the peer's PONG.
  struct ClientInfo {
    std::atomic<int> socket{-1};
    struct sockaddr_in addr{};
    uint32_t last_inbound_ms{0};
    uint32_t last_ping_sent_ms{0};
    std::atomic<bool> streaming{false};
  };

  static void server_task_trampoline_(void *param);
  void server_task_();

  bool setup_server_socket_();
  void close_server_socket_();
  void close_client_socket_();
  void close_client_socket_and_notify_();
  void accept_client_();

  bool send_message_(int socket, MessageType type,
                     const uint8_t *data, size_t len);
  void send_audio_chunk_(int socket, const uint8_t *data, size_t length);
  bool receive_message_(int socket, MessageHeader &header, uint8_t *buffer,
                        size_t buffer_size);

  // Routes one parsed frame to on_audio_frame / on_control or handles
  // PING/PONG internally. Runs on server_task_.
  void dispatch_message_(const MessageHeader &header, const uint8_t *data);

  const uint16_t tcp_port_;
  const bool task_stacks_in_psram_;

  int server_socket_{-1};
  ClientInfo client_;
  SemaphoreHandle_t client_mutex_{nullptr};

  uint8_t *tx_buffer_{nullptr};
  SemaphoreHandle_t send_mutex_{nullptr};
  uint8_t *rx_buffer_{nullptr};  // server_task_ only

  TaskHandle_t server_task_handle_{nullptr};
  StaticTask_t server_task_tcb_{};
  StackType_t *server_task_stack_{nullptr};

  // server_task_ exits cleanly when this clears.
  std::atomic<bool> running_{false};
};

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32 && USE_INTERCOM_TCP_TRANSPORT
