"""
mDNS Discovery Component for ESPHome
Query mDNS services on the local network
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID

CODEOWNERS = ["@n-IA-hane"]
DEPENDENCIES = ["network", "mdns"]
AUTO_LOAD = ["sensor", "text_sensor"]

CONF_SERVICE_TYPE = "service_type"
CONF_SCAN_INTERVAL = "scan_interval"
CONF_PEER_TIMEOUT = "peer_timeout"
CONF_ON_PEER_FOUND = "on_peer_found"
CONF_ON_PEER_LOST = "on_peer_lost"
CONF_ON_SCAN_COMPLETE = "on_scan_complete"

mdns_discovery_ns = cg.esphome_ns.namespace("mdns_discovery")
MdnsDiscovery = mdns_discovery_ns.class_("MdnsDiscovery", cg.Component)

# Triggers
PeerFoundTrigger = mdns_discovery_ns.class_(
    "PeerFoundTrigger", automation.Trigger.template(cg.std_string, cg.std_string, cg.uint16)
)
PeerLostTrigger = mdns_discovery_ns.class_(
    "PeerLostTrigger", automation.Trigger.template(cg.std_string)
)
ScanCompleteTrigger = mdns_discovery_ns.class_(
    "ScanCompleteTrigger", automation.Trigger.template(cg.int_)
)

# Action
ScanAction = mdns_discovery_ns.class_("ScanAction", automation.Action)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MdnsDiscovery),
        cv.Required(CONF_SERVICE_TYPE): cv.string,
        cv.Optional(CONF_SCAN_INTERVAL, default="10s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PEER_TIMEOUT, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ON_PEER_FOUND): automation.validate_automation(
            {
                cv.GenerateID(): cv.declare_id(PeerFoundTrigger),
            }
        ),
        cv.Optional(CONF_ON_PEER_LOST): automation.validate_automation(
            {
                cv.GenerateID(): cv.declare_id(PeerLostTrigger),
            }
        ),
        cv.Optional(CONF_ON_SCAN_COMPLETE): automation.validate_automation(
            {
                cv.GenerateID(): cv.declare_id(ScanCompleteTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_service_type(config[CONF_SERVICE_TYPE]))
    cg.add(var.set_scan_interval(config[CONF_SCAN_INTERVAL]))
    cg.add(var.set_peer_timeout(config[CONF_PEER_TIMEOUT]))

    # Triggers
    if CONF_ON_PEER_FOUND in config:
        for conf in config[CONF_ON_PEER_FOUND]:
            trigger = cg.new_Pvariable(conf[CONF_ID], var)
            await automation.build_automation(
                trigger, [(cg.std_string, "name"), (cg.std_string, "ip"), (cg.uint16, "port")], conf
            )
    if CONF_ON_PEER_LOST in config:
        for conf in config[CONF_ON_PEER_LOST]:
            trigger = cg.new_Pvariable(conf[CONF_ID], var)
            await automation.build_automation(
                trigger, [(cg.std_string, "name")], conf
            )
    if CONF_ON_SCAN_COMPLETE in config:
        for conf in config[CONF_ON_SCAN_COMPLETE]:
            trigger = cg.new_Pvariable(conf[CONF_ID], var)
            await automation.build_automation(
                trigger, [(cg.int_, "peer_count")], conf
            )


# Register action
@automation.register_action(
    "mdns_discovery.scan",
    ScanAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(MdnsDiscovery),
    }),
)
async def scan_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
