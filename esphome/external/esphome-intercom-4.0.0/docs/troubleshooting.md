# Troubleshooting

Common symptoms and fixes when setting up ESPHome Intercom.

## Contents
- [Card shows "No devices found"](#card-shows-no-devices-found)
- [No audio from ESP speaker](#no-audio-from-esp-speaker)
- [No audio from browser](#no-audio-from-browser)
- [Echo or feedback](#echo-or-feedback)
- [High latency](#high-latency)
- [ESP shows "Ringing" but browser doesn't connect](#esp-shows-ringing-but-browser-doesnt-connect)
- [Full mode: ESP doesn't see other devices](#full-mode-esp-doesnt-see-other-devices)

---

## Card shows "No devices found"

1. Verify `intercom_native:` is in `configuration.yaml`
2. Restart Home Assistant after adding the integration
3. Ensure ESP device is connected via ESPHome integration
4. Check ESP has `intercom_api` component configured
5. Clear browser cache and reload

## No audio from ESP speaker

1. Check speaker wiring and I2S pin configuration
2. Verify `speaker_enable` GPIO if your amp has an enable pin
3. Check volume level (default 80%)
4. Look for I2S errors in ESP logs

## No audio from browser

1. Check browser microphone permissions
2. Verify HTTPS (required for getUserMedia)
3. Check browser console for AudioContext errors
4. Try a different browser (Chrome recommended)

## Echo or feedback

1. Enable AEC: create an audio processor and link it via `processor_id`. With `i2s_audio_duplex`, both `esp_aec` and `esp_afe` are supported. With `intercom_api` alone (no duplex in front), use `esp_aec` only.
2. Ensure AEC switch is ON in Home Assistant
3. Reduce speaker volume
4. Increase physical distance between mic and speaker

## High latency

1. Check WiFi signal strength (should be > -70 dBm)
2. Verify Home Assistant is not overloaded
3. Check for network congestion
4. Reduce ESP log level to `INFO` or `WARN`, and disable audio telemetry if you enabled it for tuning

## ESP shows "Ringing" but browser doesn't connect

1. Check TCP port 6054 is accessible
2. Verify no firewall blocking HA→ESP connection
3. Check Home Assistant logs for connection errors
4. Try restarting the ESP device

## Full mode: ESP doesn't see other devices

1. Ensure all ESPs use `mode: full`
2. Verify `sensor.intercom_active_devices` exists in HA
3. Check ESP subscribes to this sensor via `text_sensor: platform: homeassistant`
4. Devices must be online and connected to HA
