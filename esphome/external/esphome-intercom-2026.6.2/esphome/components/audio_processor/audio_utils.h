#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
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

#ifndef ESPHOME_SCALE_BLOCK_I16_DEFINED
#define ESPHOME_SCALE_BLOCK_I16_DEFINED
// Q15 scale for standalone float gain paths. ESPHome-facing attenuation/volume
// uses esp-audio-libs Q31 in esp_audio_stack; user-facing positive mic gain
// uses esp_ae_alc. This helper remains for board-level input boost and
// intercom standalone paths that need amplification.
static inline void scale_block_i16_q15(const int16_t *in, int16_t *out, size_t len, int16_t q15) {
  if (q15 <= 0) {
    std::memset(out, 0, len * sizeof(int16_t));
    return;
  }
  if (q15 >= 32767) {
    if (in != out) std::memcpy(out, in, len * sizeof(int16_t));
    return;
  }
  for (size_t i = 0; i < len; i++) {
    out[i] = static_cast<int16_t>((static_cast<int32_t>(in[i]) * q15) >> 15);
  }
}

// Block volume / gain scale for an int16 PCM buffer.
//
// Three paths, picked by the gain value so the hot loop is always the cheapest
// option that produces correct samples:
//
//   gain == 1.0  -> memcpy (or noop when in == out). Skips the multiply entirely.
//   0 <= gain <= 1.0 -> scalar Q15 fixed-point attenuation for standalone paths.
//                       The Q15 fixed-point form is `(input * C_q15) >> 15`,
//                       which never overflows int16 for C in [0, 32767].
//   gain > 1.0 (or negative) -> scalar `scale_sample` with saturation, since
//                       Q15 cannot represent amplification factors. In
//                       esp_audio_stack, user-facing mic boost goes through
//                       esp_ae_alc; attenuation goes through esp-audio-libs Q31.
//
// `out` may alias `in`. `len` is sample count, not bytes.
static inline void scale_block_i16(const int16_t *in, int16_t *out, size_t len, float gain) {
  if (!std::isfinite(gain)) {
    std::memset(out, 0, len * sizeof(int16_t));
    return;
  }
  if (gain == 1.0f) {
    if (in != out) std::memcpy(out, in, len * sizeof(int16_t));
    return;
  }
  if (gain == 0.0f) {
    std::memset(out, 0, len * sizeof(int16_t));
    return;
  }
  if (gain >= 0.0f && gain <= 1.0f) {
    // Round-half-to-even to nearest Q15. Clamp to 32767 so gain == 1.0 - epsilon
    // does not wrap to 0 via int16 overflow.
    long q = std::lroundf(gain * 32768.0f);
    if (q > 32767) q = 32767;
    if (q < 0) q = 0;
    scale_block_i16_q15(in, out, len, static_cast<int16_t>(q));
    return;
  }
  // Amplification, negative gain, or non-ESP target: scalar with saturation.
  for (size_t i = 0; i < len; i++) out[i] = scale_sample(in[i], gain);
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
