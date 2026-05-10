## Audio duplex, Listening Chime, and 'Stop' halt word and for voice Assist!

You have a few ESP32-S3-BOX-3s and feel like you're missing out on having a 'Stop' command to make it stop answering? 
Or you would love for it to 'chime' when he hears the wake word?

As you may or may not know, this is due to a lack of native audio [duplex](https://en.wikipedia.org/wiki/Duplex_(telecommunications)) for them within ESPHome. Which means that no audio can be played while the microphone is actively listening.

This brings those two major downsides, nominaly the lack of a 'Wake Sound' when the device starts listening, as to avoid the microphone having to wait the end of the playback to start listening, and the aforementioned issue that is the lack of a ***'Stop'*** word that can stop an answer right away, like the [Home Assistant VPES](https://www.home-assistant.io/voice-pe/) do.

And while I've been looking wide and large for a custom component that could bring audio duplex to my 9 ESPBoxes, nothing ever seemed to come out during my searches..
That is until I found the [ESPHOME-Intercom](https://github.com/n-IA-hane/esphome-intercom) project, which answered my need pretty perfectly, as it includes the `i2s_audio_duplex` custom component for the intercoms to be able to have audio duplex capalities!

---

***If you're just looking to go ham at this without reading the presentation, here is the [complete yaml file](esphome\templates\ESP32-S3-BOX-3\ESP32-s3-box-3_esphome_voice_assistant_with_audio_duplex_and_stop_word.yaml)***

---

##### Minimal Requirements: 
 - ESP32-S3-BOX-3 
 - ESPHome
 - Assist flow in place

##### Custom Components in use:
 - [ESPHOME-Intercom](https://github.com/n-IA-hane/esphome-intercom) (Credit: [n-IA-hane](https://github.com/n-IA-hane) )
    -  ([i2s_audio_duplex](https://github.com/n-IA-hane/esphome-intercom/tree/main/esphome/components/i2s_audio_duplex), [esp_aec](https://github.com/n-IA-hane/esphome-intercom/tree/main/esphome/components/esp_aec), [audio_processor](https://github.com/n-IA-hane/esphome-intercom/tree/main/esphome/components/audio_processor))

---


And now, without further ado, **The good stuff**

---

-  The first step will be to locally download the [ESPHOME-Intercom](https://github.com/n-IA-hane/esphome-intercom) project and store it on your machine, I personally have it stored under `/config/esphome/external/` on my Home-Assistant host.

- You will then add this to the YAML for your ESP32-Boxes:


```yaml
external_components:
  # Local copy of n-IA-hane/esphome-intercom @ v4.0.0
  - source:
      type: local
      path: /config/esphome/external/esphome-intercom-4.0.0/esphome/components
    components: [i2s_audio_duplex, esp_aec, audio_processor]
```

> ⚠️ **Why local?** The HA add-on host kept OOM'ing during `git clone`'s pack-indexing phase. Downloading the v4.0.0 tarball off-device and dropping it under `/config/esphome/external/` sidesteps the runtime `git` step entirely.
---
#### esp32 block addition
We will need to append a few options to the `esp32->framework->sdkconfig_options:` block to take it from:

```yaml

esp32:
  board: esp32s3box
  flash_size: 16MB
  cpu_frequency: 240MHz
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240: "y"
      CONFIG_ESP32S3_DATA_CACHE_64KB: "y"
      CONFIG_ESP32S3_DATA_CACHE_LINE_64B: "y"

```
to:

```yaml
esp32:
  board: esp32s3box
  flash_size: 16MB
  cpu_frequency: 240MHz
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240: "y"
      CONFIG_ESP32S3_DATA_CACHE_64KB: "y"
      CONFIG_ESP32S3_DATA_CACHE_LINE_64B: "y"
      CONFIG_SPIRAM_FETCH_INSTRUCTIONS: "y"
      CONFIG_SPIRAM_RODATA: "y"
      CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL: "65536"
      CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC: "y"
```
**Why add those?**
These allow for better PSRAM usage on the ESP32-S3-Box-3.

---

### user_cancelled global variable
We will need a global variable in order to signify if the current action was the user cancelling the answer.

- Find the `globals:` section and add this block under the last block of the section:

```yaml
  - id: user_cancelled
    type: bool
    restore_value: false
    initial_value: "false"
```



### i2s_audio_duplex: One bus, both directions, at the same time

The[ i2s_audio_duplex component](https://github.com/n-IA-hane/esphome-intercom/blob/main/esphome/components/i2s_audio_duplex/README.md) replaces the stock `i2s_audio` with one that opens the I2S peripheral in *master TX+RX* mode. A single shared LRCLK / BCLK / MCLK serves both ADC and DAC, with separate DIN / DOUT data lines.

You will need to completely remove the to-level item 'i2s_audio' and replace it with:

```yaml
i2s_audio_duplex:
  id: i2s_duplex
  i2s_lrclk_pin: GPIO45
  i2s_bclk_pin:  GPIO17
  i2s_mclk_pin:  GPIO2
  i2s_din_pin:   GPIO16   # mic in
  i2s_dout_pin:  GPIO15   # speaker out
  sample_rate:        48000
  output_sample_rate: 16000
  slot_bit_width:     32
  bits_per_sample:    16
  correct_dc_offset:  true
  processor_id:       aec_processor
  buffers_in_psram:   true
  aec_reference:      previous_frame
```

**Why `previous_frame` reference?**
AEC needs to know what the speaker is playing *right now*. Tapping the actual outgoing DMA frame from the previous I2S period gives a tightly aligned reference signal , no extra buffering, no clock-drift, no software loopback.

**Why two sample rates?**
[Hardware likes](https://github.com/n-IA-hane/esphome-intercom/blob/main/esphome/components/i2s_audio_duplex/README.md#multi-rate-48khz-i2s-bus-with-fir-decimation) 48 kHz (clean DAC playback). Wake word and STT live at 16 kHz. The component down-samples on the way out of the mic path so MWW and VA see the rate they expect.


---

### Addition of the esp_aec block

You will need to add this a top-level item named `esp_aec:` with the following content:

```yaml
esp_aec:
  id: aec_processor
  sample_rate: 16000
  filter_length: 4
  mode: sr_low_cost
```
---
### Modifications to the voice_assistant block
The `voice_assistant:` block requires a few modifications to accomodate for the new functions.
Nominally;

Under `voice_assistant->on_tts_start` where:
```yaml
  on_tts_start:
    - text_sensor.template.publish:
        id: text_response
        state: !lambda return x;
    - lambda: id(voice_assistant_phase) = ${voice_assist_replying_phase_id};
    - script.execute: draw_display
```
needs to be replaced with:
```yaml
  on_tts_start:
    - if:
        condition:
          lambda: return id(user_cancelled);
        then:
          - voice_assistant.stop:
          - media_player.stop:
              announcement: true
          - media_player.stop: speaker_media_player
          - script.execute: set_idle_or_mute_phase
          - script.execute: draw_display
        else:
          - text_sensor.template.publish:
              id: text_response
              state: !lambda return x;
          - lambda: id(voice_assistant_phase) = ${voice_assist_replying_phase_id};
          - script.execute: draw_display
```

 And under `voice_assistant->on_listening` where:
```yaml
  on_listening:
    - lambda: id(voice_assistant_phase) = ${voice_assist_listening_phase_id};
    - text_sensor.template.publish:
        id: text_request
        state: "..."
    - text_sensor.template.publish:
        id: text_response
        state: "..."
    - script.execute: draw_display
```
is replaced with: 

```yaml
  on_listening:
    # Clear the speaker-paused flag set by global_cancel. Without this, the wake-chime
    # plus subsequent TTS would play through the paused TX path and only produce silence.
    - lambda: id(i2s_duplex).set_speaker_paused(false);
    - lambda: id(user_cancelled) = false;
    - lambda: id(voice_assistant_phase) = ${voice_assist_listening_phase_id};
    - text_sensor.template.publish:
        id: text_request
        state: "..."
    - text_sensor.template.publish:
        id: text_response
        state: "..."
    - script.execute: draw_display
    # Audible "ding" , AEC cancels it from the mic capture so STT is clean.
    - media_player.speaker.play_on_device_media_file:
        media_file: wake_word_triggered_sound
        announcement: true

```


---
### The media_player block
We will need to add our new chime sound under the `media_player->files` block
To do so, located the `media_player:` top-level item, and and make it so the `files:` block goes from:

```yaml
    files:
      - id: timer_finished_sound
        file: https://github.com/esphome/home-assistant-voice-pe/raw/dev/sounds/timer_finished.flac
```
to
```yaml
    files:
      - id: timer_finished_sound
        file: https://github.com/esphome/home-assistant-voice-pe/raw/dev/sounds/timer_finished.flac
      - id: wake_word_triggered_sound
        file: https://github.com/esphome/home-assistant-voice-pe/raw/dev/sounds/wake_word_triggered.flac
```

And we will also need to modify the `media_player->on_announcement:` block from:

```yaml
    on_announcement:
      # Stop the wake word (mWW or VA) if the mic is capturing
      - if:
          condition:
            - microphone.is_capturing:
          then:
            - script.execute: stop_wake_word
            # Ensure VA stops before moving on
            - if:
                condition:
                  - lambda: return id(wake_word_engine_location).current_option() == "In Home Assistant";
                then:
                  - wait_until:
                      - not:
                          voice_assistant.is_running:
      # Since VA isn't running, this is user-intiated media playback. Draw the mute display
      - if:
          condition:
            not:
              voice_assistant.is_running:
          then:
            - lambda: id(voice_assistant_phase) = ${voice_assist_muted_phase_id};
            - script.execute: draw_display
```
to 

```yaml
    on_announcement:
      # If the streaming wake word (HA-side) is capturing, stop it and wait for VA to settle.
      # On-device micro_wake_word keeps running through TTS so the user can barge in.
      - if:
          condition:
            and:
              - microphone.is_capturing:
              - lambda: return id(wake_word_engine_location).current_option() == "In Home Assistant";
          then:
            - script.execute: stop_wake_word
            - wait_until:
                - not:
                    voice_assistant.is_running:
      # Since VA isn't running, this is user-intiated media playback. Draw the mute display
      - if:
          condition:
            not:
              voice_assistant.is_running:
          then:
            - lambda: id(voice_assistant_phase) = ${voice_assist_muted_phase_id};
            - script.execute: draw_display

```
This will make it so the microphone does not stop listening during an annoucement.

---


### esp_aec: Acoustic echo cancellation in the mic pipeline

`esp_aec` is registered as the `processor_id` on the duplex component. Every mic frame is fed through it together with the latest speaker frame and the result is a clean capture that ignores the device's own TTS output.

You will need to add this block of code before the previous one, [i2s_audio_duplex].

```yaml
esp_aec:
  id: aec_processor
  sample_rate:    16000     # match VA / MWW input rate
  filter_length:  4         # tail length, in 16-ms blocks
  mode:           sr_low_cost  # speech-recognition tuned, low CPU

#i2s_audio_duplex:
#  id: i2s_duplex
#  i2s_lrclk_pin: GPIO45
#  <...
#  ...>
```

> **What this unlocks:** the wake-chime, the TTS reply, and even media playback no longer get re-detected by micro_wake_word or shouted into the STT request. The mic stays "open" the whole time.


We will then modify the audio adapter blocks right under the `[i2s_audio_duplex]` block from:

```yaml
#i2s_audio_duplex:
#  id: i2s_duplex
#  <...
#  ...>
#  buffers_in_psram: true
#  aec_reference: previous_frame

audio_adc:
  - platform: es7210
    i2c_id: i2c_adc_dac
    id: es7210_adc
    bits_per_sample: 16bit
    sample_rate: 16000

audio_dac:
  - platform: es8311
    i2c_id: i2c_adc_dac
    id: es8311_dac
    bits_per_sample: 16bit
    sample_rate: 48000

microphone:
  - platform: i2s_audio
    id: box_mic
    sample_rate: 16000
    i2s_din_pin: GPIO16
    bits_per_sample: 16bit
    adc_type: external

speaker:
  - platform: i2s_audio
    id: box_speaker
    i2s_dout_pin: GPIO15
    dac_type: external
    sample_rate: 48000
    bits_per_sample: 16bit
    channel: left
    audio_dac: es8311_dac
    buffer_duration: 100ms
```


```yaml

#i2s_audio_duplex:
#  id: i2s_duplex
#  <...
#  ...>
#  buffers_in_psram: true
#  aec_reference: previous_frame

audio_adc:
  - platform: es7210
    id: es7210_adc
    bits_per_sample: 16bit
    sample_rate: 48000

audio_dac:
  - platform: es8311
    id: es8311_dac
    bits_per_sample: 16bit
    sample_rate: 48000

microphone:
  - platform: i2s_audio_duplex
    id: box_mic
    i2s_audio_duplex_id: i2s_duplex

speaker:
  - platform: i2s_audio_duplex
    id: box_speaker
    i2s_audio_duplex_id: i2s_duplex
    sample_rate: 48000

```


### A second MWW model, hidden from Home Assistant

```yaml
micro_wake_word:
  id: mww
  microphone: box_mic
  stop_after_detection: false
  models:
    - model: okay_nabu           # primary activation word
    - model: https://raw.githubusercontent.com/TaterTotterson/microWakeWords/refs/heads/main/microWakeWords/stop.json
      id: stop
      internal: true            # hide from HA's wake-word switches
      probability_cutoff: 0.40  # default 0.5; sliding avg ~0.58 fires reliably
      sliding_window_size: 3    # default 5; faster trigger
  vad:
    probability_cutoff: 0.20
    sliding_window_size: 1
```


**Why looser thresholds?**
The default probability cutoff (0.5) and 5-frame window add ~50 ms of latency, which is noticeable when you're trying to interrupt mid-sentence. 0.40 / 3 frames lands in the sliding-average window of confirmed "Stop" hits and fires ~30 ms after onset.


**Why `internal: true`?**
Home Assistant defaults non-primary wake words to OFF in its per-pipeline switches. `internal: true` hides the model from HA entirely, so that it cannot be accidentaly disabled. We re-enable it after every TTS turn (in `start_wake_word`).

To do so change the `voice_assistant-> 'id: start_wake_word'` block from:

```yaml
  - id: start_wake_word
    then:
      - if:
          condition:
            and:
              - not:
                  - voice_assistant.is_running:
              - lambda: return id(wake_word_engine_location).current_option() == "On device";
          then:
            - lambda: id(va).set_use_wake_word(false);
            - micro_wake_word.start:
      - if:
          condition:
            and:
              - not:
                  - voice_assistant.is_running:
              - lambda: return id(wake_word_engine_location).current_option() == "In Home Assistant";
          then:
            - lambda: id(va).set_use_wake_word(true);
            - voice_assistant.start_continuous:
```
to: 

```yaml
  # Starts either mWW or the streaming wake word, depending on the configured location
  - id: start_wake_word
    then:
      - if:
          condition:
            and:
              - not:
                  - voice_assistant.is_running:
              - lambda: return id(wake_word_engine_location).current_option() == "On device";
          then:
            - lambda: id(va).set_use_wake_word(false);
            - micro_wake_word.start:
            - micro_wake_word.enable_model: stop
      - if:
          condition:
            and:
              - not:
                  - voice_assistant.is_running:
              - lambda: return id(wake_word_engine_location).current_option() == "In Home Assistant";
          then:
            - lambda: id(va).set_use_wake_word(true);
            - voice_assistant.start_continuous:

```

---





### The global_cancel script

We will need to create a `global_cancel` under the `script:` top-level item:


```yaml
  - id: global_cancel
    then:
      - lambda: id(user_cancelled) = true;
      - voice_assistant.stop:
      - media_player.stop:
          announcement: true
      - media_player.stop: speaker_media_player
      # ── Halt the duplex speaker output immediately ───────────────────────
      # Tracing the duplex source revealed that calling `speaker.stop` is
      # futile while media_player still has data: the speaker's `play()`
      # auto-re-registers the listener, the state machine runs
      # STOPPED → STARTING → calls parent.start_speaker(), and audio resumes.
      #
      # set_speaker_paused(true) bypasses the state machine entirely.
      # process_tx_path_ memsets the output buffer to zeros every frame
      # while paused=true, so the I2S DMA emits pure silence regardless of
      # what's still queued in the 1 MB announcement ring buffer or what
      # media_player keeps feeding. stop_speaker() additionally requests
      # a buffer reset so the queued PCM is dropped, not deferred.
      # The pause is cleared in voice_assistant.on_listening when a fresh
      # turn starts, so the wake-chime + new TTS play normally.
      - lambda: |-
          id(i2s_duplex).set_speaker_paused(true);
          id(i2s_duplex).stop_speaker();
      - switch.turn_off: timer_ringing
      - text_sensor.template.publish:
          id: text_request
          state: ""
      - text_sensor.template.publish:
          id: text_response
          state: ""
      - script.execute: set_idle_or_mute_phase
      - script.execute: draw_display
```

**`set_speaker_paused(true)`**
Bypasses the state machine. `process_tx_path_` memsets the output buffer to zeros every frame while paused, so the DMA emits *pure silence* regardless of what the announcement ring buffer or media_player keeps feeding.

**`stop_speaker()`**
Requests a buffer reset so already-queued PCM is dropped rather than deferred. The pause flag is cleared by `voice_assistant.on_listening` on the next turn, so the wake-chime + new TTS play normally.



---



### Optional Touch Screen Controls
  I've personally added the touchscreen controls to my ESP32-Boxes.

A full-screen touch zone (320×240) lives on top of the pages. It chooses one of two paths depending on what the assistant is doing.

> **What this unlocks:** This allows you to quickly stop or trigger the assistant by a simple press of the finger anywhere on the screen!

First we will add a `touchscreen` block as a top-level item:

```yaml
touchscreen:
  - platform: gt911 # Most S3-Box models use this; if it fails, try 'ft6336'
    id: board_touchscreen
    interrupt_pin: GPIO3
```
Followed by a binary sensor that triggers when the touchscreen is enabled, this will need to be placed under the top-level item `binary_sensor:`

```yaml
binary_sensor:
  - platform: touchscreen
    name: "Screen Touch"
    touchscreen_id: board_touchscreen
    x_min: 0
    x_max: 320
    y_min: 0
    y_max: 240
    on_press:
      then:
        - if:
            condition:
              or:
                # Currently listening / thinking / replying?
                - lambda: |-
                    const int phase = id(voice_assistant_phase);
                    return phase == ${voice_assist_listening_phase_id} ||
                           phase == ${voice_assist_thinking_phase_id} ||
                           phase == ${voice_assist_replying_phase_id};
                - and:
                    - media_player.is_announcing:
                    - lambda: 'return !id(user_cancelled);'
                - switch.is_on: timer_ringing
            then:
              - script.execute: global_cancel
            else:
              - if:
                  condition:
                    and:
                      - lambda: return id(voice_assistant_phase) == ${voice_assist_idle_phase_id};
                      - switch.is_off: mute
                      - not:
                          voice_assistant.is_running:
                  then:
                    - lambda: id(va).set_use_wake_word(false);
                    - micro_wake_word.enable_model: stop
                    - voice_assistant.start:
```

> **Why we do *not* stop micro_wake_word here:** with the old half-duplex stack, MWW had to be stopped before VA took the mic. With `i2s_audio_duplex` + AEC, both consume `box_mic` concurrently, leaving MWW running through the touch-triggered turn is what keeps the "Stop" word and "Okay Nabu" barge-in alive while TTS is replying.

---





### What changed from a user's point of view

| Scenario | Before (stock) | After (with intercom) |
|---|---|---|
| Say "Okay Nabu" while TTS is playing | ❌ Ignored since the wake word is paused during reply | ✅ Reply stops, returns to idle |
| Say "Stop" mid-reply | ❌ Not supported | ✅ Reply stops, returns to idle |
| Say "Stop" while idle | - | - |
| Tap the screen while idle | ❌ Ignored | ✅ Starts a turn (no wake word needed) |
| Tap the screen mid-reply | - | ✅ Cancels reply |
| Tap during a ringing timer | ⚠️ Limited | ✅ Silences alarm via `global_cancel` |
| Wake-chime + STT after wake | ⚠️ Sometimes re-triggers wake word | ✅ AEC subtracts the chime |

---

## Known Limitation

### media_player can stay stuck in ANNOUNCING

ESPHome's `audio_reader` has a hard-coded retry budget (6 × 5 s = 30 s) on `fetch_headers` and cannot be aborted from outside. When the user cancels mid-TTS, the streaming HTTP read is still in flight; `media_player.stop` only flags a stop request. Until the upstream stream errors out, VA cannot leave `STREAMING_RESPONSE` and a new turn cannot start.

---


And of course, do let me know if you entounter any issues while trying to replicate!
The code has been tested on 9 ESP32-S3-Boxes and they all seem to work for me, minus the limitation explained above!