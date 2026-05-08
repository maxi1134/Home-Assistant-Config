#pragma once

#include <cstdint>

namespace esphome {
namespace intercom_api {

// TCP port for audio streaming
static constexpr uint16_t INTERCOM_PORT = 6054;

// Message types
enum class MessageType : uint8_t {
  AUDIO = 0x01,   // PCM audio data
  START = 0x02,   // Start streaming request
  STOP = 0x03,    // Stop streaming
  PING = 0x04,    // Keep-alive ping
  PONG = 0x05,    // Keep-alive response
  ERROR = 0x06,   // Error response
  RING = 0x07,    // ESP→HA: auto_answer OFF, waiting for local answer
  ANSWER = 0x08,  // ESP→HA: call answered locally, start stream
};

// Message flags
enum class MessageFlags : uint8_t {
  NONE = 0x00,
  NO_RING = 0x02,  // START flag: skip ringing, start streaming directly (for caller in bridge)
};

// Error codes
enum class ErrorCode : uint8_t {
  BUSY = 0x01,           // Already streaming with another client
};

// Audio format constants. Wire contract: 16 kHz mono int16, 512 samples per chunk = 32 ms.
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t AUDIO_CHUNK_SIZE = 1024;     // bytes per chunk

// Protocol header
struct __attribute__((packed)) MessageHeader {
  uint8_t type;      // MessageType
  uint8_t flags;     // MessageFlags
  uint16_t length;   // Payload length (little-endian)
};

static constexpr size_t HEADER_SIZE = sizeof(MessageHeader);
static constexpr size_t MAX_AUDIO_CHUNK = 2048;  // Browser may send larger chunks
static constexpr size_t MAX_MESSAGE_SIZE = HEADER_SIZE + MAX_AUDIO_CHUNK + 64;

// Buffer sizes
static constexpr size_t RX_BUFFER_SIZE = 8192;       // ~256ms - fits 4 browser chunks
static constexpr size_t TX_BUFFER_SIZE = 4096;       // ~128ms of audio (4 chunks @ 32ms)

// Timeouts
static constexpr uint32_t PING_INTERVAL_MS = 5000;

}  // namespace intercom_api
}  // namespace esphome
