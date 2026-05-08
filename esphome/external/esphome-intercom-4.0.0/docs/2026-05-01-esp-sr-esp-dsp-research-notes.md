# 2026-05-01 `esp-sr` / `esp-dsp` Research Notes

Context snapshot saved before main release work.

## Local repo state at time of note

- Branch: `audio-core-v2`
- HEAD when researched: `7fa581925d65198197218485459ff89a5bf88ac9`
- Current custom component pins in source:
  - `esphome/components/esp_aec/__init__.py` -> `espressif/esp-sr ~2.3.0`
  - `esphome/components/esp_afe/__init__.py` -> `espressif/esp-sr ~2.3.0`
- In these custom components there is no local `idf_component.yml` / `library.json`; dependency wiring is via `add_idf_component()` in `__init__.py`.

## Actual resolved versions in current build artifacts

Verified from `.esphome/build/*/dependencies.lock` and managed component manifests:

- `esp-sr 2.3.1`
- `esp-dsp 1.7.0`
- `idf 5.5.4`

Resolved upstream commits in managed component manifests:

- `esp-sr 2.3.1` -> `98f7f642e12b2a3131e93455293a7c02e7e6433a`
- `esp-dsp 1.7.0` -> `c36523d685cc7f53cc17d3a6c8620e3634f906e4`

Important dependency note:

- Current `esp-sr 2.3.1` requires `esp-dsp 1.7.0` exactly.
- This is the coupling that previously made manual version bumping awkward.

## Upstream `esp-sr` findings

Latest researched upstream release was `2.4.3`.

Relevant `2.4.x` progression:

- `2.4.0`
- `2.4.1`
- `2.4.2`
- `2.4.3`

Key confirmed additions from upstream headers/docs:

- New AEC modes:
  - `AEC_MODE_FD_LOW_COST`
  - `AEC_MODE_FD_HIGH_PERF`
- New AFE type:
  - `AFE_TYPE_FD`
- New standalone AEC API:
  - `aec_create_from_config()`
  - `aec_linear_process()`
  - `aec_nlp_process()`
  - `aec_set_nlp_level()`
- New AFE config fields:
  - `aec_nlp_level`
  - `fixed_output_channel`
  - `output_playback_channel`

Interpretation:

- Upstream did **not** expose a separate named “dual-mic AEC model” such as `AEC2mic` / `AECnet`.
- The real shipped addition is **FD full-duplex AEC/AFE** plus output-channel control fields.

## Dual-mic / MISO note

Upstream AFE docs now describe `MISO`:

- dual-mic input
- single-channel output
- intended to choose the output channel with better SNR
- especially when WakeNet is not enabled in a dual-mic scene

Why this may matter here:

- Our `esp_afe` wrapper currently disables esp-sr WakeNet internally.
- Our dual-mic YAMLs are:
  - `yamls/full-experience/single-bus/afe/waveshare-s3-full-afe.yaml`
  - `yamls/full-experience/single-bus/afe/waveshare-p4-touch-full-afe.yaml`

Implication:

- After bumping to newer `esp-sr`, the new AFE output-channel fields may be worth testing on dual-mic AFE boards.
- Treat this as an experiment, not a default assumption.

## Critical MWW caveat

Do **not** assume new FD mode is automatically better for VA + MWW.

Confirmed upstream statement from `esp-sr` issue `#159`:

- SR AEC is linear.
- VC/nonlinear processing cleans echo more aggressively.
- Espressif said the nonlinear module lowers recognition rate for speech recognition and wake word use cases.

Practical interpretation:

- `FD` includes nonlinear processing / NLP.
- Therefore it is risky for wake-word retention until tested on our boards.
- Existing local guidance favoring `sr_low_cost` for VA + MWW still stands unless hardware tests prove otherwise.

## Upstream `esp-dsp` findings

Latest researched upstream release was `1.8.1`.

Relevant release summary:

- `1.8.1`: int8 row dot product and int8 matrix-vector work
- `1.8.0`: support for `esp32s31`
- `1.7.1`: includes `fird_f32` fix for ESP32-S3

There is **no release-note evidence** in `1.8.0` or `1.8.1` for a fix to the ESP32-P4 `dsps_fird_s16` / `dsps_fird_f32` optimized FIR issue.

## FIR on ESP32-P4: current conclusion

Keep `fir_decimator: custom` on P4.

Reason:

- Upstream issue `#117` is still open and directly reports FIR corruption on ESP32-P4 optimized kernels.
- Upstream issue `#102` is still open and documents HW loop errata / constraints on ESP32-P4.
- Current upstream `esp-dsp` still ships the optimized P4 FIR assembly with `esp.lp.setup`.
- Local hardware testing already confirmed the bug was **not** fixed in practice.

Therefore:

- Do **not** remove the P4 workaround based on version bump alone.
- Only revisit this after an isolated dedicated P4 FIR retest.

## `esp-dsp` primitive notes relevant to this project

Confirmed useful / shipped:

- `dsps_dotprod_s16` / `dsps_dotprod_f32`
  - optimized backends exist for S3 and P4
  - potentially useful as building blocks for custom DSP, correlation, RMS, level estimation

Less useful for shared S3/P4 work:

- `dsps_mulc_s16`
  - Xtensa-only optimized path
  - not a strong P4 lever

S3-only utility:

- `dsps_mem_*_aes3`
  - optimized memory helpers for S3 backend naming
  - not related to AES crypto / TLS acceleration
  - lower priority than simply removing copies

## Wrapper gaps if migrating to newer `esp-sr`

Current local wrappers only understand old SR/VC mode families.

`esp_afe` wrapper gaps:

- schema only exposes `sr` and `vc`
- no `fd`
- no exposure for:
  - `fixed_output_channel`
  - `output_playback_channel`
  - `aec_nlp_level`

`esp_aec` wrapper gaps:

- schema only exposes:
  - `sr_low_cost`
  - `sr_high_perf`
  - `voip_low_cost`
  - `voip_high_perf`
- no `fd_low_cost` / `fd_high_perf`
- high-perf detection logic would need updating for the new FD enum values

## Suggested migration order

1. Bump only `esp-sr` in the custom components to the latest intended version and let it pull the matching `esp-dsp`.
2. Build and hardware-test without changing YAML behavior.
3. Keep P4 on `fir_decimator: custom`.
4. Add wrapper support for `FD` and new AFE fields without enabling them by default.
5. Test `FD` only on dual-mic AFE boards:
   - Waveshare S3 AFE
   - Waveshare P4 Touch AFE
6. Judge `FD` on:
   - full-duplex echo quality
   - wake-word retention
   - STT stability
   - barge-in behavior

## Source pointers used during research

Local:

- `esphome/components/esp_aec/__init__.py`
- `esphome/components/esp_afe/__init__.py`
- `esphome/components/esp_aec/esp_aec.cpp`
- `esphome/components/esp_afe/esp_afe.cpp`
- `esphome/components/i2s_audio_duplex/README.md`
- `yamls/full-experience/single-bus/*/.esphome/build/*/dependencies.lock`
- managed component `idf_component.yml` files under `.esphome/build/*/managed_components/`

Upstream:

- `esp-sr` component registry changelog / dependency pages
- `esp-sr` upstream headers:
  - `include/esp32p4/esp_aec.h`
  - `include/esp32p4/esp_afe_config.h`
- `esp-sr` docs:
  - `docs/en/audio_front_end/README.rst`
  - `docs/en/acoustic_echo_cancellation/README.rst`
  - `docs/en/benchmark/README.rst`
- `esp-sr` example/test:
  - `test_apps/esp-sr/main/test_afe.cpp`
- `esp-dsp` changelog and FIR platform/assembly files
- issues:
  - `esp-dsp#102`
  - `esp-dsp#117`
  - `esp-sr#159`
