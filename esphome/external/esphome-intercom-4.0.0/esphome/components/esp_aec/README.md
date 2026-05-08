# esp_aec

Standalone Espressif AEC (Acoustic Echo Cancellation) wrapper for ESPHome.

## Contents
- [Overview](#overview)
- [When to use `esp_aec` vs `esp_afe`](#when-to-use-esp_aec-vs-esp_afe)
- [Quick start](#quick-start)
- [Configuration options](#configuration-options)
- [AEC modes](#aec-modes)
- [Public C++ API](#public-c-api)
- [Runtime mode switching from Home Assistant](#runtime-mode-switching-from-home-assistant)
- [Threading model](#threading-model)
- [Memory footprint](#memory-footprint)
- [Dependencies](#dependencies)
- [Known constraints](#known-constraints)
- [Troubleshooting](#troubleshooting)

## Overview

Wraps `espressif/esp-sr`'s AEC primitive and exposes it through the `AudioProcessor` interface. Use it when the device only needs echo cancellation on the mic path (typical full-duplex intercom or VA setup) and does not need the wider AFE pipeline that `esp_afe` provides (noise suppression, beamforming, VAD, AGC).

`esp_aec` is the default audio processor in every ready-to-flash YAML under `yamls/full-experience/single-bus/aec/` and `yamls/intercom-only/`.

## When to use `esp_aec` vs `esp_afe`

| Scenario | Pick |
|----------|------|
| Intercom only, single mic, no VA | `esp_aec` (lighter on RAM and CPU) |
| Intercom + VA + Micro Wake Word, single mic | `esp_aec` in `sr_low_cost` mode (preserves spectral features for MWW) |
| Intercom + VA + dual-mic with beamforming | `esp_afe` (BSS only available via AFE) |
| Need noise suppression or AGC on the mic path | `esp_afe` |
| Standalone `intercom_api` without `i2s_audio_duplex` (dual-bus MEMS + amp) | `esp_aec` (the AFE feed/fetch model needs the steady frames that `i2s_audio_duplex` produces) |

Both components implement `AudioProcessor` at the type level, but `esp_afe` is only safely usable behind `i2s_audio_duplex`. See [docs/reference.md](../../../docs/reference.md#audio-processing-components).

## Quick start

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/n-IA-hane/esphome-intercom
      ref: main
    components: [audio_processor, esp_aec, i2s_audio_duplex, intercom_api]

esp_aec:
  id: aec_processor
  sample_rate: 16000
  filter_length: 4
  mode: sr_low_cost

i2s_audio_duplex:
  id: i2s_duplex
  processor_id: aec_processor
  # ... (see i2s_audio_duplex README for the rest)
```

## Configuration options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `id` | ID | required | Component identifier referenced by `i2s_audio_duplex.processor_id` or `intercom_api.processor_id`. |
| `sample_rate` | int | 16000 | Must match the sample rate of the consumer. esp-sr's AEC only accepts 16 kHz frames; the upstream component is expected to decimate from the I²S bus rate when needed. |
| `filter_length` | int | 4 | AEC tail length in frames. Frame size depends on `mode`: **32 ms in SR modes, 16 ms in VOIP modes**. Range 1 to 8. Use **4** with SR modes (full-experience with MWW, ~128 ms tail), **8** with VOIP modes (intercom-only, ~128 ms tail). Higher values exit the esp-sr tested range and can trigger silent calloc failures on cross-engine switches. |
| `mode` | string | `sr_low_cost` | AEC algorithm. Pick the engine to match the use case: **VOIP modes** for intercom-only (residual echo suppressor for human ears), **SR modes** for full-experience with MWW (linear-only, preserves spectral features). Public YAMLs in this repo restrict the runtime select to a single engine per tier — see "Runtime mode switching" below. |

## AEC modes

esp-sr ships two completely different AEC engines:

| Mode | Engine | CPU on Core 0 (S3 @ 240 MHz, 16 kHz mono) | RES | MWW on post-AEC | Recommended |
|------|--------|--------------------------------------|-----|-----------------|-------------|
| `sr_low_cost` | `esp_aec3` linear | **~22 %** | No | **10/10** | Default for VA + MWW setups |
| `sr_high_perf` | `esp_aec3` FFT | ~25 % | No | 10/10 | Only when contiguous DMA-capable internal RAM is available |
| `voip_low_cost` | `dios_ssp_aec` Speex | ~58 % | Yes | 2/10 | Intercom-only, mild echo, low CPU budget |
| `voip_high_perf` | `dios_ssp_aec` | ~64 % | Yes | 2/10 | **Default for intercom-only** (with `filter_length: 8` for 128 ms tail) |

Why `sr_*` is recommended for VA + MWW: the SR engines use a linear-only adaptive filter that preserves the spectral features the wake-word neural model relies on. The VOIP engines add a residual echo suppressor (RES) that distorts those features and drops MWW detection rate from 10/10 to 2/10 in our hardware tests.

`sr_high_perf` allocates a contiguous DMA-capable internal block at switch time. The component runs a pre-flight heap check and refuses the switch cleanly (logs a warning, keeps the previous mode active) if the block is not available.

## Public C++ API

| Method | Purpose |
|--------|---------|
| `setup()` / `dump_config()` / `get_setup_priority()` | Standard ESPHome component lifecycle. |
| `bool is_initialized() const` | True when the esp-sr AEC handle is ready. |
| `FrameSpec frame_spec() const` | Frame size and channel layout that `process()` expects. |
| `bool process(mic, ref, out, mic_channels)` | Run one AEC frame. Returns false if the handle is not ready. |
| `FeatureControl feature_control(AudioFeature)` | `AEC` is `RESTART_REQUIRED`; everything else is `NOT_SUPPORTED`. |
| `bool set_feature(AudioFeature, bool enabled)` | Toggle AEC (rebuilds the handle, ~70 ms gap). |
| `ProcessorTelemetry telemetry() const` | Frame count and ring-buffer free percent for diagnostics. |
| `bool reconfigure(int type, int mode)` | Switch AEC mode by numeric code. `type` selects the engine (0 = SR / `esp_aec3`, non-zero = VC / `dios_ssp_aec`); `mode` selects the variant (0 = LOW_COST, 1 = HIGH_PERF). Combinations: (0,0) `sr_low_cost`, (0,1) `sr_high_perf`, (1,0) `voip_low_cost`, (1,1) `voip_high_perf`. Prefer `reinit_by_name` from lambdas. |
| `bool reinit_by_name(const std::string &name)` | Switch AEC mode by name (`"sr_low_cost"` etc.). Recommended entry point. Returns false on rejection (for example, when `sr_high_perf` cannot allocate DMA). |
| `std::string get_mode_name() const` | Current mode as a string. Read this after `reinit_by_name` to confirm the switch was accepted. |

## Runtime mode switching from Home Assistant

The `AEC Mode` select wires runtime modes to a Home Assistant select entity, with the device publishing back the live mode so a rejected switch never leaves HA showing the wrong value.

**Engine standard**: stay inside one engine per device tier to avoid esp-sr's silent FFT calloc-fail bug on cross-engine transitions at `filter_length > 4`. Public YAMLs restrict the options accordingly.

Intercom-only (no MWW) — VOIP engine only:

```yaml
select:
  - platform: template
    id: aec_mode_select
    name: "AEC Mode"
    options:
      - "voip_low_cost"
      - "voip_high_perf"
    initial_option: "voip_high_perf"
    optimistic: false
    restore_value: false
    set_action:
      - lambda: 'id(aec_processor).reinit_by_name(x);'
      - lambda: 'id(aec_mode_select).publish_state(id(aec_processor).get_mode_name());'
```

Full-experience with MWW — SR engine only:

```yaml
select:
  - platform: template
    id: aec_mode_select
    name: "AEC Mode"
    options:
      - "sr_low_cost"
      - "sr_high_perf"
    initial_option: "sr_low_cost"
    optimistic: false
    restore_value: false
    set_action:
      - lambda: 'id(aec_processor).reinit_by_name(x);'
      - lambda: 'id(aec_mode_select).publish_state(id(aec_processor).get_mode_name());'
```

`optimistic: false` matters: without it, `template_select::control()` auto-publishes the user-selected value after the action runs, overwriting any rejection (e.g. when `sr_high_perf` cannot allocate the contiguous DMA block).

## Threading model

None. `esp_aec::process()` runs synchronously on the caller's audio task. The caller (typically `i2s_audio_duplex`'s audio task on Core 0) owns the realtime thread. There are no internal worker tasks, no FreeRTOS objects beyond a mutex around mode-switch reinit.

## Memory footprint

| Item | Internal RAM | PSRAM |
|------|--------------|-------|
| Component overhead (handle, config) | ~4 KB | n/a |
| Working buffers (esp-sr managed, prefers PSRAM) | minimal | ~40 KB |
| `sr_high_perf` extra DMA block | ~40 KB contiguous internal | n/a |

For comparison, `esp_afe` in MR LOW_COST mode costs ~72 KB internal + ~733 KB PSRAM, and in MMR (2-mic + BSS) ~77 KB + ~1.2 MB.

## Dependencies

- ESP32 only, restricted to S3 and P4 variants (enforced in `_validate_esp32_variant`).
- Pulls `espressif/esp-sr ^2.3.0` via ESPHome's IDF component manager.
- Implements [`AudioProcessor`](../audio_processor/README.md), so it can be referenced by any component that accepts that interface.

## Known constraints

- Sample rate is fixed at 16 kHz (the rate esp-sr's AEC expects). When the I²S bus runs faster, the upstream component must decimate; `i2s_audio_duplex` does this with a 31-tap Kaiser FIR.
- Mode changes (`sr_low_cost` vs `sr_high_perf` vs `voip_*`) require a handle rebuild, which causes a ~70 ms audio gap. Do not change mode while a call is streaming; the AEC select wraps this with state guards in the ready-to-flash YAMLs.
- `filter_length` is compile-time-sized but runtime-mutable. Longer filters give better echo-tail coverage at the cost of CPU.
- The `sr_high_perf` mode needs a contiguous DMA-capable internal allocation. On a fragmented heap the pre-flight check refuses the switch and logs a warning; the device keeps running on the previous mode.

## Troubleshooting

**Echo cancellation does nothing.**
Make sure the consumer (typically `i2s_audio_duplex` or `intercom_api`) actually links `processor_id: aec_processor`. The component initialises silently even when nobody references it.

**Wake word detection rate dropped after enabling AEC.**
You are likely on a `voip_*` mode. Switch to `sr_low_cost`. The VOIP engines apply a residual echo suppressor that distorts the features the MWW model expects.

**`sr_high_perf` switch fails at runtime.**
The pre-flight check on contiguous DMA-capable internal RAM rejected the switch. Free internal heap by enabling `buffers_in_psram: true` on `i2s_audio_duplex`, or stay on `sr_low_cost`. Check `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)` in the logs.

**AEC mode select in Home Assistant shows the wrong value after a switch attempt.**
You are missing `optimistic: false` on the template select. Without it, the template auto-publishes the user-selected value over the live mode the device actually applied.
