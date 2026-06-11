import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from . import intercom_api_ns, IntercomApi, CONF_INTERCOM_API_ID

DEPENDENCIES = ["intercom_api"]

CONF_CALL = "call"
CONF_NEXT_CONTACT = "next_contact"
CONF_PREVIOUS_CONTACT = "previous_contact"
CONF_DECLINE = "decline"

IntercomCallButton = intercom_api_ns.class_(
    "IntercomCallButton", button.Button, cg.Parented.template(IntercomApi)
)
IntercomNextContactButton = intercom_api_ns.class_(
    "IntercomNextContactButton", button.Button, cg.Parented.template(IntercomApi)
)
IntercomPreviousContactButton = intercom_api_ns.class_(
    "IntercomPreviousContactButton", button.Button, cg.Parented.template(IntercomApi)
)
IntercomDeclineButton = intercom_api_ns.class_(
    "IntercomDeclineButton", button.Button, cg.Parented.template(IntercomApi)
)


def _button_schema(button_class, icon):
    return button.button_schema(button_class, icon=icon)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_INTERCOM_API_ID): cv.use_id(IntercomApi),
        cv.Optional(CONF_CALL): _button_schema(IntercomCallButton, "mdi:phone"),
        cv.Optional(CONF_NEXT_CONTACT): _button_schema(
            IntercomNextContactButton, "mdi:skip-next"
        ),
        cv.Optional(CONF_PREVIOUS_CONTACT): _button_schema(
            IntercomPreviousContactButton, "mdi:skip-previous"
        ),
        cv.Optional(CONF_DECLINE): _button_schema(
            IntercomDeclineButton, "mdi:phone-hangup"
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_INTERCOM_API_ID])

    for key in (CONF_CALL, CONF_NEXT_CONTACT, CONF_PREVIOUS_CONTACT, CONF_DECLINE):
        if key not in config:
            continue
        var = await button.new_button(config[key])
        cg.add(var.set_parent(parent))
