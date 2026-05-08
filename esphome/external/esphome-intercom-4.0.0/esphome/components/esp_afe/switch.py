import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import esp_afe_ns, EspAfe, CONF_ESP_AFE_ID

DEPENDENCIES = ["esp_afe"]

CONF_AEC = "aec"
CONF_SE = "se"
CONF_NS = "ns"
CONF_VAD = "vad"
CONF_AGC = "agc"

AfeAecSwitch = esp_afe_ns.class_(
    "AfeAecSwitch", switch.Switch, cg.Component, cg.Parented.template(EspAfe)
)
AfeNsSwitch = esp_afe_ns.class_(
    "AfeNsSwitch", switch.Switch, cg.Component, cg.Parented.template(EspAfe)
)
AfeSeSwitch = esp_afe_ns.class_(
    "AfeSeSwitch", switch.Switch, cg.Component, cg.Parented.template(EspAfe)
)
AfeVadSwitch = esp_afe_ns.class_(
    "AfeVadSwitch", switch.Switch, cg.Component, cg.Parented.template(EspAfe)
)
AfeAgcSwitch = esp_afe_ns.class_(
    "AfeAgcSwitch", switch.Switch, cg.Component, cg.Parented.template(EspAfe)
)


def _switch_schema(switch_class, icon):
    return switch.switch_schema(
        switch_class,
        icon=icon,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ).extend(
        {
            cv.GenerateID(CONF_ESP_AFE_ID): cv.use_id(EspAfe),
        }
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESP_AFE_ID): cv.use_id(EspAfe),
        cv.Optional(CONF_AEC): _switch_schema(AfeAecSwitch, "mdi:ear-hearing"),
        cv.Optional(CONF_NS): _switch_schema(AfeNsSwitch, "mdi:volume-off"),
        cv.Optional(CONF_SE): _switch_schema(AfeSeSwitch, "mdi:microphone-variant"),
        cv.Optional(CONF_VAD): _switch_schema(AfeVadSwitch, "mdi:account-voice"),
        cv.Optional(CONF_AGC): _switch_schema(AfeAgcSwitch, "mdi:tune-vertical"),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ESP_AFE_ID])

    for conf_key in (CONF_AEC, CONF_NS, CONF_SE, CONF_VAD, CONF_AGC):
        if conf_key in config:
            conf = config[conf_key]
            var = await switch.new_switch(conf)
            await cg.register_component(var, {})
            cg.add(var.set_parent(parent))
