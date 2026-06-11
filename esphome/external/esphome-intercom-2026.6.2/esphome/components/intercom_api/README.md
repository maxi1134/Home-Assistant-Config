# intercom_api

`intercom_api` is the call-signaling and network transport component for the
project intercom protocol. It does not own echo cancellation anymore.

It can run in two supported shapes:

- **Standalone native ESPHome audio**: bind `intercom_api` directly to standard
  ESPHome `microphone` and/or `speaker` components. This is the right path for
  devices whose hardware already returns processed audio, for example XMOS
  front-ends, DSP codecs, or simple tests with native I2S mic/speaker.
- **Audio stack facade**: bind it to the microphone/speaker exposed by
  `esp_audio_stack`. This is the maintained path when software AEC, AFE, codec
  routing, TDM reference, dual-bus sync, media player, Voice Assistant or Micro
  Wake Word share the same audio backend.

The component always speaks 16 kHz, 16-bit, mono PCM on the wire. Any format
conversion must happen before audio reaches `intercom_api`, either through
ESPHome `microphone_source` or through `esp_audio_stack`.

## Audio Capabilities

The endpoint capability is inferred from the YAML wiring:

| YAML wiring | Published endpoint mode | Runtime behavior |
|---|---|---|
| `microphone`/`microphone_source` + `speaker` | `full_duplex` | Sends mic audio and plays remote audio. |
| `microphone`/`microphone_source` only | `mic_only` | Sends mic audio; incoming audio is ignored. |
| `speaker` only | `speaker_only` | Plays incoming audio; no mic TX task is created. |
| neither mic nor speaker | `control_only` | Signaling/phonebook only. |

The endpoint sensor appends the mode:

```text
Name|tcp|192.168.1.40|6054|full_duplex
Name|udp|192.168.1.41|6054|6055|speaker_only
```

Home Assistant consumes this field for routing/card display and for avoiding
audio directions that cannot exist.

## Compile-Time Shape

`intercom_api` now defines its own local compile flags from YAML:

- `USE_INTERCOM_API_MIC` is emitted only when the component has
  `microphone:` or `microphone_source:`.
- `USE_INTERCOM_API_SPEAKER` is emitted only when the component has `speaker:`.

This keeps the intercom TX path out of speaker-only builds and keeps the
intercom RX-to-speaker code out of mic-only builds, even if some other ESPHome
component in the same firmware uses a microphone or speaker.

The only always-present pieces are the finite-state machine, phonebook,
transport listener/client and control signaling.

## Minimal Full-Duplex Example

```yaml
microphone:
  - platform: i2s_audio
    id: native_mic
    adc_type: external
    i2s_audio_id: rx_i2s
    i2s_din_pin: GPIO11
    channel: right
    sample_rate: 16000
    bits_per_sample: 32bit

speaker:
  - platform: i2s_audio
    id: native_speaker
    dac_type: external
    i2s_audio_id: tx_i2s
    i2s_dout_pin: GPIO14
    channel: mono
    sample_rate: 16000
    bits_per_sample: 16bit

intercom_api:
  id: intercom
  microphone_source:
    microphone: native_mic
    bits_per_sample: 16
    channels: [0]
  speaker: native_speaker
```

Use `microphone:` directly instead of `microphone_source:` only when the
referenced microphone already publishes 16 kHz, 16-bit, mono PCM.

## Mic-Only And Speaker-Only

Mic-only:

```yaml
intercom_api:
  id: intercom
  microphone: processed_mic
```

Speaker-only:

```yaml
intercom_api:
  id: intercom
  speaker: local_speaker
```

These are first-class modes, not fallback hacks. They are useful for split
installations, paging speakers, monitor/listen devices, and hardware that
already exposes only one audio direction.

## Software AEC / AFE

Standalone `intercom_api` AEC has been removed.

Removed options:

| Removed | Replacement |
|---|---|
| `intercom_api.processor_id` | Configure `processor_id` on `esp_audio_stack`. |
| `intercom_api.aec_reference_delay_ms` | Configure AEC/AFE reference buffering on `esp_audio_stack`. |
| `switch: platform: intercom_api, aec:` | Use `esp_audio_stack`, `esp_aec` or `esp_afe` controls. |

Reason: `intercom_api` is not the right owner for I2S timing, speaker reference
capture or AFE cadence. When software processing is required, `esp_audio_stack`
owns the audio graph and exposes normal ESPHome microphone/speaker interfaces
back to `intercom_api`.

## Transport

TCP is the default:

```yaml
intercom_api:
  id: intercom
  protocol: tcp
  tcp_port: 6054
```

UDP remains available for raw/low-overhead audio:

```yaml
intercom_api:
  id: intercom
  protocol: udp
  listen_port: 6054
  control_port: 6055
```

`mode: raw_udp` is a special UDP-only mode for external consumers that handle
signaling elsewhere. Normal intercom installs should omit `mode:`.

## Phonebook And HA Routing

Each ESP publishes an `intercom_endpoint` text sensor. Home Assistant builds the
central `sensor.intercom_phonebook` from those endpoints and adds itself as the
HA peer.

Home Assistant now resolves inbound TCP callers by:

1. socket source IP when it matches the endpoint host;
2. `caller_route` from the PBX-lite START payload;
3. caller friendly name from the same START payload.

That keeps routed subnet/NAT/VPN installs working when HA sees a socket source
address that differs from the ESP endpoint IP, while preserving the endpoint IP
as the address other peers should dial.

`advertise_host` in the HA integration is an advanced override for HA
multihomed/LXC/NAT installs where Home Assistant would otherwise publish an
address ESP devices cannot reach. It is not required just because ESPs and HA
are on different routed subnets.

## Auto Entities

`auto_entities: true` can create the common HA entities for minimal YAMLs:

- `auto_answer`
- `dnd`
- `ha_pbx_mode`
- `master_volume`, only when a speaker is configured
- `mic_gain`, only when a mic is configured

Existing maintained YAMLs generally declare entities through packages for
stable names and UI layout.

## Resource Notes

- Mic ring buffer and TX chunk are allocated only when a mic path is configured.
- No speaker ring, speaker task or AEC reference buffer exists in
  `intercom_api`.
- Incoming audio is written directly to the configured ESPHome speaker.
- `buffers_in_psram: true` affects only intercom-owned staging buffers.
- `task_stacks_in_psram: true` applies to the intercom TX task and transport
  tasks where supported by the transport.

## Experimental Native Dual-Bus Test

See:

```text
yamls/experimental/native-dual-bus/generic-s3-native-dual-bus-intercom-tcp.yaml
```

That profile intentionally avoids `esp_audio_stack` and binds `intercom_api` to
native ESPHome `i2s_audio` microphone/speaker components. It is for regression
testing standalone full-duplex, mic-only and speaker-only modes.
