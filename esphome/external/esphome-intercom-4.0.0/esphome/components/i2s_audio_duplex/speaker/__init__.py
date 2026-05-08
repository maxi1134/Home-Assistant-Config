"""I2S Audio Duplex Speaker Platform - Wraps duplex bus as standard ESPHome speaker"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import audio, speaker
from esphome.const import CONF_ID, CONF_NUM_CHANNELS
from .. import (
    i2s_audio_duplex_ns,
    I2SAudioDuplex,
    CONF_I2S_AUDIO_DUPLEX_ID,
)

DEPENDENCIES = ["i2s_audio_duplex"]
CODEOWNERS = ["@n-IA-hane"]

I2SAudioDuplexSpeaker = i2s_audio_duplex_ns.class_(
    "I2SAudioDuplexSpeaker",
    speaker.Speaker,
    cg.Component,
    cg.Parented.template(I2SAudioDuplex),
)


def _set_audio_properties(config):
    """Set default audio properties for mixer/resampler compatibility.

    Note: sample_rate is NOT defaulted here because the speaker operates at bus
    rate (e.g. 48kHz for ES8311), not a fixed 16kHz. The C++ setup() sets
    audio_stream_info_ from parent->get_sample_rate() at runtime.
    """
    if CONF_NUM_CHANNELS not in config:
        config[CONF_NUM_CHANNELS] = 1
    return config


def _set_stream_limits(config):
    # Speaker accepts audio at bus rate (e.g. 48kHz). ResamplerSpeaker handles upsampling
    audio.set_stream_limits(
        min_bits_per_sample=16,
        max_bits_per_sample=16,
        min_channels=1,
        max_channels=1,
        min_sample_rate=8000,
        max_sample_rate=48000,
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    speaker.SPEAKER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(I2SAudioDuplexSpeaker),
            cv.GenerateID(CONF_I2S_AUDIO_DUPLEX_ID): cv.use_id(I2SAudioDuplex),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _set_audio_properties,
    _set_stream_limits,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_I2S_AUDIO_DUPLEX_ID])
    cg.add(var.set_parent(parent))

    await speaker.register_speaker(var, config)
