import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = []

runtime_diag_ns = cg.esphome_ns.namespace("runtime_diag")
RuntimeDiag = runtime_diag_ns.class_("RuntimeDiag", cg.Component)

CONF_DUMP_INTERVAL = "dump_interval"
CONF_DUMP_WINDOW = "dump_window"
CONF_DUMP_BACKTRACES = "dump_backtraces"
CONF_DUMP_NETWORK = "dump_network"
CONF_PERIODIC_DUMP = "periodic_dump"
CONF_TASK_LOG_LIMIT = "task_log_limit"
CONF_LOOP_WATCH = "loop_watch"
CONF_LOOP_WATCH_INTERVAL = "loop_watch_interval"
CONF_LOOP_STALL_THRESHOLD = "loop_stall_threshold"
CONF_LOOP_WATCH_STACK_SIZE = "loop_watch_stack_size"
CONF_LOOP_WATCH_PRIORITY = "loop_watch_priority"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RuntimeDiag),
        cv.Optional(CONF_DUMP_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DUMP_WINDOW, default="3min"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DUMP_BACKTRACES, default=False): cv.boolean,
        cv.Optional(CONF_DUMP_NETWORK, default=True): cv.boolean,
        cv.Optional(CONF_PERIODIC_DUMP, default=False): cv.boolean,
        cv.Optional(CONF_TASK_LOG_LIMIT, default=64): cv.positive_int,
        cv.Optional(CONF_LOOP_WATCH, default=False): cv.boolean,
        cv.Optional(CONF_LOOP_WATCH_INTERVAL, default="500ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_LOOP_STALL_THRESHOLD, default="3s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_LOOP_WATCH_STACK_SIZE, default=4096): cv.positive_int,
        cv.Optional(CONF_LOOP_WATCH_PRIORITY, default=10): cv.positive_int,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_dump_interval(config[CONF_DUMP_INTERVAL]))
    cg.add(var.set_dump_window(config[CONF_DUMP_WINDOW]))
    cg.add(var.set_dump_backtraces(config[CONF_DUMP_BACKTRACES]))
    cg.add(var.set_dump_network(config[CONF_DUMP_NETWORK]))
    cg.add(var.set_periodic_dump(config[CONF_PERIODIC_DUMP]))
    cg.add(var.set_task_log_limit(config[CONF_TASK_LOG_LIMIT]))
    cg.add(var.set_loop_watch(config[CONF_LOOP_WATCH]))
    cg.add(var.set_loop_watch_interval(config[CONF_LOOP_WATCH_INTERVAL]))
    cg.add(var.set_loop_stall_threshold(config[CONF_LOOP_STALL_THRESHOLD]))
    cg.add(var.set_loop_watch_stack_size(config[CONF_LOOP_WATCH_STACK_SIZE]))
    cg.add(var.set_loop_watch_priority(config[CONF_LOOP_WATCH_PRIORITY]))
