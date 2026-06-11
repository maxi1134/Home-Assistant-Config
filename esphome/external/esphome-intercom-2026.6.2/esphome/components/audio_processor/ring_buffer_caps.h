#pragma once

#ifdef USE_ESP32

#include "esphome/components/ring_buffer/ring_buffer.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace esphome {
namespace audio_processor {

/// Ring buffer allocation policy.
///
/// ESPHome's stock RingBuffer::create() now supports EXTERNAL_FIRST and
/// INTERNAL_FIRST preferences. It still does not offer strict placement or
/// boot-time placement logging, so latency-sensitive audio code cannot prove
/// whether a hot ring landed in internal RAM.
///
/// This helper provides explicit, verifiable placement:
///   - INTERNAL: always internal RAM. Use for anything in the audio hot path.
///   - PREFER_PSRAM: try PSRAM first, fall back to internal. Use for large
///     non-realtime buffers (protocol, staging) when internal is tight.
///   - PSRAM_ONLY: PSRAM only, fail if not available. Rarely appropriate.
///
/// At creation time the helper logs name, size, policy, and actual placement
/// (verified via esp_ptr_internal). This makes memory policy auditable at boot.
enum class RingBufferPolicy {
  INTERNAL,
  PREFER_PSRAM,
  PSRAM_ONLY,
};

/// ESPHome ring_buffer::RingBuffer with caller-controlled storage capabilities.
///
/// Keep the concrete type in our ownership model: ESPHome's RingBuffer
/// destructor is not virtual, so deleting a derived buffer through
/// std::unique_ptr<RingBuffer> would be undefined behaviour even though the
/// derived object has no additional fields.
class CapsRingBuffer : public ring_buffer::RingBuffer {
 public:
  ~CapsRingBuffer();

  bool install(size_t len, uint32_t caps, RingbufferType_t type = RINGBUF_TYPE_BYTEBUF);
  const void *probe_storage() const { return this->storage_; }

  size_t read(void *data, size_t len, TickType_t ticks_to_wait = 0);
  size_t write(const void *data, size_t len);
  size_t write_without_replacement(const void *data, size_t len, TickType_t ticks_to_wait = 0,
                                   bool write_partial = true);
  BaseType_t reset();

 protected:
  bool discard_bytes_(size_t discard_bytes);

 private:
  bool is_nosplit_() const { return this->type_ == RINGBUF_TYPE_NOSPLIT; }
  RingbufferType_t type_{RINGBUF_TYPE_BYTEBUF};
};

using RingBufferPtr = std::unique_ptr<CapsRingBuffer>;

/// Create a ring buffer with an explicit memory-placement policy.
///
/// @param len    capacity in bytes
/// @param policy placement policy
/// @param name   identifier for boot-time placement log (must outlive the call)
/// @return owned pointer, or nullptr on allocation failure
RingBufferPtr create_ring_buffer(size_t len, RingBufferPolicy policy, const char *name);
RingBufferPtr create_nosplit_ring_buffer(size_t len, RingBufferPolicy policy, const char *name);

/// Convenience: force internal RAM. Use for audio hot-path ring buffers.
inline RingBufferPtr create_internal(size_t len, const char *name) {
  return create_ring_buffer(len, RingBufferPolicy::INTERNAL, name);
}

/// Convenience: prefer PSRAM with internal fallback. Use for non-realtime buffers.
inline RingBufferPtr create_prefer_psram(size_t len, const char *name) {
  return create_ring_buffer(len, RingBufferPolicy::PREFER_PSRAM, name);
}

/// Convenience: PSRAM only. Use only when internal must be preserved at all cost.
inline RingBufferPtr create_psram(size_t len, const char *name) {
  return create_ring_buffer(len, RingBufferPolicy::PSRAM_ONLY, name);
}

inline RingBufferPtr create_nosplit_internal(size_t len, const char *name) {
  return create_nosplit_ring_buffer(len, RingBufferPolicy::INTERNAL, name);
}

inline RingBufferPtr create_nosplit_prefer_psram(size_t len, const char *name) {
  return create_nosplit_ring_buffer(len, RingBufferPolicy::PREFER_PSRAM, name);
}

}  // namespace audio_processor
}  // namespace esphome

#endif  // USE_ESP32
