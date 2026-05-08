"""Switch for Intercom Audio component."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from . import IntercomAudio, intercom_audio_ns

CONF_INTERCOM_AUDIO_ID = "intercom_audio_id"
CONF_STREAMING = "streaming"
CONF_AEC = "aec"

IntercomAudioSwitch = intercom_audio_ns.class_(
    "IntercomAudioSwitch", switch.Switch, cg.Component
)

IntercomAudioAecSwitch = intercom_audio_ns.class_(
    "IntercomAudioAecSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_INTERCOM_AUDIO_ID): cv.use_id(IntercomAudio),
    cv.Optional(CONF_STREAMING): switch.switch_schema(
        IntercomAudioSwitch,
        icon="mdi:phone",
    ),
    cv.Optional(CONF_AEC): switch.switch_schema(
        IntercomAudioAecSwitch,
        icon="mdi:echo-off",
    ),
})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_INTERCOM_AUDIO_ID])

    if CONF_STREAMING in config:
        conf = config[CONF_STREAMING]
        sw = await switch.new_switch(conf)
        await cg.register_component(sw, conf)
        cg.add(sw.set_parent(parent))

    if CONF_AEC in config:
        conf = config[CONF_AEC]
        sw = await switch.new_switch(conf)
        await cg.register_component(sw, conf)
        cg.add(sw.set_parent(parent))
