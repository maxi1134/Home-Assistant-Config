#pragma once

#ifdef USE_ESP32

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/ring_buffer.h"

#include <driver/i2s_std.h>
#if SOC_I2S_SUPPORTS_TDM
#include <driver/i2s_tdm.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>

#include "../audio_processor/audio_processor.h"

namespace esphome {
namespace i2s_audio_duplex {

using audio_processor::AudioProcessor;
using audio_processor::ProcessorTelemetry;

// Maximum listener count for microphone/speaker reference counting
static constexpr UBaseType_t MAX_LISTENERS = 16;

// Callback type for mic data: receives raw PCM samples (pointer + length, zero-copy).
// IMPORTANT: Callbacks are invoked from the audio task (high priority, Core 0).
// They MUST NOT block, allocate memory, do network I/O, or hold locks.
// Target completion: <1ms to avoid I2S DMA underruns.
using MicDataCallback = std::function<void(const uint8_t *data, size_t len)>;
// Callback type for speaker output: reports frames played and timestamp (for mixer pending_playback tracking).
// Same real-time constraints as MicDataCallback apply.
using SpeakerOutputCallback = std::function<void(uint32_t frames, int64_t timestamp)>;

// FIR decimator parameters: 32-tap Q15, ~35 dB stopband, cutoff 7.5 kHz at 48 kHz.
// Coefficients and esp-dsp wiring live in the .cpp via Pimpl, so consumers of this
// header don't pay the dsps_fir.h parse cost. See FirDecimatorImpl in i2s_audio_duplex.cpp.
static constexpr size_t FIR_NUM_TAPS = 32;
static constexpr uint8_t MC_FIR_MAX_CH = 3;

// Forward declarations for Pimpl.
class FirDecimatorImpl;
class MultiChannelFirDecimatorImpl;

// FIR decimator: consumes samples at high rate, produces at low rate.
// Backed by esp-dsp dsps_fird_s16; on ESP32-S3 resolves to the _aes3 SIMD kernel.
// Strided variants deinterleave into a shared scratch buffer before the FIR,
// since the SIMD kernel requires contiguous input.
class FirDecimator {
 public:
  FirDecimator();
  ~FirDecimator();
  FirDecimator(const FirDecimator &) = delete;
  FirDecimator &operator=(const FirDecimator &) = delete;

  void init(uint32_t ratio);
  void reset();
  // When true, process_* uses the pure-float scalar FIR (custom kernel).
  // When false (default), uses dsps_fird_s16 (esp-dsp SIMD on supported chips).
  void set_use_float_fir(bool b);

  // Contiguous int16 input.
  void process(const int16_t *in, int16_t *out, size_t in_count);
  // Strided int16 input: deinterleave into scratch, then SIMD FIR.
  void process_strided(const int16_t *in, int16_t *out, size_t out_count,
                       size_t stride, size_t offset);
  // Strided int32 input with inline >>16 downshift.
  void process_strided_32(const int32_t *in, int16_t *out, size_t out_count,
                          size_t stride, size_t offset);

 private:
  std::unique_ptr<FirDecimatorImpl> impl_;
};

// Multi-channel FIR decimator: decimates N channels from TDM/stereo rx_buffer in one pass.
// Per-channel dsps_fird_s16 (SIMD _aes3 on S3). Max 3 channels (MMR: mic1 + mic2 + ref).
class MultiChannelFirDecimator {
 public:
  MultiChannelFirDecimator();
  ~MultiChannelFirDecimator();
  MultiChannelFirDecimator(const MultiChannelFirDecimator &) = delete;
  MultiChannelFirDecimator &operator=(const MultiChannelFirDecimator &) = delete;

  void init(uint32_t ratio, uint8_t num_channels);
  void reset();
  void set_use_float_fir(bool b);

  // Decimate N channels from strided int16 TDM input, producing:
  //   - mic_interleaved: [mic1, mic2, mic1, mic2, ...] (num_mic_ch interleaved, for AFE)
  //   - mic_mono: mic1 contiguous (for callbacks/MWW/intercom)
  //   - ref_out: ref contiguous (for AEC, may be nullptr if no ref channel)
  // channel_offsets: slot indices in TDM frame [mic1_slot, mic2_slot, ref_slot]
  // num_mic_ch: 1 or 2 (how many of the channels are mic, rest is ref)
  void process_multi(const int16_t *in, size_t out_count, size_t in_stride,
                     const uint8_t *channel_offsets,
                     int16_t *mic_interleaved, int16_t *mic_mono,
                     int16_t *ref_out, uint8_t num_mic_ch);
  // Same but for 32-bit I2S input with inline >>16 downshift.
  void process_multi_32(const int32_t *in, size_t out_count, size_t in_stride,
                        const uint8_t *channel_offsets,
                        int16_t *mic_interleaved, int16_t *mic_mono,
                        int16_t *ref_out, uint8_t num_mic_ch);

 private:
  std::unique_ptr<MultiChannelFirDecimatorImpl> impl_;
};

/// Full-duplex I2S codec driver.
///
/// Extends the upstream i2s_audio pattern with a single shared I2S bus
/// that carries both mic RX and speaker TX (typical for codec chips
/// like ES8311 / ES7210). Optionally integrates an AudioProcessor (set
/// via processor_id) for AEC/NS/AGC/SE on the mic path. The processor
/// may be EspAec (AEC only) or EspAfe (full pipeline); both are drop-in
/// AudioProcessor implementations.
///
/// Runs a single audio task that pulls I2S RX frames, optionally
/// decimates with FirDecimator, invokes the processor, and distributes
/// the result to registered MicDataCallback listeners. Speaker frames
/// come in via SpeakerOutputCallback and are written to I2S TX.
class I2SAudioDuplex : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // Pin setters
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }
  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }
  void set_din_pin(int pin) { this->din_pin_ = pin; }
  void set_dout_pin(int pin) { this->dout_pin_ = pin; }
  void set_sample_rate(uint32_t rate) { this->sample_rate_ = rate; }
  void set_output_sample_rate(uint32_t rate) { this->output_sample_rate_ = rate; }
  void set_bits_per_sample(uint8_t bps) { this->bits_per_sample_ = bps; }
  uint8_t get_bits_per_sample() const { return this->bits_per_sample_; }
  void set_correct_dc_offset(bool enabled) { this->correct_dc_offset_ = enabled; }
  void set_num_channels(uint8_t ch) { this->num_channels_ = ch; }
  uint8_t get_num_channels() const { return this->num_channels_; }
  void set_i2s_mode_secondary(bool secondary) { this->i2s_mode_secondary_ = secondary; }
  void set_use_apll(bool use) { this->use_apll_ = use; }
  void set_i2s_num(uint8_t num) { this->i2s_num_ = num; }
  void set_mclk_multiple(uint32_t mult) { this->mclk_multiple_ = mult; }
  void set_i2s_comm_fmt(uint8_t fmt) { this->i2s_comm_fmt_ = fmt; }
  void set_mic_channel_right(bool right) { this->mic_channel_right_ = right; }
  void set_tx_slot_right(bool right) { this->tx_slot_right_ = right; }
  void set_slot_bit_width(uint8_t sbw) { this->slot_bit_width_ = sbw; }

  // AEC setter
  void set_processor(AudioProcessor *aec);
  void set_processor_enabled(bool enabled) { this->processor_enabled_.store(enabled, std::memory_order_relaxed); }
  bool is_processor_enabled() const { return this->processor_enabled_.load(std::memory_order_relaxed); }

  // Volume control (0.0 - 1.0). Atomic: written from main loop, read from audio task.
  void set_mic_gain(float gain) { this->mic_gain_.store(gain, std::memory_order_relaxed); }
  float get_mic_gain() const { return this->mic_gain_.load(std::memory_order_relaxed); }

  // Pre-AEC mic attenuation - for hot mics like ES8311 that overdrive
  void set_mic_attenuation(float atten) { this->mic_attenuation_.store(atten, std::memory_order_relaxed); }
  float get_mic_attenuation() const { return this->mic_attenuation_.load(std::memory_order_relaxed); }
  void set_speaker_volume(float volume) { this->speaker_volume_.store(volume, std::memory_order_relaxed); }
  float get_speaker_volume() const { return this->speaker_volume_.load(std::memory_order_relaxed); }

  // ES8311 Digital Feedback mode: RX is stereo with L=DAC(ref), R=ADC(mic)
  void set_use_stereo_aec_reference(bool use) { this->use_stereo_aec_ref_ = use; }

  // YAML `fir_decimator: custom | dsps_fird_s16` (default dsps_fird_s16).
  // When custom, the FIR decimators run a pure-float scalar kernel instead of
  // the SIMD dsps_fird_s16. Workaround for chips where the SIMD kernel is
  // unreliable (notably ESP32-P4 RISC-V: esp-dsp issues #117/#102 produce
  // rect-wave artifacts, which then propagate quantization noise into esp_aec
  // adaptive filter -> musical noise on the wire). Boards that don't suffer
  // the bug should leave this on the default for the SIMD throughput.
  void set_fir_decimator_custom(bool b) { this->fir_decimator_custom_ = b; }

  // Reference channel selection: false=left (default), true=right
  void set_reference_channel_right(bool right) { this->ref_channel_right_ = right; }

  // TDM hardware reference: ES7210 in TDM mode with one slot carrying DAC feedback
  void set_use_tdm_reference(bool use) { this->use_tdm_ref_ = use; }
  void set_tdm_total_slots(uint8_t n) { this->tdm_total_slots_ = n; }
  void set_tdm_mic_slot(uint8_t slot) { this->tdm_mic_slot_ = slot; }
  void set_secondary_tdm_mic_slot(int8_t slot) { this->tdm_second_mic_slot_ = slot; }
  void set_tdm_ref_slot(uint8_t slot) { this->tdm_ref_slot_ = slot; }
  void set_tdm_slot_level_sensor_enabled(uint8_t slot, bool enabled) {
    if (slot < 8) this->tdm_slot_level_sensor_enabled_[slot] = enabled;
  }
  float get_tdm_slot_level_dbfs(uint8_t slot) const {
    if (slot >= 8) return -120.0f;
    return this->tdm_slot_level_dbfs_[slot].load(std::memory_order_relaxed);
  }

  // Microphone interface
  void add_mic_data_callback(MicDataCallback callback) { this->mic_callbacks_.push_back(callback); }
  void add_raw_mic_data_callback(MicDataCallback callback) { this->raw_mic_callbacks_.push_back(callback); }

  // Consumer registry: each consumer (a microphone wrapper, intercom TX path,
  // etc.) registers an opaque token. The audio task gates mic callbacks on
  // has_mic_consumers_. Registration survives an internal stop()+start()
  // sequence (e.g. frame_spec change), so consumers stay connected across
  // reconfigure without having to re-register. Idempotent per token.
  void register_mic_consumer(void *token);
  void unregister_mic_consumer(void *token);
  bool is_mic_running() const { return this->has_mic_consumers_.load(std::memory_order_relaxed); }

  // Speaker interface: data arrives at bus rate (from mixer/resampler)
  size_t play(const uint8_t *data, size_t len, TickType_t ticks_to_wait = portMAX_DELAY);
  void start_speaker();
  void stop_speaker();
  bool is_speaker_running() const { return this->speaker_running_.load(std::memory_order_relaxed); }
  void set_speaker_paused(bool paused) { this->speaker_paused_.store(paused, std::memory_order_relaxed); }
  bool is_speaker_paused() const { return this->speaker_paused_.load(std::memory_order_relaxed); }

  // Full duplex control
  void start();  // Start both mic and speaker
  void stop();   // Stop both

  bool is_running() const { return this->duplex_running_.load(std::memory_order_relaxed); }
  bool has_i2s_error() const { return this->has_i2s_error_.load(std::memory_order_relaxed); }

  // Speaker output callback registration (for mixer pending_playback_frames tracking)
  void add_speaker_output_callback(SpeakerOutputCallback callback) {
    this->speaker_output_callbacks_.push_back(std::move(callback));
  }

  // Getters for platform wrappers
  // get_sample_rate() returns the I2S bus rate (used by speaker for audio_stream_info)
  uint32_t get_sample_rate() const { return this->sample_rate_; }
  // get_output_sample_rate() returns the decimated rate for mic consumers (MWW/AEC/VA/intercom)
  uint32_t get_output_sample_rate() const {
    return this->output_sample_rate_ > 0 ? this->output_sample_rate_ : this->sample_rate_;
  }
  size_t get_speaker_buffer_available() const;
  size_t get_speaker_buffer_size() const;

  // Task configuration (settable from YAML)
  void set_task_priority(uint8_t prio) { this->task_priority_ = prio; }
  void set_task_core(int8_t core) { this->task_core_ = core; }
  void set_task_stack_size(uint32_t size) { this->task_stack_size_ = size; }
  void set_buffers_in_psram(bool psram) { this->buffers_in_psram_ = psram; }
  void set_audio_stack_in_psram(bool psram) { this->audio_stack_in_psram_ = psram; }
  void set_aec_reference_mode(bool use_ring_buffer) { this->aec_use_ring_buffer_ = use_ring_buffer; }
  void set_aec_ref_buffer_ms(uint32_t ms) { this->aec_ref_buffer_ms_ = ms; }
  void set_aec_ref_ring_in_psram(bool psram) { this->aec_ref_ring_in_psram_ = psram; }
  void set_telemetry_log_interval_frames(uint16_t frames) { this->telemetry_log_interval_frames_ = frames; }

 protected:
  bool init_i2s_duplex_();
  void deinit_i2s_();

  static void audio_task(void *param);
  // Top-level task entry: outer loop that spawns one audio_session_ per start()/stop()
  // cycle. Created once in setup(), never destroyed, so subsequent cycles can't trip
  // a FreeRTOS xTaskCreate race on a lingering TCB.
  void audio_task_();
  // One audio session: populate ctx, enter the processing loop, and return when
  // stop() flips duplex_running_ off or the processor's frame_spec changes.
  void audio_session_();

  // Audio task context: groups all buffers, sizes, and per-frame snapshots
  // to avoid long parameter lists in the refactored processing functions.
  struct AudioTaskCtx {
    // ── Invariants (set once at task start) ──
    uint32_t ratio{1};
    uint8_t i2s_bps{2};       // 2 or 4 bytes per I2S sample
    uint8_t num_ch{1};        // TX channels
    bool use_stereo_aec_ref{false};
    bool use_tdm_ref{false};
    bool ref_channel_right{false};
    bool correct_dc_offset{false};
    uint8_t tdm_total_slots{0};
    uint8_t tdm_mic_slot{0};
    int8_t tdm_second_mic_slot{-1};
    uint8_t tdm_ref_slot{0};
    uint8_t processor_mic_channels{1};
    uint32_t processor_spec_revision{0};

    // ── Frame sizing ──
    size_t input_frame_size{0};
    size_t output_frame_size{0};
    size_t bus_frame_size{0};
    size_t input_frame_bytes{0};
    size_t processor_input_frame_bytes{0};
    size_t output_frame_bytes{0};
    size_t bus_frame_bytes{0};
    size_t rx_frame_bytes{0};
    size_t tdm_tx_frame_bytes{0};

    // ── Working buffers (heap-allocated, owned by audio_task_) ──
    // processor_mic_buffer is interleaved when a secondary mic is active
    // (layout: [mic1[i], mic2[i]] for i in [0, rx_frames)).
    // FIR decimation reads rx_buffer with a stride, so no separate
    // deinterleave buffer is kept.
    int16_t *rx_buffer{nullptr};
    int16_t *mic_buffer{nullptr};
    int16_t *processor_mic_buffer{nullptr};
    int16_t *spk_buffer{nullptr};
    int16_t *spk_ref_buffer{nullptr};
    int16_t *tdm_tx_buffer{nullptr};
    int16_t *aec_output{nullptr};

    // ── Loop mutable state ──
    int consecutive_i2s_errors{0};
    int32_t dc_prev_input{0};
    int32_t dc_prev_output{0};
    int32_t dc_prev_input_secondary{0};
    int32_t dc_prev_output_secondary{0};
    const int16_t *processor_input{nullptr};
    int16_t *output_buffer{nullptr};  // points to mic_buffer or aec_output
    size_t current_output_frame_bytes{0};
    size_t current_output_frame_size{0};
    bool mic_separate{false};         // true if mic_buffer != rx_buffer

    // ── Per-iteration snapshots from atomics ──
    float mic_gain{1.0f};
    float mic_attenuation{1.0f};
    float speaker_volume{1.0f};
    bool processor_enabled{false};
    bool processor_ready{false};  // cached: enabled && initialized (avoids virtual call per frame)
    bool speaker_running{false};
    bool speaker_paused{false};
    bool speaker_underrun{false};
    size_t speaker_got{0};  // bytes actually read from speaker ring buffer
    bool mic_running{false};
    uint32_t now_ms{0};
  };

  // Refactored audio processing functions (called from audio_task_ main loop)
  void process_rx_path_(AudioTaskCtx &ctx);
  void process_aec_and_callbacks_(AudioTaskCtx &ctx);
  void process_tx_path_(AudioTaskCtx &ctx);
  void update_tdm_slot_levels_(const AudioTaskCtx &ctx);

  // Fill ctx.spk_ref_buffer for the mono AEC path (ring buffer or previous
  // frame, with zero-fill fallback). TDM and stereo paths pre-fill the buffer
  // during RX deinterleave and must not call this.
  void fill_mono_aec_reference_(AudioTaskCtx &ctx);

  // Pre-allocate audio task working buffers on first entry (worst-case sizing).
  // Buffers persist across internal stop()+start() cycles so the task can
  // re-enter without calling heap_caps_alloc, which fragments SPIRAM over time
  // and causes spurious "Failed to allocate" failures on feature-toggle sequences.
  bool allocate_audio_buffers_(AudioTaskCtx &ctx);

  // Pin configuration
  int lrclk_pin_{-1};
  int bclk_pin_{-1};
  int mclk_pin_{-1};
  int din_pin_{-1};   // Mic data in
  int dout_pin_{-1};  // Speaker data out

  uint32_t sample_rate_{16000};
  uint8_t bits_per_sample_{16};        // I2S bus bit depth: 16 or 32
  bool correct_dc_offset_{false};      // IIR high-pass filter to remove mic DC bias
  uint8_t num_channels_{1};            // Speaker TX channels: 1 (mono) or 2 (stereo)
  bool i2s_mode_secondary_{false};     // false = master (primary), true = slave (secondary)
  bool use_apll_{false};               // Use APLL clock source (ESP32 original only)
  uint8_t i2s_num_{0};                 // I2S port number (0 or 1)
  uint32_t mclk_multiple_{256};        // MCLK multiple: 128, 256, 384, or 512
  uint8_t i2s_comm_fmt_{0};            // 0=philips, 1=msb, 2=pcm_short, 3=pcm_long
  bool mic_channel_right_{false};      // RX mono slot: false=LEFT, true=RIGHT
  bool tx_slot_right_{false};          // TX mono slot: false=LEFT (default), true=RIGHT
  uint8_t slot_bit_width_{0};          // 0 = auto (match bits_per_sample), or 16/24/32
  uint32_t output_sample_rate_{0};     // 0 = use sample_rate_ (no decimation)
  uint32_t decimation_ratio_{1};       // sample_rate_ / output_sample_rate_ (computed in setup)

  // FIR decimators for mic path
  MultiChannelFirDecimator rx_decimator_;  // Multi-channel: TDM/stereo RX path
  FirDecimator mic_decimator_;             // Fallback: mono RX without TDM/stereo
  FirDecimator play_ref_decimator_;     // Mono mode: bus-rate ref from play() decimated in audio_task

  // I2S handles - BOTH created from single channel for duplex
  i2s_chan_handle_t tx_handle_{nullptr};
  i2s_chan_handle_t rx_handle_{nullptr};

  // State
  std::atomic<bool> duplex_running_{false};
  // Cached presence flag read by audio_task_ on the hot path; kept in sync
  // with mic_consumers_ (below) under mic_consumers_mutex_ on register/unregister.
  std::atomic<bool> has_mic_consumers_{false};
  // Opaque consumer tokens; registration survives stop()+start() by construction.
  // Fixed-size array (cap = MAX_LISTENERS = 16) avoids std::vector + std::mutex
  // in this public header, keeping the include footprint minimal.
  void *mic_consumers_[MAX_LISTENERS]{};
  size_t mic_consumer_count_{0};
  SemaphoreHandle_t mic_consumers_mutex_{nullptr};
  std::atomic<bool> speaker_running_{false};
  std::atomic<bool> speaker_paused_{false};
  // audio_task_shutdown_: terminal exit signal (component destruction).
  // audio_task_idle_:     true while the task is parked in its outer wait loop
  //                       (not owning I2S). stop() polls this to know when the
  //                       task has released the inner processing loop.
  std::atomic<bool> audio_task_shutdown_{false};
  std::atomic<bool> audio_task_idle_{true};
  TaskHandle_t audio_task_handle_{nullptr};

  // Cross-thread buffer operation request (main thread -> audio task, avoids concurrent ring buffer access)
  std::atomic<bool> request_speaker_reset_{false};

  // Mic data callbacks
  std::vector<MicDataCallback> mic_callbacks_;       // Post-AEC (for VA/STT)
  std::vector<MicDataCallback> raw_mic_callbacks_;   // Pre-AEC (for MWW)

  // Speaker output callbacks (for mixer pending_playback_frames tracking)
  std::vector<SpeakerOutputCallback> speaker_output_callbacks_;

  // Speaker ring buffer: stores data at bus rate (sample_rate_)
  std::unique_ptr<RingBuffer> speaker_buffer_;
  size_t speaker_buffer_size_{0};  // Actual allocated size (scales with decimation_ratio_)

  // AEC support
  AudioProcessor *processor_{nullptr};
  std::atomic<bool> processor_enabled_{false};  // Runtime toggle (only enabled when processor_ is set)
  int16_t *direct_aec_ref_{nullptr};     // AEC reference from previous TX frame (bus rate, mono mode)
  bool direct_aec_ref_valid_{false};     // True after first TX frame has been saved

  // AEC ring buffer reference (TYPE2-style, for no-codec setups)
  bool aec_use_ring_buffer_{false};      // Config: use ring buffer instead of previous frame
  uint32_t aec_ref_buffer_ms_{80};       // Config: ring buffer size in ms
  std::unique_ptr<RingBuffer> aec_ref_ring_buffer_;  // Ring buffer for AEC ref (bus rate, post-volume)

  // Volume control (atomic: written from main loop, read from audio task via snapshot)
  std::atomic<float> mic_gain_{1.0f};         // 0.0 - 2.0 (1.0 = unity gain, applied AFTER AEC)
  std::atomic<float> mic_attenuation_{1.0f};  // Pre-AEC attenuation for hot mics (0.1 = -20dB, applied BEFORE AEC)
  std::atomic<float> speaker_volume_{1.0f};   // 0.0 - 1.0 (for digital volume, keep 1.0 if codec has hardware volume)
  bool use_stereo_aec_ref_{false}; // ES8311 digital feedback: RX stereo with L=ref, R=mic
  bool fir_decimator_custom_{false}; // YAML fir_decimator: custom (vs default dsps_fird_s16)
  bool ref_channel_right_{false};  // Which channel is AEC reference: false=L, true=R

  // TDM hardware reference (ES7210 in TDM mode)
  bool use_tdm_ref_{false};
  uint8_t tdm_total_slots_{4};
  uint8_t tdm_mic_slot_{0};    // TDM slot index for voice mic
  int8_t tdm_second_mic_slot_{-1};  // Optional second mic slot for dual-mic AFE
  uint8_t tdm_ref_slot_{1};    // TDM slot index for AEC reference
  bool tdm_slot_level_sensor_enabled_[8] = {false};
  std::atomic<float> tdm_slot_level_dbfs_[8] = {};
  uint8_t tdm_slot_level_divider_{0};

  // AEC gating: only run echo canceller while speaker has recent real audio.
  std::atomic<uint32_t> last_speaker_audio_ms_{0};
  static constexpr uint32_t AEC_ACTIVE_TIMEOUT_MS{250};

  // Task configuration (defaults match ESP-IDF audio best practices)
  uint8_t task_priority_{19};     // Above lwIP(18), below WiFi(23)
  int8_t task_core_{0};           // Core 0: canonical Espressif AEC pattern; -1 = unpinned
  uint32_t task_stack_size_{8192};
  bool buffers_in_psram_{false};  // Non-DMA buffers in PSRAM (saves ~15KB internal RAM)
  bool audio_stack_in_psram_{false};  // Audio task stack in PSRAM (saves ~8KB internal RAM)
  bool aec_ref_ring_in_psram_{false};  // AEC reference ring in PSRAM (saves ~3-5 KB internal, costs ~13.6 us/frame Core 0)
  StackType_t *audio_task_stack_{nullptr};  // Owned when audio_stack_in_psram_ is true
  StaticTask_t audio_task_tcb_{};            // Static TCB for the permanent audio task
  uint16_t telemetry_log_interval_frames_{128};

  // Error propagation: set by audio_task_ on persistent I2S failures
  std::atomic<bool> has_i2s_error_{false};

  // Pre-allocated audio task buffers (owned by component, not by ctx).
  // Allocated once on first audio_task_ entry, sized for worst case so the
  // task can re-enter without alloc/free cycles across reconfigures.
  int16_t *prealloc_rx_buffer_{nullptr};
  int16_t *prealloc_mic_buffer_{nullptr};
  int16_t *prealloc_processor_mic_buffer_{nullptr};
  int16_t *prealloc_spk_buffer_{nullptr};
  int16_t *prealloc_spk_ref_buffer_{nullptr};
  int16_t *prealloc_aec_output_{nullptr};
  int16_t *prealloc_tdm_tx_buffer_{nullptr};
  bool audio_buffers_allocated_{false};

};

// Native actions for YAML automations
template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<I2SAudioDuplex> {
 public:
  void play(const Ts &...x) override { this->parent_->start(); }
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<I2SAudioDuplex> {
 public:
  void play(const Ts &...x) override { this->parent_->stop(); }
};

}  // namespace i2s_audio_duplex
}  // namespace esphome

#endif  // USE_ESP32
