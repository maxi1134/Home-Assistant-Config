"""Switch platform for I2S Audio Duplex - AEC enable/disable"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import i2s_audio_duplex_ns, I2SAudioDuplex, CONF_I2S_AUDIO_DUPLEX_ID
CONF_AEC = "aec"

# Switch class
AECSwitch = i2s_audio_duplex_ns.class_("AECSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_I2S_AUDIO_DUPLEX_ID): cv.use_id(I2SAudioDuplex),
    cv.Optional(CONF_AEC): switch.switch_schema(
        AECSwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:account-voice",
    ),
})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_I2S_AUDIO_DUPLEX_ID])

    if CONF_AEC in config:
        conf = config[CONF_AEC]
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
