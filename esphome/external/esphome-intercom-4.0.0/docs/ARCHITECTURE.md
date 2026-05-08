# Architecture

How the audio stack is decomposed, which task owns which buffer, and where the non-obvious decisions live. Written for a new contributor who wants to orient in one sitting without reading every `.cpp`.

Reference hardware: Waveshare ESP32-S3 Touch LCD 2.1B (ES8311 speaker codec + ES7210 TDM 2-mic ADC). ESP32-P4 and Xiaozhi Ball V3 follow the same layout with minor transport differences noted inline.

---

## 1. Component map

```
                          ┌────────────────────┐
                          │ ESPHome core        │
                          │ (WiFi, API, logger) │
                          └──────────┬──────────┘
                                     │
          ┌──────────────────────────┴──────────────────────────┐
          │                                                     │
          ▼                                                     ▼
 ┌─────────────────┐   raw mic frames     ┌───────────────────────────────┐
 │ i2s_audio_duplex│ ───────────────────▶ │ audio_processor               │
 │ (transport)     │   (via callbacks)    │ (abstract pipeline interface) │
 └────────┬────────┘                      └──────────┬────────────────────┘
          │                                          │
          │ speaker PCM                              │ implementations:
          ▼                                          │   • esp_afe (full AFE)
     ES8311 DAC                                      │   • esp_aec (AEC only)
                                                     │   • passthrough
                                                     ▼
                                       ┌──────────────────────────────┐
                                       │ Consumers (register at setup)│
                                       │  • intercom_api (TX)         │
                                       │  • micro_wake_word (MWW)     │
                                       │  • voice_assistant (VA)      │
                                       └──────────────────────────────┘
```

Ownership rules:
- `i2s_audio_duplex` owns the I²S peripheral, DMA buffers and the audio task. It knows nothing about AEC/BSS; it just moves frames.
- `audio_processor` is the contract. It takes an interleaved mic frame and returns an interleaved mic frame, optionally with VAD/beamforming metadata.
- `esp_afe` / `esp_aec` are `audio_processor` implementations wrapping Espressif's esp-sr library. They own their worker tasks.
- Consumers register with `i2s_audio_duplex` to receive processed mic frames. They never talk to the processor directly.

---

## 2. Threading model

All audio components share three conventions:

1. **Realtime I/O on Core 0**, inference and UI on Core 1.
2. **Hot path priority ≥ 19**, heavy worker priority ≤ 5, so BSS/AEC saturation can never starve I²S.
3. **No allocation in the audio task** after `setup()`. Buffers are pre-sized for worst case (2-mic MR); sub-slices are used at runtime.

| Task | Component | Core | Priority | Stack | Role |
|------|-----------|:---:|:-------:|:-----:|------|
| `i2s_audio_task` | `i2s_audio_duplex` | 0 | 19 | 8 KB PSRAM | I²S read/write, decimation, callbacks |
| `afe_feed` | `esp_afe` | 1 | 5 | 12 KB | Pops frames, calls `afe_handle_->feed()` |
| `afe_fetch` | `esp_afe` | 1 | 7 (`task_priority_ − 1`) | 4 KB | Blocks on `fetch_with_delay()`, writes ring |
| `intercom_srv` | `intercom_api` | 1 | 5 | PSRAM static | TCP accept/read/write |
| `intercom_tx` | `intercom_api` | 0 | 5 | PSRAM static | Mic capture → AEC → network (AEC-own mode) |
| `intercom_spk` | `intercom_api` | 0 | 4 | PSRAM static | Speaker playback + AEC reference |
| WiFi / lwIP / TCP-IP | ESP-IDF | 0/1 | 18/23 | n/a | System |
| MWW inference | `micro_wake_word` | 1 | 1 | n/a | TFLite inference |
| LVGL | `display` | 1 | 1 | n/a | UI render |

Why the priority choices:
- **I²S at 19** is between lwIP (18) and WiFi (23): high enough that network can't starve audio, low enough that WiFi stays responsive.
- **feed_task at 5** matches the esp-skainet reference. The BSS worker runs at the same priority on the same core with round-robin; `feed()` can block on the esp-sr internal ring without affecting the realtime path.
- **speaker/MWW/LVGL at 1** is the ESPHome convention for non-realtime work that can tolerate starvation under I/O pressure.

Core affinity:
- Core 0 is the canonical Espressif AEC core (voice pipeline + ES7210 DMA).
- Core 1 is reserved for inference (MWW), UI (LVGL) and the AFE workers. BSS dual-mic workers share core 1 with MWW; the priority gap keeps them cooperative.

The 2-mic `feed_task` runs as a dedicated task at priority 5 (not inline in the audio task). BSS processing can take longer than one frame under load; if `feed()` were called inline, the audio task would block on the esp-sr internal ring and drop frames.

---

## 3. Data flow for the S3 full AFE (MMR: 2-mic BSS + AEC + NS + AGC + VAD)

One frame = 32 ms = 512 samples @ 16 kHz per channel.

```
  ES7210 (2 mic + ref TDM) ─DMA─▶ i2s_audio_task (core 0, prio 19)
                                    │
                                    │ decimate 48k→16k (FIR, kernel selectable
                                    │                   via fir_decimator yaml:
                                    │                   dsps_fird_s16 SIMD or
                                    │                   custom float scalar)
                                    │ build interleaved frame
                                    │ raw_mic_callback (intercom_tx_passthrough,
                                    │                   diagnostics)
                                    ▼
                            audio_processor->process(frame)
                              = esp_afe::process()
                                    │
                                    │ assemble full feed frame (mic L, mic R,
                                    │                            reference)
                                    │ push NOSPLIT into feed_input_ring_
                                    │ (non-blocking, atomic)
                                    ▼
                            afe_feed task (core 1, prio 5)
                                    │
                                    │ pop frame
                                    │ afe_handle_->feed()   ← BSS worker fires
                                    │                         here, may block
                                    ▼
                            [esp-sr internal ring, owned by esp-sr]
                                    │
                                    ▼
                            afe_fetch task (core 1, prio 7)
                                    │
                                    │ afe_fetch_with_delay()  ← returns
                                    │                           processed mic +
                                    │                           VAD state
                                    │ push into fetch_output_ring_
                                    ▼
                            audio_processor->process() return
                              (reads fetch_output_ring_ non-blocking)
                                    ▼
                          i2s_audio_task emits mic_callback(frame)
                                    │
                       ┌────────────┼────────────┬──────────────┐
                       ▼            ▼            ▼              ▼
                 intercom_api    MWW        voice_assistant    (user cbs)
                 TX (if no      inference  start/stream
                  own AEC)
```

Timing budget per 32 ms frame:
- I²S DMA fill: 32 ms (hardware)
- Decimation + NOSPLIT push: < 1 ms (core 0)
- feed() BSS + AEC: 5–12 ms (core 1, async)
- fetch() + ring write: 1–2 ms (core 1, async)

End-to-end latency from mic to consumer: ~96 ms (three 32 ms frame periods, two in esp-sr internal buffering).

MR (1-mic) path: same diagram, `total_channels_=2` (mic + ref) instead of 3. The feed stack is still sized for MR worst case (WebRTC NS inline → 12 KB).

---

## 4. `audio_processor` contract

This is the only interface consumers see. Its stability is what lets `esp_afe`, `esp_aec` and the passthrough implementation be hot-swappable.

```cpp
class AudioProcessor {
 public:
  // Stable identity: sample rate, mic channels, frame size.
  virtual FrameSpec frame_spec() const = 0;

  // Monotonic counter. Increments whenever frame_spec() changes
  // (e.g. SE on↔off flips mic_channels between 2 and 1). Consumers
  // that cache buffer sizes observe this and reallocate.
  virtual uint32_t frame_spec_revision() const = 0;

  // Feed one frame in, get one frame out. In-place safe.
  // Must not block; returns false if the processor is paused.
  virtual bool process(const int16_t *in, int16_t *out) = 0;

  // Optional: VAD state, beamforming direction, etc.
  virtual ProcessorState state() const { return {}; }
};
```

Invariants the processor promises:
1. `frame_spec()` is stable between `frame_spec_revision()` bumps.
2. `process()` is call-from-any-task safe but **not** concurrent-safe. `i2s_audio_duplex` serialises all calls from its audio task.
3. Output frame shape always matches `frame_spec()` after the bump has been observed.

Invariants the caller must respect:
1. Observe `frame_spec_revision()` before reading `frame_spec()` each cycle.
2. Never call `process()` concurrently from multiple tasks.
3. When frame_spec changes, internal buffers (decimator ratio, reference extraction, ring-buffer sizes) must be recomputed before the next call.

`i2s_audio_duplex` implements this via a permanent audio task that detects revision bumps at the top of each iteration and reinitialises its local buffers in place, without recreating the FreeRTOS task.

---

## 5. Drain protocol (config change without stopping the task)

AEC toggle, SE toggle and NS toggle all change the esp-sr instance shape without tearing down `i2s_audio_duplex`. The implementation is a lock-free three-state handshake between `process()` (hot path) and `set_aec_enabled_runtime_()` / `recreate_instance_()` (config path).

Three atomics:

```
process_active_  : set by process() on entry, cleared on exit
drain_requested_ : set by config task to request quiesce
config_epoch_    : bumped on each successful swap
```

Sequence, config task side:
```
  drain_requested_ = true
  wait until process_active_ == 0 (spin with yield, bounded)
  take config_mutex_
  free old esp-sr instance
  create new esp-sr instance
  bump config_epoch_
  release config_mutex_
  drain_requested_ = false
```

Sequence, `process()` side:
```
  if (drain_requested_) return paused-output
  process_active_ = true
  … do work …
  process_active_ = false
```

Atomics, not a mutex, on the hot path: asymmetric cost. The hot path runs at 31 Hz, the config path at most once per second during user toggles. A mutex would pay lock/unlock on every frame; atomics pay only under a flag that is normally false.

The protocol is documented as a block comment in `esphome/components/esp_afe/esp_afe.h` so future contributors see it next to the code.

---

## 6. Notable design decisions

### 6.1 Why `i2s_audio_duplex` owns the audio task, not `audio_processor`

The transport owns the single audio task and exposes raw + processed frames via callbacks. The processor exposes `process()` synchronously and runs its own async workers internally.

Alternative considered: move the task into the processor, let the transport be a pure DMA pump. Rejected because the transport has hardware knowledge (TDM vs stereo, decimation ratio, reference channel placement) that the processor does not want to know, and two of the four supported use cases (intercom-only, intercom+AEC) don't have an AFE-style processor at all.

### 6.2 Why the consumer list in `i2s_audio_duplex`, not in the processor

Consumers want raw mic frames (diagnostics, intercom passthrough) *and* processed frames (MWW, VA). The transport is the only component that sees both. Putting the consumer list in the processor would force the transport to re-plumb raw callbacks separately.

### 6.3 Why esp-sr lives behind `audio_processor` and not used directly

Three reasons: the passthrough and `esp_aec` implementations don't need esp-sr; `frame_spec_revision` is a narrower contract than esp-sr's runtime reconfigure API; and the contract can be mocked without bringing up DMA, which makes consumers unit-testable.

### 6.4 Why static-allocation PSRAM stacks in `intercom_api`

PSRAM stacks on S3/P4 are the ESPHome-blessed pattern for large TCP server tasks where the stack peak is known. Internal RAM stays free for the BSS worker and MWW inference. See `esphome/components/intercom_api/intercom_api.h` for the per-task sizing rationale in code comments.

### 6.5 Why the `esp_afe` feed/fetch tasks are created dynamically

The task lifetime matches the AFE instance lifetime: toggling all features off tears the AFE down, toggling any feature back on rebuilds it. Dynamic `xTaskCreatePinnedToCore` is the natural fit for that lifecycle. Static TCBs would require reuse across stop/start, which ran into a FreeRTOS ready-list corruption under rapid reconfigure bursts on core 1. Dynamic creation pays ~16 KB heap churn per reconfigure; on a 260 KB internal heap this is acceptable.

### 6.6 Why the mic consumer registry, not a refcount

Consumers register once at setup. The transport tracks them as opaque tokens in a `std::vector`, so a `stop()` followed by `start()` (as happens during an internal reconfigure) does not lose the registration. The original implementation used an atomic refcount that was zeroed on `stop()`, which silently disconnected MWW / VA / intercom after every feature toggle. The registry is the structural fix: consumers survive transport restarts by construction.

### 6.7 Why the `i2s_audio_duplex` audio task is permanent

Reconfigure (e.g. 2-mic → 1-mic when beamforming toggles off) does not destroy the audio task. The task sits on a `frame_spec_revision` observer loop; when the revision bumps it reinitialises the decimator, the reference extraction and the output buffers in place, then resumes. Destroying and recreating the task on every toggle would cause audible gaps and lose consumer state that is keyed off the task handle.

---

## 7. The "all features disabled" fast path

`esp_afe` supports `aec_enabled=false`, `se_enabled=false`, `ns_enabled=false`, `agc_enabled=false`. With all four off, the esp-sr instance has nothing to do and `process()` short-circuits to a memcpy of the input frame.

Why it exists: symmetric config surface. Users can disable any subset, including all. A `dump_config` that reports "all features disabled, component is a passthrough" is less surprising than one that errors out.

Why it stays: it costs nothing (a single `if` on the hot path), has no runtime risk, and is exercised during reconfigure transitions (brief windows where a user has toggled all features off before re-enabling one). Removing it would force consumers to drop the processor entirely in this edge case, which they can't easily do at runtime.

---

## 8. Open design questions

Questions a fresh designer would ask, and the current answer.

### 8.1 Should `audio_processor` own its task?

**Current**: no, the transport owns the single audio task and the processor exposes `process()` synchronously.

**Alternative**: processor owns a task, transport posts frames to a queue. Cleaner separation.

**Why not in the current design**: the intercom-only build has no processor task to own anything. Two of four use cases would have an empty abstraction.

**Revisit if**: a new use case emerges where the processor is always present and the transport is pluggable.

### 8.2 Should the intercom wire format be Protobuf?

**Current**: hand-packed struct (4-byte header + PCM).

**Alternative**: Protobuf, with a Home Assistant native integration on top.

**Why not in the current design**: no current client justifies a breaking wire change. The hand-packed header is stable, 4 bytes of overhead per frame, and easy to parse in any language.

**Revisit if**: a native Home Assistant intercom integration becomes a requirement and the binary protocol blocks it.

### 8.3 Should the drain protocol be a FreeRTOS EventGroup?

**Current**: three atomics.

**Alternative**: `xEventGroupWaitBits` / `xEventGroupSetBits` on the config side.

**Why not in the current design**: atomics are lock-free on the hot path, EventGroup is not. EventGroup gives a blocking wait on the config side for free, which atomics simulate with a bounded spin. For asymmetric workloads (31 Hz hot path vs ~1 Hz config) the atomic implementation wins.

### 8.4 Should there be a test-matrix of real YAMLs for every topology?

**Current**: "intercom only" and "full AFE" have YAMLs under `yamls/`. "intercom + AEC standalone" and "duplex standalone" are verified only by code review.

**Why not in the current design**: no shipping device uses those intermediate topologies, so the maintenance surface is not justified.

**Revisit if**: a user asks for one of those configurations, or a reconfigure bug that only those paths would catch lands on a shipping device.

---

## 9. Where to look for what

| Question | File |
|---|---|
| How is a mic frame delivered to consumers? | `esphome/components/i2s_audio_duplex/i2s_audio_duplex.cpp` `audio_task_()` |
| How does the AFE pipeline swap config without glitches? | `esphome/components/esp_afe/esp_afe.cpp` `recreate_instance_()` + drain protocol |
| Where is the TCP wire format defined? | `esphome/components/intercom_api/intercom_protocol.h` |
| How is PSRAM vs internal RAM placement decided? | `esphome/components/audio_processor/ring_buffer_caps.h` |
| How do I register a new mic consumer? | Call `I2SAudioDuplex::register_mic_consumer()` from the consumer's `setup()` |
| How do I add a new processor implementation? | Subclass `AudioProcessor` in `esphome/components/audio_processor/audio_processor.h`, ship as a new component |

---

## 10. Related reading

- [`../README.md`](../README.md): project overview and quick-start
- [`DEPLOYMENT_GUIDE.md`](DEPLOYMENT_GUIDE.md): which YAML preset to pick
- [`reference.md`](reference.md): full option, action and service reference
- Per-component READMEs live alongside each component in [`../esphome/components/`](../esphome/components/)
