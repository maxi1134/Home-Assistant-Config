# Intercom Audio (UDP)

> **Note:** This is the original UDP-based audio streaming component. For most use cases, the newer `intercom_api` (TCP-based, with call state machine, HA integration, and PBX routing) is recommended instead.

Simple UDP audio streaming between ESP32 devices. No server required, no HA dependency. Point two ESPs at each other and talk.

## Installation

```yaml
external_components:
  - source: github://n-IA-hane/esphome-intercom@main
    components: [intercom_audio]
```

## ESP-to-ESP Example

Two ESPs talking to each other directly via UDP:

**Device A** (192.168.1.10):
```yaml
intercom_audio:
  id: intercom
  microphone_id: mic
  speaker_id: spk
  listen_port: 12346
  remote_ip: "192.168.1.11"
  remote_port: 12346
```

**Device B** (192.168.1.11):
```yaml
intercom_audio:
  id: intercom
  microphone_id: mic
  speaker_id: spk
  listen_port: 12346
  remote_ip: "192.168.1.10"
  remote_port: 12346
```

Both devices need a microphone and speaker configured separately (standard ESPHome `i2s_audio` platform). Works also with `i2s_audio_duplex` via `duplex_id` instead of `microphone_id` + `speaker_id`.

## TX-Only Example (Baby Monitor)

```yaml
intercom_audio:
  id: baby_monitor
  microphone_id: mic
  listen_port: 12346
  remote_ip: "192.168.1.100"
```

## go2rtc / WebRTC

The UDP stream is raw 16-bit PCM mono. It can be picked up by go2rtc for browser playback via WebRTC. Configure go2rtc to listen on the same UDP port and format.

## Configuration

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `duplex_id` | ID | - | i2s_audio_duplex (single-bus) |
| `microphone_id` | ID | - | Standard microphone component |
| `speaker_id` | ID | - | Standard speaker component |
| `aec_id` | ID | - | Optional esp_aec for echo cancellation |
| `listen_port` | int | 12346 | UDP port to listen on |
| `remote_ip` | string | "" | Remote device IP |
| `remote_port` | int | 12346 | Remote device port |
| `buffer_size` | int | 8192 | Jitter buffer size in bytes |
| `prebuffer_size` | int | 2048 | Bytes to buffer before playback |
| `on_start` | automation | - | Actions when streaming starts |
| `on_stop` | automation | - | Actions when streaming stops |

## Actions

```yaml
# Start streaming
- intercom_audio.start:
    id: intercom
    remote_ip: "192.168.1.100"

# Stop streaming
- intercom_audio.stop:
    id: intercom
```

## Sensors

```yaml
sensor:
  - platform: intercom_audio
    intercom_audio_id: intercom
    tx_packets:
      name: "TX Packets"
    rx_packets:
      name: "RX Packets"

switch:
  - platform: intercom_audio
    intercom_audio_id: intercom
    streaming:
      name: "Streaming"
```
