# Diagnostics Packages

These packages expose production-safe Home Assistant diagnostic entities.

- `native.yaml`: standard ESPHome diagnostics, including Wi-Fi signal.
- `native_no_wifi_signal.yaml`: same diagnostics without Wi-Fi RSSI polling. Use
  this on ESP32-P4 + ESP32-C6 hosted Wi-Fi targets because RSSI polling goes
  through esp-hosted RPC.

These are not runtime debug packages. They use ESPHome's native `debug`
component internally to publish heap, reset reason, device info and loop timing
as `entity_category: diagnostic`.

Low-level runtime debug packages live under `packages/debug/`, for example
`packages/debug/runtime_diag.yaml`.
