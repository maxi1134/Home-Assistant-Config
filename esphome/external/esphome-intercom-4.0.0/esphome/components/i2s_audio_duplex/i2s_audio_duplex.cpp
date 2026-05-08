#include "i2s_audio_duplex.h"

#ifdef USE_ESP32

#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <dsps_fir.h>
#include <algorithm>
#include <cmath>
#include <cstring>

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_AUDIO_PROCESSOR
#include "../audio_processor/audio_processor.h"
#endif
#include "../audio_processor/audio_utils.h"
#include "../audio_processor/ring_buffer_caps.h"

namespace esphome {
namespace i2s_audio_duplex {

static const char *const TAG = "i2s_duplex";

// =============================================================================
// FIR DECIMATOR (Pimpl)
// =============================================================================
// FIR coefficients: 32-tap (31 original + 1 zero pad), cutoff=7500Hz, fs=48kHz, Kaiser beta=8.0
// ~35dB stopband, symmetric linear phase. Q15 fixed-point for dsps_fird_s16_aes3 SIMD.
// Source float max |c| = 0.3125 -> q15 10238 (no overflow). DC gain ~0.975.
static_assert(FIR_NUM_TAPS % 8 == 0, "FIR_NUM_TAPS must be divisible by 8 for dsps_fird_s16_aes3");

namespace {
constexpr int16_t FIR_COEFFS_Q15[FIR_NUM_TAPS] = {
        1,     7,     4,   -33,   -88,   -61,   146,   415,
      350,  -357, -1335, -1407,   583,  4507,  8528, 10238,
     8528,  4507,   583, -1407, -1335,  -357,   350,   415,
      146,   -61,   -88,   -33,     4,     7,     1,     0,
};

// Float-precision FIR coefficients (32-tap Kaiser, cutoff 7500 Hz, fs 48 kHz,
// DC gain unitario). Used by the optional `fir_decimator: custom` kernel.
// Selected per-yaml: ESP32-P4 needs this to bypass the dsps_fird_s16 ASM kernel
// bug on RISC-V (esp-dsp issues #117/#102, rect-wave artifacts that propagate
// quantization noise into esp_aec adaptive filter -> musical noise on the wire).
constexpr float FIR_COEFFS_FLOAT[FIR_NUM_TAPS] = {
    4.1270231666e-05f, 2.1633893589e-04f, 1.2531119530e-04f, -9.9999988238e-04f,
    -2.6821920740e-03f, -1.8518117881e-03f, 4.4563387256e-03f, 1.2653483833e-02f,
    1.0683467077e-02f, -1.0893520506e-02f, -4.0743026823e-02f, -4.2934182572e-02f,
    1.7799016112e-02f, 1.3755146771e-01f, 2.6031620059e-01f, 3.1252367847e-01f,
    2.6031620059e-01f, 1.3755146771e-01f, 1.7799016112e-02f, -4.2934182572e-02f,
    -4.0743026823e-02f, -1.0893520506e-02f, 1.0683467077e-02f, 1.2653483833e-02f,
    4.4563387256e-03f, -1.8518117881e-03f, -2.6821920740e-03f, -9.9999988238e-04f,
    1.2531119530e-04f, 2.1633893589e-04f, 4.1270231666e-05f, 0.0f,
};

// Scalar float FIR decimator. Drop-in replacement for dsps_fird_s16, used when
// the YAML selects `fir_decimator: custom`. Slower than the SIMD kernel but
// numerically clean on every chip variant.
inline void fir_decimate_float(const int16_t *in, int16_t *out, int32_t out_count,
                               uint32_t ratio, float *delay_line, uint32_t *delay_pos) {
  for (int32_t o = 0; o < out_count; o++) {
    for (uint32_t r = 0; r < ratio; r++) {
      delay_line[*delay_pos] = static_cast<float>(*in++);
      *delay_pos = (*delay_pos + 1) & (FIR_NUM_TAPS - 1);
    }
    float acc = 0.0f;
    uint32_t idx = *delay_pos;
    for (size_t t = 0; t < FIR_NUM_TAPS; t++) {
      acc += delay_line[idx] * FIR_COEFFS_FLOAT[t];
      idx = (idx + 1) & (FIR_NUM_TAPS - 1);
    }
    if (acc > 32767.0f) acc = 32767.0f;
    if (acc < -32768.0f) acc = -32768.0f;
    out[o] = static_cast<int16_t>(acc);
  }
}

int16_t *alloc_fir_int16_preferred(size_t count, const char *who) {
  const size_t bytes = count * sizeof(int16_t);
  int16_t *p = static_cast<int16_t *>(
      heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (p == nullptr) {
    ESP_LOGW(who, "buffer (%u bytes) fell back to internal RAM (PSRAM full/unavailable)",
             static_cast<unsigned>(bytes));
    p = static_cast<int16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
  }
  return p;
}
}  // namespace

class FirDecimatorImpl {
 public:
  ~FirDecimatorImpl() {
    dsps_fird_s16_aexx_free(&this->fir_);
    if (this->scratch_ != nullptr)
      heap_caps_free(this->scratch_);
  }

  void init(uint32_t ratio) {
    this->ratio_ = ratio;
    memcpy(this->coeffs_local_, FIR_COEFFS_Q15, sizeof(FIR_COEFFS_Q15));
    dsps_fird_init_s16(&this->fir_, this->coeffs_local_, this->delay_,
                       FIR_NUM_TAPS, static_cast<int16_t>(ratio), 0, 0);
    dsps_16_array_rev(this->fir_.coeffs, this->fir_.coeffs_len);
  }

  void reset() {
    memset(this->delay_, 0, sizeof(this->delay_));
    this->fir_.pos = 0;
    this->fir_.d_pos = 0;
    memset(this->fdelay_, 0, sizeof(this->fdelay_));
    this->fdelay_pos_ = 0;
  }

  void set_use_float_fir(bool b) { this->use_float_fir_ = b; }

  void process(const int16_t *in, int16_t *out, size_t in_count) {
    if (this->ratio_ <= 1) {
      memcpy(out, in, in_count * sizeof(int16_t));
      return;
    }
    int32_t out_count = static_cast<int32_t>(in_count / this->ratio_);
    if (this->use_float_fir_) {
      fir_decimate_float(in, out, out_count, this->ratio_, this->fdelay_, &this->fdelay_pos_);
    } else {
      dsps_fird_s16(&this->fir_, in, out, out_count);
    }
  }

  void process_strided(const int16_t *in, int16_t *out, size_t out_count,
                       size_t stride, size_t offset) {
    if (this->ratio_ <= 1) {
      for (size_t i = 0; i < out_count; i++)
        out[i] = in[i * stride + offset];
      return;
    }
    size_t in_count = out_count * this->ratio_;
    if (!this->ensure_scratch_(in_count))
      return;
    for (size_t i = 0; i < in_count; i++)
      this->scratch_[i] = in[i * stride + offset];
    if (this->use_float_fir_) {
      fir_decimate_float(this->scratch_, out, static_cast<int32_t>(out_count),
                         this->ratio_, this->fdelay_, &this->fdelay_pos_);
    } else {
      dsps_fird_s16(&this->fir_, this->scratch_, out, static_cast<int32_t>(out_count));
    }
  }

  void process_strided_32(const int32_t *in, int16_t *out, size_t out_count,
                          size_t stride, size_t offset) {
    if (this->ratio_ <= 1) {
      for (size_t i = 0; i < out_count; i++)
        out[i] = static_cast<int16_t>(in[i * stride + offset] >> 16);
      return;
    }
    size_t in_count = out_count * this->ratio_;
    if (!this->ensure_scratch_(in_count))
      return;
    for (size_t i = 0; i < in_count; i++)
      this->scratch_[i] = static_cast<int16_t>(in[i * stride + offset] >> 16);
    if (this->use_float_fir_) {
      fir_decimate_float(this->scratch_, out, static_cast<int32_t>(out_count),
                         this->ratio_, this->fdelay_, &this->fdelay_pos_);
    } else {
      dsps_fird_s16(&this->fir_, this->scratch_, out, static_cast<int32_t>(out_count));
    }
  }

 private:
  bool ensure_scratch_(size_t count) {
    if (this->scratch_size_ >= count)
      return true;
    if (this->scratch_ != nullptr)
      heap_caps_free(this->scratch_);
    this->scratch_ = alloc_fir_int16_preferred(count, "FirDecim");
    this->scratch_size_ = (this->scratch_ != nullptr) ? count : 0;
    return this->scratch_ != nullptr;
  }

  uint32_t ratio_{1};
  fir_s16_t fir_{};
  alignas(16) int16_t coeffs_local_[FIR_NUM_TAPS]{};
  alignas(16) int16_t delay_[FIR_NUM_TAPS]{};
  int16_t *scratch_{nullptr};
  size_t scratch_size_{0};
  // Custom float FIR state (used when use_float_fir_ is true).
  // Always present; init/reset are cheap (memset + 1 word).
  bool use_float_fir_{false};
  alignas(16) float fdelay_[FIR_NUM_TAPS]{};
  uint32_t fdelay_pos_{0};
};

class MultiChannelFirDecimatorImpl {
 public:
  ~MultiChannelFirDecimatorImpl() {
    for (uint8_t c = 0; c < MC_FIR_MAX_CH; c++) {
      dsps_fird_s16_aexx_free(&this->fir_ch_[c]);
      if (this->out_ch_[c] != nullptr)
        heap_caps_free(this->out_ch_[c]);
    }
    if (this->scratch_ != nullptr)
      heap_caps_free(this->scratch_);
  }

  void init(uint32_t ratio, uint8_t num_channels) {
    this->ratio_ = ratio;
    this->num_channels_ = num_channels > MC_FIR_MAX_CH ? MC_FIR_MAX_CH : num_channels;
    for (uint8_t c = 0; c < this->num_channels_; c++) {
      memcpy(this->coeffs_local_ch_[c], FIR_COEFFS_Q15, sizeof(FIR_COEFFS_Q15));
      dsps_fird_init_s16(&this->fir_ch_[c], this->coeffs_local_ch_[c],
                         this->delay_ch_[c], FIR_NUM_TAPS,
                         static_cast<int16_t>(ratio), 0, 0);
      dsps_16_array_rev(this->fir_ch_[c].coeffs, this->fir_ch_[c].coeffs_len);
    }
  }

  void reset() {
    for (uint8_t c = 0; c < MC_FIR_MAX_CH; c++) {
      memset(this->delay_ch_[c], 0, sizeof(this->delay_ch_[c]));
      this->fir_ch_[c].pos = 0;
      this->fir_ch_[c].d_pos = 0;
      memset(this->fdelay_ch_[c], 0, sizeof(this->fdelay_ch_[c]));
      this->fdelay_pos_ch_[c] = 0;
    }
  }

  void set_use_float_fir(bool b) { this->use_float_fir_ = b; }

  void process_multi(const int16_t *in, size_t out_count, size_t in_stride,
                     const uint8_t *channel_offsets,
                     int16_t *mic_interleaved, int16_t *mic_mono,
                     int16_t *ref_out, uint8_t num_mic_ch) {
    if (this->ratio_ <= 1) {
      this->process_multi_passthrough_(in, out_count, in_stride, channel_offsets,
                                       mic_interleaved, mic_mono, ref_out, num_mic_ch);
      return;
    }
    const uint8_t nch = this->num_channels_;
    size_t in_count = out_count * this->ratio_;
    if (!this->ensure_buffers_(in_count, out_count, nch))
      return;

    for (uint8_t c = 0; c < nch; c++) {
      const uint8_t off = channel_offsets[c];
      for (size_t i = 0; i < in_count; i++)
        this->scratch_[i] = in[i * in_stride + off];
      if (this->use_float_fir_) {
        fir_decimate_float(this->scratch_, this->out_ch_[c], static_cast<int32_t>(out_count),
                           this->ratio_, this->fdelay_ch_[c], &this->fdelay_pos_ch_[c]);
      } else {
        dsps_fird_s16(&this->fir_ch_[c], this->scratch_, this->out_ch_[c],
                      static_cast<int32_t>(out_count));
      }
    }
    this->distribute_output_(out_count, mic_interleaved, mic_mono, ref_out, num_mic_ch);
  }

  void process_multi_32(const int32_t *in, size_t out_count, size_t in_stride,
                        const uint8_t *channel_offsets,
                        int16_t *mic_interleaved, int16_t *mic_mono,
                        int16_t *ref_out, uint8_t num_mic_ch) {
    if (this->ratio_ <= 1) {
      for (size_t o = 0; o < out_count; o++) {
        int16_t s0 = static_cast<int16_t>(in[o * in_stride + channel_offsets[0]] >> 16);
        if (mic_mono)
          mic_mono[o] = s0;
        if (num_mic_ch >= 2 && this->num_channels_ >= 2) {
          int16_t s1 = static_cast<int16_t>(in[o * in_stride + channel_offsets[1]] >> 16);
          if (mic_interleaved) {
            mic_interleaved[o * 2] = s0;
            mic_interleaved[o * 2 + 1] = s1;
          }
          if (ref_out && this->num_channels_ >= 3)
            ref_out[o] = static_cast<int16_t>(in[o * in_stride + channel_offsets[2]] >> 16);
        } else {
          if (ref_out && this->num_channels_ >= 2)
            ref_out[o] = static_cast<int16_t>(in[o * in_stride + channel_offsets[1]] >> 16);
        }
      }
      return;
    }
    const uint8_t nch = this->num_channels_;
    size_t in_count = out_count * this->ratio_;
    if (!this->ensure_buffers_(in_count, out_count, nch))
      return;

    for (uint8_t c = 0; c < nch; c++) {
      const uint8_t off = channel_offsets[c];
      for (size_t i = 0; i < in_count; i++)
        this->scratch_[i] = static_cast<int16_t>(in[i * in_stride + off] >> 16);
      if (this->use_float_fir_) {
        fir_decimate_float(this->scratch_, this->out_ch_[c], static_cast<int32_t>(out_count),
                           this->ratio_, this->fdelay_ch_[c], &this->fdelay_pos_ch_[c]);
      } else {
        dsps_fird_s16(&this->fir_ch_[c], this->scratch_, this->out_ch_[c],
                      static_cast<int32_t>(out_count));
      }
    }
    this->distribute_output_(out_count, mic_interleaved, mic_mono, ref_out, num_mic_ch);
  }

 private:
  void process_multi_passthrough_(const int16_t *in, size_t out_count, size_t in_stride,
                                  const uint8_t *channel_offsets,
                                  int16_t *mic_interleaved, int16_t *mic_mono,
                                  int16_t *ref_out, uint8_t num_mic_ch) {
    for (size_t o = 0; o < out_count; o++) {
      int16_t s0 = in[o * in_stride + channel_offsets[0]];
      if (mic_mono)
        mic_mono[o] = s0;
      if (num_mic_ch >= 2 && this->num_channels_ >= 2) {
        int16_t s1 = in[o * in_stride + channel_offsets[1]];
        if (mic_interleaved) {
          mic_interleaved[o * 2] = s0;
          mic_interleaved[o * 2 + 1] = s1;
        }
        if (ref_out && this->num_channels_ >= 3)
          ref_out[o] = in[o * in_stride + channel_offsets[2]];
      } else {
        if (ref_out && this->num_channels_ >= 2)
          ref_out[o] = in[o * in_stride + channel_offsets[1]];
      }
    }
  }

  void distribute_output_(size_t out_count, int16_t *mic_interleaved, int16_t *mic_mono,
                          int16_t *ref_out, uint8_t num_mic_ch) {
    for (size_t o = 0; o < out_count; o++) {
      int16_t s0 = this->out_ch_[0][o];
      if (mic_mono)
        mic_mono[o] = s0;
      if (num_mic_ch >= 2 && this->num_channels_ >= 2) {
        int16_t s1 = this->out_ch_[1][o];
        if (mic_interleaved) {
          mic_interleaved[o * 2] = s0;
          mic_interleaved[o * 2 + 1] = s1;
        }
        if (ref_out && this->num_channels_ >= 3)
          ref_out[o] = this->out_ch_[2][o];
      } else {
        if (ref_out && this->num_channels_ >= 2)
          ref_out[o] = this->out_ch_[1][o];
      }
    }
  }

  bool ensure_buffers_(size_t in_count, size_t out_count, uint8_t nch) {
    if (this->scratch_size_ >= in_count && this->out_size_ >= out_count)
      return true;
    if (this->scratch_ != nullptr) {
      heap_caps_free(this->scratch_);
      this->scratch_ = nullptr;
      this->scratch_size_ = 0;
    }
    for (uint8_t c = 0; c < MC_FIR_MAX_CH; c++) {
      if (this->out_ch_[c] != nullptr) {
        heap_caps_free(this->out_ch_[c]);
        this->out_ch_[c] = nullptr;
      }
    }
    this->out_size_ = 0;

    this->scratch_ = alloc_fir_int16_preferred(in_count, "MCFirDecim");
    if (this->scratch_ == nullptr)
      return false;
    this->scratch_size_ = in_count;
    for (uint8_t c = 0; c < nch; c++) {
      this->out_ch_[c] = alloc_fir_int16_preferred(out_count, "MCFirDecim");
      if (this->out_ch_[c] == nullptr)
        return false;
    }
    this->out_size_ = out_count;
    return true;
  }

  uint32_t ratio_{1};
  uint8_t num_channels_{0};
  fir_s16_t fir_ch_[MC_FIR_MAX_CH]{};
  alignas(16) int16_t coeffs_local_ch_[MC_FIR_MAX_CH][FIR_NUM_TAPS]{};
  alignas(16) int16_t delay_ch_[MC_FIR_MAX_CH][FIR_NUM_TAPS]{};
  int16_t *scratch_{nullptr};
  int16_t *out_ch_[MC_FIR_MAX_CH]{};
  size_t scratch_size_{0};
  size_t out_size_{0};
  bool use_float_fir_{false};
  alignas(16) float fdelay_ch_[MC_FIR_MAX_CH][FIR_NUM_TAPS]{};
  uint32_t fdelay_pos_ch_[MC_FIR_MAX_CH]{};
};

// FirDecimator (outer) - thin Pimpl wrapper.
FirDecimator::FirDecimator() : impl_(std::make_unique<FirDecimatorImpl>()) {}
FirDecimator::~FirDecimator() = default;
void FirDecimator::init(uint32_t ratio) { this->impl_->init(ratio); }
void FirDecimator::reset() { this->impl_->reset(); }
void FirDecimator::set_use_float_fir(bool b) { this->impl_->set_use_float_fir(b); }
void FirDecimator::process(const int16_t *in, int16_t *out, size_t in_count) {
  this->impl_->process(in, out, in_count);
}
void FirDecimator::process_strided(const int16_t *in, int16_t *out, size_t out_count,
                                   size_t stride, size_t offset) {
  this->impl_->process_strided(in, out, out_count, stride, offset);
}
void FirDecimator::process_strided_32(const int32_t *in, int16_t *out, size_t out_count,
                                      size_t stride, size_t offset) {
  this->impl_->process_strided_32(in, out, out_count, stride, offset);
}

// MultiChannelFirDecimator (outer) - thin Pimpl wrapper.
MultiChannelFirDecimator::MultiChannelFirDecimator()
    : impl_(std::make_unique<MultiChannelFirDecimatorImpl>()) {}
MultiChannelFirDecimator::~MultiChannelFirDecimator() = default;
void MultiChannelFirDecimator::init(uint32_t ratio, uint8_t num_channels) {
  this->impl_->init(ratio, num_channels);
}
void MultiChannelFirDecimator::reset() { this->impl_->reset(); }
void MultiChannelFirDecimator::set_use_float_fir(bool b) { this->impl_->set_use_float_fir(b); }
void MultiChannelFirDecimator::process_multi(const int16_t *in, size_t out_count, size_t in_stride,
                                             const uint8_t *channel_offsets,
                                             int16_t *mic_interleaved, int16_t *mic_mono,
                                             int16_t *ref_out, uint8_t num_mic_ch) {
  this->impl_->process_multi(in, out_count, in_stride, channel_offsets,
                             mic_interleaved, mic_mono, ref_out, num_mic_ch);
}
void MultiChannelFirDecimator::process_multi_32(const int32_t *in, size_t out_count, size_t in_stride,
                                                const uint8_t *channel_offsets,
                                                int16_t *mic_interleaved, int16_t *mic_mono,
                                                int16_t *ref_out, uint8_t num_mic_ch) {
  this->impl_->process_multi_32(in, out_count, in_stride, channel_offsets,
                                mic_interleaved, mic_mono, ref_out, num_mic_ch);
}

// Helper: get MCLK multiple enum from integer value
static i2s_mclk_multiple_t get_mclk_multiple(uint32_t mult) {
  switch (mult) {
    case 128: return I2S_MCLK_MULTIPLE_128;
    case 384: return I2S_MCLK_MULTIPLE_384;
    case 512: return I2S_MCLK_MULTIPLE_512;
    default: return I2S_MCLK_MULTIPLE_256;
  }
}

// Helper: get STD slot config for the configured comm format (0=philips, 1=msb, 2=pcm_short, 3=pcm_long)
// Note: PCM short/long are TDM-only in ESP-IDF; falls back to Philips in STD mode
static i2s_std_slot_config_t get_std_slot_config(uint8_t fmt, i2s_data_bit_width_t bw, i2s_slot_mode_t mode) {
  switch (fmt) {
    case 1: return I2S_STD_MSB_SLOT_DEFAULT_CONFIG(bw, mode);
    default: return I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bw, mode);
  }
}

#if SOC_I2S_SUPPORTS_TDM
// Helper: get TDM slot config for the configured comm format
static i2s_tdm_slot_config_t get_tdm_slot_config(uint8_t fmt, i2s_data_bit_width_t bw,
                                                   i2s_slot_mode_t mode, i2s_tdm_slot_mask_t mask) {
  switch (fmt) {
    case 1: return I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(bw, mode, mask);
    case 2: return I2S_TDM_PCM_SHORT_SLOT_DEFAULT_CONFIG(bw, mode, mask);
    case 3: return I2S_TDM_PCM_LONG_SLOT_DEFAULT_CONFIG(bw, mode, mask);
    default: return I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(bw, mode, mask);
  }
}
#endif  // SOC_I2S_SUPPORTS_TDM

// Audio parameters
static const size_t DMA_BUFFER_COUNT = 8;
static const size_t DEFAULT_FRAME_SIZE = 256;  // samples per frame at output rate (used when no AEC)
// Base buffer scaled by decimation_ratio_. At ratio=3 this provides ~256ms capacity,
// sufficient for stability while avoiding excessive latency from large playback backlogs.
// margin to absorb mixer jitter while saving ~24KB RAM.
static const size_t SPEAKER_BUFFER_BASE = 8192; // base speaker buffer, scaled by decimation_ratio_

// I2S new driver uses milliseconds directly, NOT FreeRTOS ticks
static const uint32_t I2S_IO_TIMEOUT_MS = 50;

void I2SAudioDuplex::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2S Audio Duplex...");

  // Mutex for the mic consumer registry. Plain FreeRTOS mutex (used as lock,
  // not as a counting semaphore); replaces std::mutex to keep the public
  // header free of <mutex>.
  this->mic_consumers_mutex_ = xSemaphoreCreateMutex();
  if (this->mic_consumers_mutex_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create mic_consumers_mutex_");
    this->mark_failed();
    return;
  }

  // Compute decimation ratio: only active when output_sample_rate is explicitly set
  // and differs from sample_rate. If not set, ratio stays 1 (no decimation, zero overhead).
  if (this->output_sample_rate_ > 0 && this->output_sample_rate_ != this->sample_rate_) {
    this->decimation_ratio_ = this->sample_rate_ / this->output_sample_rate_;
    if (this->decimation_ratio_ * this->output_sample_rate_ != this->sample_rate_) {
      ESP_LOGE(TAG, "sample_rate (%u) must be an exact multiple of output_sample_rate (%u)",
               (unsigned)this->sample_rate_, (unsigned)this->output_sample_rate_);
      this->mark_failed();
      return;
    }
    if (this->decimation_ratio_ > 6) {
      ESP_LOGE(TAG, "Decimation ratio %u exceeds maximum of 6", (unsigned)this->decimation_ratio_);
      this->mark_failed();
      return;
    }
    this->mic_decimator_.init(this->decimation_ratio_);
    this->play_ref_decimator_.init(this->decimation_ratio_);
    this->mic_decimator_.set_use_float_fir(this->fir_decimator_custom_);
    this->play_ref_decimator_.set_use_float_fir(this->fir_decimator_custom_);
    // rx_decimator_ is lazily initialized inside audio_session_ once the
    // processor has reported its frame_spec and we know how many channels
    // the RX stream carries (mono / stereo-AEC / TDM-with-or-without-second-mic).
    ESP_LOGI(TAG, "Multi-rate: bus=%uHz, output=%uHz, ratio=%u",
             (unsigned)this->sample_rate_, (unsigned)this->output_sample_rate_,
             (unsigned)this->decimation_ratio_);
  }

  // Speaker ring buffer: stores data at bus rate (e.g. 48kHz).
  // Scale buffer size with decimation ratio to accommodate higher data rate.
  // PREFER_PSRAM: staging buffer between API play() and the i2s write path, not
  // realtime-critical itself (the task drains it at priority 19), so PSRAM is fine.
  this->speaker_buffer_size_ = SPEAKER_BUFFER_BASE * this->decimation_ratio_;
  this->speaker_buffer_ = audio_processor::create_prefer_psram(
      this->speaker_buffer_size_, "i2s_duplex.speaker");
  if (!this->speaker_buffer_) {
    ESP_LOGE(TAG, "Failed to create speaker ring buffer (%u bytes)", (unsigned)this->speaker_buffer_size_);
    this->mark_failed();
    return;
  }

  // AEC reference (mono mode only; stereo/TDM get ref from I2S RX).
  // direct_aec_ref_ is allocated lazily in allocate_audio_buffers_() once the
  // processor frame spec is known. Storage matches input_frame_bytes because
  // AudioProcessor::process consumes in_ref at input_samples length; the TX-side
  // decimator writes one input-side reference frame here at the processor rate,
  // not at the bus rate.

  // Audio task is created lazily on the first start() and kept alive for the
  // rest of the component's lifetime. Creating it here would hold ~8KB of
  // internal RAM from boot even on boards where the audio path has not been
  // exercised yet, which is enough to push waveshare-s3 below the threshold
  // that the esp-sr BSS init + HTTPS TLS streaming need.
  ESP_LOGI(TAG, "I2S Audio Duplex ready (speaker_buf=%u bytes)", (unsigned)this->speaker_buffer_size_);
}

void I2SAudioDuplex::set_processor(AudioProcessor *processor) {
  this->processor_ = processor;
  this->processor_enabled_.store(processor != nullptr, std::memory_order_relaxed);
  // Note: direct_aec_ref_ is allocated later in allocate_audio_buffers_() once
  // the processor frame spec is known for the current audio session.
}

void I2SAudioDuplex::loop() {
  // Nothing to do here: frame_spec changes are handled inside the permanent
  // audio task, which restarts its own session without main-thread help.
}

void I2SAudioDuplex::dump_config() {
  ESP_LOGCONFIG(TAG, "I2S Audio Duplex:");
  ESP_LOGCONFIG(TAG, "  LRCLK Pin: %d", this->lrclk_pin_);
  ESP_LOGCONFIG(TAG, "  BCLK Pin: %d", this->bclk_pin_);
  ESP_LOGCONFIG(TAG, "  MCLK Pin: %d", this->mclk_pin_);
  ESP_LOGCONFIG(TAG, "  DIN Pin: %d", this->din_pin_);
  ESP_LOGCONFIG(TAG, "  DOUT Pin: %d", this->dout_pin_);
  ESP_LOGCONFIG(TAG, "  I2S Port: %u", this->i2s_num_);
  ESP_LOGCONFIG(TAG, "  I2S Role: %s", this->i2s_mode_secondary_ ? "secondary (slave)" : "primary (master)");
  ESP_LOGCONFIG(TAG, "  I2S Bus Rate: %u Hz", (unsigned)this->sample_rate_);
  ESP_LOGCONFIG(TAG, "  I2S Bits Per Sample: %u", this->bits_per_sample_);
  if (this->slot_bit_width_ > 0) {
    ESP_LOGCONFIG(TAG, "  Slot Bit Width: %u", this->slot_bit_width_);
  }
  ESP_LOGCONFIG(TAG, "  TX Channels: %u (%s)", this->num_channels_,
                this->num_channels_ == 2 ? "stereo" : "mono");
  ESP_LOGCONFIG(TAG, "  RX Mic Channel: %s", this->mic_channel_right_ ? "RIGHT" : "LEFT");
  static const char *const fmt_names[] = {"Philips", "MSB", "PCM Short", "PCM Long"};
  ESP_LOGCONFIG(TAG, "  Comm Format: %s", fmt_names[this->i2s_comm_fmt_ & 3]);
  ESP_LOGCONFIG(TAG, "  MCLK Multiple: %u", (unsigned)this->mclk_multiple_);
  if (this->use_apll_) {
    ESP_LOGCONFIG(TAG, "  APLL: enabled");
  }
  if (this->correct_dc_offset_) {
    ESP_LOGCONFIG(TAG, "  DC Offset Correction: enabled");
  }
  if (this->decimation_ratio_ > 1) {
    ESP_LOGCONFIG(TAG, "  Output Rate: %u Hz (decimation x%u)",
                  (unsigned)this->get_output_sample_rate(), (unsigned)this->decimation_ratio_);
    ESP_LOGCONFIG(TAG, "  FIR Decimator: %s",
                  this->fir_decimator_custom_ ? "custom (float scalar, 32-tap Kaiser)"
                                              : "dsps_fird_s16 (esp-dsp SIMD)");
  }
  ESP_LOGCONFIG(TAG, "  Speaker Buffer: %u bytes", (unsigned)this->speaker_buffer_size_);
  if (this->use_stereo_aec_ref_) {
    ESP_LOGCONFIG(TAG, "  Stereo AEC Reference: %s channel", this->ref_channel_right_ ? "RIGHT" : "LEFT");
  }
  if (this->use_tdm_ref_) {
    if (this->tdm_second_mic_slot_ >= 0) {
      ESP_LOGCONFIG(TAG, "  TDM Reference: %u slots, mic_slots=[%u,%d], ref_slot=%u",
                    this->tdm_total_slots_, this->tdm_mic_slot_,
                    this->tdm_second_mic_slot_, this->tdm_ref_slot_);
    } else {
      ESP_LOGCONFIG(TAG, "  TDM Reference: %u slots, mic_slot=%u, ref_slot=%u",
                    this->tdm_total_slots_, this->tdm_mic_slot_, this->tdm_ref_slot_);
    }
  }
  ESP_LOGCONFIG(TAG, "  AEC: %s", this->processor_ != nullptr ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Task: priority=%u, core=%d, stack=%u",
                this->task_priority_, this->task_core_, (unsigned)this->task_stack_size_);
#ifdef USE_DUPLEX_TELEMETRY
  ESP_LOGCONFIG(TAG, "  Telemetry Log Interval: %u frames", (unsigned) this->telemetry_log_interval_frames_);
#endif
}

bool I2SAudioDuplex::init_i2s_duplex_() {
  ESP_LOGCONFIG(TAG, "Initializing I2S in DUPLEX mode...");

  // Map configured bit depth to I2S enum
  // Note: 24-bit data is stored in 32-bit DMA containers (MSB-aligned)
  i2s_data_bit_width_t bit_width;
  switch (this->bits_per_sample_) {
    case 32: bit_width = I2S_DATA_BIT_WIDTH_32BIT; break;
    case 24: bit_width = I2S_DATA_BIT_WIDTH_24BIT; break;
    default: bit_width = I2S_DATA_BIT_WIDTH_16BIT; break;
  }

  // Slot bit width: auto = match data bit width, or explicit override
  i2s_slot_bit_width_t slot_bw = I2S_SLOT_BIT_WIDTH_AUTO;
  if (this->slot_bit_width_ > 0) {
    switch (this->slot_bit_width_) {
      case 32: slot_bw = I2S_SLOT_BIT_WIDTH_32BIT; break;
      case 24: slot_bw = I2S_SLOT_BIT_WIDTH_24BIT; break;
      case 16: slot_bw = I2S_SLOT_BIT_WIDTH_16BIT; break;
      default: slot_bw = I2S_SLOT_BIT_WIDTH_AUTO; break;
    }
  }

  bool need_tx = (this->dout_pin_ >= 0);
  bool need_rx = (this->din_pin_ >= 0);

  if (!need_tx && !need_rx) {
    ESP_LOGE(TAG, "At least one of din_pin or dout_pin must be configured");
    return false;
  }

  // Channel configuration
  // Clock source: APLL for accurate clocking (ESP32 original only)
  i2s_clock_src_t clk_src = I2S_CLK_SRC_DEFAULT;
#ifdef I2S_CLK_SRC_APLL
  if (this->use_apll_) clk_src = I2S_CLK_SRC_APLL;
#endif
  i2s_mclk_multiple_t mclk_mult = get_mclk_multiple(this->mclk_multiple_);

  // DMA descriptor limit is 4092 bytes. Compute max bytes per frame across
  // TX and RX configs, then clamp dma_frame_num to stay within the limit.
  // RX can be wider than TX (e.g., mono TX but stereo RX for AEC feedback).
  uint32_t bytes_per_sample = (this->bits_per_sample_ > 16) ? 4 : 2;  // 24/32-bit → 4-byte DMA container
  uint32_t tx_bytes_per_frame = this->num_channels_ * bytes_per_sample;
  uint32_t rx_bytes_per_frame = tx_bytes_per_frame;
  if (this->use_stereo_aec_ref_) {
    rx_bytes_per_frame = 2 * bytes_per_sample;  // stereo RX forced
  }
#if SOC_I2S_SUPPORTS_TDM
  if (this->use_tdm_ref_) {
    uint32_t tdm_frame = this->tdm_total_slots_ * bytes_per_sample;
    rx_bytes_per_frame = tdm_frame;
    tx_bytes_per_frame = tdm_frame;
  }
#endif
  uint32_t max_bytes_per_frame = std::max(tx_bytes_per_frame, rx_bytes_per_frame);
  // dma_frame_num scales with sample_rate so each DMA descriptor holds ~10 ms
  // of audio at any rate. A hard-coded sample count would yield 256 ms total
  // DMA latency at 16 kHz vs 85 ms at 48 kHz with the same constant, exceeding
  // the AEC filter tail at low sample rates. esp-skainet boards use the same
  // ~10 ms/descriptor pattern (e.g. 160 frames/desc at 16 kHz).
  uint32_t dma_frame_num = std::max<uint32_t>(64, (this->sample_rate_ * 10) / 1000);
  if (max_bytes_per_frame > 0) {
    uint32_t max_frames = 4092 / max_bytes_per_frame;
    if (dma_frame_num > max_frames) {
      dma_frame_num = max_frames;
    }
  }
  i2s_chan_config_t chan_cfg = {
      .id = static_cast<i2s_port_t>(this->i2s_num_),
      .role = this->i2s_mode_secondary_ ? I2S_ROLE_SLAVE : I2S_ROLE_MASTER,
      .dma_desc_num = DMA_BUFFER_COUNT,
      .dma_frame_num = dma_frame_num,
      .auto_clear_after_cb = true,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
      .auto_clear_before_cb = false,
      .intr_priority = 0,
#endif
  };

  i2s_chan_handle_t *tx_ptr = need_tx ? &this->tx_handle_ : nullptr;
  i2s_chan_handle_t *rx_ptr = need_rx ? &this->rx_handle_ : nullptr;

  esp_err_t err = i2s_new_channel(&chan_cfg, tx_ptr, rx_ptr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(err));
    return false;
  }

  ESP_LOGD(TAG, "I2S channel created: TX=%s RX=%s",
           this->tx_handle_ ? "yes" : "no",
           this->rx_handle_ ? "yes" : "no");

  auto pin_or_nc = [](int pin) -> gpio_num_t {
    return pin >= 0 ? static_cast<gpio_num_t>(pin) : GPIO_NUM_NC;
  };

#if SOC_I2S_SUPPORTS_TDM
  if (this->use_tdm_ref_) {
    // ── TDM MODE: ES7210 multi-slot RX + ES8311 slot-0 TX ──
    // STEREO with 4 slots: DMA contains all 4 interleaved slots, BCLK/FS = 64.
    // ESP-IDF MONO only puts slot 0 in DMA; STEREO gives all active slots.
    // total_slot is derived from slot_mask (not slot_mode), so BCLK doesn't change.
    // ES8311 reads/writes slot 0 as standard I2S (first 16 bits after LRCLK edge).
    // DMA frame = tdm_total_slots × 2 bytes. At 4 slots, 256 frames = 2048 bytes/desc (< 4092 limit).
    i2s_tdm_slot_mask_t tdm_mask = I2S_TDM_SLOT0;
    for (int i = 1; i < this->tdm_total_slots_; i++)
      tdm_mask = static_cast<i2s_tdm_slot_mask_t>(tdm_mask | (I2S_TDM_SLOT0 << i));

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = this->sample_rate_,
            .clk_src = clk_src,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = mclk_mult,
        },
        .slot_cfg = get_tdm_slot_config(this->i2s_comm_fmt_, bit_width, I2S_SLOT_MODE_STEREO, tdm_mask),
        .gpio_cfg = {
            .mclk = pin_or_nc(this->mclk_pin_),
            .bclk = pin_or_nc(this->bclk_pin_),
            .ws = pin_or_nc(this->lrclk_pin_),
            .dout = pin_or_nc(this->dout_pin_),
            .din = pin_or_nc(this->din_pin_),
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    // Apply slot_bit_width override BEFORE init
    if (slot_bw != I2S_SLOT_BIT_WIDTH_AUTO) {
      tdm_cfg.slot_cfg.slot_bit_width = slot_bw;
    }

    if (this->tx_handle_) {
      err = i2s_channel_init_tdm_mode(this->tx_handle_, &tdm_cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init TDM TX: %s", esp_err_to_name(err));
        this->deinit_i2s_();
        return false;
      }
    }
    if (this->rx_handle_) {
      err = i2s_channel_init_tdm_mode(this->rx_handle_, &tdm_cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init TDM RX: %s", esp_err_to_name(err));
        this->deinit_i2s_();
        return false;
      }
    }

    if (this->tdm_second_mic_slot_ >= 0) {
      ESP_LOGD(TAG, "TDM mode: %d slots, mic_slots=[%d,%d], ref_slot=%d, mask=0x%x",
               this->tdm_total_slots_, this->tdm_mic_slot_, this->tdm_second_mic_slot_,
               this->tdm_ref_slot_, (unsigned) tdm_mask);
    } else {
      ESP_LOGD(TAG, "TDM mode: %d slots, mic_slot=%d, ref_slot=%d, mask=0x%x",
               this->tdm_total_slots_, this->tdm_mic_slot_, this->tdm_ref_slot_, (unsigned) tdm_mask);
    }
  } else
#endif  // SOC_I2S_SUPPORTS_TDM
  {
    // ── STANDARD MODE ──
    i2s_slot_mode_t tx_slot_mode = (this->num_channels_ == 2)
        ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO;
    i2s_std_config_t tx_cfg = {
        .clk_cfg = {
            .sample_rate_hz = this->sample_rate_,
            .clk_src = clk_src,
            .mclk_multiple = mclk_mult,
        },
        .slot_cfg = get_std_slot_config(this->i2s_comm_fmt_, bit_width, tx_slot_mode),
        .gpio_cfg = {
            .mclk = pin_or_nc(this->mclk_pin_),
            .bclk = pin_or_nc(this->bclk_pin_),
            .ws = pin_or_nc(this->lrclk_pin_),
            .dout = pin_or_nc(this->dout_pin_),
            .din = pin_or_nc(this->din_pin_),
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    tx_cfg.slot_cfg.slot_mask = (this->num_channels_ == 2)
        ? I2S_STD_SLOT_BOTH : (this->tx_slot_right_ ? I2S_STD_SLOT_RIGHT : I2S_STD_SLOT_LEFT);
    // Apply slot_bit_width override
    if (slot_bw != I2S_SLOT_BIT_WIDTH_AUTO) {
      tx_cfg.slot_cfg.slot_bit_width = slot_bw;
    }

    // RX configuration - always independent of TX num_channels
    i2s_std_config_t rx_cfg = tx_cfg;
    if (this->use_stereo_aec_ref_) {
      rx_cfg.slot_cfg = get_std_slot_config(this->i2s_comm_fmt_, bit_width, I2S_SLOT_MODE_STEREO);
      rx_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
      ESP_LOGD(TAG, "RX configured as STEREO for ES8311 digital feedback AEC");
    } else {
      rx_cfg.slot_cfg = get_std_slot_config(this->i2s_comm_fmt_, bit_width, I2S_SLOT_MODE_MONO);
      rx_cfg.slot_cfg.slot_mask = this->mic_channel_right_ ? I2S_STD_SLOT_RIGHT : I2S_STD_SLOT_LEFT;
    }
    // Apply slot_bit_width override to RX
    if (slot_bw != I2S_SLOT_BIT_WIDTH_AUTO) {
      rx_cfg.slot_cfg.slot_bit_width = slot_bw;
    }

    if (this->tx_handle_) {
      err = i2s_channel_init_std_mode(this->tx_handle_, &tx_cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init TX channel: %s", esp_err_to_name(err));
        this->deinit_i2s_();
        return false;
      }
      ESP_LOGD(TAG, "TX channel initialized");
    }

    if (this->rx_handle_) {
      err = i2s_channel_init_std_mode(this->rx_handle_, &rx_cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init RX channel: %s", esp_err_to_name(err));
        this->deinit_i2s_();
        return false;
      }
      ESP_LOGD(TAG, "RX channel initialized (%s)", this->use_stereo_aec_ref_ ? "stereo" : "mono");
    }
  }

  if (this->tx_handle_) {
    err = i2s_channel_enable(this->tx_handle_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to enable TX channel: %s", esp_err_to_name(err));
      this->deinit_i2s_();
      return false;
    }
  }
  if (this->rx_handle_) {
    err = i2s_channel_enable(this->rx_handle_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to enable RX channel: %s", esp_err_to_name(err));
      this->deinit_i2s_();
      return false;
    }
  }

  ESP_LOGI(TAG, "I2S DUPLEX initialized (%s)", this->use_tdm_ref_ ? "TDM" : "standard");
  return true;
}

void I2SAudioDuplex::deinit_i2s_() {
  if (this->tx_handle_) {
    i2s_channel_disable(this->tx_handle_);
    i2s_del_channel(this->tx_handle_);
    this->tx_handle_ = nullptr;
  }
  if (this->rx_handle_) {
    i2s_channel_disable(this->rx_handle_);
    i2s_del_channel(this->rx_handle_);
    this->rx_handle_ = nullptr;
  }
  ESP_LOGI(TAG, "I2S deinitialized");
}

void I2SAudioDuplex::start() {
  if (this->duplex_running_.load(std::memory_order_relaxed)) {
    return;
  }

  ESP_LOGI(TAG, "Starting duplex audio...");

  // Lazy, one-shot audio task creation. After the first successful start we
  // keep the task alive for the rest of the component's lifetime; stop() just
  // parks it in the outer wait loop, so rapid toggle cycles never race on
  // xTaskCreate.
  if (this->audio_task_handle_ == nullptr) {
    const BaseType_t core = this->task_core_ >= 0 ? this->task_core_ : tskNO_AFFINITY;
    if (this->audio_stack_in_psram_) {
      // Allocate the stack in PSRAM and hand it to a statically-created task.
      // Saves ~8KB of DMA-capable internal RAM at the cost of slower stack
      // access (PSRAM is ~3x slower). Only use on boards where the internal
      // headroom is needed for mbedtls/esp-sr/etc.
      if (this->audio_task_stack_ == nullptr) {
        RAMAllocator<StackType_t> psram_stack(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        const size_t words = this->task_stack_size_ / sizeof(StackType_t);
        this->audio_task_stack_ = psram_stack.allocate(words);
        if (this->audio_task_stack_ == nullptr) {
          ESP_LOGE(TAG, "Failed to allocate PSRAM audio task stack (%u bytes)",
                   (unsigned) this->task_stack_size_);
          this->has_i2s_error_.store(true, std::memory_order_relaxed);
          return;
        }
      }
      this->audio_task_handle_ = xTaskCreateStaticPinnedToCore(
          audio_task,
          "i2s_duplex",
          this->task_stack_size_ / sizeof(StackType_t),
          this,
          this->task_priority_,
          this->audio_task_stack_,
          &this->audio_task_tcb_,
          core
      );
      if (this->audio_task_handle_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create audio task (static/PSRAM stack)");
        this->has_i2s_error_.store(true, std::memory_order_relaxed);
        return;
      }
    } else {
      BaseType_t task_created = xTaskCreatePinnedToCore(
          audio_task,
          "i2s_duplex",
          this->task_stack_size_,
          this,
          this->task_priority_,
          &this->audio_task_handle_,
          core
      );
      if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio task");
        this->has_i2s_error_.store(true, std::memory_order_relaxed);
        return;
      }
    }
  }

  // I2S channels are created on first start and reused on subsequent cycles:
  // stop() only disables them, so here we either init from scratch or just
  // re-enable. Keeping the channels alive avoids i2s_del_channel/i2s_new_channel
  // thrash and the associated brief allocation failures under SPIRAM pressure.
  const bool channels_exist = (this->tx_handle_ != nullptr || this->rx_handle_ != nullptr);
  if (!channels_exist) {
    if (!this->init_i2s_duplex_()) {
      ESP_LOGE(TAG, "Failed to initialize I2S");
      return;
    }
  } else {
    if (this->tx_handle_) {
      esp_err_t err = i2s_channel_enable(this->tx_handle_);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "TX channel re-enable failed: %s", esp_err_to_name(err));
      }
    }
    if (this->rx_handle_) {
      esp_err_t err = i2s_channel_enable(this->rx_handle_);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "RX channel re-enable failed: %s", esp_err_to_name(err));
      }
    }
  }

  this->has_i2s_error_.store(false, std::memory_order_relaxed);
  this->speaker_running_.store(this->tx_handle_ != nullptr, std::memory_order_relaxed);

  this->speaker_buffer_->reset();

  // Reset FIR decimators for clean state. rx_decimator_ is lazily initialised
  // inside audio_session_, so reset only when its consumer path is active.
  this->mic_decimator_.reset();
  if (this->use_tdm_ref_ || this->use_stereo_aec_ref_) {
    this->rx_decimator_.reset();
  }
  this->play_ref_decimator_.reset();

#ifdef USE_AUDIO_PROCESSOR
  if (this->use_stereo_aec_ref_) {
    ESP_LOGD(TAG, "ES8311 digital feedback - reference is sample-aligned");
  }
  if (this->use_tdm_ref_) {
    ESP_LOGD(TAG, "TDM hardware reference - slot %u is echo ref", this->tdm_ref_slot_);
  }
#endif

  // Wake the permanent audio task (created once in setup()).
  this->duplex_running_.store(true, std::memory_order_relaxed);
  ESP_LOGI(TAG, "Duplex audio started");
}

void I2SAudioDuplex::stop() {
  if (!this->duplex_running_.load(std::memory_order_relaxed)) {
    return;
  }

  ESP_LOGI(TAG, "Stopping duplex audio...");

  // Note: mic_consumers_ deliberately NOT cleared here. Consumers stay
  // registered across stop()/start() so the mic path is reconnected
  // automatically after an internal restart (frame_spec change).
  this->speaker_running_.store(false, std::memory_order_relaxed);
  this->duplex_running_.store(false, std::memory_order_relaxed);

  // Wait for the task to park itself in its outer idle loop before we touch
  // I2S channels. The task polls duplex_running_ at the top of each inner
  // iteration, so this usually resolves in well under 60ms.
  int wait_count = 0;
  while (!this->audio_task_idle_.load(std::memory_order_relaxed) && wait_count < 60) {
    delay(10);
    wait_count++;
  }
  if (!this->audio_task_idle_.load(std::memory_order_relaxed)) {
    ESP_LOGW(TAG, "Audio task did not park within 600ms; continuing with channel teardown");
  }

  esp_err_t err;
  if (this->tx_handle_) {
    err = i2s_channel_disable(this->tx_handle_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "TX channel disable failed: %s", esp_err_to_name(err));
    }
  }
  if (this->rx_handle_) {
    err = i2s_channel_disable(this->rx_handle_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "RX channel disable failed: %s", esp_err_to_name(err));
    }
  }

  ESP_LOGI(TAG, "Duplex audio stopped");
}

void I2SAudioDuplex::register_mic_consumer(void *token) {
  bool needs_start = false;
  size_t count_after = 0;
  bool first_consumer = false;
  bool full = false;
  if (this->mic_consumers_mutex_ == nullptr)
    return;
  xSemaphoreTake(this->mic_consumers_mutex_, portMAX_DELAY);
  // Already registered?
  for (size_t i = 0; i < this->mic_consumer_count_; i++) {
    if (this->mic_consumers_[i] == token) {
      xSemaphoreGive(this->mic_consumers_mutex_);
      return;
    }
  }
  if (this->mic_consumer_count_ >= MAX_LISTENERS) {
    full = true;
  } else {
    first_consumer = (this->mic_consumer_count_ == 0);
    this->mic_consumers_[this->mic_consumer_count_++] = token;
    count_after = this->mic_consumer_count_;
    this->has_mic_consumers_.store(true, std::memory_order_relaxed);
    needs_start = !this->duplex_running_.load(std::memory_order_relaxed);
  }
  xSemaphoreGive(this->mic_consumers_mutex_);
  if (full) {
    ESP_LOGW(TAG, "Mic consumer registry full (max=%u), refusing token=%p",
             (unsigned) MAX_LISTENERS, token);
    return;
  }
  if (first_consumer) {
    ESP_LOGI(TAG, "Mic consumer registered (token=%p), mic path active (consumers=%zu)",
             token, count_after);
  } else {
    ESP_LOGD(TAG, "Mic consumer registered (token=%p, consumers=%zu)", token, count_after);
  }
  if (needs_start) {
    this->start();
  }
}

void I2SAudioDuplex::unregister_mic_consumer(void *token) {
  size_t count_after = 0;
  bool removed = false;
  bool last_consumer_gone = false;
  if (this->mic_consumers_mutex_ == nullptr)
    return;
  xSemaphoreTake(this->mic_consumers_mutex_, portMAX_DELAY);
  for (size_t i = 0; i < this->mic_consumer_count_; i++) {
    if (this->mic_consumers_[i] == token) {
      // Swap-and-pop: order in the array is not meaningful, swap with last.
      this->mic_consumers_[i] = this->mic_consumers_[this->mic_consumer_count_ - 1];
      this->mic_consumers_[this->mic_consumer_count_ - 1] = nullptr;
      this->mic_consumer_count_--;
      removed = true;
      break;
    }
  }
  count_after = this->mic_consumer_count_;
  last_consumer_gone = removed && this->mic_consumer_count_ == 0;
  this->has_mic_consumers_.store(this->mic_consumer_count_ != 0, std::memory_order_relaxed);
  xSemaphoreGive(this->mic_consumers_mutex_);
  if (!removed) {
    return;
  }
  if (last_consumer_gone) {
    ESP_LOGI(TAG, "Last mic consumer removed (token=%p), mic path idle", token);
  } else {
    ESP_LOGD(TAG, "Mic consumer unregistered (token=%p, consumers=%zu)", token, count_after);
  }
}

void I2SAudioDuplex::start_speaker() {
  if (!this->duplex_running_.load(std::memory_order_relaxed)) {
    this->start();
  }
  this->speaker_running_.store(true, std::memory_order_relaxed);

  this->play_ref_decimator_.reset();
}

void I2SAudioDuplex::stop_speaker() {
  this->speaker_running_.store(false, std::memory_order_relaxed);
  // Request audio task to reset ring buffers (avoids concurrent access).
  this->request_speaker_reset_.store(true, std::memory_order_relaxed);
}

size_t I2SAudioDuplex::play(const uint8_t *data, size_t len, TickType_t ticks_to_wait) {
  if (!this->speaker_buffer_) {
    return 0;
  }

  // Data arrives at bus rate (e.g. 48kHz from mixer/resampler). Write directly.
  size_t written = this->speaker_buffer_->write_without_replacement((void *) data, len, ticks_to_wait, true);

  if (written > 0) {
    this->last_speaker_audio_ms_.store(millis(), std::memory_order_relaxed);
  }
  return written;
}

bool I2SAudioDuplex::allocate_audio_buffers_(AudioTaskCtx &ctx) {
  if (this->audio_buffers_allocated_) {
    return true;
  }

  static constexpr size_t AEC_ALIGN = 16;
  const uint32_t buf_caps = this->buffers_in_psram_ ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL;

  // Worst-case processor mic channels: 2 if dual-mic TDM is available, else 1.
  // Allocating for 2ch unconditionally when dual-mic is possible lets the task
  // flip between MR (1 mic) and MMR (2 mic) without reallocating on reconfigure.
  const uint8_t worst_mic_ch = (this->tdm_second_mic_slot_ >= 0) ? 2 : 1;
  const size_t worst_processor_input_bytes = ctx.input_frame_bytes * worst_mic_ch;

  this->prealloc_rx_buffer_ = static_cast<int16_t *>(
      heap_caps_malloc(ctx.rx_frame_bytes, buf_caps));

  if (ctx.mic_separate) {
    this->prealloc_mic_buffer_ = static_cast<int16_t *>(
        heap_caps_aligned_alloc(AEC_ALIGN, ctx.input_frame_bytes, buf_caps));
  }

  if (worst_mic_ch > 1) {
    this->prealloc_processor_mic_buffer_ = static_cast<int16_t *>(
        heap_caps_aligned_alloc(AEC_ALIGN, worst_processor_input_bytes, buf_caps));
  }

  this->prealloc_spk_buffer_ = static_cast<int16_t *>(
      heap_caps_malloc(ctx.bus_frame_size * ctx.num_ch * ctx.i2s_bps, buf_caps));

  if (ctx.use_stereo_aec_ref || ctx.use_tdm_ref) {
    this->prealloc_spk_ref_buffer_ = static_cast<int16_t *>(
        heap_caps_aligned_alloc(AEC_ALIGN, ctx.input_frame_bytes, buf_caps));
  }

  if (ctx.use_tdm_ref) {
    const size_t tdm_tx_bytes = ctx.bus_frame_size * ctx.tdm_total_slots * ctx.i2s_bps;
    this->prealloc_tdm_tx_buffer_ = static_cast<int16_t *>(
        heap_caps_malloc(tdm_tx_bytes, buf_caps));
  }

#ifdef USE_AUDIO_PROCESSOR
  if (this->processor_ != nullptr && this->processor_->is_initialized()) {
    if (!this->prealloc_spk_ref_buffer_ && !ctx.use_tdm_ref) {
      this->prealloc_spk_ref_buffer_ = static_cast<int16_t *>(
          heap_caps_aligned_alloc(AEC_ALIGN, ctx.input_frame_bytes, buf_caps));
    }
    this->prealloc_aec_output_ = static_cast<int16_t *>(
        heap_caps_aligned_alloc(AEC_ALIGN, ctx.output_frame_bytes, buf_caps));

    // direct_aec_ref_ stores the decimated TX reference at the processor rate.
    // Sized to ctx.input_frame_bytes: the AEC reference is the signal that
    // enters the DSP alongside the mic, so it lives on the input side of the
    // processor (AudioProcessor::process expects in_ref of input_samples len).
    // The TX-side FIR writes input_frame_size samples here per frame.
    // Honours buffers_in_psram_ alongside the rest.
    if (!this->direct_aec_ref_ && !ctx.use_stereo_aec_ref && !ctx.use_tdm_ref) {
      this->direct_aec_ref_ = static_cast<int16_t *>(
          heap_caps_aligned_alloc(AEC_ALIGN, ctx.input_frame_bytes, buf_caps));
      if (this->direct_aec_ref_ != nullptr) {
        memset(this->direct_aec_ref_, 0, ctx.input_frame_bytes);
      }
    }

    // AEC reference ring buffer (TYPE2-style, no-codec setups). Also one-shot.
    if (this->aec_use_ring_buffer_ && !ctx.use_stereo_aec_ref && !ctx.use_tdm_ref &&
        !this->aec_ref_ring_buffer_) {
      // Sized at the processor rate (post-decimation), not the bus rate, since
      // we now decimate on the TX side before storing. Items pushed are
      // input_frame_bytes (one AEC reference frame) each.
      const uint32_t output_rate = this->sample_rate_ / ctx.ratio;
      size_t rb_bytes = (output_rate * this->aec_ref_buffer_ms_ / 1000) * sizeof(int16_t);
      if (rb_bytes < ctx.input_frame_bytes * 4) rb_bytes = ctx.input_frame_bytes * 4;
      // Placement YAML-controlled: internal saves ~13.6 us/frame on Core 0 (R+W ~1 KB each),
      // PSRAM saves ~3-5 KB internal RAM (set aec_ref_ring_in_psram).
      this->aec_ref_ring_buffer_ = this->aec_ref_ring_in_psram_
          ? audio_processor::create_prefer_psram(rb_bytes, "i2s_duplex.aec_ref")
          : audio_processor::create_internal(rb_bytes, "i2s_duplex.aec_ref");
      if (!this->aec_ref_ring_buffer_) {
        return false;
      }
      ESP_LOGI(TAG, "AEC reference: ring_buffer (%zu bytes, %ums capacity)",
               rb_bytes, (unsigned)this->aec_ref_buffer_ms_);
    } else if (!ctx.use_stereo_aec_ref && !ctx.use_tdm_ref && !this->aec_ref_ring_buffer_) {
      ESP_LOGI(TAG, "AEC reference: previous_frame");
    }
  }
#endif

  // Validate required allocations
  if (!this->prealloc_rx_buffer_ || !this->prealloc_spk_buffer_) return false;
  if (ctx.mic_separate && !this->prealloc_mic_buffer_) return false;
  if (worst_mic_ch > 1 && !this->prealloc_processor_mic_buffer_) return false;
  if (ctx.use_tdm_ref && !this->prealloc_tdm_tx_buffer_) return false;
#ifdef USE_AUDIO_PROCESSOR
  if (this->processor_ != nullptr && this->processor_->is_initialized()) {
    if (!this->prealloc_aec_output_) return false;
    if ((ctx.use_stereo_aec_ref || ctx.use_tdm_ref) && !this->prealloc_spk_ref_buffer_) return false;
    // Mono AEC depends on direct_aec_ref_ as both the TX-side decimation scratch
    // and the previous-frame store. If it failed to allocate, the AEC would
    // silently run with a zero reference (TX writer is null-gated, ring writer
    // too, fill_mono falls through to zero-fill) and stay degraded until reboot,
    // because audio_buffers_allocated_ latches true on success. Fail-closed.
    if (!ctx.use_stereo_aec_ref && !ctx.use_tdm_ref && !this->direct_aec_ref_) {
      ESP_LOGE(TAG, "Mono AEC reference buffer allocation failed (%zu bytes)",
               ctx.input_frame_bytes);
      return false;
    }
  }
#endif

  this->audio_buffers_allocated_ = true;
  return true;
}

void I2SAudioDuplex::audio_task(void *param) {
  I2SAudioDuplex *self = static_cast<I2SAudioDuplex *>(param);
  self->audio_task_();
  vTaskDelete(nullptr);
}

void I2SAudioDuplex::audio_task_() {
  while (!this->audio_task_shutdown_.load(std::memory_order_relaxed)) {
    // Park in the outer loop until start() flips duplex_running_ on.
    this->audio_task_idle_.store(true, std::memory_order_relaxed);
    while (!this->duplex_running_.load(std::memory_order_relaxed)) {
      if (this->audio_task_shutdown_.load(std::memory_order_relaxed)) return;
      vTaskDelay(pdMS_TO_TICKS(20));
    }
    this->audio_task_idle_.store(false, std::memory_order_relaxed);

    // Run one audio session. Returns on stop() (duplex_running_ cleared) or on
    // processor frame_spec change (session restarts in the next outer iter).
    this->audio_session_();
  }
}

void I2SAudioDuplex::audio_session_() {
  AudioTaskCtx ctx{};
#ifdef USE_DUPLEX_TELEMETRY
  uint32_t t_frame_count = 0;
  uint32_t t_spk_underruns = 0;
#ifdef USE_AUDIO_PROCESSOR
  ProcessorTelemetry prev_processor_telem{};
#endif
#endif

  // ── Populate invariants ──
  ctx.ratio = this->decimation_ratio_;
  ctx.i2s_bps = (this->bits_per_sample_ > 16) ? 4 : 2;
  ctx.num_ch = this->num_channels_;
  ctx.use_stereo_aec_ref = this->use_stereo_aec_ref_;
  ctx.use_tdm_ref = this->use_tdm_ref_;
  ctx.ref_channel_right = this->ref_channel_right_;
  ctx.correct_dc_offset = this->correct_dc_offset_;
  ctx.tdm_total_slots = this->tdm_total_slots_;
  ctx.tdm_mic_slot = this->tdm_mic_slot_;
  ctx.tdm_second_mic_slot = this->tdm_second_mic_slot_;
  ctx.tdm_ref_slot = this->tdm_ref_slot_;

  ESP_LOGI(TAG, "Audio task started (stereo=%s, tdm=%s, decimation=%ux)",
           ctx.use_stereo_aec_ref ? "YES" : "no",
           ctx.use_tdm_ref ? "YES" : "no", (unsigned)ctx.ratio);

  // Determine frame sizes: processors may consume and produce different frame lengths.
  ctx.input_frame_size = DEFAULT_FRAME_SIZE;
  ctx.output_frame_size = DEFAULT_FRAME_SIZE;
#ifdef USE_AUDIO_PROCESSOR
  if (this->processor_ != nullptr && this->processor_->is_initialized()) {
    auto spec = this->processor_->frame_spec();
    ctx.input_frame_size = spec.input_samples;
    ctx.output_frame_size = spec.output_samples;
    ctx.processor_mic_channels = std::max<uint8_t>(1, spec.mic_channels);
    ctx.processor_spec_revision = this->processor_->frame_spec_revision();
    uint32_t out_rate = this->get_output_sample_rate();
    ESP_LOGI(TAG, "Processor: input=%u, output=%u samples, mic_ch=%u (%ums @ %uHz), revision=%u",
             (unsigned) ctx.input_frame_size, (unsigned) ctx.output_frame_size,
             (unsigned) ctx.processor_mic_channels,
             (unsigned) (ctx.input_frame_size * 1000 / out_rate), (unsigned) out_rate,
             (unsigned) ctx.processor_spec_revision);
  }
#endif

  // Init multi-channel RX decimator now that we know channel count
  if (ctx.use_tdm_ref || ctx.use_stereo_aec_ref) {
    uint8_t rx_ch = ctx.use_tdm_ref
        ? (ctx.processor_mic_channels > 1 ? 3 : 2)  // MMR or MR
        : 2;  // stereo: mic + ref
    this->rx_decimator_.init(ctx.ratio, rx_ch);
    this->rx_decimator_.set_use_float_fir(this->fir_decimator_custom_);
  }

  // ── Frame sizing ──
  ctx.bus_frame_size = ctx.input_frame_size * ctx.ratio;
  ctx.input_frame_bytes = ctx.input_frame_size * sizeof(int16_t);
  ctx.processor_input_frame_bytes = ctx.input_frame_bytes * ctx.processor_mic_channels;
  ctx.output_frame_bytes = ctx.output_frame_size * sizeof(int16_t);
  ctx.bus_frame_bytes = ctx.bus_frame_size * sizeof(int16_t);
  if (ctx.use_tdm_ref) {
    ctx.rx_frame_bytes = ctx.bus_frame_size * ctx.tdm_total_slots * ctx.i2s_bps;
  } else if (ctx.use_stereo_aec_ref) {
    ctx.rx_frame_bytes = ctx.bus_frame_size * 2 * ctx.i2s_bps;
  } else {
    ctx.rx_frame_bytes = ctx.bus_frame_size * ctx.i2s_bps;
  }

  // ── Buffer setup ──
  // Working buffers are owned by the component and pre-allocated worst-case
  // on first task entry (see allocate_audio_buffers_). Subsequent restarts
  // (frame_spec change, feature toggle) reuse the same pointers without any
  // heap_caps_alloc calls, eliminating SPIRAM fragmentation that previously
  // caused "Failed to allocate AEC output buffer" after a few reconfigures.
  ctx.mic_separate = (ctx.ratio > 1) || ctx.use_stereo_aec_ref || ctx.use_tdm_ref;

  auto alloc_fail = [this](const char *what) {
    ESP_LOGE(TAG, "Failed to allocate %s", what);
    this->has_i2s_error_.store(true, std::memory_order_relaxed);
    this->duplex_running_.store(false, std::memory_order_relaxed);
    this->speaker_running_.store(false, std::memory_order_relaxed);
  };

  if (ctx.processor_mic_channels > 2) {
    alloc_fail("unsupported processor mic channel count");
    goto cleanup;
  }
  if (ctx.processor_mic_channels > 1 && !ctx.use_tdm_ref) {
    alloc_fail("dual-mic processor requires TDM microphone slots");
    goto cleanup;
  }
  if (ctx.processor_mic_channels > 1 && ctx.tdm_second_mic_slot < 0) {
    alloc_fail("dual-mic processor requires tdm_mic_slots with two slots");
    goto cleanup;
  }

  if (!this->allocate_audio_buffers_(ctx)) {
    alloc_fail("audio buffers");
    goto cleanup;
  }

  ctx.rx_buffer = this->prealloc_rx_buffer_;
  ctx.mic_buffer = ctx.mic_separate ? this->prealloc_mic_buffer_ : ctx.rx_buffer;
  if (ctx.processor_mic_channels > 1) {
    ctx.processor_mic_buffer = this->prealloc_processor_mic_buffer_;
  }
  ctx.spk_buffer = this->prealloc_spk_buffer_;
  ctx.spk_ref_buffer = this->prealloc_spk_ref_buffer_;
  ctx.tdm_tx_buffer = this->prealloc_tdm_tx_buffer_;
  ctx.aec_output = this->prealloc_aec_output_;
  if (ctx.use_tdm_ref) {
    ctx.tdm_tx_frame_bytes = ctx.bus_frame_size * ctx.tdm_total_slots * ctx.i2s_bps;
  }

#ifdef USE_DUPLEX_TELEMETRY
  ESP_LOGD(TAG, "Heap after audio init: internal=%u, PSRAM=%u",
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#endif

  // ── Main loop ──
  while (this->duplex_running_.load(std::memory_order_relaxed)) {
    // Service ring buffer operations requested by main thread
    if (this->request_speaker_reset_.exchange(false, std::memory_order_relaxed)) {
      this->speaker_buffer_->reset();
      this->direct_aec_ref_valid_ = false;
      if (this->aec_ref_ring_buffer_) {
        this->aec_ref_ring_buffer_->reset();
      }
    }
    // Reset per-frame state
    ctx.output_buffer = nullptr;
    ctx.current_output_frame_size = ctx.input_frame_size;
    ctx.current_output_frame_bytes = ctx.input_frame_bytes;

    // Snapshot atomic state for this frame (avoids repeated .load() in sample loops)
    ctx.mic_attenuation = this->mic_attenuation_.load(std::memory_order_relaxed);
    ctx.mic_gain = this->mic_gain_.load(std::memory_order_relaxed);
    ctx.speaker_volume = this->speaker_volume_.load(std::memory_order_relaxed);
    ctx.speaker_running = this->speaker_running_.load(std::memory_order_relaxed);
    ctx.speaker_paused = this->speaker_paused_.load(std::memory_order_relaxed);
    ctx.mic_running = this->has_mic_consumers_.load(std::memory_order_relaxed);

    this->process_rx_path_(ctx);

    ctx.processor_enabled = this->processor_enabled_.load(std::memory_order_relaxed);
#ifdef USE_AUDIO_PROCESSOR
    ctx.processor_ready = ctx.processor_enabled && this->processor_ != nullptr &&
                          this->processor_->is_initialized();
#endif
    ctx.now_ms = millis();

    this->process_aec_and_callbacks_(ctx);
    this->process_tx_path_(ctx);

#ifdef USE_AUDIO_PROCESSOR
    // Frame_spec change (e.g. SE toggled, MR<->MMR switch): exit this session
    // so the outer audio_task_ wrapper can re-enter audio_session_ with a
    // fresh ctx. Preallocated buffers are already worst-case sized, so the
    // restart does not touch the heap.
    if (this->processor_ != nullptr) {
      uint32_t rev = this->processor_->frame_spec_revision();
      if (rev != ctx.processor_spec_revision) {
        ESP_LOGI(TAG, "Processor frame_spec changed (rev %u -> %u), restarting session",
                 (unsigned) ctx.processor_spec_revision, (unsigned) rev);
        break;
      }
    }
#endif

#ifdef USE_DUPLEX_TELEMETRY
    {
      // Lightweight per-frame cycle snapshot (only when telemetry: true)
      // Note: includes I2S blocking time in rx/tx, separate compute-only measurement
      // would require bracketing inside the path functions.
      // t_frame_count and t_spk_underruns declared before the loop (reset on task restart)
      t_spk_underruns += ctx.speaker_underrun ? 1 : 0;
      t_frame_count++;
      if (t_frame_count >= this->telemetry_log_interval_frames_) {
        ESP_LOGD(TAG, "Perf[%u frames]: spk_underrun=%u, heap_int=%u, heap_ps=%u",
                 (unsigned) t_frame_count, (unsigned) t_spk_underruns,
                 (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#ifdef USE_AUDIO_PROCESSOR
        if (this->processor_ != nullptr) {
          ProcessorTelemetry telem = this->processor_->telemetry();
          const uint32_t audio_stack_high_water =
              uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t);
          auto delta_u32 = [](uint32_t current, uint32_t previous) -> uint32_t {
            return current >= previous ? (current - previous) : current;
          };
          ESP_LOGD(TAG,
                   "AFE[%u]: +glitch=%u +in_drop=%u +feed_ok=%u +feed_rej=%u "
                   "+fetch_ok=%u +fetch_to=%u +out_drop=%u rb=%.0f%% "
                   "q(feed=%u pk=%u, fetch=%u pk=%u)",
                   (unsigned) telem.frame_count,
                   (unsigned) delta_u32(telem.glitch_count, prev_processor_telem.glitch_count),
                   (unsigned) delta_u32(telem.input_ring_drop, prev_processor_telem.input_ring_drop),
                   (unsigned) delta_u32(telem.feed_ok, prev_processor_telem.feed_ok),
                   (unsigned) delta_u32(telem.feed_rejected, prev_processor_telem.feed_rejected),
                   (unsigned) delta_u32(telem.fetch_ok, prev_processor_telem.fetch_ok),
                   (unsigned) delta_u32(telem.fetch_timeout, prev_processor_telem.fetch_timeout),
                   (unsigned) delta_u32(telem.output_ring_drop, prev_processor_telem.output_ring_drop),
                   telem.ringbuf_free_pct * 100.0f,
                   (unsigned) telem.feed_queue_frames,
                   (unsigned) telem.feed_queue_peak,
                   (unsigned) telem.fetch_queue_frames,
                   (unsigned) telem.fetch_queue_peak);
          ESP_LOGD(TAG,
                   "AFE timing: proc last/max=%u/%uus feed last/max=%u/%uus "
                   "fetch last/max=%u/%uus stackB audio/feed/fetch=%u/%u/%u",
                   (unsigned) telem.process_us_last,
                   (unsigned) telem.process_us_max,
                   (unsigned) telem.feed_us_last,
                   (unsigned) telem.feed_us_max,
                   (unsigned) telem.fetch_us_last,
                   (unsigned) telem.fetch_us_max,
                   (unsigned) audio_stack_high_water,
                   (unsigned) telem.feed_stack_high_water,
                   (unsigned) telem.fetch_stack_high_water);
          prev_processor_telem = telem;
        }
#endif
        t_frame_count = 0;
        t_spk_underruns = 0;
      }
    }
#endif

    // Yield: I2S read/write already block on DMA, so taskYIELD suffices.
    if (ctx.consecutive_i2s_errors > 0) {
      delay(1);
    } else {
      taskYIELD();
    }
  }

cleanup:
  // Buffers live on as component-owned preallocations; nothing to free here.
  // Returning from audio_session_ hands control back to the outer wrapper,
  // which either parks the task (stop) or re-enters with fresh ctx
  // (frame_spec change).
  ESP_LOGD(TAG, "Audio session ended");
}

// ════════════════════════════════════════════════════════════════════════════
// RX PATH: I2S read → deinterleave/decimate → mic_buffer + spk_ref_buffer
// ════════════════════════════════════════════════════════════════════════════
void I2SAudioDuplex::process_rx_path_(AudioTaskCtx &ctx) {
  if (!this->rx_handle_)
    return;

  size_t bytes_read;
  esp_err_t err = i2s_channel_read(this->rx_handle_, ctx.rx_buffer, ctx.rx_frame_bytes,
                                    &bytes_read, I2S_IO_TIMEOUT_MS);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "i2s_channel_read failed: %s", esp_err_to_name(err));
    if (++ctx.consecutive_i2s_errors > 100) {
      ESP_LOGE(TAG, "Persistent I2S read errors (%d)", ctx.consecutive_i2s_errors);
      this->has_i2s_error_.store(true, std::memory_order_relaxed);
      this->duplex_running_.store(false, std::memory_order_relaxed);
    }
    return;
  }
  if (err != ESP_OK || bytes_read != ctx.rx_frame_bytes)
    return;

  ctx.consecutive_i2s_errors = 0;

  // Convert 32-bit I2S samples to 16-bit only when FIR strided does NOT handle it.
  // When ratio > 1, the FIR decimator reads 32-bit directly via process_strided_32.
  if (ctx.i2s_bps == 4 && ctx.ratio <= 1) {
    auto *src32 = reinterpret_cast<int32_t *>(ctx.rx_buffer);
    size_t total_i2s_samples = bytes_read / sizeof(int32_t);
    for (size_t i = 0; i < total_i2s_samples; i++) {
      ctx.rx_buffer[i] = static_cast<int16_t>(src32[i] >> 16);
    }
  }

  if (ctx.use_tdm_ref) {
    this->update_tdm_slot_levels_(ctx);
  }

  ctx.output_buffer = ctx.mic_buffer;  // Default: no AEC processing
  ctx.processor_input = ctx.mic_buffer;

#if SOC_I2S_SUPPORTS_TDM
  if (ctx.use_tdm_ref) {
    const uint8_t ts = ctx.tdm_total_slots;
    const bool dual_mic = ctx.processor_mic_channels > 1 && ctx.tdm_second_mic_slot >= 0;
    uint8_t ch_offsets[MC_FIR_MAX_CH];
    uint8_t num_mic_ch;
    if (dual_mic) {
      ch_offsets[0] = ctx.tdm_mic_slot;
      ch_offsets[1] = static_cast<uint8_t>(ctx.tdm_second_mic_slot);
      ch_offsets[2] = ctx.tdm_ref_slot;
      num_mic_ch = 2;
    } else {
      ch_offsets[0] = ctx.tdm_mic_slot;
      ch_offsets[1] = ctx.tdm_ref_slot;
      num_mic_ch = 1;
    }
    if (ctx.i2s_bps == 4) {
      auto *src32 = reinterpret_cast<const int32_t *>(ctx.rx_buffer);
      this->rx_decimator_.process_multi_32(src32, ctx.input_frame_size, ts, ch_offsets,
          dual_mic ? ctx.processor_mic_buffer : nullptr, ctx.mic_buffer,
          ctx.spk_ref_buffer, num_mic_ch);
    } else {
      this->rx_decimator_.process_multi(ctx.rx_buffer, ctx.input_frame_size, ts, ch_offsets,
          dual_mic ? ctx.processor_mic_buffer : nullptr, ctx.mic_buffer,
          ctx.spk_ref_buffer, num_mic_ch);
    }
    if (dual_mic) {
      ctx.processor_input = ctx.processor_mic_buffer;
    }
  } else
#endif
  if (ctx.use_stereo_aec_ref) {
    // Stereo: mic + ref via multi-channel FIR
    const uint8_t mi = ctx.ref_channel_right ? 0 : 1;
    const uint8_t ri = ctx.ref_channel_right ? 1 : 0;
    uint8_t ch_offsets[2] = {mi, ri};
    if (ctx.i2s_bps == 4) {
      auto *src32 = reinterpret_cast<const int32_t *>(ctx.rx_buffer);
      this->rx_decimator_.process_multi_32(src32, ctx.input_frame_size, 2, ch_offsets,
          nullptr, ctx.mic_buffer, ctx.spk_ref_buffer, 1);
    } else {
      this->rx_decimator_.process_multi(ctx.rx_buffer, ctx.input_frame_size, 2, ch_offsets,
          nullptr, ctx.mic_buffer, ctx.spk_ref_buffer, 1);
    }
  } else if (ctx.ratio > 1) {
    // Mono with decimation
    if (ctx.i2s_bps == 4) {
      auto *src32 = reinterpret_cast<const int32_t *>(ctx.rx_buffer);
      this->mic_decimator_.process_strided_32(src32, ctx.mic_buffer, ctx.input_frame_size, 1, 0);
    } else {
      this->mic_decimator_.process(ctx.rx_buffer, ctx.mic_buffer, ctx.bus_frame_size);
    }
  }
  // else: Mono without decimation: mic_buffer == rx_buffer (aliased), nothing to do

  // Fused loop: DC offset + mic attenuation in one pass.
  // For dual-mic: mic1 is in mic_buffer, mic2 is in processor_mic_buffer[i*2+1]
  // (both filled by the multi-channel FIR). Apply DC+atten on both, update in-place.
  // When neither DC nor attenuation is needed, processor_mic_buffer (dual_mic case)
  // is left as-is: the multi-channel FIR has already produced correct values for both mics.
  const bool do_dc = ctx.correct_dc_offset;
  const bool do_atten = ctx.mic_attenuation != 1.0f;
  const bool dual_mic = ctx.processor_mic_channels > 1 && ctx.processor_mic_buffer != nullptr;

  if (do_dc || do_atten) {
    const float atten = ctx.mic_attenuation;
    for (size_t i = 0; i < ctx.input_frame_size; i++) {
      int16_t s1 = ctx.mic_buffer[i];

      if (do_dc) {
        int32_t inp = (int32_t) s1 << 16;
        int32_t out = inp - ctx.dc_prev_input + ctx.dc_prev_output - (ctx.dc_prev_output >> 10);
        ctx.dc_prev_input = inp;
        ctx.dc_prev_output = out;
        s1 = static_cast<int16_t>(out >> 16);
      }
      if (do_atten) {
        s1 = scale_sample(s1, atten);
      }
      ctx.mic_buffer[i] = s1;

      if (dual_mic) {
        // mic2 already in processor_mic_buffer interleaved by multi-channel FIR
        int16_t s2 = ctx.processor_mic_buffer[i * 2 + 1];
        if (do_dc) {
          int32_t inp2 = (int32_t) s2 << 16;
          int32_t out2 = inp2 - ctx.dc_prev_input_secondary + ctx.dc_prev_output_secondary -
                         (ctx.dc_prev_output_secondary >> 10);
          ctx.dc_prev_input_secondary = inp2;
          ctx.dc_prev_output_secondary = out2;
          s2 = static_cast<int16_t>(out2 >> 16);
        }
        if (do_atten) {
          s2 = scale_sample(s2, atten);
        }
        // Update both mic1 and mic2 in the interleaved buffer
        ctx.processor_mic_buffer[i * 2] = s1;
        ctx.processor_mic_buffer[i * 2 + 1] = s2;
      }
    }
  }
  // dual_mic with no DC/atten: processor_mic_buffer already correct from FIR (see top comment).
}

void I2SAudioDuplex::update_tdm_slot_levels_(const AudioTaskCtx &ctx) {
  uint8_t enabled_slots[8];
  size_t enabled_count = 0;
  const uint8_t slot_limit = std::min<uint8_t>(ctx.tdm_total_slots, 8);
  for (uint8_t slot = 0; slot < slot_limit; slot++) {
    if (this->tdm_slot_level_sensor_enabled_[slot]) {
      enabled_slots[enabled_count++] = slot;
    }
  }
  if (enabled_count == 0) {
    return;
  }

  // Probe only every ~256 ms at 16 kHz / 512-sample cadence to keep overhead low.
  this->tdm_slot_level_divider_++;
  if (this->tdm_slot_level_divider_ < 8) {
    return;
  }
  this->tdm_slot_level_divider_ = 0;

  const size_t frame_samples = ctx.bus_frame_size;
  const size_t slot_stride = ctx.tdm_total_slots;
  for (size_t i = 0; i < enabled_count; i++) {
    uint8_t slot = enabled_slots[i];
    float dbfs;
    if (ctx.i2s_bps == 4 && ctx.ratio > 1) {
      // 32-bit mode with decimation: rx_buffer has not been converted to 16-bit
      auto *src32 = reinterpret_cast<const int32_t *>(ctx.rx_buffer);
      dbfs = compute_rms_dbfs_i32_top16(src32 + slot, frame_samples, slot_stride);
    } else {
      dbfs = compute_rms_dbfs_i16(ctx.rx_buffer + slot, frame_samples, slot_stride);
    }
    this->tdm_slot_level_dbfs_[slot].store(dbfs, std::memory_order_relaxed);
  }
}

// ════════════════════════════════════════════════════════════════════════════
// AEC + CALLBACKS: raw callbacks → AEC processing → gain → post callbacks
// ════════════════════════════════════════════════════════════════════════════
void I2SAudioDuplex::process_aec_and_callbacks_(AudioTaskCtx &ctx) {
  if (!this->rx_handle_ || ctx.output_buffer == nullptr)
    return;

  // Raw mic callbacks: pre-AEC audio for MWW
  if (ctx.mic_running && !this->raw_mic_callbacks_.empty()) {
    for (auto &callback : this->raw_mic_callbacks_) {
      callback((const uint8_t *) ctx.mic_buffer, ctx.input_frame_bytes);
    }
  }

#ifdef USE_AUDIO_PROCESSOR
#if SOC_I2S_SUPPORTS_TDM
  if (ctx.use_tdm_ref && ctx.processor_ready &&
      ctx.spk_ref_buffer != nullptr && ctx.aec_output != nullptr) {
    // TDM: hardware-synced reference, no speaker gating needed.
    // TDM analog ref already reflects DAC volume. No extra scaling on ref.
    this->processor_->process(ctx.processor_input, ctx.spk_ref_buffer, ctx.aec_output, ctx.processor_mic_channels);
    ctx.output_buffer = ctx.aec_output;
    ctx.current_output_frame_size = ctx.output_frame_size;
    ctx.current_output_frame_bytes = ctx.output_frame_bytes;
  } else
#endif
  if (!ctx.use_tdm_ref && ctx.processor_ready &&
      ctx.spk_ref_buffer != nullptr && ctx.aec_output != nullptr &&
      // Stereo AEC: reference is embedded in I2S RX (L=DAC loopback), always available.
      // Mono AEC: reference comes from speaker ring buffer, only valid when speaker is active.
      (ctx.use_stereo_aec_ref ||
       (ctx.speaker_running &&
        (ctx.now_ms - this->last_speaker_audio_ms_.load(std::memory_order_relaxed) <= AEC_ACTIVE_TIMEOUT_MS)))) {

    // Mono mode: get AEC reference (direct from TX or ring buffer).
    // Reference is post-volume PCM, no additional scaling (Espressif TYPE2 pattern).
    if (!ctx.use_stereo_aec_ref) {
      this->fill_mono_aec_reference_(ctx);
    }
    // Stereo mode: spk_ref_buffer already filled from deinterleave. No extra scaling.
    // TDM mode: spk_ref_buffer filled from TDM deinterleave. No extra scaling.

    this->processor_->process(ctx.processor_input, ctx.spk_ref_buffer, ctx.aec_output, ctx.processor_mic_channels);
    ctx.output_buffer = ctx.aec_output;
    ctx.current_output_frame_size = ctx.output_frame_size;
    ctx.current_output_frame_bytes = ctx.output_frame_bytes;
  }
#endif

  // Apply mic gain (snapshot value)
  if (ctx.mic_gain != 1.0f) {
    for (size_t i = 0; i < ctx.current_output_frame_size; i++) {
      ctx.output_buffer[i] = scale_sample(ctx.output_buffer[i], ctx.mic_gain);
    }
  }

  // Post-AEC callbacks (VA/STT)
  if (ctx.mic_running) {
    for (auto &callback : this->mic_callbacks_) {
      callback((const uint8_t *) ctx.output_buffer, ctx.current_output_frame_bytes);
    }
  }
}

void I2SAudioDuplex::fill_mono_aec_reference_(AudioTaskCtx &ctx) {
  // direct_aec_ref_ and the ring buffer hold already-decimated samples at the
  // processor rate (decimation happens once on the TX side in process_tx_path_).
  // The reference is the input side of the processor, so the unit is
  // input_frame_bytes (matches AudioProcessor::process expecting in_ref of
  // input_samples length).
  const size_t ref_bytes = ctx.input_frame_bytes;
  if (this->aec_ref_ring_buffer_) {
    if (this->aec_ref_ring_buffer_->available() >= ref_bytes) {
      this->aec_ref_ring_buffer_->read(ctx.spk_ref_buffer, ref_bytes, 0);
      return;
    }
  } else if (this->direct_aec_ref_ != nullptr && this->direct_aec_ref_valid_) {
    memcpy(ctx.spk_ref_buffer, this->direct_aec_ref_, ref_bytes);
    return;
  }

  // No reference available: zero-fill (AEC will pass-through without echo cancellation)
  memset(ctx.spk_ref_buffer, 0, ref_bytes);
}

// ════════════════════════════════════════════════════════════════════════════
// TX PATH: ring buffer read → volume → format expand → I2S write
// ════════════════════════════════════════════════════════════════════════════
void I2SAudioDuplex::process_tx_path_(AudioTaskCtx &ctx) {
  if (!this->tx_handle_)
    return;

  if (ctx.speaker_running) {
    ctx.speaker_got = this->speaker_buffer_->read((void *) ctx.spk_buffer, ctx.bus_frame_bytes, 0);
    size_t got = ctx.speaker_got;
    // Treat partial frames as underrun too (the frame is padded with zero below
    // and must not be used as AEC reference, otherwise the AEC adaptive filter
    // sees a half-real / half-silent signal and fails to correlate with the mic).
    ctx.speaker_underrun = (got < ctx.bus_frame_bytes && !ctx.speaker_paused);

    if (ctx.speaker_paused) {
      memset(ctx.spk_buffer, 0, ctx.bus_frame_bytes);
    } else if (got > 0) {
      if (ctx.speaker_volume != 1.0f) {
        size_t got_samples = got / sizeof(int16_t);
        for (size_t i = 0; i < got_samples; i++) {
          ctx.spk_buffer[i] = scale_sample(ctx.spk_buffer[i], ctx.speaker_volume);
        }
      }
      if (got < ctx.bus_frame_bytes) {
        memset(((uint8_t *) ctx.spk_buffer) + got, 0, ctx.bus_frame_bytes - got);
      }
    } else {
      memset(ctx.spk_buffer, 0, ctx.bus_frame_bytes);
    }
  } else {
    memset(ctx.spk_buffer, 0, ctx.bus_frame_bytes);
    ctx.speaker_got = 0;
  }

  // Save post-volume TX data as AEC reference (skip if processor is off).
  // The reference is decimated to the processor rate HERE, on the TX side, so
  // downstream storage and consumer reads happen at the smaller output size.
  // Two safety properties of this gating:
  //   1) Decimation only runs on a complete frame (speaker_got == bus_frame_bytes),
  //      otherwise the FIR delay-line would absorb zero-padding and pollute the
  //      next valid frame's reference for ~32 samples.
  //   2) Skipping the save on a short read keeps the last good reference, same
  //      as the prior implementation.
#ifdef USE_AUDIO_PROCESSOR
  const bool full_frame =
      ctx.speaker_running && !ctx.speaker_paused && ctx.speaker_got == ctx.bus_frame_bytes;
  if (this->aec_ref_ring_buffer_ && ctx.processor_enabled) {
    if (full_frame && this->direct_aec_ref_ != nullptr) {
      // Decimate TX -> processor rate into direct_aec_ref_ scratch, then push
      // the decimated frame into the ring. direct_aec_ref_ is sized for
      // input_frame_bytes by allocate_audio_buffers_() (the FIR writes
      // bus_frame_size / ratio = input_frame_size samples).
      this->play_ref_decimator_.process(ctx.spk_buffer, this->direct_aec_ref_, ctx.bus_frame_size);
      const size_t ref_bytes = ctx.input_frame_bytes;
      size_t written = this->aec_ref_ring_buffer_->write((void *) this->direct_aec_ref_, ref_bytes);
      if (written == 0) {
        // Buffer full: discard one decimated frame (same size as the new one).
        int16_t discard[1];  // unused, see reset path
        (void) discard;
        this->aec_ref_ring_buffer_->reset();
        this->aec_ref_ring_buffer_->write((void *) this->direct_aec_ref_, ref_bytes);
      }
    }
  } else if (this->direct_aec_ref_ != nullptr && ctx.processor_enabled) {
    // Previous frame mode: decimate TX once and keep the result for the next
    // AEC iteration. Only on a full frame, otherwise we keep the last good
    // direct_aec_ref_ to avoid feeding a zero-padded reference.
    if (full_frame) {
      this->play_ref_decimator_.process(ctx.spk_buffer, this->direct_aec_ref_, ctx.bus_frame_size);
      this->direct_aec_ref_valid_ = true;
    }
  }
#endif

  // Prepare TX: format expansion + TDM interleave
  const void *tx_data;
  size_t tx_bytes;
#if SOC_I2S_SUPPORTS_TDM
  if (ctx.use_tdm_ref && ctx.tdm_tx_buffer != nullptr) {
    if (ctx.i2s_bps == 4) {
      auto *tdm32 = reinterpret_cast<int32_t *>(ctx.tdm_tx_buffer);
      memset(tdm32, 0, ctx.tdm_tx_frame_bytes);
      for (size_t i = 0; i < ctx.bus_frame_size; i++) {
        tdm32[i * ctx.tdm_total_slots] = static_cast<int32_t>(ctx.spk_buffer[i]) << 16;
      }
    } else {
      memset(ctx.tdm_tx_buffer, 0, ctx.tdm_tx_frame_bytes);
      for (size_t i = 0; i < ctx.bus_frame_size; i++) {
        ctx.tdm_tx_buffer[i * ctx.tdm_total_slots] = ctx.spk_buffer[i];
      }
    }
    tx_data = ctx.tdm_tx_buffer;
    tx_bytes = ctx.tdm_tx_frame_bytes;
  } else
#endif
  {
    size_t total_tx_samples = ctx.bus_frame_size * ctx.num_ch;
    if (ctx.num_ch == 2 && ctx.i2s_bps == 4) {
      // Fused mono->stereo + 16->32 in one backward pass
      auto *dst32 = reinterpret_cast<int32_t *>(ctx.spk_buffer);
      for (int i = static_cast<int>(ctx.bus_frame_size) - 1; i >= 0; i--) {
        int32_t s = static_cast<int32_t>(ctx.spk_buffer[i]) << 16;
        dst32[i * 2 + 1] = s;
        dst32[i * 2] = s;
      }
    } else if (ctx.num_ch == 2) {
      for (int i = static_cast<int>(ctx.bus_frame_size) - 1; i >= 0; i--) {
        ctx.spk_buffer[i * 2 + 1] = ctx.spk_buffer[i];
        ctx.spk_buffer[i * 2] = ctx.spk_buffer[i];
      }
    } else if (ctx.i2s_bps == 4) {
      auto *dst32 = reinterpret_cast<int32_t *>(ctx.spk_buffer);
      for (int i = static_cast<int>(total_tx_samples) - 1; i >= 0; i--) {
        dst32[i] = static_cast<int32_t>(ctx.spk_buffer[i]) << 16;
      }
    }
    tx_data = ctx.spk_buffer;
    tx_bytes = total_tx_samples * ctx.i2s_bps;
  }

  size_t bytes_written;
  esp_err_t err = i2s_channel_write(this->tx_handle_, tx_data, tx_bytes, &bytes_written, I2S_IO_TIMEOUT_MS);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "i2s_channel_write failed: %s", esp_err_to_name(err));
    if (++ctx.consecutive_i2s_errors > 100) {
      ESP_LOGE(TAG, "Persistent I2S write errors (%d)", ctx.consecutive_i2s_errors);
      this->has_i2s_error_.store(true, std::memory_order_relaxed);
      this->duplex_running_.store(false, std::memory_order_relaxed);
    }
  } else if (err == ESP_OK) {
    ctx.consecutive_i2s_errors = 0;
  }

  // Report frames actually consumed from the ring buffer (not silence/pad frames).
  // Using got (ring buffer read) instead of bytes_written (I2S output) prevents
  // counting silence frames as "played" during underruns.
  if (err == ESP_OK && ctx.speaker_got > 0 && !this->speaker_output_callbacks_.empty()) {
    uint32_t frames_played = ctx.speaker_got / sizeof(int16_t);
    int64_t timestamp = esp_timer_get_time();
    for (auto &cb : this->speaker_output_callbacks_) {
      cb(frames_played, timestamp);
    }
  }
}

size_t I2SAudioDuplex::get_speaker_buffer_available() const {
  if (!this->speaker_buffer_) return 0;
  return this->speaker_buffer_->available();
}

size_t I2SAudioDuplex::get_speaker_buffer_size() const {
  return this->speaker_buffer_size_;
}

}  // namespace i2s_audio_duplex
}  // namespace esphome

#endif  // USE_ESP32
