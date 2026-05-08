"""Number platform for I2S Audio Duplex - Mic gain and speaker volume"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import i2s_audio_duplex_ns, I2SAudioDuplex, CONF_I2S_AUDIO_DUPLEX_ID
CONF_MIC_GAIN = "mic_gain"
CONF_SPEAKER_VOLUME = "speaker_volume"
CONF_PRE_AEC = "pre_aec"

# Number classes
MicGainNumber = i2s_audio_duplex_ns.class_("MicGainNumber", number.Number, cg.Component)
SpeakerVolumeNumber = i2s_audio_duplex_ns.class_("SpeakerVolumeNumber", number.Number, cg.Component)

MIC_GAIN_SCHEMA = number.number_schema(
    MicGainNumber,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon="mdi:microphone",
).extend({
    cv.Optional(CONF_PRE_AEC, default=False): cv.boolean,
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_I2S_AUDIO_DUPLEX_ID): cv.use_id(I2SAudioDuplex),
    cv.Optional(CONF_MIC_GAIN): MIC_GAIN_SCHEMA,
    cv.Optional("mic_gain_pre_aec"): MIC_GAIN_SCHEMA,
    cv.Optional(CONF_SPEAKER_VOLUME): number.number_schema(
        SpeakerVolumeNumber,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:volume-high",
    ),
})


async def _setup_mic_gain(config, key, parent):
    if key in config:
        conf = config[key]
        var = await number.new_number(
            conf,
            min_value=-20.0,
            max_value=30.0,
            step=1.0,
        )
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_pre_aec(conf[CONF_PRE_AEC]))


async def to_code(config):
    parent = await cg.get_variable(config[CONF_I2S_AUDIO_DUPLEX_ID])

    await _setup_mic_gain(config, CONF_MIC_GAIN, parent)
    await _setup_mic_gain(config, "mic_gain_pre_aec", parent)

    if CONF_SPEAKER_VOLUME in config:
        conf = config[CONF_SPEAKER_VOLUME]
        var = await number.new_number(
            conf,
            min_value=0.0,
            max_value=1.0,
            step=0.05,
        )
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
