# ESP AFE - Full Audio Front-End Pipeline

> ⚠ **Important: `esp_afe` requires `i2s_audio_duplex` in front of it.** It is **not** a drop-in alternative to `esp_aec` for `intercom_api` standalone setups (dual-bus MEMS + amp without a codec). The AFE pipeline expects fixed 512-sample 16 kHz frames at a steady cadence, which only `i2s_audio_duplex` produces. If you use `intercom_api` without `i2s_audio_duplex`, set `processor_id:` to an `esp_aec` component, not `esp_afe`. See [docs/reference.md](../../../docs/reference.md#audio-processing-components) for the full topology matrix.

ESPHome component wrapping Espressif's **ESP-SR AFE** (Audio Front End) framework. Provides a complete audio processing pipeline (**AEC + spatial source separation (BSS) + Noise Suppression + VAD + AGC**) in a single component, with runtime control switches and diagnostic sensors. Supports single-mic (MR) and dual-mic (MMR) configurations.

## Overview

`esp_afe` uses the closed-source `esp-sr` library's AFE pipeline, which chains multiple DSP stages depending on configuration:

**Single-mic (MR) mode** (`se_enabled: false` or `mic_num: 1`):
```
[mic + ref] -> |AEC| -> |NS| -> |VAD| -> |AGC| -> [clean output]
```

**Dual-mic (MMR) mode** (`se_enabled: true` with `mic_num: 2`):
```
[mic1 + mic2 + ref] -> |AEC| -> |SE(BSS)| -> [clean output]
```

> **Note**: When SE/BSS is active, esp-sr replaces NS and AGC with spatial source separation. BSS suppresses directional noise better than NS but does not normalize volume (no AGC). NS and AGC switches have no effect while SE is active.

Unlike `esp_aec` (standalone echo cancellation only), `esp_afe` provides a full signal processing pipeline. Both components implement the `AudioProcessor` interface, but they are **only** drop-in replacements behind `i2s_audio_duplex`. With standalone `intercom_api` (no duplex driver), use `esp_aec`: the AFE feed/fetch task model needs the steady producer that `i2s_audio_duplex` provides and that the standalone intercom path does not.

### When to use esp_afe vs esp_aec

| Feature | esp_aec | esp_afe |
|---------|---------|---------|
| Echo Cancellation | Yes | Yes |
| Spatial voice isolation (BSS) | No | Yes (dual-mic) |
| Noise Suppression | No | Yes (WebRTC, single-mic mode) |
| Voice Activity Detection | No | Yes (WebRTC) |
| Automatic Gain Control | No | Yes (WebRTC, single-mic mode) |
| Runtime switches in HA | AEC only | AEC, SE, NS, VAD, AGC |
| Diagnostic sensors | No | Input volume, output RMS, voice presence |
| CPU usage (SR LOW_COST) | ~22% Core 0 | ~23% Core 0 (8.4% feed + 15% fetch) |
| Internal RAM overhead | ~80 KB | ~100 KB (MR), ~120 KB (MMR with BSS) |
| Supported platforms | ESP32-S3, ESP32-P4 | ESP32-S3, ESP32-P4 |

**Choose `esp_aec`** when you need minimal RAM usage and only echo cancellation.
**Choose `esp_afe`** when you want noise suppression, VAD, AGC, and diagnostic sensors.

## Requirements

- **ESP32-S3** or **ESP32-P4** with PSRAM
- ESP-IDF framework
- `i2s_audio_duplex` in front of the processor. `intercom_api` can use the
  processed mic only when it sits behind the duplex driver.

## Installation

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/n-IA-hane/esphome-intercom
      ref: main
    components: [audio_processor, i2s_audio_duplex, esp_afe]
```

> **Note**: `audio_processor` must be listed in `components:` because it provides the shared `AudioProcessor` interface header. It is also auto-loaded by `esp_afe`, but ESPHome's `external_components` loader requires it to be explicitly listed.

## Configuration

### Basic Setup

```yaml
esp_afe:
  id: afe_processor
  type: sr
  mode: low_cost

i2s_audio_duplex:
  id: i2s_duplex
  # ... pins ...
  processor_id: afe_processor
```

### Full Configuration

```yaml
esp_afe:
  id: afe_processor
  type: sr                    # sr (speech recognition) or vc (voice communication)
  mode: low_cost              # low_cost or high_perf
  mic_num: 2                  # Number of microphones (1 or 2)
  se_enabled: true            # BSS spatial voice isolation, requires mic_num: 2
  aec_enabled: true           # Echo cancellation
  aec_filter_length: 4        # Echo tail in frames (4 = 64ms)
  ns_enabled: true            # Noise suppression (WebRTC)
  vad_enabled: false          # Voice activity detection
  vad_mode: 3                 # VAD aggressiveness (0-4, higher = more aggressive)
  vad_min_speech_ms: 128      # Min speech duration to trigger VAD
  vad_min_noise_ms: 1000      # Min noise duration before VAD clears
  vad_delay_ms: 128           # VAD state transition delay
  agc_enabled: true           # Automatic gain control (WebRTC)
  agc_compression_gain: 9     # AGC compression gain (0-30 dB)
  agc_target_level: 3         # AGC target level (0-31, lower = louder)
  memory_alloc_mode: more_psram  # Memory allocation strategy
  afe_linear_gain: 1.0        # Linear gain applied to output (0.1-10.0)
  task_core: 1                # FreeRTOS task core (0 or 1)
  task_priority: 8            # FreeRTOS task priority (1-24, default 8)
  ringbuf_size: 8             # Internal ring buffer size in frames (default 8)
```

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `id` | ID | Required | Component ID |
| `type` | string | `sr` | AFE type: `sr` (speech recognition, linear AEC) or `vc` (voice communication, nonlinear AEC) |
| `mode` | string | `low_cost` | AFE mode: `low_cost` or `high_perf` |
| `mic_num` | int | `1` | Number of microphones (1 or 2). Set to 2 for dual-mic BSS voice isolation with `se_enabled: true` |
| `aec_enabled` | bool | **true** | Enable acoustic echo cancellation |
| `aec_filter_length` | int | `4` | AEC filter length in frames (1-8). 4 = 64ms tail, sufficient for most setups |
| `ns_enabled` | bool | **true** | Enable noise suppression (WebRTC engine) |
| `agc_enabled` | bool | **true** | Enable automatic gain control (WebRTC engine) |
| `se_enabled` | bool | **false** | Enable spatial source separation (BSS). Requires `mic_num: 2`. Replaces NS and AGC with spatial source separation |
| `vad_enabled` | bool | **false** | Enable voice activity detection |
| `vad_mode` | int | `3` | VAD aggressiveness (0-4). Higher = rejects more noise but may miss quiet speech |
| `vad_min_speech_ms` | int | `128` | Minimum speech duration to trigger voice detection (32-60000 ms) |
| `vad_min_noise_ms` | int | `1000` | Minimum noise duration before VAD clears (64-60000 ms) |
| `vad_delay_ms` | int | `128` | VAD state transition delay (0-60000 ms) |
| `vad_mute_playback` | bool | `false` | When VAD detects speech, mute the speaker output to prevent acoustic feedback during voice commands. Useful for voice-assistant pipelines that play TTS while still listening. |
| `vad_enable_channel_trigger` | bool | `false` | Per-channel VAD triggering (multi-mic setups). esp-sr exposes which mic channel detected the speech, useful for beamforming-aware downstream consumers. |
| `agc_compression_gain` | int | `9` | AGC compression gain in dB (0-30) |
| `agc_target_level` | int | `3` | AGC target level (0-31, lower value = louder output) |
| `memory_alloc_mode` | string | `more_psram` | Memory allocation: `more_internal`, `internal_psram_balance`, `more_psram` |
| `afe_linear_gain` | float | `1.0` | Linear gain multiplier applied to output (0.1-10.0) |
| `task_core` | int | `1` | FreeRTOS task core affinity (0 or 1) |
| `task_priority` | int | `8` | FreeRTOS task priority (1-24) |
| `ringbuf_size` | int | `8` | Internal ring buffer size in frames (2-32). Larger = more latency tolerance, more memory |
| `feed_buf_in_psram` | bool | `false` | Place the ~3 KB sample-interleave scratch buffer in PSRAM. Default internal saves ~41 us/frame on Core 0 (the buffer is written and re-read every audio frame). Set `true` on memory-constrained builds to free internal RAM at the cost of Core 0 PSRAM traffic. |
| `feed_ring_in_psram` | bool | `false` | Place the ~12 KB feed staging ring (Core 0 → feed_task) in PSRAM. Default internal saves ~20 us/frame on Core 0 writes. Set `true` if internal RAM headroom is tight. |
| `fetch_ring_in_psram` | bool | `false` | Place the ~4 KB fetch output ring (fetch_task → Core 0) in PSRAM. Default internal saves ~6.8 us/frame on Core 0 reads. Set `true` if internal RAM headroom is tight. |

> **Buffer placement guidance**: defaults are tuned for fastest Core 0 audio path. Total internal cost when all three flags are `false` is ~19 KB. On S3/P4 builds where AFE/MWW/LVGL/TLS compete for internal RAM, set `feed_buf_in_psram`, `feed_ring_in_psram` and `fetch_ring_in_psram` to `true` (the YAMLs in `yamls/full-experience/single-bus/afe/` ship with all three set to `true`). Each flag is independent so you can fine-tune. Cumulative Core 0 cost when all `true` is ~68 us/frame on Octal PSRAM 80 MHz, lower bound.

> **Defaults are designed so that a minimal config already enables AEC + NS + AGC.** You only need to declare options that differ from the defaults. In particular:
> - `aec_enabled`, `ns_enabled`, `agc_enabled` are **true** by default. Only set them if you want to **disable** a feature.
> - `se_enabled` and `vad_enabled` are **false** by default. Set them to `true` to opt in.
> - `memory_alloc_mode` defaults to `more_psram`, `task_core` to `1`, `task_priority` to `8`. Override only if your hardware requires it.
>
> **Minimal single-mic** (AEC + NS + AGC out of the box):
> ```yaml
> esp_afe:
>   id: afe_processor
>   type: sr
>   mode: low_cost
> ```
>
> **Minimal dual-mic** (adds BSS voice isolation):
> ```yaml
> esp_afe:
>   id: afe_processor
>   type: sr
>   mode: low_cost
>   mic_num: 2
>   se_enabled: true
> ```
>
> Everything else (AEC, NS, AGC, memory, task settings) uses sensible defaults and does not need to be repeated.

### AFE Type and Mode

The combination of `type` and `mode` determines the AEC engine and DSP pipeline:

| type + mode | AEC Engine | CPU (Core 0) | MWW Compatible | Use Case |
|-------------|-----------|-------------|----------------|----------|
| `sr` + `low_cost` | `esp_aec3` (linear, SIMD) | **~23%** | **Yes** (10/10) | VA + MWW + Intercom |
| `sr` + `high_perf` | `esp_aec3` (FFT) | ~25% | Yes | Not recommended (DMA memory on S3) |
| `vc` + `low_cost` | `dios_ssp_aec` (Speex) | ~60% | No (2/10) | VoIP without wake word |
| `vc` + `high_perf` | `dios_ssp_aec` | ~64% | No | VoIP without wake word |

> **Important**: Use `sr` + `low_cost` for Voice Assistant + MWW setups. The `vc` modes add a residual echo suppressor (RES) that distorts spectral features, reducing MWW detection from 10/10 to 2/10.

NS and AGC always use the WebRTC engine regardless of type/mode. They work at boot but cannot be toggled individually at runtime via the AFE vtable (see [Feature Toggle Behavior](#feature-toggle-behavior)).

## Platform Entities

### Switch Platform

Runtime control of AFE features via Home Assistant switches:

```yaml
switch:
  - platform: esp_afe
    esp_afe_id: afe_processor
    aec:
      name: "Echo Cancellation"
      restore_mode: RESTORE_DEFAULT_ON
    se:
      name: "Beamforming"
      restore_mode: RESTORE_DEFAULT_ON
    ns:
      name: "Noise Suppression"
      restore_mode: RESTORE_DEFAULT_ON
    vad:
      name: "Voice Detection"
      restore_mode: RESTORE_DEFAULT_OFF
    agc:
      name: "Auto Gain Control"
      restore_mode: RESTORE_DEFAULT_ON
```

| Switch | Icon | Description |
|--------|------|-------------|
| `aec` | `mdi:ear-hearing` | Echo cancellation toggle (live, no audio gap) |
| `se` | `mdi:microphone-variant` | Beamforming toggle (requires AFE reinit, ~70ms gap). Only available with `mic_num: 2` |
| `ns` | `mdi:volume-off` | Noise suppression toggle (requires AFE reinit, ~70ms gap). No effect when SE is active |
| `vad` | `mdi:account-voice` | Voice activity detection toggle (requires AFE reinit) |
| `agc` | `mdi:tune-vertical` | Auto gain control toggle (requires AFE reinit). No effect when SE is active |

### Binary Sensor Platform

```yaml
binary_sensor:
  - platform: esp_afe
    esp_afe_id: afe_processor
    vad:
      name: "Voice Presence"
      update_interval: 100ms
```

| Sensor | Device Class | Description |
|--------|-------------|-------------|
| `vad` | `sound` | Voice activity state. ON when speech detected, OFF when noise/silence. Requires `vad_enabled: true` in config |

### Sensor Platform

```yaml
sensor:
  - platform: esp_afe
    esp_afe_id: afe_processor
    input_volume:
      name: "Input Volume"
      update_interval: 250ms
    output_rms:
      name: "Output RMS"
      update_interval: 250ms
```

| Sensor | Unit | Description |
|--------|------|-------------|
| `input_volume` | dBFS | RMS level of mic input before processing. Useful for mic gain calibration |
| `output_rms` | dBFS | RMS level of processed output. Compare with input to see NS/AGC effect |

## Actions

### esp_afe.set_mode

Change the AFE type and mode at runtime. The entire AFE pipeline is recreated (~70 ms gap).

```yaml
select:
  - platform: template
    id: afe_mode_select
    name: "AFE Mode"
    options:
      - sr_low_cost
      - sr_high_perf
      - voip_low_cost
      - voip_high_perf
    initial_option: "sr_low_cost"
    optimistic: false           # do NOT auto-publish; we publish the live mode below
    restore_value: false
    set_action:
      - esp_afe.set_mode:
          id: afe_processor
          mode: !lambda 'return x;'
      - lambda: 'id(afe_mode_select).publish_state(id(afe_processor).get_mode_name());'
```

Valid mode strings: `sr_low_cost`, `sr_high_perf`, `voip_low_cost`, `voip_high_perf`.

`get_mode_name()` returns the live mode as a string after the reinit. The `optimistic: false` plus the explicit `publish_state()` at the end is the recommended pattern: it stops `template_select::control()` from auto-publishing the user-selected value over a rejected switch (e.g. when `sr_high_perf` cannot allocate the contiguous DMA-capable internal block).

## Feature Toggle Behavior

AEC and NS/AGC/VAD toggle differently due to ESP-SR internals:

| Feature | Toggle Method | Audio Gap | Notes |
|---------|-------------|-----------|-------|
| AEC | Live vtable call | None | Immediate on/off via `afe_enable_aec()` / `afe_disable_aec()` |
| SE | AFE reinit | ~70ms | Toggles between MMR (beamforming) and MR (single-mic) pipeline |
| NS | AFE reinit | ~70ms | WebRTC NS doesn't support runtime vtable toggle. Full AFE destroy + recreate. No effect when SE is active |
| AGC | AFE reinit | ~70ms | Same as NS. No effect when SE is active |
| VAD | AFE reinit | ~70ms | Same as NS |

**Why reinit?** The WebRTC NS, AGC, and VAD engines are controlled internally via `webrtc_process(data, enable_ns, enable_agc)`. The enable/disable vtable entries only exist for neural model backends (NSNet, VADNet), not for WebRTC. Toggling these features requires destroying and rebuilding the entire AFE instance with the changed config flags.

The reinit is safe: the old AFE is destroyed first (ESP-SR's FFT resources are a global singleton, only one instance can exist), then the new one is built. If the build fails, audio falls through to passthrough mode.

## Architecture

```
               AudioProcessor interface
                       |
              +--------+--------+
              |                 |
           EspAec            EspAfe
         (AEC only)    (AEC+NS+VAD+AGC)
              |                 |
              +--------+--------+
                       |
            +----------+-----------+
            |                      |
    i2s_audio_duplex         intercom_api
    (processor_id)         (processor_id)
```

Both `EspAec` and `EspAfe` implement `AudioProcessor`. The consumer components (`i2s_audio_duplex` and `intercom_api`) call `process(mic, ref, out)` without knowing which implementation is behind it. The supported pairings are:

| Consumer | esp_aec | esp_afe |
|----------|---------|---------|
| `i2s_audio_duplex` | yes | yes |
| `intercom_api` standalone (no `i2s_audio_duplex`) | yes | **no** (the AFE feed/fetch tasks need the steady frames `i2s_audio_duplex` produces) |

### Internal Pipeline

```
feed() ----> AFE internal tasks ----> fetch()
  |           (FreeRTOS, Core 1)         |
  |                                      |
  mic + ref interleaved             clean mono output
  (512 samples * 2 channels)        (512 samples)
```

The AFE framework runs its own internal FreeRTOS tasks for feed/fetch. `process()` bridges synchronously: it feeds the interleaved mic+ref buffer, then fetches the processed output. A mutex protects concurrent access during reinit.

## Complete Example

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/n-IA-hane/esphome-intercom
      ref: main
    components: [audio_processor, intercom_api, i2s_audio_duplex, esp_afe]

esp_afe:
  id: afe_processor
  type: sr
  mode: low_cost
  mic_num: 2                  # 2 for dual-mic BSS voice isolation (default: 1)
  se_enabled: true            # BSS voice isolation (requires mic_num: 2, default: false)
  vad_enabled: true           # Voice activity detection (default: false)
  # aec_enabled, ns_enabled, agc_enabled are true by default - no need to repeat

i2s_audio_duplex:
  id: i2s_duplex
  # ... I2S pins ...
  processor_id: afe_processor
  buffers_in_psram: true

# AFE switches
switch:
  - platform: esp_afe
    esp_afe_id: afe_processor
    aec:
      name: "Echo Cancellation"
    se:
      name: "Beamforming"
    ns:
      name: "Noise Suppression"
    vad:
      name: "Voice Detection"
    agc:
      name: "Auto Gain Control"

# AFE diagnostic sensors
sensor:
  - platform: esp_afe
    esp_afe_id: afe_processor
    input_volume:
      name: "Input Volume"
    output_rms:
      name: "Output RMS"

# VAD binary sensor
binary_sensor:
  - platform: esp_afe
    esp_afe_id: afe_processor
    vad:
      name: "Voice Presence"

# Runtime mode switching
select:
  - platform: template
    name: "AFE Mode"
    options:
      - sr_low_cost
      - sr_high_perf
    initial_option: sr_low_cost
    set_action:
      - esp_afe.set_mode:
          id: afe_processor
          mode: !lambda 'return x;'
```

## Memory Usage

| Component | Internal RAM | PSRAM | Notes |
|-----------|-------------|-------|-------|
| AFE framework | ~55-70 KB | Variable | Closed-source esp-sr allocations, cannot be moved to PSRAM |
| SE/BSS (beamforming) | ~15-25 KB | Variable | Only allocated when `se_enabled: true` with `mic_num: 2` |
| AEC filter | ~20-30 KB | ~60 KB | Depends on filter_length and mode |
| NS (WebRTC) | ~10-15 KB | ~10 KB | Allocated even when SE replaces it at runtime |
| AGC (WebRTC) | ~2-3 KB | ~3 KB | |
| Feed buffer | - | ~2-6 KB | 512-1024 * 2-3 channels * 2 bytes (PSRAM first) |
| Ring buffers | - | Variable | ringbuf_size * frame_size |

With `memory_alloc_mode: more_psram`, total internal RAM usage is approximately:
- **~100 KB** for MR mode (single-mic: AEC + NS + AGC)
- **~120 KB** for MMR mode (dual-mic: AEC + BSS beamforming)
- **~80 KB** for standalone `esp_aec`

### IRAM Optimization (Critical for ESP32-S3)

On ESP32-S3, IRAM and DRAM share the same 512 KB of SRAM. Every KB of code placed in IRAM reduces available DRAM heap by 1 KB. With `CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y`, code runs from the PSRAM instruction cache, making IRAM placement unnecessary for most functions.

**Add these sdkconfig options to free ~30 KB of internal DRAM:**

```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_SPIRAM_FETCH_INSTRUCTIONS: "y"  # prerequisite
      CONFIG_SPIRAM_RODATA: "y"              # prerequisite
      CONFIG_ESP_WIFI_IRAM_OPT: "n"          # ~10 KB freed
      CONFIG_ESP_WIFI_RX_IRAM_OPT: "n"       # ~17 KB freed
      CONFIG_ESP_PHY_IRAM_OPT: "n"           # ~3.5 KB freed
```

Without these options, the AFE + TLS media streaming will exhaust internal RAM and cause `esp-aes: Failed to allocate memory` errors or heap corruption crashes.

> **Tip**: Monitor free internal RAM with a template sensor using `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`. If it drops below ~25 KB, WiFi/TLS stability will suffer.

## Known Limitations

1. **SE replaces NS and AGC**: When beamforming (BSS) is active, esp-sr uses the pipeline `AEC -> SE(BSS) -> output`, skipping NS and AGC entirely. BSS provides spatial noise suppression but does not normalize volume. Disabling SE at runtime switches to the full `AEC -> NS -> AGC -> output` pipeline.

2. **NS/AGC/VAD/SE runtime toggle**: Requires full AFE reinit (~70ms audio gap). Only AEC can be toggled without interruption.

3. **data_volume**: The AFE's built-in `data_volume` field is always 0.0 dB because it requires WakeNet to be active. Input/output RMS is computed locally instead.

4. **ESP-SR is closed source**: No API to move AFE internal allocations to PSRAM. The ~55-86 KB internal RAM overhead is unavoidable. Use the IRAM optimization options above to compensate.

5. **Single instance**: ESP-SR's FFT resources are a global singleton. Only one AFE (or AEC) instance can exist at a time.

## Troubleshooting

### AFE setup fails (NULL config)

Check logs for `afe_config_init returned NULL`. This means esp-sr couldn't initialize with the requested type/mode. Verify your `type` and `mode` combination is valid.

### High internal RAM usage / esp-aes: Failed to allocate memory

With AFE active, free internal RAM may drop to ~15 KB without IRAM optimization. This causes TLS failures (`esp-aes: Failed to allocate memory`) and WiFi heap corruption crashes. Solutions in order of impact:

1. **Apply IRAM optimization** (see [IRAM Optimization](#iram-optimization-critical-for-esp32-s3)) - frees ~30 KB
2. Set `memory_alloc_mode: more_psram`
3. Set `se_enabled: false` if beamforming is not needed (frees ~17 KB)
4. Reduce `ringbuf_size` (minimum 2, default 8)
5. Consider using `esp_aec` instead if you don't need NS/AGC/VAD/SE

### Switch toggle has no effect

NS, AGC, and VAD toggles require AFE reinit. Check logs for `Reinit requested` and `Reinit complete`. If reinit fails, the old AFE is destroyed and audio falls to passthrough.

### Voice presence always OFF

Ensure `vad_enabled: true` in the `esp_afe` config. VAD is disabled by default.

### Audio gap when toggling NS/AGC/VAD

This is expected (~70ms). Only AEC can be toggled without audio interruption.

## License

MIT License
