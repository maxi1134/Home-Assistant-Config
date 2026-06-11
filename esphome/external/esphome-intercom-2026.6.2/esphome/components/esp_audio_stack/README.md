# ESP Audio Stack - Full-Duplex Audio Backend for ESPHome

`esp_audio_stack` is the shared audio backend used by the maintained full voice
and intercom profiles in this repository. It keeps the normal ESPHome
`microphone`, `speaker`, media player, mixer, Voice Assistant and Micro Wake
Word facade, but moves low-level audio ownership to the ESP-IDF and Espressif
audio libraries: `esp_driver_i2s`, `esp_codec_dev`, `gmf_io`, `esp_audio_effects`
and, through optional processors, ESP-SR/GMF AFE.

The component is not an official Espressif product. It is a repo-native ESPHome
component that integrates Espressif libraries so board YAMLs can cover shared
codec buses, no-codec MEMS/amp builds, dual I2S buses, stereo speaker output,
hardware and software AEC references, and full AFE processors without each
profile reimplementing bus ownership.

## What It Solves

ESPHome's normal audio components are excellent when microphone and speaker are
independent devices. They are not enough for every full-duplex voice target:

- codec boards often put ADC and DAC on the same I2S bus;
- AEC needs a frame-aligned speaker reference, not an unrelated playback stream;
- VA, MWW, intercom and media playback need to share the same mic/speaker safely;
- dual-mic AFE and codec feedback paths need fixed layout/rate conversion before
  ESPHome consumers see audio.

`esp_audio_stack` owns that lower layer and exposes a normal ESPHome surface
above it.

```
Hardware / codec / I2S
        ↓
esp_audio_stack: I2S, codec IO, rate/layout conversion, speaker reference
        ↓
optional AudioProcessor: esp_aec or esp_afe
        ↓
ESPHome microphone + speaker surfaces
        ↓
MWW, Voice Assistant, media_player, mixer, intercom_api, custom components
```

## Quick Map

| Use case | Recommended shape |
|---|---|
| ES8311/ES8388/ES8374/ES8389 codec board | Single-bus `esp_audio_stack.codec` with shared STD I2S bus |
| ES7210 ADC + ES8311 DAC board | TDM mic slots plus optional `use_tdm_reference` |
| INMP441 + MAX98357A on one bus | Single-bus STD I2S, `slot_bit_width: 32`, software AEC reference |
| INMP441 + MAX98357A on two buses | Dual-bus `rx_bus` + `tx_bus`, `rx_slot_mode: stereo` if the mic is strapped to a fixed stereo slot |
| Voice Assistant + MWW + intercom | `esp_audio_stack` + `esp_aec` or `esp_afe`, then normal ESPHome consumers |
| Full AFE board | `esp_audio_stack` feeding `esp_afe`; MWW remains ESPHome/TFLite |
| Standalone audio backend, no intercom | `esp_audio_stack` alone, optionally with `esp_aec` / `esp_afe` |

## Capabilities

- **True full duplex**: simultaneous RX and TX on one shared I2S bus, or on two
  separate ESP-IDF simplex controllers via `rx_bus` / `tx_bus`.
- **ESPHome facade preserved**: exposes standard `microphone` and `speaker`
  platforms, so existing ESPHome VA/MWW/media/mixer components can consume it.
- **Codec backend**: codec boards use `esp_codec_dev`; codec payload IO can run
  through synchronous `gmf_io/io_codec_dev` or optional GMF data-bus/task knobs.
- **No-codec backend**: discrete MEMS microphones and I2S amplifiers use direct
  `esp_driver_i2s` read/write without codec shims.
- **Audio processors**: `processor_id` can point at `esp_aec` for lightweight
  echo cancellation or `esp_afe` for GMF/ESP-SR AFE processing.
- **Post-processor mic output**: MWW, VA and intercom receive one stable
  processed microphone stream. If a configured processor is unavailable, output
  is silenced rather than silently falling back to raw mic audio.
- **AEC reference options**: software ring buffer, previous-frame reference,
  ES8311 stereo digital feedback, or ES7210 TDM analog feedback.
- **Multi-rate operation**: the bus can run at 48 kHz for better codec/DAC
  behavior while mic/ref/processor output runs at 16 kHz through
  `esp_ae_rate_cvt`.
- **Layout conversion**: 32-bit MEMS mic samples, stereo slot selection, TDM
  slot extraction, stereo speaker output, mono duplication and TX bit expansion
  use `esp_audio_effects` primitives.
- **Stereo speaker output**: `speaker_channels: 2` exposes a real two-channel
  ESPHome speaker and writes interleaved L/R PCM to STD stereo TX.
- **Runtime controls**: persistent master volume, post-processor mic gain,
  AEC enable switch, state hooks and optional telemetry.
- **PSRAM controls**: buffers, AEC reference ring and task stacks can be placed
  in PSRAM where it is safe; DMA-critical pieces stay internal.
- **Compile-time pruning**: dual-bus, TDM, stereo reference, ring reference,
  stereo TX, telemetry and codec paths are compiled only when YAML needs them.

## Important Knobs

Most boards should start from a maintained YAML and only change pins, codec type
and gain. These are the knobs users most often need when building a new target:

| Knob | Why it matters |
|---|---|
| `sample_rate` / `output_sample_rate` | Run codec/speaker at the bus rate while feeding mic/AEC/VA at the processor rate. |
| `codec.input` / `codec.output` | Select `es7210`, `es8311`, `es8388`, `es8374` or `es8389` through `esp_codec_dev`. |
| `rx_bus` / `tx_bus` | Use separate I2S controllers for discrete mic and amplifier. |
| `bits_per_sample` / `slot_bit_width` | Required for 24/32-bit codecs and 32-bit MEMS microphones. |
| `mic_channel` / `rx_slot_mode` | Select the actual MEMS mic slot. `rx_slot_mode: stereo` reads both STD slots and then selects `mic_channel` in software. |
| `num_channels` / `speaker_channels` / `tx_channel` | Separate physical TX bus layout from public mono/stereo speaker behavior. |
| `processor_id` | Attach `esp_aec` or `esp_afe`. Omit it for raw full-duplex audio. |
| `aec_reference` / `aec_reference_buffer_ms` | Tune software AEC reference storage for no-codec boards. |
| `use_stereo_aec_reference` / `reference_channel` | Use ES8311 digital DAC feedback as a sample-aligned reference. |
| `use_tdm_reference` / `tdm_*` | Use ES7210 TDM mic/ref/tx slots. |
| `input_gain` | Board-level pre-processor gain staging. |
| `mic_gain` number | User-facing post-processor mic trim, `-20..30 dB`. |
| `master_volume_min_db` | Tune perceived loudness curve while keeping 0% as hard mute. |
| `buffers_in_psram` / `aec_ref_ring_in_psram` / `audio_task_stack_in_psram` | Save internal RAM on large full profiles. |
| `audio_effects.*` | Expose official `esp_ae_rate_cvt` complexity/performance policy. |
| `gmf_io.reader.*` / `gmf_io.writer.*` | Enable optional GMF codec IO buffering/task controls. |

## Topology Notes

### Shared Codec Bus

Use one I2S bus when the codec owns ADC and DAC on the same pins. The stack
creates official IDF TX/RX channels and opens codec-dev data devices on top.

### Dual I2S Bus

Use `rx_bus` and `tx_bus` when the mic and amp have independent clocks. The ESP
should normally stay I2S primary on both buses. On INMP441-style modules, strap
`L/R` to the wanted slot and set:

```yaml
esp_audio_stack:
  mic_channel: right   # or left
  rx_slot_mode: stereo # read both STD slots, then pick mic_channel
```

This is intentionally different from `use_stereo_aec_reference`: stereo slot
mode is only for mic slot selection; the AEC reference still comes from the
speaker software reference path.

### Processor Choice

Use `esp_aec` for the light path: one mic plus one reference, lower memory and
clear modes. Use `esp_afe` when you need the full ESP-SR AFE pipeline:
dual-mic Speech Enhancement/BSS, NS, VAD or AGC. Wake word handling remains
ESPHome Micro Wake Word.

## Architecture

```mermaid
flowchart TD
    RX["🎛️ I2S RX"] --> Stereo["🎚️ Stereo codec<br/>L=mic, R=ref"]
    RX --> TDM["🎚️ TDM codec<br/>mic slots + ref slot"]
    RX --> Mono["🎙️ Mono mic<br/>ref from speaker history"]

    Stereo --> Mic["🎙️ mic frame"]
    Stereo --> Ref["🔁 speaker reference"]
    TDM --> Mic
    TDM --> Ref
    Mono --> Mic

    Mic --> Atten["🎚️ input gain<br/>optional"]
    Atten --> Raw["🧪 raw callbacks<br/>pre-processing diagnostics"]
    Atten --> Processor["🧠 audio_processor.process<br/>esp_aec or esp_afe"]

    Ref --> RefBuf["📦 spk_ref_buffer"]
    RefBuf --> AecRef["🔁 AEC reference"]
    AecRef --> Processor

    Raw --> Diag["📊 diagnostics"]

    Processor --> Post["🎙️ processed mic callbacks"]
    Post --> VA["🗣️ Voice Assistant"]
    Post --> ICT["📞 Intercom TX"]
    Post --> MWW["👂 Micro Wake Word"]

    Mixer["🔊 mixer<br/>VA TTS + Intercom RX"] --> SPK["📦 speaker buffer"]
    SPK --> VOL["🎚️ volume scaling"]
    VOL --> TX["🎛️ I2S TX"]
    VOL --> DirectRef["🔁 software AEC ref<br/>previous_frame mode"]
    RingBuf["📦 AEC ref ring buffer<br/>mono ring_buffer"] -.-> AecRef
```

### Task Layout

| Task | Core | Priority | Role |
|------|------|----------|------|
| `audio_stack` (audio_task) | **Core 0** | **19** | I2S read/write + rate conversion + audio processor (esp_aec/esp_afe) |
| `intercom_tx` | Core 0 | 5 | Only when `intercom_api` is present: mic to network |
| `intercom_spk` | Core 0 | 4 | Only when standalone `intercom_api.processor_id` is used |
| `intercom_srv` | Core 1 | 5 | Only when `intercom_api` is present: TCP RX, call FSM |
| `mixer` (ESPHome) | Any | 10 | Mix VA + intercom audio to speaker |
| `MWW inference` (ESPHome) | Unpinned | 3→**8** | Wake word TFLite inference (boost via on_boot lambda) |
| ESPHome main loop / LVGL | Core 1 | 1 | Switches, sensors, display, etc. |
| WiFi driver (ESP-IDF) | Core 0 | 23 | System; can briefly preempt audio_task |

**Core allocation rationale:**
- **Core 0**: Real-time audio (I2S + AEC). WiFi (prio 23) briefly preempts for sub-ms bursts.
- **Core 1**: MWW inference + mixer + LVGL + ESPHome main loop. MWW priority boosted to 8 (above resampler/loopTask, below mixer) for reliable barge-in during TTS.

**CPU budget** (sr_low_cost: 512 samples @ 16kHz = 32ms per frame):
- Without AEC: ~300us processing (< 2% of a core)
- With SR AEC active: ~22% of Core 0 (vs ~58% with VOIP mode)
- IDLE headroom: Core 0 ~70%, Core 1 ~73% (measured with music + MWW + VA)

## Requirements

- **ESP32**, **ESP32-S3**, or **ESP32-P4**. The component has been exercised on
  S3 and P4, including RISC-V dual-core P4 with 32 MB PSRAM; the shipped P4
  product presets remain experimental while board-specific SDIO/LVGL/runtime
  issues are investigated. Single-core SoCs (C3, C5, C6, H2, S2) are supported
  but cannot pin `task_core` to Core 1.
- AEC requires PSRAM (S3/P4). TDM requires `SOC_I2S_SUPPORTS_TDM` (S3, P4, C3, C5, C6, H2).
- Audio codec with shared I2S bus (ES8311 recommended), or discrete I2S mic + amp on the same bus
- ESP-IDF framework

## Installation

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/n-IA-hane/esphome-intercom
      ref: main
    components: [audio_processor, esp_audio_stack, esp_aec]
    # Or with esp_afe (full AFE pipeline: AEC + NS + VAD + AGC):
    # components: [audio_processor, esp_audio_stack, esp_afe]
```

`intercom_api` is not required. Add it only when the device is also an
intercom endpoint:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/n-IA-hane/esphome-intercom
      ref: main
    components: [audio_processor, esp_audio_stack, esp_aec, intercom_api]
```

If the device also uses ESPHome's `speaker` media player for HA media/TTS
playback, full-experience profiles should include the project-local `speaker`
fork too:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/n-IA-hane/esphome-intercom
      ref: main
    components: [audio_processor, esp_audio_stack, esp_aec, intercom_api, speaker]
```

That fork does not change I2S ownership. It adds the
`pause_releases_pipeline` media-player mode documented in
[`../speaker/README.md`](../speaker/README.md), so paused media does not keep a
pipeline around while Voice Assistant TTS, timer alarms or intercom audio need
the same mixer/output graph.

### Component Boundaries

`esp_audio_stack` owns the shared I2S bus, codec/data backend, rate conversion,
AEC reference extraction, microphone output and speaker input. It loads
Espressif `esp_codec_dev` and `esp_audio_effects` internally because those are
part of the bus backend.

The component does not load or require `intercom_api`. When `intercom_api` is
also present, it is just another consumer of the microphone and speaker
surfaces. Compile-time validation prevents both components from owning the same
audio processor or DC-offset stage, but that validation is conditional and does
not create a dependency.

`esp_aec` and `esp_afe` are optional `AudioProcessor` providers:

- `esp_aec` can be called by `esp_audio_stack` or by standalone `intercom_api`
  on separate mic/speaker hardware.
- `esp_afe` should be called by `esp_audio_stack`, because the AFE manager
  expects steady 16 kHz frames and stable mic/reference timing.

## Configuration

### Basic Setup

```yaml
esp_audio_stack:
  id: audio_stack
  i2s_lrclk_pin: GPIO45      # Word Select (WS/LRCLK)
  i2s_bclk_pin: GPIO9        # Bit Clock (BCK/BCLK)
  i2s_mclk_pin: GPIO16       # Master Clock (optional, some codecs need it)
  i2s_din_pin: GPIO10        # Data In (from codec ADC → ESP mic)
  i2s_dout_pin: GPIO8        # Data Out (from ESP → codec DAC speaker)
  sample_rate: 16000

microphone:
  - platform: esp_audio_stack
    id: mic_component
    esp_audio_stack_id: audio_stack

speaker:
  - platform: esp_audio_stack
    id: spk_component
    esp_audio_stack_id: audio_stack
```

### Dual-Bus Codec-Less Setup

Use `rx_bus` and `tx_bus` when the microphone and amplifier are wired to
different ESP32 I2S controllers, for example INMP441 plus MAX98357A on separate
BCLK/LRCLK pairs. This follows ESP-IDF simplex channel allocation: one RX
channel on the mic port and one TX channel on the speaker port. The ESP should
normally stay I2S primary on both buses so it owns both clocks.

Dual-bus support is compile-time gated. If `rx_bus` and `tx_bus` are absent, the
generated build does not define `USE_ESP_AUDIO_STACK_DUAL_BUS` and the C++
split-data-interface path is not compiled.

```yaml
esp_audio_stack:
  id: audio_stack
  rx_bus:
    i2s_num: 0
    i2s_lrclk_pin: GPIO37
    i2s_bclk_pin: GPIO36
    i2s_din_pin: GPIO35
  tx_bus:
    i2s_num: 1
    i2s_lrclk_pin: GPIO6
    i2s_bclk_pin: GPIO5
    i2s_dout_pin: GPIO7
  sample_rate: 48000
  output_sample_rate: 16000
  slot_bit_width: 32
  correct_dc_offset: true
  processor_id: aec_processor
```

First-version limits:

- `rx_bus` and `tx_bus` must be configured together and must use different
  `i2s_num` values.
- Dual-bus mode is standard I2S only. TDM reference remains a shared-bus codec
  topology.
- Put `i2s_lrclk_pin`, `i2s_bclk_pin`, `i2s_din_pin` and `i2s_dout_pin` inside
  `rx_bus` or `tx_bus`. Do not mix top-level bus pins with split-bus pins.
- For physically separated mic/speaker clocks, start no-codec AEC tests with
  `esp_aec.filter_length: 8`; this gives the adaptive filter a longer delay
  tail without adding custom DSP.

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `id` | ID | Required | Component ID |
| `i2s_lrclk_pin` | pin | Required | Word Select / LR Clock pin |
| `i2s_bclk_pin` | pin | Required | Bit Clock pin |
| `i2s_mclk_pin` | pin | -1 | Master Clock pin (if codec requires) |
| `i2s_din_pin` | pin | -1 | Data input from codec (microphone) |
| `i2s_dout_pin` | pin | -1 | Data output to codec (speaker) |
| `rx_bus` | object | - | Optional dual-bus RX configuration with `i2s_num`, `i2s_lrclk_pin`, `i2s_bclk_pin`, optional `i2s_mclk_pin`, and `i2s_din_pin`. Requires `tx_bus`. |
| `tx_bus` | object | - | Optional dual-bus TX configuration with `i2s_num`, `i2s_lrclk_pin`, `i2s_bclk_pin`, optional `i2s_mclk_pin`, and `i2s_dout_pin`. Requires `rx_bus`. |
| `sample_rate` | int | 16000 | I2S bus sample rate (8000-48000) |
| `output_sample_rate` | int | - | Mic/AEC output rate. If set, enables sample-rate conversion (must divide `sample_rate` evenly, max ratio 6) |
| `audio_effects.rate_cvt_complexity` | int | 3 | Official `esp_ae_rate_cvt` complexity knob, 1-3. Higher is better quality and more CPU. |
| `audio_effects.rate_cvt_perf_type` | string | `speed` | Official `esp_ae_rate_cvt` performance policy: `speed` maps to `ESP_AE_RATE_CVT_PERF_TYPE_SPEED`, `memory` maps to `ESP_AE_RATE_CVT_PERF_TYPE_MEMORY`. |
| `gmf_io.reader.io_size` | int | 0 | Official `io_codec_dev` read transfer size. `0` keeps Espressif's default synchronous IO in the audio task. |
| `gmf_io.reader.buffer_size` | int | 0 | Official GMF reader data-bus buffer size. Non-zero with a reader task enables buffered GMF IO. |
| `gmf_io.reader.task_stack_size` | int | 0 | Official GMF reader task stack. `0` disables the extra IO task. |
| `gmf_io.reader.task_priority` | int | 0 | Official GMF reader task priority when a reader task is enabled. |
| `gmf_io.reader.task_core` | int | 0 | Official GMF reader task core, 0 or 1 depending on SoC. |
| `gmf_io.reader.task_stack_in_psram` | bool | false | Place the GMF reader task stack in PSRAM. Enables `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY`. |
| `gmf_io.reader.speed_monitor` | bool | false | Enable GMF IO speed statistics for the reader. |
| `gmf_io.reader.task_timeout_ms` | int | 0 | Optional GMF IO task close/control timeout override in milliseconds. |
| `gmf_io.writer.*` | same as reader | same | Same official `io_codec_dev` knobs for the TX writer side. |
| `processor_id` | ID | - | Reference to audio processor component (`esp_aec` or `esp_afe`) for echo cancellation and audio processing |
| `input_gain` | float | 1.0 | Input gain before the processor (0.01-32.0). <1.0 attenuates hot mics, >1.0 amplifies weak mics. Keep this as board-level tuning; normal user-facing volume should be handled by the post-AEC/AFE `mic_gain` number. |
| `master_volume_min_db` | float | - | Optional 1% master-volume floor in dB (-96..0). Omit it to keep codec-dev's native curve on hardware codecs; set it to tune board UX. No-codec software volume defaults to ESPHome's -49 dB curve. |
| `slot_bit_width` | int | auto | I2S slot width in bits (16 or 32). Set to 32 for MEMS mics without codec (INMP441, MSM261, SPH0645). |
| `correct_dc_offset` | bool | false | Enable DC offset removal. Required for MEMS mics without built-in HPF (MSM261, SPH0645). |
| `mic_channel` | string | `left` | Which STD slot carries the microphone: `left` or `right`. In mono RX mode this becomes the IDF slot mask. With `rx_slot_mode: stereo`, both STD slots are read and this selects the slot in software. |
| `rx_slot_mode` | string | `mono` | `mono` reads only `mic_channel`. `stereo` reads both STD RX slots and then selects `mic_channel`; useful for MEMS mics strapped to L/R where the wire behaves better as a full stereo frame. This is not an AEC reference mode. |
| `use_stereo_aec_reference` | bool | false | ES8311 digital feedback mode (see below) |
| `reference_channel` | string | left | Which stereo channel carries AEC reference: `left` or `right` |
| `use_tdm_reference` | bool | false | TDM hardware reference mode (ES7210, see below) |
| `tdm_total_slots` | int | 4 | Number of TDM slots (2-8) |
| `tdm_mic_slot` | int | 0 | Single TDM slot index for the voice microphone. Use `tdm_mic_slots` instead for dual-mic Speech Enhancement. |
| `tdm_mic_slots` | list of int | - | List of 1 or 2 TDM slot indices for dual-mic configurations (e.g. ES7210 capturing two MEMS mics on slots 0 and 2). Mutually exclusive with `tdm_mic_slot`. |
| `tdm_ref_slot` | int | 1 | TDM slot index for AEC reference (e.g. MIC3 capturing DAC output) |
| `tdm_tx_slot` | int | 0 | TDM slot index where speaker TX PCM is written. Useful for boards/codecs that expect playback on a non-zero TDM slot. |
| `task_priority` | int | 19 | FreeRTOS priority of the audio task (1-24). Default 19 is above lwIP (18), below WiFi (23). |
| `task_core` | int | 0 | Core affinity: 0 or 1 for pinned, -1 for unpinned. Default 0 keeps the non-network realtime I2S bridge below Wi-Fi and above lwIP on the ESP-IDF protocol core. |
| `task_stack_size` | int | 8192 | Audio task stack size in bytes (4096-32768). Increase if you see stack overflow warnings. |
| `buffers_in_psram` | bool | false | Move component-owned frame buffers (RX scratch, speaker frame scratch, processor interleave, mic/ref/output buffers) to PSRAM where possible. DMA descriptors and I2S driver buffers remain internal. Saves internal heap on full builds at the cost of PSRAM traffic. |
| `audio_task_stack_in_psram` | bool | false | Place the audio task's own stack in PSRAM. Saves ~8KB of DMA-capable internal RAM at the cost of slower function return paths. Only enable on boards that need the internal headroom. With GMF-backed `esp_afe`, heavy esp-sr work runs in Espressif manager tasks, so PSRAM stack latency is acceptable here. Requires `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y` (already default on our YAMLs). Leave it `false` on any board that does not hit the "not enough internal RAM" boundary; normal lifecycle is identical to the non-opt-in path. |
| `aec_reference` | string | `ring_buffer` | Mono-mode AEC reference source for no-codec setups. `ring_buffer` is the Espressif/ADF TYPE2-style software reference: speaker TX is staged in a delay-tunable ring before being fed to the processor. `previous_frame` is a lighter custom mode that reuses the prior TX frame, with no ring buffer and no delay tuning. Ignored when `use_stereo_aec_reference` or `use_tdm_reference` is true. |
| `aec_reference_buffer_ms` | int | 80 | Capacity of the AEC reference ring buffer in milliseconds (32 to 500). Only used with `aec_reference: ring_buffer`. Larger values absorb more producer/consumer jitter at the cost of latency. |
| `aec_ref_ring_in_psram` | bool | false | Place the AEC reference ring buffer storage (~3-5 KB) in PSRAM. Default `false` keeps it in internal RAM, saving ~13.6 us/frame on Core 0 (the audio task reads and writes the ring every frame). Set `true` to save internal RAM at the cost of Core 0 PSRAM traffic. Has no effect when `aec_reference: previous_frame` or when `use_stereo_aec_reference` / `use_tdm_reference` is set, since the ring path is not compiled for those modes. |

### I2S Bus Advanced Options

These options expose the underlying I2S driver controls. Defaults are tuned for the supported codecs (ES8311, ES7210); change only when matching a specific hardware quirk or non-standard codec.

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `bits_per_sample` | int | 16 | Sample bit depth on the wire (16, 24, or 32). Must match the codec's PCM format. |
| `num_channels` | int | 1 | Physical TX bus channel count on STD I2S: 1 or 2. Full-duplex codec-reference profiles often use `2` even while the public speaker stream remains mono. |
| `speaker_channels` | int | 1 | Public ESPHome speaker stream channels: `1` mono default, `2` true interleaved stereo. Requires `num_channels: 2` on STD I2S. TDM profiles always expose mono speaker input and place it in `tdm_tx_slot`. |
| `mic_channel` | string | `left` | Same as above; listed here because it maps directly to the IDF STD slot layout. |
| `tx_channel` | string | `left` | Which stereo channel the speaker writes to: `left` or `right`. |
| `i2s_mode` | string | `primary` | I2S role: `primary` (ESP drives the clocks, default) or `secondary` (codec drives the clocks). |
| `use_apll` | bool | false | Use the APLL clock source for cleaner audio at non-multiple-of-8 kHz rates. Costs APLL availability for other peripherals. |
| `i2s_num` | int | 0 | Which I2S peripheral port to use (0-2 depending on SoC). Lets you reserve a port when other components also use I2S. |
| `mclk_multiple` | int | 256 | MCLK to LRCLK ratio (128, 256, 384, or 512). Most codecs accept 256. |
| `i2s_comm_fmt` | string | `philips` | I2S frame format: `philips` (default), `msb`, `pcm_short`, `pcm_long`. Codec datasheets specify which one. |
| `telemetry` | bool | false | Enable per-stage cycle counting and diagnostic logging. Adds a small overhead in the audio task; only enable while tuning. |
| `telemetry_log_interval_frames` | int | 128 | When `telemetry: true`, log the snapshot every N audio frames (1-8192). Defaults to 128 frames = ~4 s at 16 kHz / 32 ms frames. |

### Microphone Options

The public `microphone: platform: esp_audio_stack` entry is intentionally thin
and always exposes the post-processor stream. Raw/pre-AEC audio is not a
standard microphone output path for MWW, VA or intercom:

```yaml
microphone:
  - platform: esp_audio_stack
    id: mic_main
    esp_audio_stack_id: audio_stack
```

It mirrors ESPHome's microphone layer for actions, data callbacks, mute state,
and consumer compatibility. The hardware format belongs to the shared
full-duplex bus, so put mic hardware settings on `esp_audio_stack`, not on the
`microphone` child:

| Upstream `i2s_audio.microphone` option | `esp_audio_stack` equivalent |
|----------------------------------------|-------------------------------|
| `i2s_din_pin` | `esp_audio_stack.i2s_din_pin` |
| `sample_rate` | `esp_audio_stack.sample_rate` for the bus, plus `output_sample_rate` for mic/AEC/VA consumers |
| `bits_per_sample` | `esp_audio_stack.bits_per_sample` |
| `channel` | `esp_audio_stack.mic_channel` |
| `i2s_mode` | `esp_audio_stack.i2s_mode` |
| `use_apll` | `esp_audio_stack.use_apll` |
| `mclk_multiple` | `esp_audio_stack.mclk_multiple` |
| `correct_dc_offset` | `esp_audio_stack.correct_dc_offset` |
| `pdm` | Not supported by the full-duplex single-bus path; use ESPHome's native PDM microphone on a separate input path |

Setting `sample_rate`, `bits_per_sample`, or `num_channels` on the child
microphone is rejected at configuration time because those values would not
change the shared I2S bus.

### Speaker Options

The public `speaker: platform: esp_audio_stack` entry follows the parent audio
stack format. With `esp_audio_stack.speaker_channels: 1` it accepts mono PCM.
With `esp_audio_stack.num_channels: 2` and `speaker_channels: 2` it exposes a
two-channel ESPHome speaker and
expects standard interleaved L/R 16-bit PCM at the bus sample rate:

```yaml
esp_audio_stack:
  id: audio_stack
  sample_rate: 48000
  num_channels: 2
  speaker_channels: 2

speaker:
  - platform: esp_audio_stack
    id: hw_speaker
    esp_audio_stack_id: audio_stack
```

For no-codec or codec software-reference AEC, stereo TX is downmixed to mono for
the AEC reference with Espressif `esp_ae_ch_cvt` before rate conversion. This
keeps the public speaker stereo while preserving the mono `AudioProcessor`
contract used by `esp_aec` and `esp_afe`.

### Codec Options

Codec-backed builds use `esp_codec_dev` for register control, mute, volume,
input gain and data-device ownership. ES7210 remains input-only because it is a
multi-channel ADC. The other listed codecs can be used as ADC, DAC, or both when
the board wiring and codec datasheet match the selected I2S format:

```yaml
esp_audio_stack:
  id: audio_stack
  codec:
    i2c_id: bus_a
    input:
      type: es8388
      address: 0x10
      gain_db: 24
    output:
      type: es8388
      address: 0x10
```

| Codec type | Input | Output | Notes |
|------------|-------|--------|-------|
| `es7210` | yes | no | Multi-mic ADC, supports `mic_selected`, `ref_channel` and per-channel reference gain. |
| `es8311` | yes | yes | Single ADC/DAC codec, supports `use_mclk` and `no_dac_ref`; used by Spotpear and P4/WS3 DAC output. |
| `es8388` | yes | yes | Stereo-capable codec through `esp_codec_dev`; board analog routing still decides which channels are useful. |
| `es8374` | yes | yes | Codec-dev supported ADC/DAC codec. |
| `es8389` | yes | yes | Codec-dev supported ADC/DAC codec; `use_mclk` and `no_dac_ref` are passed through. |

The schema intentionally does not expose codec-specific private register writes.
If a board needs an analog mux, PA, or PGA quirk, keep that in a board package or
a codec-dev supported option, not as a hidden generic stack behavior.

### Rate Conversion Backend

Mic/ref rate conversion, 32-bit to 16-bit conversion, channel deinterleave and TX interleave/bit expansion are handled by Espressif `esp_audio_effects` primitives. This branch intentionally does not keep a selectable in-tree conversion backend.

| Primitive | Used for |
|-----------|----------|
| `esp_ae_rate_cvt` | Bus-rate mic/reference frames to processor rate. Multi-channel RX uses one handle so mic/ref latency stays coupled. |
| `esp_ae_bit_cvt` | 32-bit I2S samples to 16-bit processor PCM on RX, and 16-bit speaker PCM to 32-bit bus slots on TX. |
| `esp_ae_deintlv_process` | Stereo/TDM RX slot split before selecting mic/reference channels. |
| `esp_ae_intlv_process` | Dual-mic processor input, TDM TX slot layout and mono-speaker duplication onto a stereo STD TX bus. True `speaker_channels: 2` STD output is already interleaved by ESPHome and is written directly. |
| `esp_ae_ch_cvt` | Stereo speaker downmix to mono when a software AEC reference is needed. |

The selected primitives are logged at boot in `dump_config()`:

```
[C][esp_audio_stack:...]:   Rate Converter: esp_ae_rate_cvt
```

#### Example: P4 yaml using the Espressif backend

```yaml
esp_audio_stack:
  id: audio_stack
  sample_rate: 48000
  output_sample_rate: 16000
  audio_effects:
    rate_cvt_complexity: 3
    rate_cvt_perf_type: speed
  gmf_io:
    reader:
      speed_monitor: true
    writer:
      speed_monitor: true
  processor_id: aec_processor
  # ... other options ...
```

`gmf_io.reader` and `gmf_io.writer` default to Espressif's synchronous
`io_codec_dev` mode: no extra GMF IO task and no data-bus buffering. That is
the closest replacement for the old direct codec read/write calls. Setting
`io_size`, `buffer_size` and `task_stack_size` enables GMF's buffered IO task
for that direction; the ESPHome microphone/speaker API above it does not
change.

#### Notes

- `esp_ae_rate_cvt` is a target-specific prebuilt Espressif software library, not a documented hardware resampling peripheral.
- The multichannel TDM/stereo path deinterleaves the selected slots and calls one `esp_ae_rate_cvt_deintlv_process()` handle for the selected mic/ref channels, keeping their conversion latency coupled.

### AEC with Voice Assistant + MWW

Use `sr_low_cost` AEC mode for simultaneous VA + MWW. This is critical: VOIP modes add a residual echo suppressor (RES) that distorts spectral features MWW relies on (confirmed: VOIP = 2/10 detection, SR = 10/10). Espressif engineer (esp-sr #159): *"For speech recognition and wake word models, adding the non-linear module reduces recognition accuracy."*

```yaml
esp_aec:
  id: aec_component
  sample_rate: 16000
  filter_length: 4        # 64ms tail (4 for integrated codec, 8 for separate mic+speaker)
  mode: sr_low_cost       # Linear-only AEC, preserves spectral features for MWW

esp_audio_stack:
  id: audio_stack
  # ... pins ...
  processor_id: aec_component   # or esp_afe component
  buffers_in_psram: true  # Required for sr_low_cost (512-sample frames need more memory)

microphone:
  - platform: esp_audio_stack
    id: mic_aec
    esp_audio_stack_id: audio_stack

micro_wake_word:
  microphone: mic_aec     # Post-AEC works with SR linear AEC

voice_assistant:
  microphone: mic_aec
```

With SR linear AEC, MWW detects reliably on post-AEC audio even during TTS playback. No separate unprocessed microphone path is needed. MWW task priority can be boosted from default 3 to 8 via `on_boot` lambda for reliable barge-in.

### AEC Mode Comparison

| Mode | Engine | CPU (Core 0) | RES | MWW compatible |
|------|--------|-------------|-----|----------------|
| `sr_low_cost` | `esp_aec3_728` (linear, SIMD) | **~22%** | No | **Yes** (10/10) |
| `sr_high_perf` | `esp_aec3_hps16fft` (linear, FFT) | ~25% | No | Yes |
| `voip_low_cost` | `dios_ssp_aec` (Speex-based) | **~58%** | Yes (always) | **No** (2/10) |
| `voip_high_perf` | `dios_ssp_aec` | ~64% | Yes (always) | No |

SR modes use `esp_aec3` (pure linear adaptive filter, no non-linear processing). VOIP modes use `dios_ssp_aec` (linear + two-stage RES). Use `sr_low_cost` for VA + MWW + intercom. Do not use `sr_high_perf` on ESP32-S3 (exhausts DMA memory).

### ES8311 Digital Feedback AEC (Recommended)

For **ES8311 codec**, enable `use_stereo_aec_reference` for **perfect echo cancellation**:

```yaml
esp_audio_stack:
  id: audio_stack
  # ... pins ...
  processor_id: aec_component
  use_stereo_aec_reference: true  # ES8311 digital feedback
```

**How it works:**
- ES8311 register 0x44 is configured to output DAC+ADC on ASDOUT as stereo
- L channel = ADC microphone, R channel = DAC loopback reference when
  `no_dac_ref: false` writes the Espressif ES8311 `ADCL + DACR` setting.
  Set `reference_channel: right` for this codec loopback mode.
- Reference is **sample-accurate** (same I2S frame as mic) → best possible AEC
- The reference comes directly from the I2S RX deinterleave, sample-accurate

### TDM Hardware Reference (ES7210 + ES8311)

For boards with **ES7210** (multi-channel ADC) + **ES8311** (DAC), the ES7210 can capture the ES8311 DAC analog output on a dedicated ADC slot, giving a sample-aligned AEC reference without the ES8311 digital feedback mode.

The shipped baseline (`packages/codec/es7210_tdm.yaml`) follows the **Espressif Korvo-2** reference: MIC3 / slot 2 = AEC ref @ 30 dB. Boards that route the DAC to a different ADC slot must override the affected PGA register from their own `on_boot` lambda **after** the baseline script runs. Set `tdm_ref_slot` accordingly in YAML.

| Board | DAC routed to | YAML setting | PGA override needed |
|---|---|---|---|
| WS3 / Spotpear / Korvo-2 | MIC3 / slot 2 | `tdm_ref_slot: 2` (baseline default) | none (baseline already sets MIC3 = 30 dB) |
| Waveshare P4 Touch | MIC2 / slot 1 | `tdm_ref_slot: 1` | reset MIC2 PGA to 0 dB; MIC3 stays at baseline |

```yaml
esp_audio_stack:
  id: audio_stack
  # ... pins ...
  processor_id: aec_component
  use_tdm_reference: true
  tdm_total_slots: 4
  tdm_mic_slot: 0           # MIC1 = voice
  tdm_ref_slot: 2           # MIC3 = DAC feedback (Korvo-2 baseline)
```

**Reference health monitor**: while the speaker is actively driving samples, the audio task watches the chosen ref slot's RMS. If it stays below -60 dBFS for ~3.2 s (100 frames at 32 ms), it emits a one-shot WARN:

```
[W][audio_stack] TDM AEC reference silent for 100 frames while speaker active (ref -72.4 dBFS); check tdm_ref_slot wiring or set use_tdm_reference: false
```

That is the canary for "you forgot the per-board PGA override" or wiring fault. Workaround: set `use_tdm_reference: false` to fall back to the software ring-buffer reference (AEC quality drops, but the call still works).

> **Note**: `use_tdm_reference` and `use_stereo_aec_reference` are mutually exclusive. TDM mode uses `I2S_SLOT_MODE_STEREO` for the I2S channel (required to get all TDM slots in DMA).

### Multi-Rate: 48kHz I2S Bus with Espressif Rate Conversion

Many audio codecs operate cleanly at **48 kHz**. Running the I2S bus at
16 kHz can force the codec PLL and filters into a less favorable operating
point, which often results in audible artifacts, worse SNR, and suboptimal
DAC/ADC performance. At 48 kHz the codec usually produces cleaner audio: lower
noise floor, better high-frequency response for TTS and media playback.

The challenge: AEC (ESP-SR), Micro Wake Word (TFLite Micro), Voice Assistant STT, and intercom all require **16kHz** input. The solution is to run the I2S bus at 48kHz and convert the mic/ref path to 16kHz with Espressif's official `esp_ae_rate_cvt` from `esp_audio_effects`.

#### Signal Flow

```
                    ┌─── Speaker path ──────────────────────────────→ I2S TX (48kHz)
                    │    (native rate, no resampling)
I2S bus: 48kHz ─────┤
                    │    ┌─ esp_ae_rate_cvt ×3 ─┐
                    └─── Mic path (48kHz) ───┘──→ 16kHz ──→ AEC / MWW / VA / intercom
```

`esp_ae_rate_cvt` is the standalone C API behind Espressif's GMF `aud_rate_cvt` element. The TDM/stereo path uses one multi-channel converter handle for selected mic/ref channels, so the relative latency between microphones and reference stays coupled. Mono software-reference AEC uses the same converter for both RX mic and TX reference.

If `output_sample_rate` is omitted the conversion ratio is 1. Bit-depth and layout conversion still use `esp_audio_effects` when the bus format needs it.

| Parameter | Value |
|-----------|-------|
| Backend | Espressif `esp_audio_effects` / `esp_ae_rate_cvt` |
| Processing mode | Interleaved for mono, deinterleaved multi-channel for TDM/stereo mic/ref |
| Complexity | 3 |
| Performance mode | `ESP_AE_RATE_CVT_PERF_TYPE_SPEED` |
| Supported ratios | 2, 3, 4, 5, 6 |
| Allocation timing | Audio-effects handles and scratch buffers are prepared before the first realtime audio frame |

#### esp_audio_stack Config

```yaml
esp_audio_stack:
  id: audio_stack
  # ... pins ...
  sample_rate: 48000           # I2S bus rate (ES8311/ES7210 native, best DAC quality)
  output_sample_rate: 16000    # Mic/AEC/MWW/VA converted to 16kHz via esp_ae_rate_cvt
  processor_id: aec_component
  use_stereo_aec_reference: true    # Reference from I2S RX stereo deinterleave (no delay needed)

esp_aec:
  id: aec_component
  sample_rate: 16000           # AEC always operates on 16kHz audio
  filter_length: 4
  mode: sr_low_cost      # Linear AEC, preserves spectral features for MWW
```

#### Speaker Path: ResamplerSpeaker + Mixer

Since the I2S bus runs at 48kHz the speaker must also receive 48kHz audio. ESPHome's `resampler` speaker platform transparently converts any input rate to the target rate:

```yaml
speaker:
  # Hardware output: writes 48kHz PCM to the I2S bus
  - platform: esp_audio_stack
    id: hw_speaker
    esp_audio_stack_id: audio_stack
    buffer_duration: 500ms       # Same semantics as ESPHome i2s_audio.speaker
    timeout: 500ms               # Use "never" only for always-on raw streams

  # Mixer combines VA TTS and intercom at 48kHz
  - platform: mixer
    id: audio_mixer
    output_speaker: hw_speaker
    num_channels: 1
    source_speakers:
      - id: va_speaker_mix
        timeout: 10s
      - id: intercom_speaker_mix
        timeout: 10s

  # ResamplerSpeakers: convert any input rate → 48kHz before the mixer
  - platform: resampler
    id: va_speaker               # VA TTS and media player output here
    output_speaker: va_speaker_mix

  - platform: resampler
    id: intercom_speaker         # 16kHz intercom RX upsampled → 48kHz
    output_speaker: intercom_speaker_mix
```

The `resampler` platform uses polyphase interpolation. For 16kHz→48kHz with default settings (`filters: 16, taps: 16`), CPU overhead on ESP32-S3 is approximately 2% of Core 1 during playback. If you see `[W] component took a long time` warnings for `resampler.speaker` you can try `filters: 8, taps: 8` to reduce CPU at a minimal quality cost, or `filters: 4, taps: 4` for minimal CPU.

#### How Home Assistant Knows to Send 48kHz

HA reads the `sample_rate` from the `announcement_pipeline` in the `media_player` config and transcodes audio accordingly via `ffmpeg_proxy`:

```yaml
media_player:
  - platform: speaker
    pause_releases_pipeline: true
    announcement_pipeline:
      speaker: va_speaker        # Points to the resampler speaker
      format: FLAC
      sample_rate: 48000         # HA will transcode TTS and media to FLAC 48kHz
      num_channels: 1
```

For TTS, HA requests the TTS engine at 48kHz directly. For radio/media streams, `ffmpeg_proxy` transcodes the source to FLAC 48kHz before sending it to the device. In both cases audio arrives at the ESP at 48kHz and goes to the speaker without any intermediate downsampling.

> **Note**: Do not patch ES8311 feedback registers from YAML. Codec setup is
> owned by `esp_codec_dev`; `use_stereo_aec_reference` and `reference_channel`
> select the supported stack behavior. Without a hardware feedback mode,
> no-codec builds use `aec_reference` (`ring_buffer` by default,
> `previous_frame` for light profiles).

## Pin Mapping by Codec

### ES8311 (Spotpear Ball v2, AI Voice Kits)
```yaml
esp_audio_stack:
  i2s_lrclk_pin: GPIO45   # LRCK
  i2s_bclk_pin: GPIO9     # SCLK
  i2s_mclk_pin: GPIO16    # MCLK (required)
  i2s_din_pin: GPIO10     # SDOUT (codec → ESP)
  i2s_dout_pin: GPIO8     # SDIN (ESP → codec)
  sample_rate: 48000             # ES8311 native rate (better DAC quality)
  output_sample_rate: 16000      # Mic/AEC/MWW/VA at 16kHz (rate conversion x3)
  use_stereo_aec_reference: true # Digital feedback (recommended)
```

### ES8311 + ES7210 TDM (Waveshare ESP32-S3-AUDIO-Board, Korvo-2 wiring)
```yaml
esp_audio_stack:
  i2s_lrclk_pin: GPIO14   # LRCK (shared bus)
  i2s_bclk_pin: GPIO13    # SCLK
  i2s_mclk_pin: GPIO12    # MCLK (required)
  i2s_din_pin: GPIO15     # ES7210 SDOUT (codec -> ESP)
  i2s_dout_pin: GPIO16    # ES8311 SDIN (ESP -> codec)
  sample_rate: 16000
  use_tdm_reference: true
  tdm_total_slots: 4
  tdm_mic_slot: 0          # MIC1 = voice
  tdm_ref_slot: 2          # MIC3 = DAC analog feedback (Korvo-2 baseline)
  # Waveshare P4 Touch: tdm_ref_slot: 1 (MIC2) + override MIC2 PGA to 0 dB in on_boot
```

### ES8388 (LyraT, Audio Dev Boards)
```yaml
esp_audio_stack:
  i2s_lrclk_pin: GPIO25   # LRCK
  i2s_bclk_pin: GPIO5     # SCLK
  i2s_mclk_pin: GPIO0     # MCLK (required)
  i2s_din_pin: GPIO35     # DOUT
  i2s_dout_pin: GPIO26    # DIN
  sample_rate: 16000
```

## When to Use This vs Standard i2s_audio

| Scenario | Use This Component | Use Standard i2s_audio |
|----------|-------------------|----------------------|
| ES8311/ES8388/ES8374/ES8389 codec | Yes | No for shared-bus full duplex |
| INMP441 + MAX98357A on same bus | Yes (direct TX reference, `slot_bit_width: 32`) | No |
| INMP441 + MAX98357A on separate buses | Either works | Yes |
| PDM microphone + I2S speaker | No | Yes (different protocols) |
| Need true full-duplex on single bus | Yes | Limited |
| VA + MWW + Intercom on same device | Yes (single bus) | Yes (dual bus with mixer speaker) |

## GPIO Mode Switching (VA + Intercom)

When combining Voice Assistant and Intercom on the same device, a single GPIO button can handle all actions using page-aware logic:

```yaml
binary_sensor:
  - platform: gpio
    pin:
      number: GPIO0
      inverted: true
    id: main_button
    on_click:
      - min_length: 0ms
        max_length: 500ms
        then:
          # Single click: action depends on current page
          - if:
              condition:
                # Timer ringing → stop timer (highest priority, any page)
                lambda: 'return id(voice_assist_phase) == 20;'
              then:
                - voice_assistant.stop:
          - if:
              condition:
                lvgl.page.is_showing: ic_idle_page
              then:
                - intercom_api.call_toggle:
                    id: intercom
          - if:
              condition:
                lvgl.page.is_showing: ic_ringing_in_page
              then:
                - intercom_api.answer_call:
                    id: intercom
          - if:
              condition:
                or:
                  - lvgl.page.is_showing: ic_ringing_out_page
                  - lvgl.page.is_showing: ic_in_call_page
              then:
                - intercom_api.call_toggle:
                    id: intercom
          # VA pages: start/stop voice assistant (default)
      - min_length: 500ms
        max_length: 1000ms
        then:
          # Double click: next contact (IC idle) or decline (IC ringing)
          - if:
              condition:
                lvgl.page.is_showing: ic_idle_page
              then:
                - intercom_api.next_contact:
                    id: intercom
          - if:
              condition:
                lvgl.page.is_showing: ic_ringing_in_page
              then:
                - intercom_api.decline_call:
                    id: intercom
    on_multi_click:
      - timing:
          - ON for at least 1s
        then:
          # Long press: switch between VA and Intercom modes
```

### Button Behavior Summary

| Current Page | Single Click | Double Click | Long Press |
|---|---|---|---|
| VA idle | Start voice assistant | n/a | Switch to intercom |
| VA active | Stop voice assistant | n/a | n/a |
| IC idle | Call selected contact | Next contact | Switch to VA |
| IC ringing in | Answer call | Decline call | Switch to VA |
| IC ringing out | Hangup | n/a | Switch to VA |
| IC in call | Hangup | n/a | Switch to VA |
| Timer ringing | Stop timer (any page) | n/a | n/a |

> **Note**: Wake word detection is ALWAYS active regardless of the current mode. Mode switching only affects the display and button behavior.

## Technical Notes

- **Sample Format**: 16-bit signed PCM, mono TX / stereo RX (ES8311 feedback mode)
- **DMA Buffers**: owned by Espressif's official `esp_driver_i2s` channel layer.
  The component keeps the previous low-latency policy by setting
  `i2s_chan_config_t.dma_desc_num = 6` and computing `dma_frame_num` for about
  10 ms per descriptor, clamped to IDF's 4092-byte DMA descriptor limit. There
  is still no public ESPHome YAML DMA knob; if latency needs tuning, expose the
  relevant IDF channel fields explicitly instead of adding a fallback backend.
- **Speaker Buffer**: the public `speaker: platform: esp_audio_stack` exposes ESPHome-compatible `buffer_duration` and `timeout` options. Default `buffer_duration: 500ms` allocates a mono PCM staging ring at the I2S bus rate, preferably in PSRAM.
- **Task Priority**: 19 (above lwIP at 18, below WiFi at 23). Configurable via `task_priority` YAML option.
- **Core Affinity**: Pinned to Core 0 for the non-network realtime I2S bridge. Espressif's GMF AFE path keeps manager feed on Core 0 and manager fetch plus the GMF pipeline task on Core 1 via `esp_afe`; Core 1 remains available for GMF output, MWW inference and LVGL. Configurable via `task_core` YAML option.
- **Processor Surface**: Mono, stereo and TDM processor modes all keep callbacks on the processed surface. Mono software-reference mode zero-fills the reference when playback is idle instead of switching around the processor.
- **Thread Safety**: All cross-thread variables use `std::atomic` with `memory_order_relaxed`. Speaker/media volume, input attenuation and mic attenuation are converted to ESPHome 2026.5 `esp-audio-libs` Q31 gain values when they change, then the audio task snapshots those fixed-point values once per frame and applies them through `esp_audio_libs::gain::apply()`. Positive user-facing mic gain uses Espressif `esp_ae_alc` instead of a local scalar boost. Ring buffer resets use atomic request flags (`request_speaker_reset_`) to avoid concurrent access between main thread and audio task.
- **Task Structure**: `audio_task_()` is split into `process_rx_path_()`, `process_aec_and_callbacks_()`, and `process_tx_path_()`, sharing state via `AudioTaskCtx` struct. AEC buffers use 16-byte aligned allocation for ESP-SR SIMD safety.
- **I2S hardware lifecycle**: the component creates and initializes official
  IDF `esp_driver_i2s` TX/RX channels on demand. `esp_codec_dev_open()` then
  enables the underlying data interfaces, and `gmf_io/io_codec_dev` performs
  RX/TX payload transfer. Runtime `stop()` parks the audio task, closes GMF IO,
  closes `esp_codec_dev`, then deletes the IDF channels. Mic-only
  configurations still create a clock-only TX channel with `dout = GPIO_NUM_NC`,
  matching Espressif's TX-clock-for-RX full-duplex pattern.
- **Runtime state hooks**: the parent component exposes a minimal runtime state
  machine for YAML automation. `on_state` receives `idle`, `mic`, `speaker` or
  `duplex`. `on_mic_start`/`on_mic_idle` fire on microphone consumer edges.
  `on_speaker_start`/`on_speaker_idle` fire on speaker playback edges and are
  the right hooks for amplifier power gating; do not use the generic audio-stack
  `on_start`/`on_idle` for speaker power if wake word or VA can keep the mic
  path active.
- **Mic Gain**: -20 to +30 dB range (applied post-AEC in audio_task). Stored via `ESPPreferenceObject` and restored on boot. Mic gain is applied to post-AEC output (affects VA/intercom/MWW equally). Values at or below 0 dB use ESPHome's Q31 `esp-audio-libs` gain path, including zero as a `memset()` fast path. Positive gain uses Espressif `esp_ae_alc` with the YAML number's 1 dB step. **Clipping warning**: positive gain can still saturate the PCM stream. On loud speech with gain > +6 dB, peak samples can clip and produce harmonic distortion that degrades STT and intercom audio. If you need gain > +6 dB to bring a weak MEMS mic up to working levels, pair this component with `esp_afe` and `agc_enabled: true`; with standalone `esp_aec` there is no automatic ceiling.
- **Cross-Component Validation**: `FINAL_VALIDATE_SCHEMA` checks at compile time that `esp_audio_stack` and `intercom_api` don't both configure an audio processor (`processor_id`) or DC offset removal. If both components are present, `esp_audio_stack` takes ownership of audio processing and DC offset; `intercom_api` should NOT set `processor_id` or `dc_offset_removal`.

### Audio task lifecycle

The audio task is an internal FreeRTOS task with three properties worth knowing about in the current design:

- **Permanent early creation**. The task is created during component setup and then parks in its outer wait loop until `esp_audio_stack.start` flips `audio_stack_running_`. This reserves the TCB/stack before Wi-Fi/API/VA churn fragments internal RAM.
- **No task churn; intentional I2S churn**. Once created, the audio task lives
  for the rest of the device's uptime. I2S channels do not:
  `esp_audio_stack.stop` deletes the IDF TX/RX channel handles after the task
  parks, so the bus is really stopped. A subsequent `start` reuses the same
  TCB/stack and creates fresh `esp_driver_i2s` channels.
- **In-place reconfigure**. When the linked processor's `frame_spec` changes
  (for example after an AFE mode or graph rebuild), the audio task observes the
  `frame_spec_revision()` bump at the top of its main loop and reinitialises
  rate conversion, reference extraction and output buffers in place.
  Worst-case-sized buffers are preallocated by the parked audio task once the
  processor frame shape is known, before the first media/call activation. The
  reconfigure path does not call `heap_caps_alloc` and cannot fragment SPIRAM.

This is why mic consumers (MWW, Voice Assistant, intercom) survive an internal stop/start cycle: the task and its consumer registry are intact across reconfigure.

### Mic consumer registry (C++ API)

Components that want to receive processed mic frames register themselves once at setup with an opaque token:

```cpp
esp_audio_stack_->register_mic_consumer(this);   // typically `this` of the consumer component
// ... runs forever ...
esp_audio_stack_->unregister_mic_consumer(this); // optional, only if the consumer is destroyed
```

The token is just an identity; the registry tracks live consumers in a fixed-size
array and gates the mic capture (no consumers means no callbacks fired). The
registry survives `stop()` and internal reconfigure cycles, so consumers do not
need to re-register. Replaces the older `mic_ref_count_` atomic refcount, which
lost its count on `stop()` and silently disconnected MWW / VA after every
internal reconfigure.

### Mono-mode AEC reference

When neither `use_stereo_aec_reference` nor `use_tdm_reference` is enabled, the AEC reference comes from the speaker output. Two options via `aec_reference:`:

- **`ring_buffer`** (default): speaker TX is stored in an Espressif/ADF TYPE2-style ring buffer with `aec_reference_buffer_ms` of capacity. The mono-reference helper reads from the ring; on starvation it zero-fills (the AEC handles that as a "no echo this frame") rather than reusing stale data. Better frame alignment on no-codec setups (discrete MEMS mic + I²S amp) at the cost of `aec_reference_buffer_ms` of latency.
- **`previous_frame`**: the audio task uses the prior TX frame as the AEC reference. Simple, lower RAM and smaller compile-time surface; no TYPE2 ring buffer or delay tuning is compiled into that build.

### PSRAM and sdkconfig Requirements

For reliable audio on PSRAM-based devices, these sdkconfig options are **critical** and are NOT defaults:

**ESP32-S3:**
```yaml
sdkconfig_options:
  CONFIG_ESP32S3_DATA_CACHE_64KB: "y"       # Default is 32KB, too small for audio+PSRAM
  CONFIG_ESP32S3_DATA_CACHE_LINE_64B: "y"   # Default is 32B; 64B reduces cache misses
  CONFIG_SPIRAM_FETCH_INSTRUCTIONS: "y"     # Move code to PSRAM, frees internal SRAM
  CONFIG_SPIRAM_RODATA: "y"                 # Move read-only data to PSRAM
  CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC: "y"    # TLS buffers in PSRAM (saves ~40KB internal)
```

**ESP32-P4:**
```yaml
sdkconfig_options:
  CONFIG_CACHE_L2_CACHE_256KB: "y"          # Default is 128KB; 256KB for MIPI DSI+audio+LVGL
  CONFIG_SPIRAM_FETCH_INSTRUCTIONS: "y"
  CONFIG_SPIRAM_RODATA: "y"
  CONFIG_ESP_TASK_WDT_TIMEOUT_S: "30"       # esp32_hosted + MIPI can block main loop
  CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC: "y"
```

Removing any of these causes audio glitch at stream startup (cache cold-start: every PSRAM access is a miss until the working set loads, ~1 second).

## Logging

The driver and its helper entities log under namespaced tags so callers can mute pieces independently.

| Tag | Source | Function |
|---|---|---|
| `audio_stack` | `esp_audio_stack.cpp`, `audio_pipeline.cpp` | Driver init/teardown, channel start/stop, mic consumer registry, AEC reference selection, throttled I2S read/write WARNs |
| `audio_stack.mic_gain` | `number.h` | `dump_config` for the mic gain helper number |
| `audio_stack.master_volume` | `number.h` | `dump_config` for the Master Volume helper number |
| `audio_stack.tdm_slot_sensor` | `sensor.h` | `dump_config` for the TDM slot level sensor |
| `audio_stack.aec_switch` | `switch.h` | `dump_config` for the AEC enable switch |

**Default levels**

- `WARN` - GMF codec IO read/write failures (rate-limited 1st-5th + every 100th via `I2S_LOG_W_THROTTLED`), `TX/RX channel re-enable failed`, audio task did not park within 600 ms
- `INFO` - driver lifecycle (`ESP audio stack initialized`, `Audio stack started/stopped`, `Mic consumer registered/removed`, `Audio stack going idle`), AEC reference mode chosen
- `DEBUG` - channel creation details (TDM mask, rate-conversion ratio), per-frame audio session start/end, TDM slot configuration

**Telemetry overhead**

When `telemetry: true` AND the global `level: DEBUG`, `audio_pipeline.cpp` logs a per-frame snapshot every `telemetry_log_interval_frames` frames (default 128 ≈ 4 s @ 16 kHz / 32 ms). The block is **fully stripped at compile time** when either flag is missing - `#if defined(USE_ESP_AUDIO_STACK_TELEMETRY) && ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG`. Production YAMLs ship with `telemetry: false`, so no telemetry compute or log strings reach the binary regardless of log level.

**Quiet AEC tuning**

When chasing AEC issues you usually only want `audio_stack` itself, not the helper entities:

```yaml
logger:
  level: DEBUG
  logs:
    audio_stack.mic_gain: INFO
    audio_stack.master_volume: INFO
    audio_stack.aec_switch: INFO
    audio_stack.tdm_slot_sensor: INFO
```

## Troubleshooting

### No Audio Output
1. Check MCLK connection (many codecs require it)
2. Verify codec I2C initialization (check logs)
3. Ensure speaker amp is enabled (GPIO control if applicable)

### Audio Crackling
1. Reduce sample_rate (try 8000 or 16000)
2. Check for WiFi interference (pin to Core 1)
3. Verify PSRAM is available if using AEC

### Echo During Calls
1. Enable AEC: set `processor_id` on `esp_audio_stack`
2. For ES8311: enable `use_stereo_aec_reference: true` + configure register 0x44
3. Adjust `filter_length` (4 for integrated codec, 8 for separate speaker)

### TDM Mode: Audio Corruption (Whistle/Machine Gun Noise)
1. Ensure I2S uses `I2S_SLOT_MODE_STEREO` (not MONO) for TDM. MONO only puts slot 0 in DMA
2. Check ES7210 TDM register initialization (reg 0x12 = 0x02 for TDM mode)
3. Verify `tdm_total_slots` matches the actual ES7210 slot count
4. If the slot mapping is correct but corruption persists, inspect the computed
   `esp_driver_i2s` `dma_frame_num` in logs and compare it with the active
   sample rate, slot width and `tdm_total_slots`.

### MWW Not Detecting During TTS
1. **Use `sr_low_cost` AEC mode** (not VOIP). See [AEC Mode Comparison](#aec-mode-comparison).
2. **MWW on `mic_aec`** (post-AEC), not on an unprocessed microphone path.
3. **Enable `buffers_in_psram: true`** for SR mode's 512-sample frames on ESP32-S3.
4. **Boost MWW priority to 8** via on_boot lambda (ESPHome defaults to 3, below mixer at 10).
5. Do NOT use `sr_high_perf` on ESP32-S3 (exhausts DMA memory).

### Switches/Display Slow With AEC On
With `audio_stack` on **Core 0**, AEC no longer competes with LVGL/display on Core 1. This issue is resolved by correct core assignment. If you still see display slowness, check that no other high-priority task is pinned to Core 1.

### SPI Errors (err 101) With AEC
1. Use `mode: sr_low_cost` or `mode: voip_low_cost`. `sr_high_perf` exhausts DMA memory on ESP32-S3
2. Enable `buffers_in_psram: true` to free internal heap
3. Reduce display update interval (500ms+) to avoid SPI bus contention
4. Check free heap in logs after boot

## Known Limitations

- **Media files should match bus sample rate**: For best quality, use media files at the bus `sample_rate` (e.g. 48kHz). The `resampler` speaker handles conversion from any rate, but native rate avoids resampling artifacts.
- **loopTask long-operation warnings during 48kHz streaming**: ESPHome reports `[W] mixer.speaker took a long time (110ms)` and similar warnings for `resampler.speaker`, `api`, `wifi` during audio playback. This is **expected and harmless**. These components run in loopTask (Core 1, prio 1) and process audio/network chunks that take >30ms. All real-time audio runs in dedicated tasks on Core 0 and is unaffected.
- **AEC is ESP-SR closed-source**: Cannot reset the adaptive filter without recreating the handle. When a processor is configured, this component no longer switches to raw mic audio during idle or reinit windows; unavailable processed output is silenced.
- **TDM analog reference vs ES8311 digital feedback**: The digital feedback path (ES8311 stereo loopback) provides a cleaner reference signal for AEC than the TDM analog path (ES7210 MIC3 capturing speaker output). Analog loopback introduces non-linear distortion from the DAC/amplifier chain that the AEC linear adaptive filter cannot fully model. Expect ~95-98% echo cancellation with analog reference vs ~99% with digital feedback. Both are adequate for voice assistant and intercom use.
- **AEC reference**: The reference signal is always the exact post-volume PCM sent to the speaker, with no additional scaling. For hardware codec setups (ES8311, TDM), the reference naturally includes hardware volume. For software reference (no codec), the reference includes software volume. Pre-AEC input gain/gain affects only the mic signal, not the reference; use it sparingly because it changes what the AEC/AFE sees.

## License

MIT License
