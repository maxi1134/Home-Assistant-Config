import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.core import CORE, TimePeriod
from esphome.const import (
    CONF_ID,
    CONF_MICROPHONE,
    CONF_SPEAKER,
    CONF_ICON,
    CONF_NAME,
    CONF_MODE,
    CONF_DISABLED_BY_DEFAULT,
)
from esphome.components import audio, microphone, speaker, text_sensor

CODEOWNERS = ["@n-IA-hane"]
DEPENDENCIES = ["esp32"]


def AUTO_LOAD(config):
    # audio_processor is still an internal helper provider for task/ring-buffer
    # utilities used by the transport. It is not an intercom DSP mode.
    base = ["audio_processor", "button", "switch", "number", "text_sensor"]
    discovery = (config or {}).get("discovery") or {}
    if (config or {}).get("announce", False) or (
        isinstance(discovery, dict) and discovery.get("mdns", False)
    ):
        base.append("mdns")
    return base

_LOGGER = logging.getLogger(__name__)

CONF_INTERCOM_API_ID = "intercom_api_id"
CONF_DC_OFFSET_REMOVAL = "dc_offset_removal"
CONF_TASK_STACKS_IN_PSRAM = "task_stacks_in_psram"
CONF_BUFFERS_IN_PSRAM = "buffers_in_psram"
CONF_AUTO_ENTITIES = "auto_entities"
CONF_MICROPHONE_SOURCE = "microphone_source"

CONF_PROCESSOR_ID = "processor_id"
CONF_AEC_REF_DELAY_MS = "aec_reference_delay_ms"
CONF_RINGING_TIMEOUT = "ringing_timeout"
CONF_CALLING_TIMEOUT = "calling_timeout"
CONF_ON_DESTINATION_CHANGED = "on_destination_changed"
CONF_ON_UPDATE_CONTACTS = "on_update_contacts"
CONF_DELETE_CONTACT_MISSING_FROM = "delete_contact_missing_from"
CONF_UPDATES_NUMBER = "updates_number"
CONF_HA_PHONEBOOK_TEXT_SENSOR_ID = "ha_phonebook_text_sensor_id"
# HA publishes one protocol-aware roster at `sensor.intercom_phonebook`. The
# shipped subscription package binds that HA text_sensor here; intercom_api then
# normalizes each protocol-tagged slot into the local TCP or UDP dial plan.

CONF_ON_RINGING = "on_ringing"
CONF_ON_STREAMING = "on_streaming"
CONF_ON_IDLE = "on_idle"
# FSM triggers
CONF_ON_OUTGOING_CALL = "on_outgoing_call"
CONF_ON_DEST_RINGING = "on_dest_ringing"
CONF_ON_HANGUP = "on_hangup"
CONF_ON_CALL_FAILED = "on_call_failed"
CONF_REASON = "reason"

# PBX-lite transport selection. TCP is default; UDP uses the same control
# protocol with separate audio/control ports.
CONF_PROTOCOL = "protocol"
CONF_REMOTE_IP = "remote_ip"
CONF_REMOTE_PORT = "remote_port"
CONF_LISTEN_PORT = "listen_port"
CONF_CONTROL_PORT = "control_port"
CONF_TCP_PORT = "tcp_port"
CONF_ROUTING_MODE = "routing_mode"
CONF_USE_HA_AS_FIRST_CONTACT = "use_ha_as_first_contact"
CONF_AUDIO_DEBUG = "audio_debug"
CONF_ANNOUNCE = "announce"
CONF_DISCOVERY = "discovery"
CONF_MDNS = "mdns"
CONF_ENABLED = "enabled"
CONF_STARTUP_SCAN = "startup_scan"
CONF_INTERVAL = "interval"
CONF_QUERY_TIMEOUT = "query_timeout"
CONF_MAX_RESULTS = "max_results"
CONF_PROTOCOLS = "protocols"
CONF_NETWORK_SOCKET_HEADROOM = "network_socket_headroom"

ROUTING_DEVICE_INDEPENDENT = "device_independent"
ROUTING_HA_PBX = "ha_pbx"
PROTOCOL_TCP = "tcp"
PROTOCOL_UDP = "udp"

# Mode constants. PBX-lite is the implicit default (phonebook / contacts /
# destination / caller entities always exposed); explicit `mode:` is only
# needed to opt into raw-UDP-no-signaling.
MODE_RAW_UDP = "raw_udp"  # raw UDP audio, no PBX-lite signaling

intercom_api_ns = cg.esphome_ns.namespace("intercom_api")
IntercomApi = intercom_api_ns.class_("IntercomApi", cg.Component)
TransportType = intercom_api_ns.enum("TransportType", is_class=True)

# === Action classes (for YAML: intercom_api.next_contact, etc.) ===
NextContactAction = intercom_api_ns.class_("NextContactAction", automation.Action)
PrevContactAction = intercom_api_ns.class_("PrevContactAction", automation.Action)
StartAction = intercom_api_ns.class_("StartAction", automation.Action)
StopAction = intercom_api_ns.class_("StopAction", automation.Action)
AnswerCallAction = intercom_api_ns.class_("AnswerCallAction", automation.Action)
DeclineCallAction = intercom_api_ns.class_("DeclineCallAction", automation.Action)
CallToggleAction = intercom_api_ns.class_("CallToggleAction", automation.Action)
PublishEntityStatesAction = intercom_api_ns.class_("PublishEntityStatesAction", automation.Action)

# Parameterized actions
SetVolumeAction = intercom_api_ns.class_("SetVolumeAction", automation.Action)
SetMicGainDbAction = intercom_api_ns.class_("SetMicGainDbAction", automation.Action)
SetContactsAction = intercom_api_ns.class_("SetContactsAction", automation.Action)
SetContactAction = intercom_api_ns.class_("SetContactAction", automation.Action)
CallContactAction = intercom_api_ns.class_("CallContactAction", automation.Action)
SetRemoteEndpointAction = intercom_api_ns.class_("SetRemoteEndpointAction", automation.Action)
AddContactAction = intercom_api_ns.class_("AddContactAction", automation.Action)
RemoveContactAction = intercom_api_ns.class_("RemoveContactAction", automation.Action)
FlushContactsAction = intercom_api_ns.class_("FlushContactsAction", automation.Action)
UpdateContactsAction = intercom_api_ns.class_("UpdateContactsAction", automation.Action)
SetHaPeerNameAction = intercom_api_ns.class_("SetHaPeerNameAction", automation.Action)
SimulateIncomingCallAction = intercom_api_ns.class_("SimulateIncomingCallAction", automation.Action)

# === Condition classes (for YAML: intercom_api.is_idle, etc.) ===
IntercomIsIdleCondition = intercom_api_ns.class_("IntercomIsIdleCondition", automation.Condition)
IntercomIsRingingCondition = intercom_api_ns.class_("IntercomIsRingingCondition", automation.Condition)
IntercomIsStreamingCondition = intercom_api_ns.class_("IntercomIsStreamingCondition", automation.Condition)
IntercomIsCallingCondition = intercom_api_ns.class_("IntercomIsCallingCondition", automation.Condition)
IntercomIsIncomingCondition = intercom_api_ns.class_("IntercomIsIncomingCondition", automation.Condition)
IntercomIsInCallCondition = intercom_api_ns.class_("IntercomIsInCallCondition", automation.Condition)
IntercomDestinationIsCondition = intercom_api_ns.class_("IntercomDestinationIsCondition", automation.Condition)
IntercomIsHaDestinationCondition = intercom_api_ns.class_("IntercomIsHaDestinationCondition", automation.Condition)

# Auto-entity classes: declared here so to_code below can construct them even
# when YAML does not include explicit `switch:` / `number:` platform blocks.
# The explicit platforms reuse the same class names through the namespace.
from esphome.components import switch as _switch_ns, number as _number_ns
IntercomApiAutoAnswerCls = intercom_api_ns.class_(
    "IntercomApiAutoAnswer", _switch_ns.Switch, cg.Parented.template(IntercomApi)
)
IntercomApiDndSwitchCls = intercom_api_ns.class_(
    "IntercomApiDndSwitch", _switch_ns.Switch, cg.Parented.template(IntercomApi)
)
IntercomRoutingModeSwitchCls = intercom_api_ns.class_(
    "IntercomRoutingModeSwitch", _switch_ns.Switch, cg.Parented.template(IntercomApi)
)
IntercomApiVolumeCls = intercom_api_ns.class_(
    "IntercomApiVolume", _number_ns.Number, cg.Parented.template(IntercomApi)
)
IntercomApiMicGainCls = intercom_api_ns.class_(
    "IntercomApiMicGain", _number_ns.Number, cg.Parented.template(IntercomApi)
)

MDNS_DISCOVERY_DEFAULTS = {
    CONF_ENABLED: True,
    CONF_STARTUP_SCAN: True,
    CONF_INTERVAL: TimePeriod(seconds=60),
    CONF_QUERY_TIMEOUT: TimePeriod(milliseconds=1000),
    CONF_MAX_RESULTS: 8,
    CONF_PROTOCOLS: None,
}


MDNS_DISCOVERY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_STARTUP_SCAN, default=True): cv.boolean,
        cv.Optional(CONF_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_QUERY_TIMEOUT, default="1000ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_RESULTS, default=8): cv.int_range(min=1, max=32),
        # Omit protocols to scan only the local transport. Explicit override is
        # for advanced bridge/fallback builds that intentionally want both.
        cv.Optional(CONF_PROTOCOLS): cv.ensure_list(
            cv.one_of(PROTOCOL_TCP, PROTOCOL_UDP, lower=True)
        ),
    }
)


def _mdns_discovery_config(config):
    discovery = config.get(CONF_DISCOVERY) or {}
    mdns_cfg = discovery.get(CONF_MDNS, False) if isinstance(discovery, dict) else False
    if mdns_cfg is False:
        return None
    if mdns_cfg is True:
        out = MDNS_DISCOVERY_DEFAULTS.copy()
    else:
        out = MDNS_DISCOVERY_DEFAULTS.copy()
        out.update(mdns_cfg)
        if not out[CONF_ENABLED]:
            return None

    protocols = out.get(CONF_PROTOCOLS)
    if protocols is None:
        protocols = [config.get(CONF_PROTOCOL, PROTOCOL_TCP)]
    out[CONF_PROTOCOLS] = protocols
    return out


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(IntercomApi),
        # Mode: optional opt-in for raw UDP audio without PBX-lite signaling.
        # Omit the key for the default PBX-lite behaviour.
        cv.Optional(CONF_MODE): cv.one_of(MODE_RAW_UDP, lower=True),
        # Transport: tcp (default, one PBX-lite socket for signaling+audio)
        # or udp (PBX-lite control socket + raw PCM audio socket).
        cv.Optional(CONF_PROTOCOL, default=PROTOCOL_TCP): cv.one_of(
            PROTOCOL_TCP, PROTOCOL_UDP, lower=True
        ),
        # PBX-lite project default: 6054 for the data plane on both TCP and
        # UDP, 6055 for the UDP control plane (framed signaling). TCP and
        # UDP can share 6054 because they live on different protocol stacks.
        # Override per-device when the host LAN already routes those ports
        # to other services - HA's config flow exposes the matching knobs
        # so endpoint rows and listener binds stay coherent.
        #
        # UDP-only options (ignored for tcp).
        cv.Optional(CONF_REMOTE_IP, default=""): cv.string,
        cv.Optional(CONF_REMOTE_PORT, default=6054): cv.port,
        cv.Optional(CONF_LISTEN_PORT, default=6054): cv.port,
        # UDP signaling port (MessageHeader frames). Must differ from
        # listen_port so audio (raw PCM) and control (framed) stay on
        # separate sockets. UDP contacts may carry a per-peer control port
        # as Name|IP|audio_port|control_port; short Name|IP|audio_port
        # falls back to this local control_port.
        cv.Optional(CONF_CONTROL_PORT, default=6055): cv.port,
        # TCP-only option (ignored for udp). Both server (accept inbound
        # calls from peers) and client (originate to dest's tcp_port) use
        # the same number.
        cv.Optional(CONF_TCP_PORT, default=6054): cv.port,
        # Routing mode for outgoing calls:
        #  - device_independent (default): dial the phonebook entry directly.
        #    True peer-to-peer; HA only sees the call when it is the dest.
        #  - ha_pbx: dial the HA peer IP+port instead, leaving HA to bridge to
        #    the real dest_name. Lets HA log every call and keep services
        #    like intercom_native.forward functional even with otherwise
        #    direct ESP<->ESP reachability.
        cv.Optional(CONF_ROUTING_MODE, default=ROUTING_DEVICE_INDEPENDENT): cv.one_of(
            ROUTING_DEVICE_INDEPENDENT, ROUTING_HA_PBX, lower=True
        ),
        # On the first post-boot phonebook population, select the HA peer row
        # learned from `Name|ha|...` as the current destination. This keeps a
        # freshly booted ESP tuned to HA instead of whichever contact happens
        # to be first in the HA-published CSV order.
        cv.Optional(CONF_USE_HA_AS_FIRST_CONTACT, default=False): cv.boolean,
        # Targeted diagnostics: logs PCM peak/RMS on intercom TX/RX.
        # Keep disabled by default; enable only on devices under audio-level test.
        cv.Optional(CONF_AUDIO_DEBUG, default=False): cv.boolean,
        # Publish this device's canonical endpoint as an mDNS TXT record:
        #   endpoint=Name|tcp|ip|tcp_port
        #   endpoint=Name|udp|ip|audio_port|control_port
        # Compile-time opt-in for ESP-only installs. HA-managed devices do not
        # need this: they publish intercom_endpoint over the native API and HA
        # builds the central phonebook from that sensor.
        cv.Optional(CONF_ANNOUNCE, default=False): cv.boolean,
        cv.Optional(CONF_DISCOVERY, default={}): cv.Schema(
            {
                # Opt-in ESP-side discovery for direct ESP peers. It scans only
                # _intercom-* services and merges endpoint TXT rows into the
                # phonebook.
                cv.Optional(CONF_MDNS, default=False): cv.Any(
                    cv.boolean, MDNS_DISCOVERY_SCHEMA
                ),
            }
        ),
        # Preferred path: use the native ESPHome microphone directly. Maintained
        # esp_audio_stack profiles already expose 16 kHz / 16-bit / mono audio,
        # so MicrophoneSource would only add an avoidable copy/conversion pass.
        cv.Optional(CONF_MICROPHONE): cv.use_id(microphone.Microphone),
        # Compatibility/advanced path for raw microphones that need channel,
        # bit-depth, or integer gain conversion before intercom_api sees them.
        cv.Optional(CONF_MICROPHONE_SOURCE): microphone.microphone_source_schema(),
        cv.Optional(CONF_SPEAKER): cv.use_id(speaker.Speaker),
        # DC offset removal for mics with significant DC bias (e.g., SPH0645)
        cv.Optional(CONF_DC_OFFSET_REMOVAL, default=False): cv.boolean,
        # Place intercom network task stacks in PSRAM. The TX task exists only
        # when a microphone is configured; transport/control tasks are owned by
        # the selected transport. Default false keeps stacks in internal RAM,
        # required on plain ESP32 boards without PSRAM. Set true on heavy S3/P4
        # builds when PSRAM stacks are enabled in sdkconfig.
        cv.Optional(CONF_TASK_STACKS_IN_PSRAM, default=False): cv.boolean,
        # Auto-create the boilerplate switches/numbers (auto_answer, volume,
        # mic_gain). Default false for backward compatibility with yamls that
        # already declare them via `switch:`/`number: - platform: intercom_api`.
        # Set to true on a minimal new yaml to skip that boilerplate.
        cv.Optional(CONF_AUTO_ENTITIES, default=False): cv.boolean,
        # Standalone intercom DSP/AEC was removed. Use native ESPHome mic/speaker
        # directly, or put software AEC/AFE on esp_audio_stack and pass its
        # microphone/speaker facade here.
        cv.Optional(CONF_PROCESSOR_ID): cv.invalid(
            "intercom_api.processor_id was removed. Use native ESPHome "
            "microphone/speaker directly, or put processor_id on esp_audio_stack "
            "for software AEC/AFE."
        ),
        cv.Optional(CONF_AEC_REF_DELAY_MS): cv.invalid(
            "intercom_api.aec_reference_delay_ms was removed with standalone "
            "intercom AEC. Configure AEC on esp_audio_stack instead."
        ),
        # Place intercom staging buffers in PSRAM. This applies only to buffers
        # still owned by intercom_api: the optional mic ring, TX chunk, and mic
        # processing scratch. Software AEC/AFE buffers belong to esp_audio_stack.
        cv.Optional(CONF_BUFFERS_IN_PSRAM, default=False): cv.boolean,
        # Ringing timeout: auto-decline call if not answered within this time
        cv.Optional(CONF_RINGING_TIMEOUT): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_CALLING_TIMEOUT): cv.positive_time_period_milliseconds,
        # Trigger when incoming call (auto_answer OFF)
        cv.Optional(CONF_ON_RINGING): automation.validate_automation(single=True),
        # Trigger when streaming starts
        cv.Optional(CONF_ON_STREAMING): automation.validate_automation(single=True),
        # Trigger when state returns to idle
        cv.Optional(CONF_ON_IDLE): automation.validate_automation(single=True),
        # FSM triggers
        cv.Optional(CONF_ON_OUTGOING_CALL): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_DEST_RINGING): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_HANGUP): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_CALL_FAILED): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_DESTINATION_CHANGED): automation.validate_automation(single=True),
        # Phonebook update cycle hook (fires on update_contacts() action). Wire
        # mDNS scan or any other source here so it feeds the same open cycle.
        cv.Optional(CONF_ON_UPDATE_CONTACTS): automation.validate_automation(single=True),
        # Bind a YAML-declared `homeassistant` text_sensor (typically via
        # packages/intercom/phonebook_subscribe.yaml) as the authoritative
        # HA-side source. Optional: when absent the HA path is skipped and
        # update_contacts() only fires the trigger for any mDNS chain.
        cv.Optional(CONF_HA_PHONEBOOK_TEXT_SENSOR_ID): cv.use_id(
            text_sensor.TextSensor
        ),
        # Contact pruning: delete a contact after N consecutive update cycles
        # in which no source reported it. Optional - absent means pruning is
        # disabled (slots survive forever until explicit remove_contact /
        # flush_contacts). Range 1..10.
        cv.Optional(CONF_DELETE_CONTACT_MISSING_FROM): cv.Schema(
            {
                cv.Required(CONF_UPDATES_NUMBER): cv.int_range(min=1, max=10),
            }
        ),
        # Additional TCP socket headroom for full-experience firmware where HA
        # API/logging, media HTTP, TTS/announcement HTTP and intercom can overlap.
        # Validation-only: no runtime code is generated by this option.
        cv.Optional(CONF_NETWORK_SOCKET_HEADROOM, default=0): cv.int_range(
            min=0, max=32
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _consume_intercom_sockets(config):
    """Reserve lwIP sockets for intercom_api at validation time.

    TCP mode: one TCP listening socket + up to three concurrent TCP client
    sockets (mirrors the upstream api component pattern).
    UDP mode: a single datagram socket (no listen, no per-peer connection).
    """
    from esphome.components import socket

    if config.get(CONF_PROTOCOL, PROTOCOL_TCP) == PROTOCOL_UDP:
        # Two UDP sockets: audio (listen_port) + control (control_port).
        socket.consume_sockets(2, "intercom_api", socket.SocketType.UDP)(config)
    else:
        socket.consume_sockets(3, "intercom_api")(config)
        socket.consume_sockets(1, "intercom_api", socket.SocketType.TCP_LISTEN)(config)
    extra = config.get(CONF_NETWORK_SOCKET_HEADROOM, 0)
    if extra:
        socket.consume_sockets(extra, "intercom_api_headroom")(config)
    return config


def _final_validate(config):
    """Cross-component validation + socket reservation."""
    from esphome.core import CORE
    full_config = CORE.config or {}

    protocol = config.get(CONF_PROTOCOL, PROTOCOL_TCP)
    mode = config.get(CONF_MODE)
    is_raw = mode == MODE_RAW_UDP
    mdns_discovery = _mdns_discovery_config(config)
    if is_raw and protocol != PROTOCOL_UDP:
        raise cv.Invalid(
            "intercom_api.mode: raw_udp requires protocol: udp "
            "(raw UDP consumers expect bare datagrams; TCP framing is unsupported)."
        )
    if is_raw and config.get(CONF_USE_HA_AS_FIRST_CONTACT):
        raise cv.Invalid(
            "intercom_api.use_ha_as_first_contact is not valid with mode: raw_udp "
            "(raw UDP has no PBX-lite phonebook)."
        )
    if is_raw and mdns_discovery is not None:
        raise cv.Invalid(
            "intercom_api.discovery.mdns is not valid with mode: raw_udp "
            "(raw UDP has no PBX-lite phonebook)."
        )
    mdns_config = full_config.get("mdns")
    if (mdns_discovery is not None or config.get(CONF_ANNOUNCE)) and isinstance(mdns_config, dict) and mdns_config.get("disabled"):
        raise cv.Invalid("intercom_api mDNS announce/discovery requires mdns to be enabled.")
    if protocol == PROTOCOL_UDP:
        # remote_ip used to be required at compile-time; it's now an optional
        # YAML-pinned fallback. Runtime start() refuses if neither phonebook nor
        # YAML fallback resolves a peer.
        if config[CONF_LISTEN_PORT] == config[CONF_CONTROL_PORT]:
            raise cv.Invalid(
                "intercom_api.control_port must differ from listen_port "
                "(audio and control travel on separate UDP sockets)."
            )

    if CONF_MICROPHONE in config and CONF_MICROPHONE_SOURCE in config:
        raise cv.Invalid(
            "Use only one of intercom_api.microphone or intercom_api.microphone_source."
        )

    if CONF_MICROPHONE in config:
        try:
            audio.final_validate_audio_schema(
                "intercom_api",
                audio_device=CONF_MICROPHONE,
                bits_per_sample=16,
                channels=1,
                sample_rate=16000,
                audio_device_issue=True,
            )(config)
        except AssertionError:
            _LOGGER.warning(
                "intercom_api could not validate the referenced microphone audio "
                "format because that microphone component does not publish ESPHome "
                "audio stream limits. Continuing for external/native microphone "
                "components; the runtime expects 16 kHz, 16-bit, mono PCM."
            )

    if CONF_MICROPHONE_SOURCE in config:
        microphone.final_validate_microphone_source_schema(
            "intercom_api", sample_rate=16000
        )(config[CONF_MICROPHONE_SOURCE])

    if CONF_SPEAKER in config:
        try:
            audio.final_validate_audio_schema(
                "intercom_api",
                audio_device=CONF_SPEAKER,
                bits_per_sample=16,
                channels=1,
                sample_rate=16000,
                audio_device_issue=True,
            )(config)
        except AssertionError:
            _LOGGER.warning(
                "intercom_api speaker format was not declared by the referenced "
                "speaker component, so ESPHome cannot check it at compile time. "
                "Continuing anyway; intercom_api will send 16 kHz, 16-bit, mono "
                "PCM to that speaker."
            )

    # Check if esp_audio_stack is also configured
    audio_stack_configs = full_config.get("esp_audio_stack", [])

    if audio_stack_configs:
        # Warn about DC offset double-filtering
        if config.get(CONF_DC_OFFSET_REMOVAL, False):
            for audio_stack in (audio_stack_configs if isinstance(audio_stack_configs, list) else [audio_stack_configs]):
                if isinstance(audio_stack, dict) and audio_stack.get("correct_dc_offset", False):
                    raise cv.Invalid(
                        "Both intercom_api.dc_offset_removal and esp_audio_stack.correct_dc_offset are enabled. "
                        "Double DC-block filtering causes instability. "
                        "Use correct_dc_offset on esp_audio_stack only."
                    )

    _consume_intercom_sockets(config)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def _add_core_settings(var, config, is_raw_udp: bool):
    cg.add(var.set_raw_udp_mode(is_raw_udp))

    if CONF_MICROPHONE in config:
        cg.add_define("USE_INTERCOM_API_MIC")
        mic = await cg.get_variable(config[CONF_MICROPHONE])
        cg.add(var.set_microphone(mic))

    if CONF_MICROPHONE_SOURCE in config:
        cg.add_define("USE_INTERCOM_API_MIC")
        mic_source = await microphone.microphone_source_to_code(config[CONF_MICROPHONE_SOURCE])
        cg.add(var.set_microphone_source(mic_source))

    if CONF_SPEAKER in config:
        cg.add_define("USE_INTERCOM_API_SPEAKER")
        spk = await cg.get_variable(config[CONF_SPEAKER])
        cg.add(var.set_speaker(spk))

    cg.add(var.set_dc_offset_removal(config[CONF_DC_OFFSET_REMOVAL]))
    cg.add(var.set_task_stacks_in_psram(config[CONF_TASK_STACKS_IN_PSRAM]))
    cg.add(var.set_buffers_in_psram(config[CONF_BUFFERS_IN_PSRAM]))
    cg.add(var.set_use_ha_as_first_contact(config[CONF_USE_HA_AS_FIRST_CONTACT]))
    cg.add(var.set_audio_debug(config[CONF_AUDIO_DEBUG]))
    if config[CONF_PROTOCOL] == PROTOCOL_UDP:
        cg.add_define("USE_INTERCOM_UDP_TRANSPORT")
    else:
        cg.add_define("USE_INTERCOM_TCP_TRANSPORT")
    if config[CONF_ANNOUNCE]:
        cg.add_define("USE_INTERCOM_MDNS_ANNOUNCE")
        cg.add(var.set_mdns_announce_enabled(True))


def _add_transport_settings(var, config):
    # Transport: TCP default, or UDP PBX-lite with separate audio/control ports.
    if config[CONF_PROTOCOL] == PROTOCOL_UDP:
        cg.add(var.set_protocol(TransportType.UDP))
        cg.add(var.set_remote_ip(config[CONF_REMOTE_IP]))
        cg.add(var.set_remote_port(config[CONF_REMOTE_PORT]))
        cg.add(var.set_listen_port(config[CONF_LISTEN_PORT]))
        cg.add(var.set_control_port(config[CONF_CONTROL_PORT]))
    else:
        cg.add(var.set_protocol(TransportType.TCP))
        cg.add(var.set_tcp_port(config[CONF_TCP_PORT]))

    # Routing mode: enum on the C++ side, string on the YAML side.
    routing_enum = cg.RawExpression(
        "esphome::intercom_api::IntercomApi::IntercomRoutingMode::"
        + ("HA_PBX" if config[CONF_ROUTING_MODE] == ROUTING_HA_PBX else "DEVICE_INDEPENDENT")
    )
    cg.add(var.set_routing_mode(routing_enum))


def _add_mdns_discovery_settings(var, config):
    mdns_discovery = _mdns_discovery_config(config)
    if mdns_discovery is not None:
        cg.add_define("USE_INTERCOM_MDNS_DISCOVERY")
        _LOGGER.warning(
            "intercom_api discovery.mdns is intended for ESP-only installs without "
            "Home Assistant phonebook management. In HA-managed installs, use "
            "packages/intercom/phonebook_subscribe.yaml instead; running ESP-side "
            "mDNS discovery together with HA API/intercom sockets adds lwIP/socket "
            "load and can make devices less resilient during HA disconnects."
        )
        protocols = set(mdns_discovery[CONF_PROTOCOLS])
        cg.add(var.set_mdns_discovery_enabled(True))
        cg.add(var.set_mdns_discovery_scan_tcp(PROTOCOL_TCP in protocols))
        cg.add(var.set_mdns_discovery_scan_udp(PROTOCOL_UDP in protocols))
        cg.add(var.set_mdns_discovery_startup_scan(mdns_discovery[CONF_STARTUP_SCAN]))
        cg.add(
            var.set_mdns_discovery_interval_ms(
                mdns_discovery[CONF_INTERVAL].total_milliseconds
            )
        )
        cg.add(
            var.set_mdns_discovery_query_timeout_ms(
                mdns_discovery[CONF_QUERY_TIMEOUT].total_milliseconds
            )
        )
        cg.add(var.set_mdns_discovery_max_results(mdns_discovery[CONF_MAX_RESULTS]))


async def _add_device_and_audio_processor_settings(var, config):
    # Set device name (used to exclude self from the contacts list)
    from esphome.core import CORE
    cg.add(var.set_device_name(CORE.friendly_name or CORE.name))
    # PBX-lite routing key (yaml node name slug, e.g. "spotpear-ball-v2"). Used
    # as caller_route_id in MSG_START payloads and matches the slug HA uses
    # for the esphome.{slug}_start_call action registration.
    cg.add(var.set_device_route_id(CORE.name))

    # Ringing timeout (auto-decline if not answered)
    if CONF_RINGING_TIMEOUT in config:
        cg.add(var.set_ringing_timeout(config[CONF_RINGING_TIMEOUT]))

    # Calling timeout: caller in OUTGOING with no peer ANSWER/DECLINE.
    # Auto-fires DECLINE("timeout") when expired.
    if CONF_CALLING_TIMEOUT in config:
        cg.add(var.set_calling_timeout(config[CONF_CALLING_TIMEOUT]))

    # Optional contact pruning configuration. Absent = pruning disabled (the
    # default kept in C++ as prune_threshold_=0).
    if CONF_DELETE_CONTACT_MISSING_FROM in config:
        prune = config[CONF_DELETE_CONTACT_MISSING_FROM]
        cg.add(var.set_prune_threshold(prune[CONF_UPDATES_NUMBER]))


async def _build_intercom_automations(var, config):
    # on_ringing automation
    if CONF_ON_RINGING in config:
        await automation.build_automation(
            var.get_ringing_trigger(), [], config[CONF_ON_RINGING]
        )

    # on_streaming automation
    if CONF_ON_STREAMING in config:
        await automation.build_automation(
            var.get_streaming_trigger(), [], config[CONF_ON_STREAMING]
        )

    # on_idle automation
    if CONF_ON_IDLE in config:
        await automation.build_automation(
            var.get_idle_trigger(), [], config[CONF_ON_IDLE]
        )

    # FSM triggers.
    if CONF_ON_OUTGOING_CALL in config:
        await automation.build_automation(
            var.get_outgoing_call_trigger(), [], config[CONF_ON_OUTGOING_CALL]
        )

    if CONF_ON_DEST_RINGING in config:
        await automation.build_automation(
            var.get_dest_ringing_trigger(), [], config[CONF_ON_DEST_RINGING]
        )

    if CONF_ON_HANGUP in config:
        await automation.build_automation(
            var.get_hangup_trigger(), [(cg.std_string, "reason")], config[CONF_ON_HANGUP]
        )

    if CONF_ON_CALL_FAILED in config:
        await automation.build_automation(
            var.get_call_failed_trigger(), [(cg.std_string, "reason")], config[CONF_ON_CALL_FAILED]
        )

    if CONF_ON_DESTINATION_CHANGED in config:
        await automation.build_automation(
            var.get_destination_changed_trigger(), [], config[CONF_ON_DESTINATION_CHANGED]
        )

    if CONF_ON_UPDATE_CONTACTS in config:
        await automation.build_automation(
            var.get_update_contacts_trigger(), [], config[CONF_ON_UPDATE_CONTACTS]
        )


async def _new_intercom_text_sensor(config, suffix: str, name: str, icon: str):
    sensor_id = cv.declare_id(text_sensor.TextSensor)(f"{config[CONF_ID].id}_{suffix}")
    return await text_sensor.new_text_sensor(
        {
            CONF_ID: sensor_id,
            CONF_NAME: name,
            CONF_ICON: icon,
            CONF_DISABLED_BY_DEFAULT: False,
        }
    )


async def _build_intercom_text_sensors(var, config, is_raw_udp: bool):
    # === Auto-create sensors ===

    # State sensor: always created.
    state_sensor = await _new_intercom_text_sensor(
        config, "state", "Intercom State", "mdi:phone-settings"
    )
    cg.add(var.set_state_sensor(state_sensor))

    # Transport sensor: diagnostic, exposes the active transport ("udp" or
    # "tcp") so the HA-side intercom_native integration can route calls from a
    # self-describing `intercom_api:` declaration.
    transport_sensor = await _new_intercom_text_sensor(
        config, "transport", "Intercom Transport", "mdi:swap-horizontal"
    )
    cg.add(var.set_transport_sensor(transport_sensor))

    # Endpoint sensor: authoritative HA/ESP routing identity. HA consumes this
    # instead of inferring IP/ports from registry data.
    endpoint_sensor = await _new_intercom_text_sensor(
        config, "endpoint", "Intercom Endpoint", "mdi:lan-connect"
    )
    cg.add(var.set_endpoint_sensor(endpoint_sensor))

    # Last terminal reason: required by intercom_native/card mirror mode.
    # ESP-to-ESP direct calls do not pass through HA as a signaling bridge;
    # the source ESP must therefore publish the terminal reason as state so
    # HA can render the real local/remote/decline/timeout outcome.
    last_reason_sensor = await _new_intercom_text_sensor(
        config, "last_reason", "Intercom Last Reason", "mdi:phone-alert"
    )
    cg.add(var.set_last_reason_sensor(last_reason_sensor))

    # PBX-lite is the single product mode now: phonebook / destination /
    # caller entities are always exposed. Empty phonebook is normal at boot
    # (HA sensor subscription or YAML script populates it). raw_udp skips them: it has
    # no signaling and no phonebook concept.
    if not is_raw_udp:
        dest_sensor = await _new_intercom_text_sensor(
            config, "dest", "Destination", "mdi:phone-forward"
        )
        cg.add(var.set_destination_sensor(dest_sensor))

        caller_sensor = await _new_intercom_text_sensor(
            config, "caller", "Caller", "mdi:phone-incoming"
        )
        cg.add(var.set_caller_sensor(caller_sensor))

        contacts_sensor = await _new_intercom_text_sensor(
            config, "contacts", "Contacts", "mdi:account-group"
        )
        cg.add(var.set_contacts_sensor(contacts_sensor))

        # HA-side phonebook subscription is wired via the optional
        # `ha_phonebook_text_sensor_id` config option (see schema). Codegen
        # cannot reliably auto-create a `homeassistant` text_sensor here
        # because its build-time include chain depends on the platform being
        # discovered through the regular `text_sensor:` block. The shipped
        # device YAMLs declare the subscription in
        # packages/intercom/phonebook_subscribe.yaml and pass its id back.
        if CONF_HA_PHONEBOOK_TEXT_SENSOR_ID in config:
            ha_sensor = await cg.get_variable(
                config[CONF_HA_PHONEBOOK_TEXT_SENSOR_ID]
            )
            cg.add(var.set_ha_phonebook_sensor(ha_sensor))


async def _build_intercom_auto_entities(var, config):
    # Optional auto-entities: gated to opt-in (auto_entities: true) so yamls
    # that already declare `switch:`/`number: - platform: intercom_api`
    # don't end up with two competing entity registrations. New minimal
    # yamls can flip this on and skip the boilerplate altogether.
    if config[CONF_AUTO_ENTITIES]:
        from esphome.components import switch as switch_module, number as number_module
        from esphome.const import CONF_RESTORE_MODE, CONF_ENTITY_CATEGORY

        aa_id = cv.declare_id(IntercomApiAutoAnswerCls)(f"{config[CONF_ID].id}_auto_answer")
        aa = await switch_module.new_switch({
            CONF_ID: aa_id,
            CONF_NAME: "Auto Answer",
            CONF_ICON: "mdi:phone-in-talk",
            CONF_DISABLED_BY_DEFAULT: False,
            CONF_RESTORE_MODE: switch_module.RESTORE_MODES["RESTORE_DEFAULT_ON"],
            CONF_ENTITY_CATEGORY: "config",
        })
        cg.add(aa.set_parent(var))
        cg.add(var.register_auto_answer_switch(aa))

        dnd_id = cv.declare_id(IntercomApiDndSwitchCls)(f"{config[CONF_ID].id}_dnd")
        dnd = await switch_module.new_switch({
            CONF_ID: dnd_id,
            CONF_NAME: "Do Not Disturb",
            CONF_ICON: "mdi:minus-circle",
            CONF_DISABLED_BY_DEFAULT: False,
            CONF_RESTORE_MODE: switch_module.RESTORE_MODES["RESTORE_DEFAULT_OFF"],
            CONF_ENTITY_CATEGORY: "config",
        })
        cg.add(dnd.set_parent(var))
        cg.add(var.register_dnd_switch(dnd))

        if CONF_SPEAKER in config:
            vol_id = cv.declare_id(IntercomApiVolumeCls)(f"{config[CONF_ID].id}_volume")
            vol = await number_module.new_number(
                {
                    CONF_ID: vol_id,
                    CONF_NAME: "Master Volume",
                    CONF_ICON: "mdi:volume-high",
                    CONF_DISABLED_BY_DEFAULT: False,
                    CONF_MODE: number_module.NUMBER_MODES["SLIDER"],
                },
                min_value=0,
                max_value=100,
                step=5,
            )
            cg.add(vol.set_parent(var))
            cg.add(var.register_volume_number(vol))

        if CONF_MICROPHONE in config or CONF_MICROPHONE_SOURCE in config:
            mg_id = cv.declare_id(IntercomApiMicGainCls)(f"{config[CONF_ID].id}_mic_gain")
            mg = await number_module.new_number(
                {
                    CONF_ID: mg_id,
                    CONF_NAME: "Mic Gain",
                    CONF_ICON: "mdi:microphone",
                    CONF_DISABLED_BY_DEFAULT: False,
                    CONF_MODE: number_module.NUMBER_MODES["SLIDER"],
                },
                min_value=-20,
                max_value=20,
                step=1,
            )
            cg.add(mg.set_parent(var))
            cg.add(var.register_mic_gain_number(mg))

        # Runtime routing-mode toggle. RESTORE_DEFAULT_OFF means a fresh
        # device starts in device_independent unless the user overrode it
        # via the YAML option (the parent will sync the published state
        # at boot using routing_mode_ as the source of truth for the
        # initial publish when restore is disabled).
        rm_id = cv.declare_id(IntercomRoutingModeSwitchCls)(
            f"{config[CONF_ID].id}_ha_pbx_mode"
        )
        rm_sw = await switch_module.new_switch({
            CONF_ID: rm_id,
            CONF_NAME: "HA PBX Mode",
            CONF_ICON: "mdi:phone-forward",
            CONF_DISABLED_BY_DEFAULT: False,
            CONF_RESTORE_MODE: switch_module.RESTORE_MODES["RESTORE_DEFAULT_OFF"],
            CONF_ENTITY_CATEGORY: "config",
        })
        cg.add(rm_sw.set_parent(var))
        cg.add(var.register_routing_mode_switch(rm_sw))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    is_raw_udp = config.get(CONF_MODE) == MODE_RAW_UDP
    await _add_core_settings(var, config, is_raw_udp)
    _add_transport_settings(var, config)
    _add_mdns_discovery_settings(var, config)
    await _add_device_and_audio_processor_settings(var, config)
    await _build_intercom_automations(var, config)
    await _build_intercom_text_sensors(var, config, is_raw_udp)
    await _build_intercom_auto_entities(var, config)


# === Action registrations ===
# Simple action schema that just references the intercom_api component
INTERCOM_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(IntercomApi),
    }
)


async def _new_parented_action(config, action_id, template_arg):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    return var


def _register_simple_action(name, action_class):
    """Register a parameter-less Parented<IntercomApi> action.

    The codegen all the simple actions need is identical:
        var = new_Pvariable; parent = use_id(intercom_api); var.set_parent(parent)
    Bundle it in one helper so adding a new simple action is one line below
    instead of an eight-line decorator + coroutine block.
    """
    @automation.register_action(name, action_class, INTERCOM_ACTION_SCHEMA, synchronous=True)
    async def _to_code(config, action_id, template_arg, args):
        return await _new_parented_action(config, action_id, template_arg)
    return _to_code


_register_simple_action("intercom_api.next_contact", NextContactAction)
_register_simple_action("intercom_api.prev_contact", PrevContactAction)
_register_simple_action("intercom_api.start", StartAction)
_register_simple_action("intercom_api.stop", StopAction)
_register_simple_action("intercom_api.answer_call", AnswerCallAction)


@automation.register_action(
    "intercom_api.decline_call",
    DeclineCallAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(IntercomApi),
            cv.Optional(CONF_REASON): cv.templatable(cv.string),
        }
    ),
    synchronous=True,
)
async def decline_call_action_to_code(config, action_id, template_arg, args):
    var = await _new_parented_action(config, action_id, template_arg)
    if CONF_REASON in config:
        templ = await cg.templatable(config[CONF_REASON], args, cg.std_string)
        cg.add(var.set_reason(templ))
    return var


_register_simple_action("intercom_api.call_toggle", CallToggleAction)
_register_simple_action("intercom_api.publish_entity_states", PublishEntityStatesAction)


# === Parameterized actions ===

CONF_VOLUME = "volume"
CONF_GAIN_DB = "gain_db"
CONF_CONTACTS_CSV = "contacts_csv"


def _register_templated_action(name, action_class, key, validator, cpp_type, setter):
    @automation.register_action(
        name,
        action_class,
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(IntercomApi),
                cv.Required(key): cv.templatable(validator),
            }
        ),
        synchronous=True,
    )
    async def _to_code(config, action_id, template_arg, args):
        var = await _new_parented_action(config, action_id, template_arg)
        templ = await cg.templatable(config[key], args, cpp_type)
        cg.add(setter(var, templ))
        return var
    return _to_code


_register_templated_action(
    "intercom_api.set_volume",
    SetVolumeAction,
    CONF_VOLUME,
    cv.float_range(min=0.0, max=1.0),
    float,
    lambda var, value: var.set_volume(value),
)
_register_templated_action(
    "intercom_api.set_mic_gain_db",
    SetMicGainDbAction,
    CONF_GAIN_DB,
    cv.float_range(min=-20.0, max=20.0),
    float,
    lambda var, value: var.set_gain_db(value),
)
_register_templated_action(
    "intercom_api.set_contacts",
    SetContactsAction,
    CONF_CONTACTS_CSV,
    cv.string,
    cg.std_string,
    lambda var, value: var.set_contacts_csv(value),
)


CONF_CONTACT = "contact"


_register_templated_action(
    "intercom_api.set_contact",
    SetContactAction,
    CONF_CONTACT,
    cv.string,
    cg.std_string,
    lambda var, value: var.set_contact(value),
)
_register_templated_action(
    "intercom_api.call_contact",
    CallContactAction,
    CONF_CONTACT,
    cv.string,
    cg.std_string,
    lambda var, value: var.set_contact(value),
)


CONF_IP = "ip"
CONF_PORT = "port"


@automation.register_action(
    "intercom_api.set_remote_endpoint",
    SetRemoteEndpointAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(IntercomApi),
            cv.Required(CONF_IP): cv.templatable(cv.string),
            cv.Optional(CONF_PORT, default=6054): cv.templatable(cv.port),
            cv.Optional(CONF_CONTROL_PORT, default=0): cv.templatable(cv.int_range(min=0, max=65535)),
        }
    ),
    synchronous=True,
)
async def set_remote_endpoint_action_to_code(config, action_id, template_arg, args):
    var = await _new_parented_action(config, action_id, template_arg)
    templ_ip = await cg.templatable(config[CONF_IP], args, cg.std_string)
    cg.add(var.set_ip(templ_ip))
    templ_port = await cg.templatable(config[CONF_PORT], args, cg.uint16)
    cg.add(var.set_port(templ_port))
    templ_control = await cg.templatable(config[CONF_CONTROL_PORT], args, cg.uint16)
    cg.add(var.set_control_port(templ_control))
    return var


CONF_ENTRY = "entry"
CONF_CALLER = "caller"


_register_templated_action(
    "intercom_api.add_contact",
    AddContactAction,
    CONF_ENTRY,
    cv.string,
    cg.std_string,
    lambda var, value: var.set_entry(value),
)
_register_templated_action(
    "intercom_api.remove_contact",
    RemoveContactAction,
    CONF_ENTRY,
    cv.string,
    cg.std_string,
    lambda var, value: var.set_entry(value),
)
_register_templated_action(
    "intercom_api.set_ha_peer_name",
    SetHaPeerNameAction,
    CONF_NAME,
    cv.string,
    cg.std_string,
    lambda var, value: var.set_name(value),
)


_register_simple_action("intercom_api.flush_contacts", FlushContactsAction)
_register_simple_action("intercom_api.update_contacts", UpdateContactsAction)


@automation.register_action(
    "intercom_api.simulate_incoming_call",
    SimulateIncomingCallAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(IntercomApi),
            cv.Required(CONF_CALLER): cv.templatable(cv.string),
        }
    ),
    synchronous=True,
)
async def simulate_incoming_call_action_to_code(config, action_id, template_arg, args):
    var = await _new_parented_action(config, action_id, template_arg)
    templ = await cg.templatable(config[CONF_CALLER], args, cg.std_string)
    cg.add(var.set_caller(templ))
    return var


# === Condition registrations ===
# Simple condition schema that just references the intercom_api component
INTERCOM_CONDITION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(IntercomApi),
    }
)


def _register_simple_condition(name, condition_class):
    """Register a parameter-less Parented<IntercomApi> condition."""
    @automation.register_condition(name, condition_class, INTERCOM_CONDITION_SCHEMA)
    async def _to_code(config, condition_id, template_arg, args):
        var = cg.new_Pvariable(condition_id, template_arg)
        parent = await cg.get_variable(config[CONF_ID])
        cg.add(var.set_parent(parent))
        return var
    return _to_code


_register_simple_condition("intercom_api.is_idle", IntercomIsIdleCondition)
_register_simple_condition("intercom_api.is_ringing", IntercomIsRingingCondition)
_register_simple_condition("intercom_api.is_streaming", IntercomIsStreamingCondition)
_register_simple_condition("intercom_api.is_calling", IntercomIsCallingCondition)
_register_simple_condition("intercom_api.is_incoming", IntercomIsIncomingCondition)
_register_simple_condition("intercom_api.is_in_call", IntercomIsInCallCondition)


CONF_DESTINATION = "destination"

@automation.register_condition(
    "intercom_api.destination_is",
    IntercomDestinationIsCondition,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(IntercomApi),
            cv.Required(CONF_DESTINATION): cv.templatable(cv.string),
        }
    ),
)
async def intercom_destination_is_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    templ = await cg.templatable(config[CONF_DESTINATION], args, cg.std_string)
    cg.add(var.set_destination(templ))
    return var


_register_simple_condition("intercom_api.is_ha_destination", IntercomIsHaDestinationCondition)
