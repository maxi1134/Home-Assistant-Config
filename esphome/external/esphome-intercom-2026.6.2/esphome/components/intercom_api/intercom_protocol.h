#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace esphome {
namespace intercom_api {

static constexpr uint16_t INTERCOM_PORT = 6054;  // default TCP listen port

enum class MessageType : uint8_t {
  AUDIO = 0x01,
  START = 0x02,
  HANGUP = 0x03,   // BYE. Pre-streaming cancel uses DECLINE.
  PING = 0x04,
  PONG = 0x05,
  ERROR = 0x06,    // technical fault
  RING = 0x07,     // provisional: dest is presenting the call
  ANSWER = 0x08,   // final: dest accepted
  DECLINE = 0x09,  // empty reason = silent remote_hangup; non-empty = user-visible
};

// u8-length-prefixed string fields; capped well below UDP MTU.
static constexpr size_t INTERCOM_MAX_CALL_ID_LEN = 64;
static constexpr size_t INTERCOM_MAX_ROUTE_ID_LEN = 64;
static constexpr size_t INTERCOM_MAX_NAME_LEN = 64;
static constexpr size_t INTERCOM_MAX_REASON_LEN = 160;

// Wire layout. Keep aligned with docs/INTERCOM_PROTOCOL.md and
// custom_components/intercom_native/protocol.py.
//
//   header (3 bytes): u8 type | u16 length (LE)
//   body: call_id_len:u8 | call_id[call_id_len] | per-type tail
//   PING/PONG bodies use call_id_len = 0.
//
// Helpers return bytes written/read or 0 on failure.

inline size_t encode_call_id_prefix(uint8_t *out, size_t out_cap, const std::string &call_id) {
  if (call_id.size() > INTERCOM_MAX_CALL_ID_LEN) return 0;
  size_t need = 1 + call_id.size();
  if (out_cap < need) return 0;
  out[0] = static_cast<uint8_t>(call_id.size());
  std::memcpy(out + 1, call_id.data(), call_id.size());
  return need;
}

inline size_t decode_call_id_prefix(const uint8_t *in, size_t in_len, std::string *out_call_id) {
  if (in_len < 1) return 0;
  uint8_t cid_len = in[0];
  if (in_len < 1u + cid_len) return 0;
  out_call_id->assign(reinterpret_cast<const char *>(in + 1), cid_len);
  return 1u + cid_len;
}

inline size_t encode_lp_string(uint8_t *out, size_t out_cap, const std::string &s, size_t max_len) {
  if (s.size() > max_len || s.size() > 0xFF) return 0;
  size_t need = 1 + s.size();
  if (out_cap < need) return 0;
  out[0] = static_cast<uint8_t>(s.size());
  std::memcpy(out + 1, s.data(), s.size());
  return need;
}

inline size_t decode_lp_string(const uint8_t *in, size_t in_len, std::string *out) {
  if (in_len < 1) return 0;
  uint8_t len = in[0];
  if (in_len < 1u + len) return 0;
  out->assign(reinterpret_cast<const char *>(in + 1), len);
  return 1u + len;
}

enum class ErrorCode : uint8_t {
  BUSY = 0x01,
};

// Wire contract: 16 kHz mono int16, 512 samples per chunk = 32 ms.
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t AUDIO_CHUNK_BYTES = 1024;

struct __attribute__((packed)) MessageHeader {
  uint8_t type;
  uint16_t length;  // little-endian on the wire
};

static constexpr size_t HEADER_SIZE = sizeof(MessageHeader);

// Explicit LE (de)serialisers; never memcpy the packed struct.
inline void encode_header(uint8_t *out, const MessageHeader &h) {
  out[0] = h.type;
  out[1] = static_cast<uint8_t>(h.length & 0xFF);
  out[2] = static_cast<uint8_t>((h.length >> 8) & 0xFF);
}
inline MessageHeader decode_header(const uint8_t *in) {
  MessageHeader h;
  h.type = in[0];
  h.length = static_cast<uint16_t>(static_cast<uint16_t>(in[1]) |
                                   (static_cast<uint16_t>(in[2]) << 8));
  return h;
}
static constexpr size_t MAX_AUDIO_CHUNK = 2048;  // browser sends larger chunks
static constexpr size_t MAX_MESSAGE_SIZE = HEADER_SIZE + MAX_AUDIO_CHUNK + 64;

static constexpr size_t RX_BUFFER_SIZE = 8192;   // ~256 ms / 4 browser chunks
static constexpr size_t TX_BUFFER_SIZE = 4096;   // ~128 ms / 4 chunks @ 32 ms

static constexpr uint32_t PING_INTERVAL_MS = 5000;
// 3x PING tolerates dropped pings on a flaky LAN before declaring the peer dead.
static constexpr uint32_t KEEPALIVE_DEADLINE_MS = PING_INTERVAL_MS * 3;

}  // namespace intercom_api
}  // namespace esphome
