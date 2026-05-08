#include "ring_buffer_caps.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

namespace esphome {
namespace audio_processor {

static const char *const TAG = "ring_buffer_caps";

namespace {

// Subclass that bypasses esphome::RingBuffer::create() to install a storage
// buffer with explicit heap capabilities. Protected members of RingBuffer
// become accessible through inheritance.
//
// Cleanup: esphome::RingBuffer::~RingBuffer() calls vRingbufferDelete(handle_)
// then frees storage_ via RAMAllocator::deallocate() which is a plain free().
// Since heap_caps_malloc() memory is valid to free() with libc free() in
// ESP-IDF, the base destructor handles teardown correctly. No virtual
// destructor is needed because the subclass has no extra state.
class CapsRingBuffer : public RingBuffer {
 public:
  bool install(size_t len, uint32_t caps) {
    uint8_t *mem = static_cast<uint8_t *>(heap_caps_malloc(len, caps));
    if (mem == nullptr)
      return false;
    this->storage_ = mem;
    this->size_ = len;
    this->handle_ = xRingbufferCreateStatic(len, RINGBUF_TYPE_BYTEBUF, mem, &this->structure_);
    if (this->handle_ == nullptr) {
      heap_caps_free(mem);
      this->storage_ = nullptr;
      this->size_ = 0;
      return false;
    }
    return true;
  }
  const void *probe_storage() const { return this->storage_; }
};

const char *policy_str(RingBufferPolicy p) {
  switch (p) {
    case RingBufferPolicy::INTERNAL: return "internal";
    case RingBufferPolicy::PREFER_PSRAM: return "prefer_psram";
    case RingBufferPolicy::PSRAM_ONLY: return "psram_only";
  }
  return "?";
}

}  // namespace

std::unique_ptr<RingBuffer> create_ring_buffer(size_t len, RingBufferPolicy policy, const char *name) {
  auto rb = std::unique_ptr<CapsRingBuffer>(new CapsRingBuffer());

  bool ok = false;
  switch (policy) {
    case RingBufferPolicy::INTERNAL:
      ok = rb->install(len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      break;
    case RingBufferPolicy::PREFER_PSRAM:
      ok = rb->install(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!ok)
        ok = rb->install(len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      break;
    case RingBufferPolicy::PSRAM_ONLY:
      ok = rb->install(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      break;
  }

  if (!ok) {
    ESP_LOGE(TAG, "ringbuffer '%s': alloc %u bytes FAILED (policy=%s)",
             name, static_cast<unsigned>(len), policy_str(policy));
    return nullptr;
  }

  // Report the real placement. For PREFER_PSRAM we want to know whether the
  // fallback hit, so never trust the requested policy alone.
  const void *storage = rb->probe_storage();
  const char *placement = esp_ptr_internal(storage) ? "internal" : "psram";
  ESP_LOGI(TAG, "ringbuffer '%s': size=%u policy=%s placement=%s",
           name, static_cast<unsigned>(len), policy_str(policy), placement);

  return std::unique_ptr<RingBuffer>(rb.release());
}

}  // namespace audio_processor
}  // namespace esphome

#endif  // USE_ESP32
