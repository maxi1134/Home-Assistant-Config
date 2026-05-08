#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace esphome {

#ifndef ESPHOME_SCALE_SAMPLE_DEFINED
#define ESPHOME_SCALE_SAMPLE_DEFINED
// Scale a 16-bit PCM sample by a float gain with saturation clamping.
// Supports gains > 1.0 (amplification) unlike ESPHome's Q15 scale_audio_samples().
// On ESP32-S3 (hardware FPU) this compiles to a single MADD.S instruction.
static inline int16_t scale_sample(int16_t sample, float gain) {
  int32_t s = static_cast<int32_t>(sample * gain);
  if (s > 32767) return 32767;
  if (s < -32768) return -32768;
  return static_cast<int16_t>(s);
}
#endif

#ifndef ESPHOME_COMPUTE_RMS_DBFS_DEFINED
#define ESPHOME_COMPUTE_RMS_DBFS_DEFINED
// 20*log10(32768): subtract from 10*log10(mean) to get dBFS without an
// extra sqrt() (more numerically stable for small means).
static constexpr float RMS_DBFS_OFFSET = 90.30899870f;
static constexpr float RMS_DBFS_SILENCE = -120.0f;

// RMS power in dBFS for int16 PCM samples. Returns -120 dBFS for silence.
// `stride` lets callers walk channel-interleaved buffers (e.g. TDM slots).
static inline float compute_rms_dbfs_i16(const int16_t *data, size_t samples, size_t stride = 1) {
  if (data == nullptr || samples == 0) return RMS_DBFS_SILENCE;
  uint64_t sumsq = 0;
  for (size_t i = 0; i < samples; i++) {
    int32_t s = data[i * stride];
    sumsq += static_cast<uint64_t>(s * s);
  }
  float mean = static_cast<float>(sumsq) / static_cast<float>(samples);
  if (mean <= 0.0f) return RMS_DBFS_SILENCE;
  return 10.0f * log10f(mean) - RMS_DBFS_OFFSET;
}

// Variant for int32 (e.g. TDM slot data with top 16 bits used). Each sample
// is right-shifted by 16 to land in int16 range before squaring.
static inline float compute_rms_dbfs_i32_top16(const int32_t *data, size_t samples, size_t stride = 1) {
  if (data == nullptr || samples == 0) return RMS_DBFS_SILENCE;
  uint64_t sumsq = 0;
  for (size_t i = 0; i < samples; i++) {
    int32_t s = data[i * stride] >> 16;
    sumsq += static_cast<uint64_t>(s * s);
  }
  float mean = static_cast<float>(sumsq) / static_cast<float>(samples);
  if (mean <= 0.0f) return RMS_DBFS_SILENCE;
  return 10.0f * log10f(mean) - RMS_DBFS_OFFSET;
}
#endif

}  // namespace esphome
