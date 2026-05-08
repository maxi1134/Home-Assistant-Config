"""Sensor platform for mDNS Discovery."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, STATE_CLASS_MEASUREMENT

from . import MdnsDiscovery, mdns_discovery_ns

CONF_MDNS_DISCOVERY_ID = "mdns_discovery_id"
CONF_PEER_COUNT = "peer_count"

MdnsDiscoverySensor = mdns_discovery_ns.class_("MdnsDiscoverySensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MDNS_DISCOVERY_ID): cv.use_id(MdnsDiscovery),
        cv.Optional(CONF_PEER_COUNT): sensor.sensor_schema(
            MdnsDiscoverySensor,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(cv.polling_component_schema("1s")),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MDNS_DISCOVERY_ID])

    if CONF_PEER_COUNT in config:
        sens = await sensor.new_sensor(config[CONF_PEER_COUNT])
        await cg.register_component(sens, config[CONF_PEER_COUNT])
        cg.add(sens.set_parent(parent))
