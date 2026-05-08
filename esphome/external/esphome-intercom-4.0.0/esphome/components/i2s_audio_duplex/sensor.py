"""Sensor platform for I2S Audio Duplex diagnostics."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import i2s_audio_duplex_ns, I2SAudioDuplex, CONF_I2S_AUDIO_DUPLEX_ID

CONF_SLOT = "slot"
CONF_TDM_SLOT_LEVELS = "tdm_slot_levels"

TdmSlotLevelSensor = i2s_audio_duplex_ns.class_(
    "TdmSlotLevelSensor",
    sensor.Sensor,
    cg.PollingComponent,
    cg.Parented.template(I2SAudioDuplex),
)


def _slot_level_schema():
    return (
        sensor.sensor_schema(
            TdmSlotLevelSensor,
            unit_of_measurement="dBFS",
            accuracy_decimals=1,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:microphone-message",
        )
        .extend(cv.polling_component_schema("500ms"))
        .extend({cv.Required(CONF_SLOT): cv.int_range(min=0, max=7)})
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_I2S_AUDIO_DUPLEX_ID): cv.use_id(I2SAudioDuplex),
        cv.Optional(CONF_TDM_SLOT_LEVELS): cv.ensure_list(_slot_level_schema()),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_I2S_AUDIO_DUPLEX_ID])

    for conf in config.get(CONF_TDM_SLOT_LEVELS, []):
        slot = conf[CONF_SLOT]
        cg.add(parent.set_tdm_slot_level_sensor_enabled(slot, True))
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_slot(slot))
