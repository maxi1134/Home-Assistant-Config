# Reference

Options, actions, conditions, entities, services and automation examples for ESPHome Intercom. Use this page to look up a specific setting or callback.

## Contents
- [intercom_api component](#intercom_api-component)
- [Event callbacks](#event-callbacks)
- [Actions](#actions)
- [Conditions](#conditions)
- [Audio processing components](#audio-processing-components)
- [esp_aec component](#esp_aec-component)
- [esp_afe component](#esp_afe-component)
- [Entities and controls](#entities-and-controls)
- [Home Assistant services](#home-assistant-services)
- [ESP-to-ESP vs browser-to-ESP](#esp-to-esp-vs-browser-to-esp)
- [Auto answer (card)](#auto-answer-card)
- [Automation examples](#automation-examples)
- [Events](#events)

---

## intercom_api component

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `id` | ID | Required | Component ID |
| `mode` | string | `simple` | `simple` (browser only) or `full` (ESP↔ESP) |
| `microphone` | ID | Required | Reference to microphone component |
| `speaker` | ID | Required | Reference to speaker component |
| `processor_id` | ID | - | Reference to an `esp_aec` component. Must be `esp_aec`, not `esp_afe`: when `intercom_api` drives its own mic (no `i2s_audio_duplex` in front), the AFE feed/fetch pipeline cannot be fed correctly. Accepts any `AudioProcessor` implementation at the type level, but only `esp_aec` is supported in practice. |
| `dc_offset_removal` | bool | false | Remove DC offset (for mics like SPH0645) |
| `ringing_timeout` | time | 0s | Auto-decline after timeout (0 = disabled) |
| `tasks_stack_in_psram` | bool | false | Place the three internal task stacks in PSRAM. Saves ~28 KB internal heap on S3/P4 full-experience builds where AFE/MWW/LVGL compete for it. Requires PSRAM and `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY: "y"`. Default false keeps `intercom_api` working on plain ESP32 boards without PSRAM. |

## Event callbacks

| Callback | Trigger | Use Case |
|----------|---------|----------|
| `on_ringing` | Incoming call (auto_answer OFF) | Turn on ringing LED/sound, show display page |
| `on_outgoing_call` | User initiated call | Show "Calling..." status |
| `on_answered` | Call was answered (local or remote) | Log event |
| `on_streaming` | Audio streaming active | Solid LED, enable amp |
| `on_idle` | State returns to idle | Turn off LED, disable amp |
| `on_hangup` | Call ended normally | Log with reason string |
| `on_call_failed` | Call failed (unreachable, busy, etc.) | Show error with reason string |

## Actions

| Action | Description |
|--------|-------------|
| `intercom_api.start` | Start outgoing call |
| `intercom_api.stop` | Hangup current call |
| `intercom_api.answer_call` | Answer incoming call |
| `intercom_api.decline_call` | Decline incoming call |
| `intercom_api.call_toggle` | Smart: idle→call, ringing→answer, streaming→hangup |
| `intercom_api.next_contact` | Select next contact (Full mode) |
| `intercom_api.prev_contact` | Select previous contact (Full mode) |
| `intercom_api.set_contacts` | Update contact list from CSV |
| `intercom_api.set_contact` | Select a specific contact by name |
| `intercom_api.set_volume` | Set speaker volume (float, 0.0–1.0) |
| `intercom_api.set_mic_gain_db` | Set microphone gain (float, -20.0 to +20.0 dB) |

## Conditions

| Condition | Returns true when |
|-----------|-------------------|
| `intercom_api.is_idle` | State is Idle |
| `intercom_api.is_ringing` | State is Ringing (incoming) |
| `intercom_api.is_calling` | State is Outgoing (waiting answer) |
| `intercom_api.is_in_call` | State is Streaming (active call) |
| `intercom_api.is_streaming` | Audio is actively streaming |
| `intercom_api.is_answering` | Call is being answered |
| `intercom_api.is_incoming` | Has incoming call |

## Audio processing components

Two audio processing components are available, both implementing the `AudioProcessor` interface:

- **`esp_aec`**: Standalone echo cancellation. Lightweight (~80 KB internal RAM). Use when you only need AEC.
- **`esp_afe`**: Full AFE pipeline with two modes:
  - **Single-mic (MR)**: AEC + NS + VAD + AGC (~100 KB internal RAM)
  - **Dual-mic (MMR)**: AEC + spatial source separation (BSS) (~120 KB internal RAM). Spatial voice isolation using 2 microphones
  - Runtime toggle switches, diagnostic sensors, and mode switching in Home Assistant

They are drop-in replacements **only behind `i2s_audio_duplex`**, which feeds the processor with the fixed 512-sample 16 kHz frames that `esp_afe` requires. When `intercom_api` talks to the processor directly (no `i2s_audio_duplex`, typical of dual-bus MEMS + I2S amp setups), use `esp_aec` only. The AFE pipeline needs a stable producer task that `intercom_api`'s standalone mic path does not provide.

## esp_aec component

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `id` | ID | Required | Component ID |
| `sample_rate` | int | 16000 | Must match audio sample rate |
| `filter_length` | int | 4 | Echo tail in frames. Range 1 to 8. Frame size depends on `mode`: 32 ms in SR modes, 16 ms in VOIP modes. Use **4** with SR modes (full-experience with MWW, ~128 ms tail), **8** with VOIP modes (intercom-only, ~128 ms tail). |
| `mode` | string | `voip_low_cost` | AEC algorithm mode. Pick the engine to match the use case: **VOIP modes** for intercom-only (human ears, residual echo suppressor active), **SR modes** for full-experience with MWW (linear-only, preserves spectral features). Do not mix engines at runtime, see "AEC engine standard" below. |

**AEC modes** (ESP-SR library, two completely different engines):

| Mode | Engine | CPU (Core 0) | RES | MWW on post-AEC | Recommended |
|------|--------|-------------|-----|-----------------|-------------|
| `sr_low_cost` | `esp_aec3` (linear) | **~22 %** | No | **10/10** | **Yes, for VA + MWW** |
| `sr_high_perf` | `esp_aec3` (FFT) | ~25 % | No | 10/10 | Only when DMA-capable internal RAM is available |
| `voip_low_cost` | `dios_ssp_aec` (Speex) | ~58 % | Yes | 2/10 | Intercom-only, mild echo, low CPU budget |
| `voip_high_perf` | `dios_ssp_aec` | ~64 % | Yes | 2/10 | **Default for intercom-only** (with `filter_length: 8`) |

### AEC engine standard (intercom-only vs full-experience)

`aec_create()` in esp-sr 2.3.1 has a silent FFT-table calloc-fail bug
that crashes the next `aec_process` if `filter_length > 4` and the
contiguous DMA-capable internal block is too small. The bug only emerges
on cross-engine transitions (VOIP ↔ SR), because each engine allocates
a different scratch layout. Public YAMLs in this repo restrict the
runtime AEC select to a single engine per tier:

| Tier | filter_length | Initial mode | Runtime select options | Engine |
|---|---|---|---|---|
| Intercom-only (no MWW) | 8 | `voip_high_perf` | `voip_low_cost`, `voip_high_perf` | `dios_ssp_aec` |
| Full-experience AEC (with MWW) | 4 | `sr_low_cost` | `sr_low_cost`, `sr_high_perf` | `esp_aec3` |

Full-experience AFE setups select the engine via `esp_afe.mode` /
`esp_afe.afe_type` and do not use the `esp_aec` select at all.

CPU figures measured on ESP32-S3 at 240 MHz feeding one 16 kHz mic channel (S3 reference hardware, esp-sr 2.3.1). They scale roughly linearly with the sample rate.

> **Important**: SR modes use a **linear-only** adaptive filter that preserves spectral features for neural wake word detection. VOIP modes add a **residual echo suppressor** (RES) that distorts features, reducing MWW detection from 10/10 to 2/10. Use `sr_low_cost` for VA + MWW setups. SR mode requires `buffers_in_psram: true` on ESP32-S3 (512-sample frames need more memory than the internal heap can usually spare). `sr_high_perf` needs a contiguous DMA-capable internal block at switch time; the component runs a pre-flight heap check and refuses the switch if the block is not available, rather than crashing.

## esp_afe component

Full audio front-end pipeline with runtime control, diagnostics, and dual-mic BSS voice isolation.

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `id` | ID | Required | Component ID |
| `type` | string | `sr` | `sr` (speech recognition, linear AEC) or `vc` (voice communication, nonlinear AEC) |
| `mode` | string | `low_cost` | `low_cost` or `high_perf` |
| `mic_num` | int | 1 | Number of microphones (1 or 2). Set to 2 for BSS voice isolation |
| `se_enabled` | bool | false | Spatial source separation (BSS). Requires `mic_num: 2`. When active, replaces NS and AGC with spatial source separation |
| `aec_enabled` | bool | true | Echo cancellation |
| `aec_filter_length` | int | 4 | AEC filter length in frames (1-8) |
| `ns_enabled` | bool | true | Noise suppression (WebRTC). No effect when SE is active |
| `vad_enabled` | bool | false | Voice activity detection |
| `agc_enabled` | bool | true | Automatic gain control (WebRTC). No effect when SE is active |
| `memory_alloc_mode` | string | `more_psram` | Memory allocation strategy |

```yaml
# Single-mic mode (AEC + NS + AGC):
esp_afe:
  id: afe_processor
  type: sr
  mode: low_cost

# Dual-mic mode (AEC + BSS voice isolation):
esp_afe:
  id: afe_processor
  type: sr
  mode: low_cost
  mic_num: 2
  se_enabled: true
```

> **Important**: On ESP32-S3 with PSRAM, add IRAM optimization to sdkconfig to free ~30 KB of internal RAM required by the AFE. See the [esp_afe README](../esphome/components/esp_afe/README.md#iram-optimization-critical-for-esp32-s3) for details.

**Platform entities** (switches, sensors, binary sensors):

```yaml
switch:
  - platform: esp_afe
    esp_afe_id: afe_processor
    aec:
      name: "Echo Cancellation"
    se:
      name: "Beamforming"
    ns:
      name: "Noise Suppression"
    agc:
      name: "Auto Gain Control"
    vad:
      name: "Voice Detection"

sensor:
  - platform: esp_afe
    esp_afe_id: afe_processor
    input_volume:
      name: "Input Volume"
    output_rms:
      name: "Output RMS"

binary_sensor:
  - platform: esp_afe
    esp_afe_id: afe_processor
    vad:
      name: "Voice Presence"
```

See the full [esp_afe README](../esphome/components/esp_afe/README.md) for all options, toggle behavior details, and troubleshooting.

---

## Entities and controls

### Auto-created entities (always)

| Entity | Type | Description |
|--------|------|-------------|
| `sensor.{name}_intercom_state` | Text Sensor | Current state: Idle, Ringing, Streaming, etc. |

### Auto-created entities (Full mode only)

| Entity | Type | Description |
|--------|------|-------------|
| `sensor.{name}_destination` | Text Sensor | Currently selected contact |
| `sensor.{name}_caller` | Text Sensor | Who is calling (during incoming call) |
| `sensor.{name}_contacts` | Text Sensor | Contact count |

### Platform entities (declared in YAML)

| Platform | Entities |
|----------|----------|
| `switch` | `auto_answer`, `aec` |
| `number` | `speaker_volume` (0-100%), `mic_gain` (-20 to +20 dB) |
| `button` | Call, Next Contact, Prev Contact, Decline (template) |

---

## Home Assistant services

The integration exposes native Home Assistant services for intercom control. All services use **target device selectors**, so you pick devices from a dropdown in the automation editor (no need to know device IDs).

> **Note:** The device picker shows all ESPHome devices. Select your intercom device under **Devices** tab (not Entities). Only devices with `intercom_api` configured will work. If you select a non-intercom device, the service logs an error and does nothing.

### Available services

| Service | Target | Fields | Description |
|---------|--------|--------|-------------|
| `intercom_native.answer` | Device | - | Answer an incoming call |
| `intercom_native.decline` | Device | - | Decline an incoming call |
| `intercom_native.hangup` | Device | - | End an active call |
| `intercom_native.call` | Device (dest) | `source` (optional device) | Start a call. With `source`: ESP-to-ESP bridge. Without: HA-to-ESP P2P |
| `intercom_native.forward` | Device (source/caller) | `forward_to` (device) | Forward an active or ringing call to another device |

## ESP-to-ESP vs browser-to-ESP

| Capability | ESP-to-ESP (Bridge) | Browser-to-ESP (P2P) |
|---|---|---|
| `call` | Fully automatable | Opens TCP but no browser mic |
| `answer` | Fully automatable | Works if mic permission is persistent |
| `decline` / `hangup` | Fully automatable | Fully automatable |
| `forward` | Fully automatable | Fully automatable |

For browser P2P calls, the intercom card must be open and the browser must have granted persistent microphone permission (Chrome "Allow", not "Allow this time").

## Auto answer (card)

The intercom card has an **Auto Answer** checkbox (visible in idle state only):

1. Enable the checkbox (this requests mic permission via user gesture)
2. When an incoming call arrives and the checkbox is on, the card auto-answers if the browser has persistent mic permission
3. If permission is not persistent, the card falls back to showing Answer/Decline buttons

The preference is saved per device in localStorage.

## Automation examples

### Doorbell routing based on presence

When the doorbell rings, forward to the indoor panel if someone is home, or send a push notification if everyone is away.

```yaml
alias: Doorbell call routing
triggers:
  - trigger: event
    event_type: esphome.intercom_call
actions:
  - choose:
      - conditions:
          - condition: state
            entity_id: person.daniele
            state: "home"
        sequence:
          - action: intercom_native.forward
            target:
              device_id: "{{ trigger.event.data.device_id }}"
            data:
              forward_to: "<indoor_panel_device_id>"
      - conditions:
          - condition: state
            entity_id: person.daniele
            state: "not_home"
        sequence:
          - action: notify.mobile_app_phone
            data:
              title: "Doorbell"
              message: "{{ trigger.event.data.caller }} is calling"
              data:
                clickAction: /lovelace/intercom
                importance: high
                channel: doorbell
                actions:
                  - action: URI
                    title: "Open Intercom"
                    uri: /lovelace/intercom
mode: single
```

### Bridge two ESP devices from an automation

Start a call between the kitchen and bedroom intercoms via an HA automation (e.g. triggered by a button or schedule).

```yaml
alias: Call kitchen from bedroom
triggers:
  - trigger: state
    entity_id: input_button.call_kitchen
actions:
  - action: intercom_native.call
    target:
      device_id: "<kitchen_device_id>"
    data:
      source: "<bedroom_device_id>"
mode: single
```

### Doorbell with actionable notification and decline

Send a push notification with Answer and Decline actions. If the user taps Decline, the call is ended.

```yaml
alias: Doorbell notification with actions
triggers:
  - trigger: event
    event_type: esphome.intercom_call
actions:
  - action: notify.mobile_app_phone
    data:
      title: "Doorbell"
      message: "{{ trigger.event.data.caller }} is calling"
      data:
        tag: intercom
        channel: doorbell
        importance: high
        clickAction: /lovelace/intercom
        actions:
          - action: URI
            title: "Open Intercom"
            uri: /lovelace/intercom
          - action: DECLINE_INTERCOM
            title: "Decline"
  - wait_for_trigger:
      - trigger: event
        event_type: mobile_app_notification_action
        event_data:
          action: DECLINE_INTERCOM
    timeout: "00:00:30"
  - condition: template
    value_template: "{{ wait.trigger is not none }}"
  - action: intercom_native.decline
    target:
      device_id: "{{ trigger.event.data.device_id }}"
  - action: notify.mobile_app_phone
    data:
      message: clear_notification
      data:
        tag: intercom
mode: single
```

### Night mode: auto-decline all calls

During night hours, automatically decline all incoming calls.

```yaml
alias: Night mode auto-decline
triggers:
  - trigger: event
    event_type: esphome.intercom_call
conditions:
  - condition: time
    after: "23:00:00"
    before: "07:00:00"
actions:
  - action: intercom_native.decline
    target:
      device_id: "{{ trigger.event.data.device_id }}"
mode: single
```

## Events

| Event | Payload | When |
|-------|---------|------|
| `esphome.intercom_call` | `caller`, `device_id` | ESP calls Home Assistant |
| `intercom_bridge_state` | `bridge_id`, `source_device_id`, `dest_device_id`, `state` | Bridge state changes |
| `intercom_forward_state` | `bridge_id`, `source_name`, `new_dest_name`, `state` | Call forwarded (forwarding/ringing/connected/failed) |
