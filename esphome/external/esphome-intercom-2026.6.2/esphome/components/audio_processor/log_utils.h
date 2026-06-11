#pragma once

#ifdef USE_ESP32

#include <cstdint>

#include "esphome/core/log.h"

// Rate-limited WARN: emit on the first 5 occurrences and then every 100th.
// Each call site gets its own static counter (statics are scoped per
// expansion site). Avoids log floods on hot-path repeating errors (TCP/UDP
// send failures, I2S DMA hiccups) while still surfacing persistent faults.
//
// The macro expects a `TAG` symbol in scope - every .cpp using ESP_LOG*
// already declares one, so this is consistent with ESPHome conventions.
#define LOG_W_THROTTLED(fmt, ...) \
  do { \
    static uint32_t _throttled_n = 0; \
    _throttled_n++; \
    if (_throttled_n <= 5 || _throttled_n % 100 == 0) { \
      ESP_LOGW(TAG, fmt " [n=%u]", ##__VA_ARGS__, (unsigned) _throttled_n); \
    } \
  } while (0)

#endif  // USE_ESP32
