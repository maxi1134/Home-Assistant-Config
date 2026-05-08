"""
Intercom Audio Component for ESPHome

Provides UDP-based audio streaming. Supports two modes:
1. Separate microphone + speaker (half-duplex on shared I2S bus)
2. i2s_audio_duplex component (true full-duplex)

Supports:
- Home Assistant mode (go2rtc/WebRTC)
- P2P mode (ESP-to-ESP direct)
- Echo cancellation integration (esp_aec)
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import microphone, speaker
from esphome.const import CONF_ID, CONF_PORT

CODEOWNERS = ["@n-IA-hane"]
DEPENDENCIES = []
AUTO_LOAD = ["sensor", "text_sensor", "switch"]

CONF_MICROPHONE_ID = "microphone_id"
CONF_SPEAKER_ID = "speaker_id"
CONF_DUPLEX_ID = "duplex_id"  # New: i2s_audio_duplex component
CONF_AEC_ID = "aec_id"
CONF_LISTEN_PORT = "listen_port"
CONF_REMOTE_IP = "remote_ip"
CONF_REMOTE_PORT = "remote_port"
CONF_BUFFER_SIZE = "buffer_size"
CONF_PREBUFFER_SIZE = "prebuffer_size"
CONF_ON_START = "on_start"
CONF_ON_STOP = "on_stop"
CONF_DC_OFFSET_REMOVAL = "dc_offset_removal"

intercom_audio_ns = cg.esphome_ns.namespace("intercom_audio")
IntercomAudio = intercom_audio_ns.class_("IntercomAudio", cg.Component)

# Actions
StartAction = intercom_audio_ns.class_("StartAction", automation.Action)
StopAction = intercom_audio_ns.class_("StopAction", automation.Action)
ResetCountersAction = intercom_audio_ns.class_("ResetCountersAction", automation.Action)

# Forward declare esp_aec if available
esp_aec_ns = cg.esphome_ns.namespace("esp_aec")
EspAec = esp_aec_ns.class_("EspAec")

# Forward declare i2s_audio_duplex
i2s_audio_duplex_ns = cg.esphome_ns.namespace("i2s_audio_duplex")
I2SAudioDuplex = i2s_audio_duplex_ns.class_("I2SAudioDuplex")


def validate_audio_config(config):
    """Validate audio configuration.

    Supported modes:
    - duplex_id: Single I2S bus full-duplex (ES8311, ES8388)
    - microphone_id only: TX-only mode (ambient listening, baby monitor)
    - speaker_id only: RX-only mode (announcements, notifications)
    - microphone_id + speaker_id: Dual I2S bus full-duplex
    """
    has_duplex = CONF_DUPLEX_ID in config
    has_mic = CONF_MICROPHONE_ID in config
    has_spk = CONF_SPEAKER_ID in config

    if has_duplex:
        if has_mic or has_spk:
            raise cv.Invalid("Cannot use duplex_id together with microphone_id/speaker_id")
    else:
        if not has_mic and not has_spk:
            raise cv.Invalid(
                "At least one audio source required: duplex_id, microphone_id, or speaker_id"
            )

    # Validate buffer sizes
    buffer_size = config.get(CONF_BUFFER_SIZE, 8192)
    prebuffer_size = config.get(CONF_PREBUFFER_SIZE, 2048)
    if prebuffer_size >= buffer_size:
        raise cv.Invalid(
            f"prebuffer_size ({prebuffer_size}) must be smaller than "
            f"buffer_size ({buffer_size})"
        )
    # Minimum buffer size for reasonable audio (64ms at 16kHz = 2048 bytes)
    if buffer_size < 2048:
        raise cv.Invalid(
            f"buffer_size ({buffer_size}) is too small, minimum is 2048 bytes"
        )

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(IntercomAudio),
        cv.Optional(CONF_DUPLEX_ID): cv.use_id(I2SAudioDuplex),
        cv.Optional(CONF_MICROPHONE_ID): cv.use_id(microphone.Microphone),
        cv.Optional(CONF_SPEAKER_ID): cv.use_id(speaker.Speaker),
        cv.Optional(CONF_AEC_ID): cv.use_id(EspAec),
        cv.Optional(CONF_LISTEN_PORT, default=12346): cv.All(
            cv.port, cv.Range(min=1024, max=65535)
        ),
        cv.Optional(CONF_REMOTE_IP, default=""): cv.templatable(cv.string),
        cv.Optional(CONF_REMOTE_PORT, default=12346): cv.templatable(cv.All(
            cv.port, cv.Range(min=1024, max=65535)
        )),
        cv.Optional(CONF_BUFFER_SIZE, default=8192): cv.All(
            cv.positive_int, cv.Range(min=2048, max=65536)
        ),
        cv.Optional(CONF_PREBUFFER_SIZE, default=2048): cv.All(
            cv.positive_int, cv.Range(min=512, max=32768)
        ),
        cv.Optional(CONF_DC_OFFSET_REMOVAL, default=False): cv.boolean,
        cv.Optional(CONF_ON_START): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_STOP): automation.validate_automation(single=True),
    }).extend(cv.COMPONENT_SCHEMA),
    validate_audio_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Check which mode: duplex or separate mic/speaker
    if CONF_DUPLEX_ID in config:
        # Full duplex mode using i2s_audio_duplex
        duplex = await cg.get_variable(config[CONF_DUPLEX_ID])
        cg.add(var.set_duplex(duplex))
    else:
        # Separate mode: mic only, speaker only, or both
        if CONF_MICROPHONE_ID in config:
            mic = await cg.get_variable(config[CONF_MICROPHONE_ID])
            cg.add(var.set_microphone(mic))

        if CONF_SPEAKER_ID in config:
            spk = await cg.get_variable(config[CONF_SPEAKER_ID])
            cg.add(var.set_speaker(spk))

    # Optional AEC (USE_ESP_AEC defined by esp_aec component)
    if CONF_AEC_ID in config:
        aec = await cg.get_variable(config[CONF_AEC_ID])
        cg.add(var.set_aec(aec))

    # UDP settings
    cg.add(var.set_listen_port(config[CONF_LISTEN_PORT]))

    # Remote IP - always use lambda (evaluated at start() time)
    # User can provide: static IP, substitution variable, or lambda
    if CONF_REMOTE_IP in config and config[CONF_REMOTE_IP]:
        template_ = await cg.templatable(config[CONF_REMOTE_IP], [], cg.std_string)
        cg.add(var.set_remote_ip_lambda(template_))

    # Remote port - always use lambda (evaluated at start() time)
    template_ = await cg.templatable(config[CONF_REMOTE_PORT], [], cg.uint16)
    cg.add(var.set_remote_port_lambda(template_))

    # Buffer settings
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_prebuffer_size(config[CONF_PREBUFFER_SIZE]))

    # DC offset removal (for mics with significant DC bias like SPH0645)
    cg.add(var.set_dc_offset_removal(config[CONF_DC_OFFSET_REMOVAL]))

    # Automations
    if CONF_ON_START in config:
        await automation.build_automation(
            var.get_start_trigger(), [], config[CONF_ON_START]
        )
    if CONF_ON_STOP in config:
        await automation.build_automation(
            var.get_stop_trigger(), [], config[CONF_ON_STOP]
        )


# Action: start streaming
@automation.register_action("intercom_audio.start", StartAction, cv.Schema({
    cv.GenerateID(): cv.use_id(IntercomAudio),
    cv.Optional(CONF_REMOTE_IP): cv.templatable(cv.string),
    cv.Optional(CONF_REMOTE_PORT): cv.templatable(cv.port),
}))
async def start_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if CONF_REMOTE_IP in config:
        template_ = await cg.templatable(config[CONF_REMOTE_IP], args, cg.std_string)
        cg.add(var.set_remote_ip(template_))
    if CONF_REMOTE_PORT in config:
        template_ = await cg.templatable(config[CONF_REMOTE_PORT], args, cg.uint16)
        cg.add(var.set_remote_port(template_))
    return var


# Action: stop streaming
@automation.register_action("intercom_audio.stop", StopAction, cv.Schema({
    cv.GenerateID(): cv.use_id(IntercomAudio),
}))
async def stop_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


# Action: reset packet counters
@automation.register_action("intercom_audio.reset_counters", ResetCountersAction, cv.Schema({
    cv.GenerateID(): cv.use_id(IntercomAudio),
}))
async def reset_counters_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
