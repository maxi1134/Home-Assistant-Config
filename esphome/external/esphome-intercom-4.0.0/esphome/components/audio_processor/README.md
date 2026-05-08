# audio_processor

Shared C++ interface for audio-processor components. Not a user-facing component on its own: it only exposes the headers that `esp_aec`, `esp_afe`, `i2s_audio_duplex` and `intercom_api` consume.

## Contents
- [Purpose](#purpose)
- [Why an interface](#why-an-interface)
- [YAML usage](#yaml-usage)
- [The `AudioProcessor` contract](#the-audioprocessor-contract)
- [Frame spec and revision](#frame-spec-and-revision)
- [Feature controls](#feature-controls)
- [Telemetry](#telemetry)
- [Ring-buffer helpers](#ring-buffer-helpers)
- [Implementing a custom processor](#implementing-a-custom-processor)
- [Threading model](#threading-model)
- [Dependencies](#dependencies)
- [Known constraints](#known-constraints)

## Purpose

Defines the `AudioProcessor` virtual base class and the value types used to describe a processing frame (`FrameSpec`), feature toggles (`AudioFeature`, `FeatureControl`) and telemetry (`ProcessorTelemetry`). Consumers reference the interface, never the concrete implementation, so swapping `esp_aec` for `esp_afe` in a device YAML does not require code changes in `i2s_audio_duplex` or `intercom_api` (subject to the runtime compatibility caveats covered below).

Also ships `ring_buffer_caps.h`, a small helper that creates ring buffers with an explicit memory-placement policy (internal RAM vs PSRAM). Audio hot-path buffers should always be allocated through the helper so placement is auditable at boot.

## Why an interface

The audio stack supports four topologies:

| Topology | Producer | Processor |
|----------|----------|-----------|
| Intercom only, dual-bus | ESPHome `microphone` + `intercom_api` mic capture | `esp_aec` |
| Intercom only, single-bus | `i2s_audio_duplex` | `esp_aec` |
| Full intercom + VA, single-bus AEC only | `i2s_audio_duplex` | `esp_aec` |
| Full intercom + VA, single-bus full AFE | `i2s_audio_duplex` | `esp_afe` |

The `AudioProcessor` contract lets the same consumers (`i2s_audio_duplex`, `intercom_api`) work with any of these processors at YAML level, and lets future processors drop in without touching consumer code. It is also unit-testable: the contract can be mocked without bringing up DMA.

Note: `esp_afe` only works behind `i2s_audio_duplex`. Its feed/fetch task model needs a steady producer that pushes fixed 512-sample 16 kHz frames. The standalone `intercom_api` mic path does not provide that, so the `esp_aec` + `intercom_api` combination is the only supported one for dual-bus setups. The interface is wider than the supported configurations.

## YAML usage

`audio_processor` has no config of its own. Include it in any device YAML alongside the concrete processor:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/n-IA-hane/esphome-intercom
      ref: main
    components: [audio_processor, esp_aec, i2s_audio_duplex, intercom_api]
```

You never instantiate `audio_processor:` directly in a YAML. Listing it in `external_components` makes its headers available to the other components.

## The `AudioProcessor` contract

```cpp
namespace esphome::audio_processor {

class AudioProcessor {
 public:
  // Stable identity of the frames this processor produces.
  virtual FrameSpec frame_spec() const = 0;

  // Monotonic counter. Bumps whenever frame_spec() changes
  // (e.g. SE on or off flips mic_channels between 2 and 1).
  virtual uint32_t frame_spec_revision() const = 0;

  // Process one frame. mic and ref are the input buffers, out is
  // the destination. mic_channels is 1 or 2. Returns false if the
  // processor is paused (config swap in flight) or not ready.
  virtual bool process(const int16_t *mic, const int16_t *ref,
                       int16_t *out, int mic_channels) = 0;

  // Optional capability discovery. NOT_SUPPORTED features can be
  // set without effect; consumers that care should check first.
  virtual FeatureControl feature_control(AudioFeature) const = 0;
  virtual bool set_feature(AudioFeature, bool enabled) = 0;

  // Diagnostic snapshot, safe to read from any thread.
  virtual ProcessorTelemetry telemetry() const = 0;
};

}  // namespace esphome::audio_processor
```

Invariants the processor promises:
1. `frame_spec()` is stable between `frame_spec_revision()` bumps.
2. `process()` is call-from-any-task safe but not concurrent-safe. The caller serialises calls.
3. The output frame shape always matches `frame_spec()` after the revision bump has been observed.

Invariants the caller must respect:
1. Observe `frame_spec_revision()` before reading `frame_spec()` each cycle.
2. Never call `process()` concurrently from multiple tasks.
3. When `frame_spec` changes, internal buffers (decimator ratio, reference extraction, ring sizes) must be recomputed before the next call.

`i2s_audio_duplex` implements this with a permanent audio task that detects revision bumps at the top of each iteration and reinitialises its local buffers in place, without recreating the FreeRTOS task.

## Frame spec and revision

```cpp
struct FrameSpec {
  uint32_t sample_rate;     // 16000 in practice
  uint8_t  mic_channels;    // 1 or 2 (BSS)
  uint8_t  ref_channels;    // 0 or 1
  size_t   input_samples;   // per-channel samples expected by process()
  size_t   output_samples;  // per-channel samples produced by process()
};
```

`frame_spec_revision()` is a `std::atomic<uint32_t>` bump every time any of those fields changes. Consumers cache the spec and re-read it only when the revision changes, avoiding atomic reads on the hot path.

For `esp_aec` the revision is constant after `setup()`. For `esp_afe` it bumps when SE (beamforming) is toggled, because `mic_channels` flips between 1 and 2.

## Feature controls

```cpp
enum class AudioFeature { AEC, NS, VAD, AGC, SE };

enum class FeatureControl {
  NOT_SUPPORTED,    // set_feature() ignored
  BOOT_ONLY,        // configured at YAML, fixed at runtime
  RESTART_REQUIRED, // toggling tears the processor down and rebuilds it
  LIVE_TOGGLE,      // safe to toggle while audio is flowing
};
```

`esp_aec` reports `AEC = RESTART_REQUIRED` and everything else `NOT_SUPPORTED`.

`esp_afe` reports `AEC = LIVE_TOGGLE` (handle stays initialised, the engine is enabled or disabled via the esp-sr vtable in microseconds). `NS / VAD / AGC = RESTART_REQUIRED` (each toggle rebuilds the AFE handle, ~70 ms gap). `SE` is `RESTART_REQUIRED` only when the device has at least 2 mic channels, otherwise `NOT_SUPPORTED`.

## Telemetry

```cpp
struct ProcessorTelemetry {
  // Audio activity (esp_afe only — esp_aec leaves these at defaults)
  bool  voice_present;            // VAD raw state
  float input_volume_dbfs;        // Mic RMS, dBFS
  float output_rms_dbfs;          // Post-process RMS, dBFS
  float ringbuf_free_pct;         // 0.0 to 1.0
  // Aggregate counters
  uint32_t frame_count;           // Frames processed since boot
  uint32_t glitch_count;          // Passthrough fallbacks (fetch starvation)
  // Feed/fetch ring statistics (esp_afe two-task pipeline)
  uint32_t feed_ok, feed_rejected;
  uint32_t fetch_ok, fetch_timeout;
  uint32_t input_ring_drop, output_ring_drop;
  uint32_t feed_queue_frames, feed_queue_peak;
  uint32_t fetch_queue_frames, fetch_queue_peak;
  // Per-stage timing (microseconds)
  uint32_t process_us_last, process_us_max;
  uint32_t feed_us_last,    feed_us_max;
  uint32_t fetch_us_last,   fetch_us_max;
  // Stack pressure
  uint32_t feed_stack_high_water, fetch_stack_high_water;
};
```

Read with `processor->telemetry()` from any thread. Implementations write each field via atomics (relaxed ordering); the snapshot is racy across fields but each field is consistent. Used by the diagnostic sensors (`Frame count`, `Ring free %`, `Process µs max`, etc.) that `esp_afe` exposes; `esp_aec` leaves most fields at default.

## Ring-buffer helpers

`ring_buffer_caps.h` wraps `RingBuffer::create` with explicit placement.

| Helper | When to use |
|--------|-------------|
| `create_internal(len, name)` | Audio hot-path ring buffers that must never incur PSRAM bus contention (the I²S RX and TX buffers, AEC reference). |
| `create_prefer_psram(len, name)` | Large non-realtime buffers; tries PSRAM first, falls back to internal. Examples: AFE feed input ring, fetch output ring. |
| `create_psram(len, name)` | PSRAM only; fails if PSRAM is absent. Rarely needed. |

The helpers log placement at boot (`Ring buffer 'aec_ref' allocated in INTERNAL RAM, 4096 bytes`), so a quick `esphome logs` reveals the actual layout without rebuilding.

## Implementing a custom processor

If you want to ship your own processor (DSP frontend, alternative library), subclass `AudioProcessor` in a new ESPHome external component:

1. Create `esphome/components/my_processor/` with `__init__.py`, `my_processor.h` and `my_processor.cpp`.
2. Inherit `Component` and `AudioProcessor`. Implement the six pure virtual methods plus `setup()` / `dump_config()` / `get_setup_priority()`.
3. Choose your frame spec at `setup()` time. Bump `frame_spec_revision_` whenever it changes (use a `std::atomic<uint32_t>`).
4. Decide your threading model. The processor can run `process()` synchronously on the caller's task (like `esp_aec`), or use background tasks with a drain protocol (like `esp_afe`, see [docs/ARCHITECTURE.md](../../../docs/ARCHITECTURE.md#5-drain-protocol-config-change-without-stopping-the-task)).
5. Use `audio_processor::create_internal` / `create_prefer_psram` for ring buffers so placement is consistent with the rest of the stack.
6. List your component in the consumer YAML's `external_components`. The consumer references it by id with `processor_id:`, no consumer code changes.

## Threading model

None directly: the component only defines interfaces. Each implementation owns its own task topology:

- `esp_aec`: no background tasks; `process()` runs synchronously on the caller's audio task.
- `esp_afe`: two tasks (`feed_task`, `fetch_task`) coordinating with `process()` via a lock-free drain handshake.

## Dependencies

ESP32 only. `ring_buffer_caps.cpp` links against `heap_caps_aligned_alloc` from esp-idf and `RingBuffer` from ESPHome core.

## Known constraints

- Frame sample counts must match between producer (the processor) and consumer (`i2s_audio_duplex` or `intercom_api`). Consumers read `frame_spec_revision()` at init and poll in their audio loop; on change they restart to re-read `frame_spec()`.
- Placement policy is advisory: ESPHome's default `RingBuffer::create` falls back between pools silently. Prefer the `ring_buffer_caps.h` helpers whenever placement matters.
- `esp_afe` is type-compatible with `intercom_api.processor_id` but not behaviourally compatible when `intercom_api` runs without `i2s_audio_duplex` in front. The AFE feed/fetch tasks need a steady producer at fixed-size frames.
