"""I2S Audio Duplex Component - Full duplex I2S for simultaneous mic+speaker

Exposes standard ESPHome microphone and speaker platforms for compatibility with
Voice Assistant and intercom_api components.

Multi-rate support: set output_sample_rate to decimate mic audio internally.
  sample_rate: I2S bus rate (e.g. 48000 for high-quality DAC output)
  output_sample_rate: mic/AEC/MWW/VA rate (e.g. 16000, must divide sample_rate evenly)
If output_sample_rate is omitted, no decimation occurs (backward compatible).
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import CONF_ID, CONF_NUM_CHANNELS, CONF_SAMPLE_RATE
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)

CODEOWNERS = ["@n-IA-hane"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["switch", "number", "sensor"]

CONF_I2S_LRCLK_PIN = "i2s_lrclk_pin"
CONF_I2S_BCLK_PIN = "i2s_bclk_pin"
CONF_I2S_MCLK_PIN = "i2s_mclk_pin"
CONF_I2S_DIN_PIN = "i2s_din_pin"
CONF_I2S_DOUT_PIN = "i2s_dout_pin"
CONF_OUTPUT_SAMPLE_RATE = "output_sample_rate"
CONF_PROCESSOR_ID = "processor_id"
CONF_MIC_ATTENUATION = "mic_attenuation"
CONF_USE_STEREO_AEC_REF = "use_stereo_aec_reference"
CONF_REFERENCE_CHANNEL = "reference_channel"
CONF_BITS_PER_SAMPLE = "bits_per_sample"
CONF_SLOT_BIT_WIDTH = "slot_bit_width"
CONF_CORRECT_DC_OFFSET = "correct_dc_offset"
CONF_MIC_CHANNEL = "mic_channel"
CONF_I2S_MODE = "i2s_mode"
CONF_USE_APLL = "use_apll"
CONF_I2S_NUM = "i2s_num"
CONF_MCLK_MULTIPLE = "mclk_multiple"
CONF_I2S_COMM_FMT = "i2s_comm_fmt"
CONF_USE_TDM_REFERENCE = "use_tdm_reference"
CONF_TDM_TOTAL_SLOTS = "tdm_total_slots"
CONF_TDM_MIC_SLOT = "tdm_mic_slot"
CONF_TDM_MIC_SLOTS = "tdm_mic_slots"
CONF_TDM_REF_SLOT = "tdm_ref_slot"
CONF_I2S_AUDIO_DUPLEX_ID = "i2s_audio_duplex_id"
CONF_TASK_PRIORITY = "task_priority"
CONF_TASK_CORE = "task_core"
CONF_TASK_STACK_SIZE = "task_stack_size"
CONF_TX_CHANNEL = "tx_channel"
CONF_BUFFERS_IN_PSRAM = "buffers_in_psram"
CONF_AEC_REF_RING_IN_PSRAM = "aec_ref_ring_in_psram"
CONF_AUDIO_STACK_IN_PSRAM = "audio_stack_in_psram"
CONF_AEC_REFERENCE_MODE = "aec_reference"
CONF_AEC_REF_BUFFER_MS = "aec_reference_buffer_ms"
CONF_TELEMETRY = "telemetry"
CONF_TELEMETRY_LOG_INTERVAL_FRAMES = "telemetry_log_interval_frames"
CONF_FIR_DECIMATOR = "fir_decimator"

FIR_DECIMATOR_OPTIONS = ("custom", "dsps_fird_s16")

i2s_audio_duplex_ns = cg.esphome_ns.namespace("i2s_audio_duplex")
I2SAudioDuplex = i2s_audio_duplex_ns.class_("I2SAudioDuplex", cg.Component)
StartAction = i2s_audio_duplex_ns.class_("StartAction", automation.Action)
StopAction = i2s_audio_duplex_ns.class_("StopAction", automation.Action)

# AudioProcessor abstract interface (defined in audio_processor/audio_processor.h)
# Both esp_aec::EspAec and future esp_afe::EspAfe inherit from this.
AudioProcessor = cg.esphome_ns.namespace("audio_processor").class_("AudioProcessor")

# I2S port count per SoC variant (from SOC_I2S_NUM in soc_caps.h)
I2S_PORTS = {
    VARIANT_ESP32: 2,
    VARIANT_ESP32C3: 1,
    VARIANT_ESP32C5: 1,
    VARIANT_ESP32C6: 1,
    VARIANT_ESP32C61: 1,
    VARIANT_ESP32H2: 1,
    VARIANT_ESP32P4: 3,
    VARIANT_ESP32S2: 1,
    VARIANT_ESP32S3: 2,
}

# SoC variants with TDM support (from SOC_I2S_SUPPORTS_TDM in soc_caps.h)
TDM_VARIANTS = {
    VARIANT_ESP32C3, VARIANT_ESP32C5, VARIANT_ESP32C6, VARIANT_ESP32C61,
    VARIANT_ESP32H2, VARIANT_ESP32S3, VARIANT_ESP32P4,
}


def _validate_sample_rates(config):
    """Validate that output_sample_rate divides sample_rate evenly."""
    if CONF_OUTPUT_SAMPLE_RATE in config:
        sr = config[CONF_SAMPLE_RATE]
        osr = config[CONF_OUTPUT_SAMPLE_RATE]
        if sr % osr != 0:
            raise cv.Invalid(
                f"sample_rate ({sr}) must be an exact multiple of "
                f"output_sample_rate ({osr})"
            )
        ratio = sr // osr
        if ratio > 6:
            raise cv.Invalid(
                f"Decimation ratio {ratio} (={sr}/{osr}) exceeds maximum of 6"
            )
    return config


def _validate_tdm_config(config):
    """Validate TDM reference configuration."""
    use_tdm = config.get(CONF_USE_TDM_REFERENCE, False)
    use_stereo = config.get(CONF_USE_STEREO_AEC_REF, False)

    if use_tdm and use_stereo:
        raise cv.Invalid(
            "use_tdm_reference and use_stereo_aec_reference are mutually exclusive"
        )

    if use_tdm:
        total_slots = config.get(CONF_TDM_TOTAL_SLOTS, 4)
        ref_slot = config.get(CONF_TDM_REF_SLOT, 1)
        mic_slots = config.get(CONF_TDM_MIC_SLOTS, [config.get(CONF_TDM_MIC_SLOT, 0)])

        if len(set(mic_slots)) != len(mic_slots):
            raise cv.Invalid("tdm_mic_slots must not contain duplicates")

        for mic_slot in mic_slots:
            if mic_slot == ref_slot:
                raise cv.Invalid(
                    f"tdm_mic_slots contains ref slot {ref_slot}; microphone and reference slots must differ"
                )

        max_slot = max([ref_slot, *mic_slots])
        if total_slots <= max_slot:
            raise cv.Invalid(
                f"tdm_total_slots ({total_slots}) must be > {max_slot} "
                f"(highest slot index)"
            )

    return config


def _validate_pcm_format(config):
    """Validate that PCM short/long formats require TDM mode."""
    fmt = config.get(CONF_I2S_COMM_FMT, "philips")
    use_tdm = config.get(CONF_USE_TDM_REFERENCE, False)
    if fmt in ("pcm_short", "pcm_long") and not use_tdm:
        raise cv.Invalid(
            f"i2s_comm_fmt '{fmt}' requires use_tdm_reference: true "
            f"(PCM short/long are TDM-only in ESP-IDF)"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(I2SAudioDuplex),
        cv.Required(CONF_I2S_LRCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_I2S_BCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_I2S_MCLK_PIN, default=-1): cv.Any(
            cv.int_range(min=-1, max=-1),
            pins.internal_gpio_output_pin_number,
        ),
        cv.Optional(CONF_I2S_DIN_PIN, default=-1): cv.Any(
            cv.int_range(min=-1, max=-1),
            pins.internal_gpio_input_pin_number,
        ),
        cv.Optional(CONF_I2S_DOUT_PIN, default=-1): cv.Any(
            cv.int_range(min=-1, max=-1),
            pins.internal_gpio_output_pin_number,
        ),
        cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(min=8000, max=48000),
        cv.Optional(CONF_BITS_PER_SAMPLE, default=16): cv.one_of(16, 24, 32, int=True),
        cv.Optional(CONF_SLOT_BIT_WIDTH, default="auto"): cv.Any(
            cv.one_of("auto", lower=True),
            cv.one_of(16, 24, 32, int=True),
        ),
        cv.Optional(CONF_CORRECT_DC_OFFSET, default=False): cv.boolean,
        cv.Optional(CONF_NUM_CHANNELS, default=1): cv.one_of(1, 2, int=True),
        cv.Optional(CONF_MIC_CHANNEL, default="left"): cv.one_of("left", "right", lower=True),
        cv.Optional(CONF_TX_CHANNEL, default="left"): cv.one_of("left", "right", lower=True),
        cv.Optional(CONF_I2S_MODE, default="primary"): cv.one_of("primary", "secondary", lower=True),
        cv.Optional(CONF_USE_APLL, default=False): cv.boolean,
        cv.Optional(CONF_I2S_NUM, default=0): cv.int_range(min=0, max=2),
        cv.Optional(CONF_MCLK_MULTIPLE, default=256): cv.one_of(128, 256, 384, 512, int=True),
        cv.Optional(CONF_I2S_COMM_FMT, default="philips"): cv.one_of(
            "philips", "msb", "pcm_short", "pcm_long", lower=True,
        ),
        # Output sample rate for mic/AEC/MWW/VA (decimated from bus rate)
        # If omitted, equals sample_rate (no decimation)
        cv.Optional(CONF_OUTPUT_SAMPLE_RATE): cv.int_range(min=8000, max=48000),
        cv.Optional(CONF_PROCESSOR_ID): cv.use_id(AudioProcessor),
        # Pre-AEC mic gain/attenuation: <1.0 attenuates (hot mics), >1.0 amplifies (weak mics)
        cv.Optional(CONF_MIC_ATTENUATION, default=1.0): cv.float_range(min=0.01, max=32.0),
        # ES8311 digital feedback: RX is stereo with L=DAC(reference), R=ADC(mic)
        # Requires ES8311 register 0x44 bits[6:4]=4 (ADCDAT_SEL=DACL+ADC)
        cv.Optional(CONF_USE_STEREO_AEC_REF, default=False): cv.boolean,
        # Which stereo channel carries the AEC reference (default: left)
        cv.Optional(CONF_REFERENCE_CHANNEL, default="left"): cv.one_of("left", "right", lower=True),
        # TDM hardware reference: ES7210 in TDM mode with one slot carrying DAC feedback
        cv.Optional(CONF_USE_TDM_REFERENCE, default=False): cv.boolean,
        cv.Optional(CONF_TDM_TOTAL_SLOTS, default=4): cv.int_range(min=2, max=8),
        cv.Optional(CONF_TDM_MIC_SLOT, default=0): cv.int_range(min=0, max=7),
        cv.Optional(CONF_TDM_MIC_SLOTS): cv.All(
            cv.ensure_list(cv.int_range(min=0, max=7)),
            cv.Length(min=1, max=2),
        ),
        cv.Optional(CONF_TDM_REF_SLOT, default=1): cv.int_range(min=0, max=7),
        # Audio task tuning (advanced)
        cv.Optional(CONF_TASK_PRIORITY, default=19): cv.int_range(min=1, max=24),
        cv.Optional(CONF_TASK_CORE, default=0): cv.int_range(min=-1, max=1),
        cv.Optional(CONF_TASK_STACK_SIZE, default=8192): cv.int_range(min=4096, max=32768),
        # Use PSRAM for non-DMA audio buffers (saves ~15KB internal RAM).
        # Requires PSRAM. DMA buffers (I2S RX/TX) always use internal RAM.
        cv.Optional(CONF_BUFFERS_IN_PSRAM, default=False): cv.boolean,
        # Place the audio task's own stack in PSRAM. Saves ~8KB internal RAM
        # but increases per-frame latency because every function return has
        # to traverse PSRAM. Only enable on boards that need the internal
        # headroom (e.g. waveshare-s3 running 2-mic BSS plus concurrent TLS
        # streams). Requires CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y.
        cv.Optional(CONF_AUDIO_STACK_IN_PSRAM, default=False): cv.boolean,
        # AEC reference mode for no-codec setups (ignored if stereo/TDM ref is configured):
        #   previous_frame: use the TX frame from the previous cycle (default, ~32ms delay)
        #   ring_buffer: TYPE2-style ring buffer with configurable delay (better alignment)
        cv.Optional(CONF_AEC_REFERENCE_MODE, default="previous_frame"): cv.one_of(
            "previous_frame", "ring_buffer", lower=True,
        ),
        # Ring buffer capacity in ms (only used with aec_reference: ring_buffer)
        cv.Optional(CONF_AEC_REF_BUFFER_MS, default=80): cv.int_range(min=32, max=500),
        # Place AEC reference ring buffer in PSRAM. Default false = internal RAM
        # (~13.6 us/frame faster on Core 0, costs ~3-5 KB internal). Set true to
        # save internal RAM at the cost of Core 0 PSRAM traffic.
        cv.Optional(CONF_AEC_REF_RING_IN_PSRAM, default=False): cv.boolean,
        # Enable per-stage cycle counting and diagnostics (debug only, adds overhead)
        cv.Optional(CONF_TELEMETRY, default=False): cv.boolean,
        cv.Optional(CONF_TELEMETRY_LOG_INTERVAL_FRAMES, default=128): cv.int_range(min=1, max=8192),
        # FIR decimator kernel selection. Default `dsps_fird_s16` (esp-dsp SIMD,
        # `_aes3` on ESP32-S3). Set to `custom` to use a pure-float scalar
        # implementation that bypasses the SIMD kernel; required on chips where
        # the SIMD path is unreliable (e.g. ESP32-P4 RISC-V, esp-dsp #117/#102).
        cv.Optional(CONF_FIR_DECIMATOR, default="dsps_fird_s16"):
            cv.one_of(*FIR_DECIMATOR_OPTIONS, lower=True),
    }).extend(cv.COMPONENT_SCHEMA),
    _validate_sample_rates,
    _validate_tdm_config,
    _validate_pcm_format,
)


def _final_validate(config):
    """Validate i2s_num against SoC port count and TDM against SoC capability."""
    variant = get_esp32_variant()
    if variant not in I2S_PORTS:
        raise cv.Invalid(f"Unsupported ESP32 variant: {variant}")

    max_ports = I2S_PORTS[variant]
    i2s_num = config.get(CONF_I2S_NUM, 0)
    if i2s_num >= max_ports:
        raise cv.Invalid(
            f"i2s_num {i2s_num} exceeds available I2S ports on {variant} "
            f"(max port index: {max_ports - 1})"
        )

    use_tdm = config.get(CONF_USE_TDM_REFERENCE, False)
    if use_tdm and variant not in TDM_VARIANTS:
        raise cv.Invalid(
            f"use_tdm_reference requires TDM support, but {variant} does not have SOC_I2S_SUPPORTS_TDM"
        )

    # Single-core SoCs cannot pin to Core 1
    SINGLE_CORE_VARIANTS = {
        VARIANT_ESP32C3, VARIANT_ESP32C5, VARIANT_ESP32C6, VARIANT_ESP32C61,
        VARIANT_ESP32H2, VARIANT_ESP32S2,
    }
    task_core = config.get(CONF_TASK_CORE, 0)
    if task_core > 0 and variant in SINGLE_CORE_VARIANTS:
        raise cv.Invalid(
            f"task_core={task_core} not available on {variant} (single-core SoC)"
        )

    # APLL is only available on ESP32, ESP32-S2, and ESP32-P4
    APLL_VARIANTS = {VARIANT_ESP32, VARIANT_ESP32S2, VARIANT_ESP32P4}
    if config.get(CONF_USE_APLL, False) and variant not in APLL_VARIANTS:
        raise cv.Invalid(
            f"use_apll is not supported on {variant}. "
            f"APLL clock source is only available on ESP32, ESP32-S2, and ESP32-P4."
        )

    from esphome.core import CORE
    full_config = CORE.config or {}

    # esp_aec and esp_afe are mutually exclusive: both provide AudioProcessor
    has_aec = "esp_aec" in full_config
    has_afe = "esp_afe" in full_config
    if has_aec and has_afe:
        raise cv.Invalid(
            "esp_aec and esp_afe are mutually exclusive. "
            "Use esp_afe (full AFE pipeline with AEC+NS+AGC+beamforming) "
            "or esp_aec (standalone echo cancellation), not both."
        )

    # Cross-component validation: check for audio processor conflict with intercom_api.
    # Both components accept processor_id; if both set it the audio processor is fed
    # from two producers and races on every frame. Reject at validation time.

    intercom_configs = full_config.get("intercom_api", [])
    if intercom_configs:
        has_duplex_processor = CONF_PROCESSOR_ID in config and config.get(CONF_PROCESSOR_ID) is not None
        for ic in (intercom_configs if isinstance(intercom_configs, list) else [intercom_configs]):
            if isinstance(ic, dict) and ic.get("processor_id") is not None and has_duplex_processor:
                raise cv.Invalid(
                    "Both i2s_audio_duplex and intercom_api have processor_id configured. "
                    "This causes a race condition on the audio processor. "
                    "Use processor_id on only ONE component (i2s_audio_duplex recommended)."
                )

    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Define USE_I2S_AUDIO_DUPLEX so other components know it's available
    cg.add_define("USE_I2S_AUDIO_DUPLEX")

    cg.add(var.set_lrclk_pin(config[CONF_I2S_LRCLK_PIN]))
    cg.add(var.set_bclk_pin(config[CONF_I2S_BCLK_PIN]))
    cg.add(var.set_mclk_pin(config[CONF_I2S_MCLK_PIN]))
    cg.add(var.set_din_pin(config[CONF_I2S_DIN_PIN]))
    cg.add(var.set_dout_pin(config[CONF_I2S_DOUT_PIN]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))
    cg.add(var.set_correct_dc_offset(config[CONF_CORRECT_DC_OFFSET]))
    cg.add(var.set_num_channels(config[CONF_NUM_CHANNELS]))
    cg.add(var.set_i2s_mode_secondary(config[CONF_I2S_MODE] == "secondary"))
    cg.add(var.set_use_apll(config[CONF_USE_APLL]))
    cg.add(var.set_i2s_num(config[CONF_I2S_NUM]))
    cg.add(var.set_mclk_multiple(config[CONF_MCLK_MULTIPLE]))

    # Map comm format string to enum index
    comm_fmt_map = {"philips": 0, "msb": 1, "pcm_short": 2, "pcm_long": 3}
    cg.add(var.set_i2s_comm_fmt(comm_fmt_map[config[CONF_I2S_COMM_FMT]]))

    # Mic channel selection (for mono RX: which I2S slot to capture)
    cg.add(var.set_mic_channel_right(config[CONF_MIC_CHANNEL] == "right"))

    # TX channel selection (for mono TX: which I2S slot to output)
    cg.add(var.set_tx_slot_right(config[CONF_TX_CHANNEL] == "right"))

    # Slot bit width: 0 = auto (match bits_per_sample)
    sbw = config[CONF_SLOT_BIT_WIDTH]
    cg.add(var.set_slot_bit_width(0 if sbw == "auto" else sbw))

    # Set output sample rate if specified (enables decimation)
    if CONF_OUTPUT_SAMPLE_RATE in config:
        cg.add(var.set_output_sample_rate(config[CONF_OUTPUT_SAMPLE_RATE]))

    # Set mic attenuation for hot mics (applied BEFORE AEC)
    cg.add(var.set_mic_attenuation(config[CONF_MIC_ATTENUATION]))

    # ES8311 digital feedback mode: stereo RX with L=ref, R=mic
    cg.add(var.set_use_stereo_aec_reference(config[CONF_USE_STEREO_AEC_REF]))

    # Reference channel selection (left or right)
    cg.add(var.set_reference_channel_right(config[CONF_REFERENCE_CHANNEL] == "right"))

    # TDM hardware reference configuration
    if config[CONF_USE_TDM_REFERENCE]:
        cg.add(var.set_use_tdm_reference(True))
        cg.add(var.set_tdm_total_slots(config[CONF_TDM_TOTAL_SLOTS]))
        mic_slots = config.get(CONF_TDM_MIC_SLOTS, [config[CONF_TDM_MIC_SLOT]])
        cg.add(var.set_tdm_mic_slot(mic_slots[0]))
        if len(mic_slots) > 1:
            cg.add(var.set_secondary_tdm_mic_slot(mic_slots[1]))
        cg.add(var.set_tdm_ref_slot(config[CONF_TDM_REF_SLOT]))

    # Audio task tuning
    cg.add(var.set_task_priority(config[CONF_TASK_PRIORITY]))
    cg.add(var.set_task_core(config[CONF_TASK_CORE]))
    cg.add(var.set_task_stack_size(config[CONF_TASK_STACK_SIZE]))
    cg.add(var.set_buffers_in_psram(config[CONF_BUFFERS_IN_PSRAM]))
    cg.add(var.set_audio_stack_in_psram(config[CONF_AUDIO_STACK_IN_PSRAM]))
    cg.add(var.set_fir_decimator_custom(config[CONF_FIR_DECIMATOR] == "custom"))

    # AEC reference mode (only relevant for no-codec setups)
    cg.add(var.set_aec_reference_mode(config[CONF_AEC_REFERENCE_MODE] == "ring_buffer"))
    cg.add(var.set_aec_ref_buffer_ms(config[CONF_AEC_REF_BUFFER_MS]))
    cg.add(var.set_aec_ref_ring_in_psram(config[CONF_AEC_REF_RING_IN_PSRAM]))

    # Telemetry: per-stage cycle counting and diagnostics
    if config[CONF_TELEMETRY]:
        cg.add_define("USE_DUPLEX_TELEMETRY")
        cg.add(var.set_telemetry_log_interval_frames(config[CONF_TELEMETRY_LOG_INTERVAL_FRAMES]))

    # Link audio processor if configured
    if CONF_PROCESSOR_ID in config:
        processor = await cg.get_variable(config[CONF_PROCESSOR_ID])
        cg.add(var.set_processor(processor))
        cg.add_define("USE_AUDIO_PROCESSOR")


# === Actions ===
I2S_AUDIO_DUPLEX_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(I2SAudioDuplex)}
)


@automation.register_action(
    "i2s_audio_duplex.start", StartAction, I2S_AUDIO_DUPLEX_ACTION_SCHEMA, synchronous=True
)
@automation.register_action(
    "i2s_audio_duplex.stop", StopAction, I2S_AUDIO_DUPLEX_ACTION_SCHEMA, synchronous=True
)
async def i2s_audio_duplex_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
