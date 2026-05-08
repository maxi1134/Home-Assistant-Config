"""Text sensors for Intercom Audio component."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC

from . import IntercomAudio, intercom_audio_ns

CONF_INTERCOM_AUDIO_ID = "intercom_audio_id"
CONF_STATE = "state"
CONF_MODE = "mode"

IntercomAudioTextSensor = intercom_audio_ns.class_(
    "IntercomAudioTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

IntercomAudioModeTextSensor = intercom_audio_ns.class_(
    "IntercomAudioModeTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_INTERCOM_AUDIO_ID): cv.use_id(IntercomAudio),
    cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend({cv.GenerateID(): cv.declare_id(IntercomAudioTextSensor)}),
    cv.Optional(CONF_MODE): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend({cv.GenerateID(): cv.declare_id(IntercomAudioModeTextSensor)}),
})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_INTERCOM_AUDIO_ID])

    if CONF_STATE in config:
        conf = config[CONF_STATE]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(sens.set_parent(parent))

    if CONF_MODE in config:
        conf = config[CONF_MODE]
        sens = await text_sensor.new_text_sensor(conf)
        await cg.register_component(sens, conf)
        cg.add(sens.set_parent(parent))
