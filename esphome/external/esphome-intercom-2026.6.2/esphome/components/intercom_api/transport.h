#pragma once

#ifdef USE_ESP32

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "intercom_protocol.h"

namespace esphome {
namespace intercom_api {

/// Abstract transport for audio + control. IntercomApi composes one and
/// never touches sockets directly.
///
/// Threading contract:
///   - Implementations may spawn FreeRTOS tasks.
///   - send_audio_frame must be safe from a Core 0 audio-priority task.
///   - Callbacks fire from the transport's own task; handlers must
///     marshal work via ring buffers / atomics and never block.
///   - PING/PONG keepalive is transport-internal and never crosses
///     on_control; only protocol-semantic messages do.
class IntercomTransport {
 public:
  virtual ~IntercomTransport() = default;

  /// Idempotent.
  virtual bool start() = 0;
  virtual void stop() = 0;

  /// Drop the current peer session without tearing the transport down.
  /// TCP closes the accepted client; UDP no-op (no per-session state).
  virtual void disconnect() {}

  /// TCP: a client is accepted. UDP: mirrors start()/stop().
  virtual bool is_connected() const = 0;

  /// Best-effort send. PCM s16le 16 kHz mono, AUDIO_CHUNK_BYTES bytes.
  /// Safe from a high-priority audio task; may drop on backpressure.
  virtual void send_audio_frame(const uint8_t *pcm, size_t bytes) = 0;

  /// Returns true when the message was committed to the wire. The FSM
  /// uses the return value to detect "leg silently dropped".
  virtual bool send_control(MessageType type,
                            const uint8_t *payload = nullptr,
                            size_t len = 0) = 0;

  /// Used in dump_config / ESP_LOGCONFIG ("tcp", "udp", ...).
  virtual const char *transport_name() const = 0;

  /// UDP retargets sendto. `port` is the audio port; `control_port` is
  /// optional for short Name|IP|port contacts. TCP no-op.
  virtual void set_remote(const std::string &ip, uint16_t port, uint16_t control_port = 0) {}

  /// Open an outbound leg for an originating call. TCP connects to the
  /// peer; UDP no-op (control_socket_ is already bound, set_remote
  /// retargets sendto).
  virtual bool originate(const std::string &host, uint16_t port) { return true; }

  /// Lazy audio path. UDP binds the audio socket and spawns recv_task
  /// only here so an idle device isn't a passive PCM listener. TCP no-op.
  virtual bool start_audio_path() { return true; }
  virtual void stop_audio_path() {}

  // === Callbacks (set by IntercomApi before start()) ===

  /// Buffer lifetime = callback duration only.
  std::function<void(const uint8_t *pcm, size_t bytes)> on_audio_frame;

  /// Protocol messages only; PING/PONG never cross.
  std::function<void(MessageType type, const uint8_t *payload, size_t len)>
      on_control;

  /// TCP: per accept/disconnect. UDP: once per start/stop.
  std::function<void(bool connected)> on_connection_change;

  /// TCP gate before accept. UDP ignores (no per-session concept).
  std::function<bool()> should_accept_session;
};

}  // namespace intercom_api
}  // namespace esphome

#endif  // USE_ESP32
