#pragma once

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome {
namespace audio_processor {

/// Pin a FreeRTOS task to a core, with optional static stack from PSRAM.
///
/// Two creation paths are folded into one call so callers do not have to
/// repeat the if/else. Both paths log the failure reason via the provided
/// `log_tag` (uses ESP_LOGE).
///
///  - `psram_stack=true`: allocate `stack_bytes` worth of StackType_t storage
///    from PSRAM, then call xTaskCreateStaticPinnedToCore.
///    On allocation failure, returns false without touching the task system.
///    On success, *stack_out holds the allocated stack pointer (caller must
///    pass it back to cleanup_pinned_task() when stopping).
///
///  - `psram_stack=false`: classic xTaskCreatePinnedToCore with FreeRTOS
///    managing the stack on the internal heap. *stack_out stays nullptr.
///
/// `tcb_out` and `stack_out` are only touched when `psram_stack=true`.
/// `handle_out` is always set (to the new handle, or to nullptr on failure).
///
/// Returns true if the task is alive and pinned. Returns false if either the
/// stack allocation or the task creation failed; in that case the caller
/// stays responsible for any other resources it allocated for the task.
inline bool start_pinned_task(TaskFunction_t fn, const char *name, uint32_t stack_bytes,
                              void *param, UBaseType_t prio, BaseType_t core,
                              bool psram_stack, const char *log_tag,
                              TaskHandle_t *handle_out, StaticTask_t *tcb_out,
                              StackType_t **stack_out) {
  *handle_out = nullptr;
  const uint32_t stack_words = (stack_bytes + sizeof(StackType_t) - 1) / sizeof(StackType_t);
  if (psram_stack) {
    *stack_out = nullptr;
    RAMAllocator<StackType_t> alloc(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
    *stack_out = alloc.allocate(stack_words);
    if (*stack_out == nullptr) {
      ESP_LOGE(log_tag, "Failed to allocate PSRAM stack for %s task", name);
      return false;
    }
    *handle_out = xTaskCreateStaticPinnedToCore(fn, name, stack_bytes, param, prio,
                                                 *stack_out, tcb_out, core);
  } else {
    BaseType_t ok = xTaskCreatePinnedToCore(fn, name, stack_bytes,
                                             param, prio, handle_out, core);
    if (ok != pdPASS) {
      *handle_out = nullptr;
    }
  }
  if (*handle_out == nullptr) {
    ESP_LOGE(log_tag, "Failed to create %s task", name);
    if (psram_stack && *stack_out != nullptr) {
      RAMAllocator<StackType_t> alloc(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
      alloc.deallocate(*stack_out, stack_words);
      *stack_out = nullptr;
    }
    return false;
  }
  return true;
}

/// Force-delete a task and free its PSRAM stack. Use ONLY from setup
/// failure paths where the task was just spawned and has not yet entered
/// any blocking upstream API call. Calling vTaskDelete on a task that
/// is mid-recv / mid-esp-sr fetch / mid-i2s_channel_read leaves
/// internal locks corrupted; for the steady-state stop path use the
/// run-flag + done-semaphore pattern and then call cleanup_pinned_task.
inline void force_delete_pinned_task(TaskHandle_t *handle, StackType_t **stack, uint32_t stack_bytes) {
  if (handle != nullptr && *handle != nullptr) {
    if (eTaskGetState(*handle) != eDeleted) {
      vTaskDelete(*handle);
    }
    *handle = nullptr;
  }
  if (stack != nullptr && *stack != nullptr) {
    const uint32_t stack_words = (stack_bytes + sizeof(StackType_t) - 1) / sizeof(StackType_t);
    RAMAllocator<StackType_t> alloc(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
    alloc.deallocate(*stack, stack_words);
    *stack = nullptr;
  }
}

/// Free the PSRAM stack of a task that has already self-deleted.
///
/// Contract: the caller MUST have already driven the task to exit
/// (running flag + done semaphore / event group), and only then invoke
/// this helper. The helper does NOT call vTaskDelete: forcing it on a
/// task still inside an upstream library call (esp-sr fetch,
/// lwIP recv, ...) leaves internal locks corrupted and can race with
/// the destruction of resources the task is using (use-after-free).
///
/// On `eDeleted` the static stack is freed and `*handle`/`*stack` are
/// nulled. Otherwise the helper logs an error and leaks the stack so we
/// fail safe instead of free-while-in-use.
inline void cleanup_pinned_task(TaskHandle_t *handle, StackType_t **stack, uint32_t stack_bytes) {
  if (handle != nullptr && *handle != nullptr) {
    if (eTaskGetState(*handle) != eDeleted) {
      ESP_LOGE("task_utils",
               "cleanup_pinned_task: task still alive; caller did not wait "
               "on its done signal - leaking stack to avoid UAF");
      *handle = nullptr;
      return;
    }
    *handle = nullptr;
  }
  if (stack != nullptr && *stack != nullptr) {
    const uint32_t stack_words = (stack_bytes + sizeof(StackType_t) - 1) / sizeof(StackType_t);
    RAMAllocator<StackType_t> alloc(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
    alloc.deallocate(*stack, stack_words);
    *stack = nullptr;
  }
}

/// Convenience factory for the most common allocator we hand around: an
/// `RAMAllocator<T>` set to allocate from PSRAM. Equivalent to writing
/// `RAMAllocator<T>(RAMAllocator<T>::ALLOC_EXTERNAL)` inline, but documents
/// intent and reads better at call sites that allocate audio buffers.
template<typename T>
inline RAMAllocator<T> psram_allocator() {
  return RAMAllocator<T>(RAMAllocator<T>::ALLOC_EXTERNAL);
}

}  // namespace audio_processor
}  // namespace esphome

#endif  // USE_ESP32
