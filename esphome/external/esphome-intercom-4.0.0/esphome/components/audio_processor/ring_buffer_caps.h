#pragma once

#ifdef USE_ESP32

#include "esphome/core/ring_buffer.h"

#include <memory>

namespace esphome {
namespace audio_processor {

/// Ring buffer allocation policy.
///
/// The stock esphome::RingBuffer::create() uses the default RAMAllocator which
/// tries PSRAM first, falling back to internal RAM. For audio hot-path buffers
/// this is dangerous: the placement is implicit and can silently move a buffer
/// that is read every audio frame to PSRAM, causing glitches under bus contention
/// with LVGL/flash cache.
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

/// Create a ring buffer with an explicit memory-placement policy.
///
/// @param len    capacity in bytes
/// @param policy placement policy
/// @param name   identifier for boot-time placement log (must outlive the call)
/// @return owned pointer, or nullptr on allocation failure
std::unique_ptr<RingBuffer> create_ring_buffer(size_t len, RingBufferPolicy policy, const char *name);

/// Convenience: force internal RAM. Use for audio hot-path ring buffers.
inline std::unique_ptr<RingBuffer> create_internal(size_t len, const char *name) {
  return create_ring_buffer(len, RingBufferPolicy::INTERNAL, name);
}

/// Convenience: prefer PSRAM with internal fallback. Use for non-realtime buffers.
inline std::unique_ptr<RingBuffer> create_prefer_psram(size_t len, const char *name) {
  return create_ring_buffer(len, RingBufferPolicy::PREFER_PSRAM, name);
}

/// Convenience: PSRAM only. Use only when internal must be preserved at all cost.
inline std::unique_ptr<RingBuffer> create_psram(size_t len, const char *name) {
  return create_ring_buffer(len, RingBufferPolicy::PSRAM_ONLY, name);
}

}  // namespace audio_processor
}  // namespace esphome

#endif  // USE_ESP32
