# Breaking changes

## 2026.6.2: HA softphone and full-device runtime cleanup

`2026.6.2` documents only changes from `2026.6.1`. The PBX-lite phonebook
attribute migration, native-audio split and other older migrations remain in
their original sections below.

### Lovelace card modes

The card now has an explicit mode distinction.

`hybrid` remains the default and keeps the previous behavior: the card is bound
to one ESP through `device_id` and mirrors/controls that ESP. If that ESP has
Home Assistant selected as destination, the browser/card acts as the Home
Assistant audio leg and starts a call toward the attached ESP.

`ha_softphone` is the new mode requested in
[#46](https://github.com/n-IA-hane/esphome-intercom/issues/46). It represents
Home Assistant itself as an independent intercom endpoint, with its own
destination selector, Auto Answer and Do Not Disturb state.

| Card intent | Configuration |
|---|---|
| Mirror/control one ESP endpoint | `mode: hybrid` with `device_id` |
| One dashboard card representing Home Assistant itself | `mode: ha_softphone` with no `device_id` |

### Lovelace card option cleanup

The extended-info card option is now `show_extended_info`. Copied card YAML that
still uses the previous extended-info key must be updated; the old key is not
kept as a compatibility alias.

### Home Assistant as a direct PBX-lite callee

Home Assistant now accepts valid inbound PBX-lite calls addressed to HA itself
even when the caller is not already present in the ESP phonebook. The phonebook
is still required when HA must resolve or forward a call to another endpoint.

This mainly matters for diagnostics, custom clients and softphone-like tools: HA
can ring as an endpoint, while routing to another ESP still uses the phonebook.

### YAML: full AFE runtime defaults

Maintained full AFE profiles now boot with the runtime entities reflecting the
pipeline that actually starts on the device: AEC and VAD default ON, and boards
with enough headroom default to FD high-perf. Users can still switch AEC/VAD/AFE
mode from Home Assistant after boot.

If you copied old full AFE YAML blocks, check that your template switches/selects
do not restore stale HA values over the boot pipeline unless that is explicitly
what you want.

### Optional Voice Assistant timers

Voice Assistant timer support is provided as an optional package. It is not part
of the headless core package. Add it only to devices that should expose timer
behavior or UI hooks.

Display YAMLs that include timers now treat the active timer alarm as the owner
of the display until it is dismissed. Custom display YAMLs that copied only the
old `on_timer_finished` action should use `id(timer_ringing).state` as the
authoritative condition, not only a one-shot Voice Assistant phase value.

### YAML: full-experience media player speaker fork

Maintained full-experience YAMLs that use `platform: speaker` media playback
now include the project-local `speaker` external component. This keeps ESPHome's
normal speaker/media-player model, but adds `pause_releases_pipeline` so HA
media pause releases the media pipeline before TTS, timer alarms or intercom
audio need the same speaker graph.

Users of maintained YAMLs do not need to edit anything. Custom full-experience
YAMLs that copied only the media-player block must either include
`packages/media_player/full_mono_48k.yaml` or carry both pieces:

```yaml
external_components:
  - source: github://n-IA-hane/esphome-intercom
    ref: main
    components: [speaker]

media_player:
  - platform: speaker
    pause_releases_pipeline: true
```

Native intercom-only YAMLs without `platform: speaker` media playback do not
need this fork.

### Build cache

After moving to `2026.6.2`, clear ESPHome build caches once before compiling.
This matters because the audio backend, IDF managed components and generated
sdkconfig can change at the same time.

```bash
find . -type d -name .esphome -prune -exec rm -rf {} +
```

## 2026.5.0: PBX-lite protocol migration

`2026.5.0` is a major upgrade from the `4.x` line. The project moved from
"PBX-like" wiring to a real **PBX-lite** protocol: still deliberately small, but
with the pieces an intercom system needs in practice: endpoint-aware phonebook
rows, explicit call state, ringing, answer, decline, hangup, error reasons,
direct same-transport ESP calls, HA bridge/PBX routing and browser softphone
legs.

If you are upgrading from a working installation, apply these edits before you
flash the new firmware or restart Home Assistant.

## YAML: `intercom_api`

| Was | Is | Action |
|---|---|---|
| `mode: simple` | _(unset)_ | Remove the line. PBX-lite is the implicit default. |
| `mode: full` | _(unset)_ | Remove the line. PBX-lite is the implicit default. |
| `mode: webrtc` | `mode: raw_udp` | Rename. Same semantics: audio-only UDP, no signaling. |

The `mode:` key is now optional and only accepts `raw_udp`. Any other value fails ESPHome validation at compile time.

PBX-lite is the default. The old `simple` / `full` distinction is gone because
there is no longer a separate "doorbell mode" versus "full intercom mode": a
doorbell is just a phonebook with one HA/browser destination, and a room
intercom is the same state machine with more contacts.

## YAML tree: dual-bus boards

The never-validated generic dual-bus YAML was replaced by maintained
`esp_audio_stack` TCP/UDP profiles. These profiles use `rx_bus` / `tx_bus`
instead of the old `i2s_audio` + standalone `intercom_api.processor_id` path.

| Old path | New path |
|---|---|
| `yamls/intercom-only/dual-bus/generic-s3-dual-intercom_NOT_READY.yaml` | `yamls/intercom-only/dual-bus/generic-s3-intercom-tcp.yaml` or `yamls/intercom-only/dual-bus/generic-s3-intercom-udp.yaml` |
| `yamls/experimental/dual-bus/intercom-only/generic-s3-dual-intercom.yaml` | `yamls/intercom-only/dual-bus/generic-s3-intercom-tcp.yaml` or `yamls/intercom-only/dual-bus/generic-s3-intercom-udp.yaml` |

Update any local fork or symlink that pointed at the old paths.

## Home Assistant: bus events

The separate HA bus events were replaced by one unified call event. If you have
automations or scripts triggering on these, update the trigger:

| Was | Is |
|---|---|
| `intercom_state` / `intercom_native_state_changed` | `intercom_native.call_event` with `scope: session` |
| `intercom_bridge_state` / `intercom_native_bridge_state_changed` | `intercom_native.call_event` with `scope: bridge` |
| `intercom_forward_state` / `intercom_native_forward_state_changed` | `intercom_native.call_event` with `scope: forward` |

Use `type` for automations (`outgoing`, `ringing`, `answered`, `ended`,
`missed`, `failed`) and `state` when you need the exact internal state. The
state-text ESPHome sensor (`sensor.<name>_intercom_state`) is unchanged.

## Home Assistant: phonebook and HA peer name

The phonebook is now endpoint-first. ESPs publish
`sensor.<device>_intercom_endpoint` as `Name|protocol|ip|ports`, HA builds
`sensor.intercom_phonebook`, and firmware packages subscribe to its `phonebook`
attribute.

Do not rely on a hardcoded `"Home Assistant"` contact anymore. The HA peer name
is `hass.config.location_name`, so the contact can be `"Home"`, `"Office"`,
`"Beach House"` or any other name chosen in HA settings.

## C++: `intercom_api` namespace

Only relevant if you have downstream code against the `IntercomApi` C++ class:

- `IntercomApi::set_full_mode(bool)` removed. It was a no-op since the simple/full distinction was retired.
- `IntercomApi::set_webrtc_mode(bool)` renamed to `set_raw_udp_mode(bool)`.
- Protected member `webrtc_mode_` renamed to `raw_udp_mode_`.
