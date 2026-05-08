import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import intercom_api_ns, IntercomApi, CONF_INTERCOM_API_ID

DEPENDENCIES = ["intercom_api"]

# Switch types
CONF_ACTIVE = "active"
CONF_AUTO_ANSWER = "auto_answer"
CONF_AEC = "aec"

# C++ classes (simple - parent syncs state after boot)
IntercomApiSwitch = intercom_api_ns.class_(
    "IntercomApiSwitch", switch.Switch, cg.Parented.template(IntercomApi)
)
IntercomApiAutoAnswer = intercom_api_ns.class_(
    "IntercomApiAutoAnswer", switch.Switch, cg.Parented.template(IntercomApi)
)
IntercomAecSwitch = intercom_api_ns.class_(
    "IntercomAecSwitch", switch.Switch, cg.Parented.template(IntercomApi)
)


def _switch_schema(switch_class, icon, entity_category=None):
    """Create switch schema for a specific switch type."""
    kwargs = {"icon": icon}
    if entity_category is not None:
        kwargs["entity_category"] = entity_category
    return switch.switch_schema(
        switch_class,
        **kwargs,
    ).extend(
        {
            cv.GenerateID(CONF_INTERCOM_API_ID): cv.use_id(IntercomApi),
        }
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_INTERCOM_API_ID): cv.use_id(IntercomApi),
        # On/off control for intercom
        cv.Optional(CONF_ACTIVE): _switch_schema(IntercomApiSwitch, "mdi:phone"),
        # Auto-answer incoming calls (default ON)
        cv.Optional(CONF_AUTO_ANSWER): _switch_schema(
            IntercomApiAutoAnswer, "mdi:phone-in-talk", entity_category=ENTITY_CATEGORY_CONFIG
        ),
        # AEC (Echo Cancellation) - default OFF
        cv.Optional(CONF_AEC): _switch_schema(
            IntercomAecSwitch, "mdi:ear-hearing", entity_category=ENTITY_CATEGORY_CONFIG
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_INTERCOM_API_ID])

    if CONF_ACTIVE in config:
        conf = config[CONF_ACTIVE]
        var = await switch.new_switch(conf)
        cg.add(var.set_parent(parent))

    if CONF_AUTO_ANSWER in config:
        conf = config[CONF_AUTO_ANSWER]
        var = await switch.new_switch(conf)
        cg.add(var.set_parent(parent))
        # Register with parent for state sync after boot
        cg.add(parent.register_auto_answer_switch(var))

    if CONF_AEC in config:
        conf = config[CONF_AEC]
        var = await switch.new_switch(conf)
        cg.add(var.set_parent(parent))
        # Register with parent for state sync after boot
        cg.add(parent.register_aec_switch(var))
