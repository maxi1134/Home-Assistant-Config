import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_STEP,
    CONF_MODE,
)

from . import intercom_api_ns, IntercomApi, CONF_INTERCOM_API_ID

DEPENDENCIES = ["intercom_api"]

# Number types
CONF_SPEAKER_VOLUME = "speaker_volume"
CONF_MIC_GAIN = "mic_gain"

# C++ classes (simple - parent syncs state after boot)
IntercomApiVolume = intercom_api_ns.class_(
    "IntercomApiVolume", number.Number, cg.Parented.template(IntercomApi)
)
IntercomApiMicGain = intercom_api_ns.class_(
    "IntercomApiMicGain", number.Number, cg.Parented.template(IntercomApi)
)


def _number_schema(number_class, icon, min_val, max_val, step):
    """Create number schema for a specific number type."""
    return number.number_schema(
        number_class,
        icon=icon,
    ).extend(
        {
            cv.GenerateID(CONF_INTERCOM_API_ID): cv.use_id(IntercomApi),
            cv.Optional(CONF_MIN_VALUE, default=min_val): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default=max_val): cv.float_,
            cv.Optional(CONF_STEP, default=step): cv.float_,
            cv.Optional(CONF_MODE, default="SLIDER"): cv.enum(
                number.NUMBER_MODES, upper=True
            ),
        }
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_INTERCOM_API_ID): cv.use_id(IntercomApi),
        # Speaker volume (0-100%), default 100%
        cv.Optional(CONF_SPEAKER_VOLUME): _number_schema(
            IntercomApiVolume, "mdi:volume-high", 0, 100, 5
        ),
        # Mic gain in dB (-20 to +20), default 0dB
        cv.Optional(CONF_MIC_GAIN): _number_schema(
            IntercomApiMicGain, "mdi:microphone", -20, 20, 1
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_INTERCOM_API_ID])

    if CONF_SPEAKER_VOLUME in config:
        conf = config[CONF_SPEAKER_VOLUME]
        var = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        cg.add(var.set_parent(parent))
        # Register with parent for state sync after boot
        cg.add(parent.register_volume_number(var))

    if CONF_MIC_GAIN in config:
        conf = config[CONF_MIC_GAIN]
        var = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        cg.add(var.set_parent(parent))
        # Register with parent for state sync after boot
        cg.add(parent.register_mic_gain_number(var))
