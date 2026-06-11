#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_INTERCOM_UDP_TRANSPORT)

#include "transport.h"

#include <lwip/sockets.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <string>

namespace esphome {
namespace intercom_api {

/// Dual-socket UDP transport.
///
/// Audio socket (listen_port, default 6054): raw PCM s16le 16 kHz mono,
/// one frame per datagram, no header (go2rtc-compatible).
/// Control socket (control_port, default 6055): MessageHeader-framed
/// signaling on a separate port so the audio path stays zero-copy and
/// go2rtc interop isn't broken by header bytes.
///
/// No per-packet integrity, loss masking, or keepalive. Threat model is
/// LAN-only; the FSM relies on HA-side ACK+retry for control.
///
/// Threading: recv_task (Core 1) drains audio_socket_; ctrl_task (Core 1,
/// lower prio) drains control_socket_. send_*() can be called from any
/// task (lwIP serialises per-socket internally).
class UdpTransport : public IntercomTransport {
 public:
  // 8 KB on both stacks: recv_task callback chain (RingBuffer + AEC ref
  // ScopedLock + logger) and ctrl_task's inline start_audio_path()
  // (socket + bind + xTaskCreate) both need the headroom.
  static constexpr uint32_t kRecvTaskStackBytes = 8192;
  static constexpr uint32_t kCtrlTaskStackBytes = 8192;

  UdpTransport(uint16_t listen_port, std::string remote_ip, uint16_t remote_port,
               uint16_t control_port, uint16_t remote_control_port,
               bool task_stacks_in_psram);
  ~UdpTransport() override;

  // IntercomTransport
  bool start() override;
  void stop() override;
  bool is_connected() const override;
  void send_audio_frame(const uint8_t *pcm, size_t bytes) override;
  bool send_control(MessageType type,
                    const uint8_t *payload = nullptr, size_t len = 0) override;
  const char *transport_name() const override { return "udp"; }

  // Control socket stays bound; audio socket is lazy (idle = no listener).
  bool start_audio_path() override;
  void stop_audio_path() override;

  // Updates the peer destination. `port` is audio; `control_port` falls back
  // to the local control_port_ when omitted for short Name|IP|port contacts.
  void set_remote(const std::string &ip, uint16_t port, uint16_t control_port = 0) override;

 protected:
  static void recv_task_trampoline_(void *param);
  static void ctrl_task_trampoline_(void *param);
  void recv_task_();
  void ctrl_task_();

  bool resolve_remote_(uint16_t port, struct sockaddr_in &out) const;
  bool source_matches_remote_(const struct sockaddr_in &src) const;
  bool send_decline_to_(const struct sockaddr_in &dst,
                        const std::string &call_id,
                        const char *reason);

  const uint16_t listen_port_;
  const uint16_t control_port_;
  const bool task_stacks_in_psram_;

  // Atomics so ctrl_task can update on inbound MSG_START while tx_task
  // reads in send_audio_frame (no std::string race). Host order; the
  // helpers do the htonl/htons.
  std::atomic<uint32_t> remote_ip_v4_{0};   // 0 = invalid
  std::atomic<uint16_t> remote_port_{0};
  std::atomic<uint16_t> remote_control_port_{0};

  int audio_socket_{-1};
  int control_socket_{-1};
  TaskHandle_t recv_task_handle_{nullptr};
  StaticTask_t recv_task_tcb_{};
  StackType_t *recv_task_stack_{nullptr};
  TaskHandle_t ctrl_task_handle_{nullptr};
  StaticTask_t ctrl_task_tcb_{};
  StackType_t *ctrl_task_stack_{nullptr};

  std::atomic<bool> running_{false};
  std::atomic<bool> active_{false};        // mirrors start/stop
  std::atomic<bool> audio_active_{false};  // start_audio_path / stop_audio_path
};

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32 && USE_INTERCOM_UDP_TRANSPORT
