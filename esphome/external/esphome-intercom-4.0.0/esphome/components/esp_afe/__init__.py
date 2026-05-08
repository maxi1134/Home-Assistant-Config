import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_MODE, CONF_TYPE
from esphome.core import CORE
from esphome.components.esp32 import add_idf_component

CODEOWNERS = ["@n-IA-hane"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["audio_processor", "switch", "binary_sensor", "sensor"]

_SUPPORTED_VARIANTS = ("ESP32S3", "ESP32P4")


def _validate_esp32_variant(config):
    if CORE.is_esp32:
        import esphome.components.esp32 as esp32

        variant = esp32.get_esp32_variant()
        if variant not in _SUPPORTED_VARIANTS:
            raise cv.Invalid(
                f"esp_afe requires {' or '.join(_SUPPORTED_VARIANTS)}, got {variant}"
            )
    return config


def _validate_feature_config(config):
    if config[CONF_SE_ENABLED] and config[CONF_MIC_NUM] < 2:
        raise cv.Invalid("se_enabled requires mic_num: 2")
    return config


AudioProcessor = cg.esphome_ns.namespace("audio_processor").class_("AudioProcessor")

esp_afe_ns = cg.esphome_ns.namespace("esp_afe")
EspAfe = esp_afe_ns.class_("EspAfe", cg.Component, AudioProcessor)
SetModeAction = esp_afe_ns.class_("SetModeAction", automation.Action)

CONF_ESP_AFE_ID = "esp_afe_id"
CONF_MIC_NUM = "mic_num"
CONF_AEC_ENABLED = "aec_enabled"
CONF_AEC_FILTER_LENGTH = "aec_filter_length"
CONF_SE_ENABLED = "se_enabled"
CONF_NS_ENABLED = "ns_enabled"
CONF_VAD_ENABLED = "vad_enabled"
CONF_AGC_ENABLED = "agc_enabled"
CONF_AGC_COMPRESSION_GAIN = "agc_compression_gain"
CONF_AGC_TARGET_LEVEL = "agc_target_level"
CONF_VAD_MODE = "vad_mode"
CONF_VAD_MIN_SPEECH_MS = "vad_min_speech_ms"
CONF_VAD_MIN_NOISE_MS = "vad_min_noise_ms"
CONF_VAD_DELAY_MS = "vad_delay_ms"
CONF_VAD_MUTE_PLAYBACK = "vad_mute_playback"
CONF_VAD_ENABLE_CHANNEL_TRIGGER = "vad_enable_channel_trigger"
CONF_MEMORY_ALLOC_MODE = "memory_alloc_mode"
CONF_AFE_LINEAR_GAIN = "afe_linear_gain"
CONF_TASK_CORE = "task_core"
CONF_TASK_PRIORITY = "task_priority"
CONF_RINGBUF_SIZE = "ringbuf_size"
CONF_FEED_BUF_IN_PSRAM = "feed_buf_in_psram"
CONF_FEED_RING_IN_PSRAM = "feed_ring_in_psram"
CONF_FETCH_RING_IN_PSRAM = "fetch_ring_in_psram"

AFE_TYPES = {
    "sr": 0,  # AFE_TYPE_SR: speech recognition, linear AEC
    "vc": 1,  # AFE_TYPE_VC: voice communication, nonlinear AEC
}

AFE_MODES = {
    "low_cost": 0,   # AFE_MODE_LOW_COST
    "high_perf": 1,  # AFE_MODE_HIGH_PERF
}

MEMORY_ALLOC_MODES = {
    "more_internal": 1,
    "internal_psram_balance": 2,
    "more_psram": 3,
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EspAfe),
            cv.Optional(CONF_TYPE, default="sr"): cv.enum(AFE_TYPES, lower=True),
            cv.Optional(CONF_MODE, default="low_cost"): cv.enum(AFE_MODES, lower=True),
            cv.Optional(CONF_MIC_NUM, default=1): cv.int_range(min=1, max=2),
            cv.Optional(CONF_AEC_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_AEC_FILTER_LENGTH, default=4): cv.int_range(min=1, max=8),
            cv.Optional(CONF_SE_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_NS_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_VAD_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_VAD_MODE, default=3): cv.int_range(min=0, max=4),
            cv.Optional(CONF_VAD_MIN_SPEECH_MS, default=128): cv.int_range(min=32, max=60000),
            cv.Optional(CONF_VAD_MIN_NOISE_MS, default=1000): cv.int_range(min=64, max=60000),
            cv.Optional(CONF_VAD_DELAY_MS, default=128): cv.int_range(min=0, max=60000),
            cv.Optional(CONF_VAD_MUTE_PLAYBACK, default=False): cv.boolean,
            cv.Optional(CONF_VAD_ENABLE_CHANNEL_TRIGGER, default=False): cv.boolean,
            cv.Optional(CONF_AGC_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_AGC_COMPRESSION_GAIN, default=9): cv.int_range(min=0, max=30),
            cv.Optional(CONF_AGC_TARGET_LEVEL, default=3): cv.int_range(min=0, max=31),
            cv.Optional(CONF_MEMORY_ALLOC_MODE, default="more_psram"): cv.enum(
                MEMORY_ALLOC_MODES, lower=True
            ),
            cv.Optional(CONF_AFE_LINEAR_GAIN, default=1.0): cv.float_range(min=0.1, max=10.0),
            cv.Optional(CONF_TASK_CORE, default=1): cv.int_range(min=0, max=1),
            cv.Optional(CONF_TASK_PRIORITY, default=8): cv.int_range(min=1, max=24),
            cv.Optional(CONF_RINGBUF_SIZE, default=8): cv.int_range(min=2, max=32),
            # Buffer placement knobs. Default false = internal RAM (fastest on
            # Core 0); set true to free internal at the cost of PSRAM traffic.
            #   feed_buf_in_psram   : ~3 KB scratch built then re-read every frame (~41 us/frame)
            #   feed_ring_in_psram  : ~12 KB staging ring written every frame (~20 us/frame)
            #   fetch_ring_in_psram : ~4 KB output ring read every frame (~6.8 us/frame)
            cv.Optional(CONF_FEED_BUF_IN_PSRAM, default=False): cv.boolean,
            cv.Optional(CONF_FEED_RING_IN_PSRAM, default=False): cv.boolean,
            cv.Optional(CONF_FETCH_RING_IN_PSRAM, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_esp32_variant,
    _validate_feature_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_afe_type(config[CONF_TYPE]))
    cg.add(var.set_afe_mode(config[CONF_MODE]))
    cg.add(var.set_mic_num(config[CONF_MIC_NUM]))
    cg.add(var.set_aec_enabled(config[CONF_AEC_ENABLED]))
    cg.add(var.set_aec_filter_length(config[CONF_AEC_FILTER_LENGTH]))
    cg.add(var.set_se_enabled(config[CONF_SE_ENABLED]))
    cg.add(var.set_ns_enabled(config[CONF_NS_ENABLED]))
    cg.add(var.set_vad_enabled(config[CONF_VAD_ENABLED]))
    cg.add(var.set_vad_mode(config[CONF_VAD_MODE]))
    cg.add(var.set_vad_min_speech_ms(config[CONF_VAD_MIN_SPEECH_MS]))
    cg.add(var.set_vad_min_noise_ms(config[CONF_VAD_MIN_NOISE_MS]))
    cg.add(var.set_vad_delay_ms(config[CONF_VAD_DELAY_MS]))
    cg.add(var.set_vad_mute_playback(config[CONF_VAD_MUTE_PLAYBACK]))
    cg.add(var.set_vad_enable_channel_trigger(config[CONF_VAD_ENABLE_CHANNEL_TRIGGER]))
    cg.add(var.set_agc_enabled(config[CONF_AGC_ENABLED]))
    cg.add(var.set_agc_compression_gain(config[CONF_AGC_COMPRESSION_GAIN]))
    cg.add(var.set_agc_target_level(config[CONF_AGC_TARGET_LEVEL]))
    cg.add(var.set_memory_alloc_mode(config[CONF_MEMORY_ALLOC_MODE]))
    cg.add(var.set_afe_linear_gain(config[CONF_AFE_LINEAR_GAIN]))
    cg.add(var.set_task_core(config[CONF_TASK_CORE]))
    cg.add(var.set_task_priority(config[CONF_TASK_PRIORITY]))
    cg.add(var.set_ringbuf_size(config[CONF_RINGBUF_SIZE]))
    cg.add(var.set_feed_buf_in_psram(config[CONF_FEED_BUF_IN_PSRAM]))
    cg.add(var.set_feed_ring_in_psram(config[CONF_FEED_RING_IN_PSRAM]))
    cg.add(var.set_fetch_ring_in_psram(config[CONF_FETCH_RING_IN_PSRAM]))

    cg.add_define("USE_AUDIO_PROCESSOR")

    add_idf_component(name="espressif/esp-sr", ref="~2.3.0")


@automation.register_action(
    "esp_afe.set_mode",
    SetModeAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EspAfe),
            cv.Required(CONF_MODE): cv.templatable(cv.string),
        }
    ),
    synchronous=True,
)
async def set_mode_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    templ = await cg.templatable(config[CONF_MODE], args, cg.std_string)
    cg.add(var.set_mode(templ))
    return var
