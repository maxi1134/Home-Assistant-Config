# Deployment guide

A short map of the YAML tree so you can pick the right starting point for
a new device without reading every file.

```
yamls/
├── intercom-only/     no wake word, no voice assistant
│   ├── dual-bus/      mic and speaker on separate I2S buses
│   └── single-bus/    one full-duplex I2S bus (codec or per-chip ADC+DAC)
└── full-experience/   intercom + wake word + voice assistant
    ├── dual-bus/
    └── single-bus/
        ├── aec/       mic pipeline = esp_aec (echo cancellation only)
        └── afe/       mic pipeline = esp_afe (AEC + NS + AGC + optional BSS)
```

## Decision tree

Follow the first branch that matches your hardware and intent.

1. **Do you want wake word + voice assistant on the device, or only
   intercom-style room-to-room calls?**
   - Just intercom → `yamls/intercom-only/`
   - Wake word / VA too → `yamls/full-experience/`

2. **How many I2S buses does your hardware expose?**
   - Two (mic bus + speaker bus, independent pins) → `dual-bus/`
   - One full-duplex bus (codec does both, or a shared bus with TDM) →
     `single-bus/`

3. **(full-experience / single-bus only) Do you have two microphones?**
   - Yes, and you want beamforming / noise suppression / AGC →
     `afe/` (runs `esp_afe`, esp-sr 2-mic BSS)
   - One mic, or you only need echo cancellation → `aec/` (runs
     `esp_aec`, echo cancellation only)

## Concrete examples

| Device | Chip | Mics | Config | YAML |
|---|---|---|---|---|
| Waveshare S3 Audio board | ESP32-S3 | 2 (ES7210 TDM) | full + afe | `full-experience/single-bus/afe/waveshare-s3-full-afe.yaml` |
| Waveshare P4 touch panel | ESP32-P4 | 2 (ES7210 TDM) | full + afe | `full-experience/single-bus/afe/waveshare-p4-touch-full-afe.yaml` |
| Xiaozhi Ball V3 | ESP32-S3 | 1 (ES8311 ADC) | full + afe (SR low-cost) | `full-experience/single-bus/afe/xiaozhi-ball-v3-full-afe.yaml` |
| Xiaozhi intercom-only | ESP32-S3 | 1 | intercom + single | `intercom-only/single-bus/xiaozhi-intercom.yaml` |
| Generic S3 speaker + MEMS | ESP32-S3 | 1 | intercom + dual | `intercom-only/dual-bus/generic-s3-dual-intercom_NOT_READY.yaml` |
| Generic S3 single-bus MEMS+amp | ESP32-S3 | 1 | intercom + duplex | `intercom-only/single-bus/generic-s3-intercom.yaml` |
| Generic S3 single-bus MEMS+amp + VA/MWW | ESP32-S3 | 1 | full + aec (SR low-cost) | `full-experience/single-bus/aec/generic-s3-full-aec.yaml` |

### AEC engine standard: VOIP for intercom-only, SR for full-experience

The `AEC Mode` select in every public YAML is restricted to a single esp-sr
engine to keep runtime mode switches stable. esp-sr's `aec_create()` has a
silent FFT-table calloc-fail bug on cross-engine transitions when
`filter_length > 4`; staying inside one engine sidesteps it entirely.

| Tier | filter_length | mode default | select runtime | Engine |
|---|---|---|---|---|
| **Intercom-only** (no MWW) | 8 | `voip_high_perf` | `voip_low_cost`, `voip_high_perf` | dios_ssp_aec |
| **Full-experience AEC** (with MWW) | 4 | `sr_low_cost` | `sr_low_cost`, `sr_high_perf` | esp_aec3 |

Why the split:

- **Intercom-only** wants human-ear quality, so the VOIP modes' non-linear
  residual echo suppressor is exactly what you want. `filter_length: 8` is
  required because voip frames are 16 ms each (vs 32 ms in SR), so 8 taps
  give the same 128 ms of echo coverage as `filter_length: 4` in SR.
- **Full-experience AEC** runs MWW on the post-AEC mic, and the VOIP RES
  destroys the spectral features the wake-word neural model relies on
  (10/10 → 2/10 detection rate observed). SR modes are linear-only and
  preserve the spectrum, so MWW keeps working.

Default `aec_reference: previous_frame` works for both. Switch to
`aec_reference: ring_buffer` (with `aec_reference_buffer_ms`) only if a
specific room/hardware combo shows residual echo with the default.

## Hardware sizing

- **AFE + 2 mic BSS + two concurrent HTTPS streams** (music + TTS) is
  tight on internal RAM. On S3 boards enable
  `i2s_audio_duplex.audio_stack_in_psram: true` (see the
  `i2s_audio_duplex/README.md` "Advanced options" section) and
  `CONFIG_MBEDTLS_HARDWARE_AES: "n"`, otherwise `esp-aes` can fail to
  allocate DMA memory and break TLS reads mid-stream.
- **1-mic SR low-cost** (Xiaozhi) does not stress the budget and needs
  no extra tuning.
- **ESP32-P4** has more internal RAM than S3 and generally does not
  need the PSRAM-stack workaround.

## Path resolution: local vs remote

Each public YAML carries one line per external resource. Three knobs:

- `substitutions.ext_components_source` — where ESPHome resolves
  `external_components` from. Local: `../../../esphome/components`.
  Remote: `github://OWNER/REPO@BRANCH`.
- `substitutions.assets_base` — where image/audio assets are fetched
  from. Local: `../../../`. Remote:
  `https://github.com/OWNER/REPO/raw/BRANCH/`.
- `packages:` entries — each one is either `!include ../../../packages/<name>.yaml`
  (local) or `github://OWNER/REPO/packages/<name>.yaml@BRANCH` (remote).

The public YAMLs on `main` ship in **remote mode** so they compile
directly from GitHub without manual path edits.

If you are developing inside a local clone and want the YAMLs to point
back at your working tree, run:

```bash
./scripts/yaml_paths.sh local
```

To switch them back to release mode, point them at `main`, a release
branch, or your own fork:

```bash
./scripts/yaml_paths.sh remote --branch main
./scripts/yaml_paths.sh remote --url github://YOUR-USER/esphome-intercom --branch YOUR-BRANCH
```

For one-off local tweaks (wifi credentials, GPIO swaps, board-specific
sdkconfig), create a sibling `<board>-local.yaml` (gitignored via
`*-local.yaml`) that imports the public YAML and overrides only the
blocks you need.

## When to touch the sdkconfig

The YAMLs ship sdkconfig defaults sized for the common case. Board-specific
overrides that the defaults do not cover:

- `CONFIG_MBEDTLS_HARDWARE_AES: "n"` (see above, waveshare-s3 only).
- `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY: "y"`, required if you set
  `i2s_audio_duplex.audio_stack_in_psram: true`. Already on in our
  YAMLs.
- `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC` + `CONFIG_MBEDTLS_DYNAMIC_BUFFER`
  keep TLS allocations out of internal RAM. Already on in all
  full-experience YAMLs that do HTTPS streaming.

## Pointers

- Per-component docs: `esphome/components/<name>/README.md`.
- Architecture overview: `docs/ARCHITECTURE.md`.
- Troubleshooting: `docs/troubleshooting.md`.
- YAML path toggle helper: `scripts/yaml_paths.sh`.
