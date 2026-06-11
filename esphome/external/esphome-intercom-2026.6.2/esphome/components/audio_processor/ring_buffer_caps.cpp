#include "ring_buffer_caps.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

#include <algorithm>
#include <cstring>

namespace esphome {
namespace audio_processor {

static const char *const TAG = "ring_buffer_caps";

namespace {

const char *policy_str(RingBufferPolicy p) {
  switch (p) {
    case RingBufferPolicy::INTERNAL: return "internal";
    case RingBufferPolicy::PREFER_PSRAM: return "prefer_psram";
    case RingBufferPolicy::PSRAM_ONLY: return "psram_only";
  }
  return "?";
}

}  // namespace

CapsRingBuffer::~CapsRingBuffer() {
  if (this->handle_ != nullptr) {
    vRingbufferDelete(this->handle_);
    heap_caps_free(this->storage_);
    this->handle_ = nullptr;
    this->storage_ = nullptr;
    this->size_ = 0;
  }
}

bool CapsRingBuffer::install(size_t len, uint32_t caps, RingbufferType_t type) {
  uint8_t *mem = static_cast<uint8_t *>(heap_caps_malloc(len, caps));
  if (mem == nullptr)
    return false;
  this->storage_ = mem;
  this->size_ = len;
  this->type_ = type;
  this->handle_ = xRingbufferCreateStatic(len, type, mem, &this->structure_);
  if (this->handle_ == nullptr) {
    heap_caps_free(mem);
    this->storage_ = nullptr;
    this->size_ = 0;
    this->type_ = RINGBUF_TYPE_BYTEBUF;
    return false;
  }
  return true;
}

size_t CapsRingBuffer::read(void *data, size_t len, TickType_t ticks_to_wait) {
  if (!this->is_nosplit_())
    return ring_buffer::RingBuffer::read(data, len, ticks_to_wait);

  size_t item_len = 0;
  void *item = xRingbufferReceive(this->handle_, &item_len, ticks_to_wait);
  if (item == nullptr)
    return 0;

  const size_t copied = std::min(item_len, len);
  std::memcpy(data, item, copied);
  vRingbufferReturnItem(this->handle_, item);
  return copied;
}

size_t CapsRingBuffer::write(const void *data, size_t len) {
  if (!this->is_nosplit_())
    return ring_buffer::RingBuffer::write(data, len);

  while (this->free() < len) {
    if (!this->discard_bytes_(len - this->free()))
      break;
  }
  return this->write_without_replacement(data, len, 0, false);
}

size_t CapsRingBuffer::write_without_replacement(const void *data, size_t len, TickType_t ticks_to_wait,
                                                 bool write_partial) {
  if (!this->is_nosplit_())
    return ring_buffer::RingBuffer::write_without_replacement(data, len, ticks_to_wait, write_partial);

  if (xRingbufferSend(this->handle_, data, len, ticks_to_wait))
    return len;
  return 0;
}

BaseType_t CapsRingBuffer::reset() {
  if (!this->is_nosplit_())
    return ring_buffer::RingBuffer::reset();

  while (true) {
    size_t item_len = 0;
    void *item = xRingbufferReceive(this->handle_, &item_len, 0);
    if (item == nullptr)
      return pdPASS;
    vRingbufferReturnItem(this->handle_, item);
  }
}

bool CapsRingBuffer::discard_bytes_(size_t discard_bytes) {
  if (!this->is_nosplit_())
    return ring_buffer::RingBuffer::discard_bytes_(discard_bytes);

  size_t discarded = 0;
  while (discarded < discard_bytes) {
    size_t item_len = 0;
    void *item = xRingbufferReceive(this->handle_, &item_len, 0);
    if (item == nullptr)
      break;
    discarded += item_len;
    vRingbufferReturnItem(this->handle_, item);
  }
  return discarded >= discard_bytes;
}

static RingBufferPtr create_ring_buffer_with_type(size_t len, RingBufferPolicy policy, const char *name,
                                                  RingbufferType_t type) {
  auto rb = RingBufferPtr(new CapsRingBuffer());

  bool ok = false;
  switch (policy) {
    case RingBufferPolicy::INTERNAL:
      ok = rb->install(len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, type);
      break;
    case RingBufferPolicy::PREFER_PSRAM:
      ok = rb->install(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, type);
      if (!ok)
        ok = rb->install(len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, type);
      break;
    case RingBufferPolicy::PSRAM_ONLY:
      ok = rb->install(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, type);
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
  ESP_LOGI(TAG, "ringbuffer '%s': size=%u policy=%s type=%s placement=%s",
           name, static_cast<unsigned>(len), policy_str(policy),
           type == RINGBUF_TYPE_NOSPLIT ? "nosplit" : "bytebuf", placement);

  return rb;
}

RingBufferPtr create_ring_buffer(size_t len, RingBufferPolicy policy, const char *name) {
  return create_ring_buffer_with_type(len, policy, name, RINGBUF_TYPE_BYTEBUF);
}

RingBufferPtr create_nosplit_ring_buffer(size_t len, RingBufferPolicy policy, const char *name) {
  return create_ring_buffer_with_type(len, policy, name, RINGBUF_TYPE_NOSPLIT);
}

}  // namespace audio_processor
}  // namespace esphome

#endif  // USE_ESP32
