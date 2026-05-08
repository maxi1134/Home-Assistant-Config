"""Sensors for Intercom Audio component."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
    STATE_CLASS_MEASUREMENT,
    UNIT_EMPTY,
)

from . import IntercomAudio, intercom_audio_ns

CONF_INTERCOM_AUDIO_ID = "intercom_audio_id"
CONF_TX_PACKETS = "tx_packets"
CONF_RX_PACKETS = "rx_packets"
CONF_BUFFER_FILL = "buffer_fill"

IntercomAudioSensor = intercom_audio_ns.class_(
    "IntercomAudioSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_INTERCOM_AUDIO_ID): cv.use_id(IntercomAudio),
    cv.Optional(CONF_TX_PACKETS): sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ).extend({cv.GenerateID(): cv.declare_id(IntercomAudioSensor)}).extend(cv.polling_component_schema("1s")),
    cv.Optional(CONF_RX_PACKETS): sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ).extend({cv.GenerateID(): cv.declare_id(IntercomAudioSensor)}).extend(cv.polling_component_schema("1s")),
    cv.Optional(CONF_BUFFER_FILL): sensor.sensor_schema(
        unit_of_measurement="B",
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend({cv.GenerateID(): cv.declare_id(IntercomAudioSensor)}).extend(cv.polling_component_schema("1s")),
})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_INTERCOM_AUDIO_ID])

    if CONF_TX_PACKETS in config:
        conf = config[CONF_TX_PACKETS]
        sens = await sensor.new_sensor(conf)
        await cg.register_component(sens, conf)
        cg.add(sens.set_parent(parent))
        cg.add(sens.set_sensor_type(0))  # TX

    if CONF_RX_PACKETS in config:
        conf = config[CONF_RX_PACKETS]
        sens = await sensor.new_sensor(conf)
        await cg.register_component(sens, conf)
        cg.add(sens.set_parent(parent))
        cg.add(sens.set_sensor_type(1))  # RX

    if CONF_BUFFER_FILL in config:
        conf = config[CONF_BUFFER_FILL]
        sens = await sensor.new_sensor(conf)
        await cg.register_component(sens, conf)
        cg.add(sens.set_parent(parent))
        cg.add(sens.set_sensor_type(2))  # Buffer
