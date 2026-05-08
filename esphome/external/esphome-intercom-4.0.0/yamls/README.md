# Device Configurations

Ready-to-flash ESPHome YAML configurations for tested hardware.

## How to use

1. Download the YAML file for your device
2. Create a `secrets.yaml` with your WiFi credentials:
   ```yaml
   wifi_ssid: "your_network"
   wifi_password: "your_password"
   ```
3. Compile with ESPHome. The public YAMLs on `main` point at the GitHub copy of this repository, so components, packages, and assets are fetched automatically.

## Structure

```
yamls/
  intercom-only/         Intercom without Voice Assistant or Wake Word
    single-bus/          Devices using i2s_audio_duplex (mic+speaker on same I2S bus)
    dual-bus/            Devices with separate I2S buses for mic and speaker

  full-experience/       VA + MWW + Intercom (complete voice assistant hub)
    single-bus/
      aec/               i2s_audio_duplex + esp_aec
      afe/               i2s_audio_duplex + esp_afe
    dual-bus/            Separate I2S buses + esp_aec
```

## Single-bus vs Dual-bus

- **Single-bus**: mic and speaker share one I2S peripheral via `i2s_audio_duplex`. Used by devices with audio codecs (ES8311, ES7210+ES8311). Enables stereo AEC reference, TDM multi-mic, and 48kHz bus rate with FIR decimation to 16kHz.

- **Dual-bus**: mic and speaker on separate I2S peripherals using standard ESPHome `i2s_audio`. Simpler setup for MEMS mic + class-D amp boards (SPH0645 + MAX98357A).

## Audio processor: esp_aec vs esp_afe

- **esp_aec**: Lightweight echo cancellation only (~40 KB). Recommended for intercom-only and single-mic setups.
- **esp_afe**: Full Espressif AFE pipeline (AEC + NS + VAD + AGC + optional dual-mic BSS voice isolation). Higher RAM cost, but adds the full frontend and runtime diagnostics. See the [esp_afe component README](../esphome/components/esp_afe/README.md) for details.

## Local development vs release mode

The public YAMLs in `main` are stored in **remote mode** so they compile
straight from GitHub. If you are working inside a local clone and want
them to point back at your checkout, run:

```bash
./scripts/yaml_paths.sh local
```

When you are ready to switch them back to the published form:

```bash
./scripts/yaml_paths.sh remote --branch main
```

## Not sure which one to pick?

See [../docs/DEPLOYMENT_GUIDE.md](../docs/DEPLOYMENT_GUIDE.md) for a decision tree that maps hardware and requirements to the right preset.
