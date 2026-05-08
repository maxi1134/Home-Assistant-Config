"""Text sensor platform for mDNS Discovery."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from . import MdnsDiscovery, mdns_discovery_ns

CONF_MDNS_DISCOVERY_ID = "mdns_discovery_id"
CONF_PEERS_LIST = "peers_list"

MdnsDiscoveryTextSensor = mdns_discovery_ns.class_(
    "MdnsDiscoveryTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MDNS_DISCOVERY_ID): cv.use_id(MdnsDiscovery),
        cv.Optional(CONF_PEERS_LIST): text_sensor.text_sensor_schema(
            MdnsDiscoveryTextSensor,
        ).extend(cv.polling_component_schema("5s")),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MDNS_DISCOVERY_ID])

    if CONF_PEERS_LIST in config:
        sens = await text_sensor.new_text_sensor(config[CONF_PEERS_LIST])
        await cg.register_component(sens, config[CONF_PEERS_LIST])
        cg.add(sens.set_parent(parent))
