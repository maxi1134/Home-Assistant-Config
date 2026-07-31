import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import automation
from esphome.components.ethernet import EthernetComponent
from esphome.const import (
    CONF_CLK_PIN,
    CONF_CS_PIN,
    CONF_ID,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_RESET_PIN,
    CONF_TYPE,
)
from esphome.core import CORE

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["esp32", "ethernet"]

# Match the locally-defined keys ethernet uses for these (not in esphome.const).
CONF_CLOCK_SPEED = "clock_speed"
CONF_ETHERNET_ID = "ethernet_id"
CONF_INTERFACE = "interface"
CONF_ON_LINK_UP = "on_link_up"

w5500_link_monitor_ns = cg.esphome_ns.namespace("w5500_link_monitor")
W5500LinkMonitor = w5500_link_monitor_ns.class_(
    "W5500LinkMonitor", cg.PollingComponent
)
StartAction = w5500_link_monitor_ns.class_(
    "StartAction", automation.Action, cg.Parented.template(W5500LinkMonitor)
)

spi_host_device_t = cg.global_ns.enum("spi_host_device_t")

SPI_INTERFACE_MAP = {
    "spi2": spi_host_device_t.SPI2_HOST,
    "spi3": spi_host_device_t.SPI3_HOST,
}

# SPI bus, pins, and clock speed are inherited from the referenced ethernet
# component — the design is that the two share the W5500 bus, with only one
# active per boot. Inheriting also means we pick up ethernet's variant-aware
# GPIO validation, clock-speed range check, and SPI-host conflict detection
# for free instead of duplicating those validators here.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(W5500LinkMonitor),
        cv.GenerateID(CONF_ETHERNET_ID): cv.use_id(EthernetComponent),
        cv.Optional(CONF_ON_LINK_UP): automation.validate_automation(),
    }
).extend(cv.polling_component_schema("2s"))


def _resolve_ethernet_config(fconf, eth_id):
    # get_path_for_id walks the whole config tree, so this also works if
    # ethernet ever gains MULTI_CONF.
    eth_path = fconf.get_path_for_id(eth_id)[:-1]
    return fconf.get_config_for_path(eth_path)


def _final_validate(config):
    # `cv.use_id(EthernetComponent)` already guaranteed at schema-validation
    # time that the referenced id resolves to an ethernet component — only the
    # W5500-type check is left here.
    eth_config = _resolve_ethernet_config(fv.full_config.get(), config[CONF_ETHERNET_ID])
    if eth_config.get(CONF_TYPE) != "W5500":
        raise cv.Invalid(
            "w5500_link_monitor only supports `ethernet:` configured with type: W5500."
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


@automation.register_action(
    "w5500_link_monitor.start",
    StartAction,
    automation.maybe_simple_id(
        {cv.Required(CONF_ID): cv.use_id(W5500LinkMonitor)}
    ),
    synchronous=True,
)
async def w5500_link_monitor_start_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Pull SPI host, pins, and clock from the validated ethernet config so the
    # two stay in lockstep automatically. Resolve via the user-supplied
    # ethernet_id rather than the singleton key so the YAML link is honored.
    eth_config = _resolve_ethernet_config(CORE.config, config[CONF_ETHERNET_ID])
    cg.add(var.set_interface(SPI_INTERFACE_MAP[eth_config[CONF_INTERFACE]]))
    cg.add(var.set_clock_speed(eth_config[CONF_CLOCK_SPEED]))
    cg.add(var.set_clk_pin(eth_config[CONF_CLK_PIN]))
    cg.add(var.set_miso_pin(eth_config[CONF_MISO_PIN]))
    cg.add(var.set_mosi_pin(eth_config[CONF_MOSI_PIN]))
    cg.add(var.set_cs_pin(eth_config[CONF_CS_PIN]))
    cg.add(var.set_reset_pin(eth_config[CONF_RESET_PIN]))

    for conf in config.get(CONF_ON_LINK_UP, []):
        await automation.build_callback_automation(
            var, "add_on_link_up_callback", [], conf
        )
