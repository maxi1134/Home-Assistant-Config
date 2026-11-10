"""Jellyfin Cast: play Jellyfin items on Google Cast TVs through the official Jellyfin receiver.

Home Assistant's cast integration can launch any receiver app, but the Jellyfin receiver
("Jellyfin for Google Cast", app F007D354) only plays what a sender tells it on its own
channel: the server, an access token and the item, as the Jellyfin apps send them when
you cast. This integration is that sender. Like DashCast, it reuses the cast entity's own
pychromecast connection, so nothing else connects to the TV.

From there it is a cast from the Jellyfin apps: the receiver asks the server for a stream
that fits the device (HLS in fMP4 segments played by Shaka, or the file as it is when the
TV can take it) and reports playback to Jellyfin under the token's user.

configuration.yaml:

    jellyfin_cast:
      url: http://192.168.0.20:8096
      user_id: <Jellyfin id of the token's user>
      token: !secret jellyfin_cast_token

Action jellyfin_cast.play_item (response optional) launches the receiver when needed,
sends PlayNow and waits until the TV plays the item. next_track / previous_track move through
the receiver's own queue (the Cast integration offers no next/previous for this app). The item is an item_id, or a name
that the server looks up as the token's user (a show gives its Next Up or a random
episode; see lookup.py). The answer has ok, what was found, and either the play method,
PlaySessionId and stream URL (token hidden) or an error, so a script can fall back to
another way of casting.

Added 2026-11-09 for Maxi: the Default Media Receiver casts sometimes had the sound out of
sync.
"""

from __future__ import annotations

import copy
import logging
from pathlib import Path
from typing import Any

import aiohttp
import voluptuous as vol

from homeassistant.const import ATTR_ENTITY_ID, ATTR_FRIENDLY_NAME, CONF_TOKEN, CONF_URL
from homeassistant.core import Event, HomeAssistant, ServiceCall, ServiceResponse, SupportsResponse, callback
from homeassistant.exceptions import ServiceValidationError
from homeassistant.helpers import config_validation as cv, entity_registry as er
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.selector import validate_selector
from homeassistant.helpers.service import async_set_service_schema
from homeassistant.helpers.entity_component import DATA_INSTANCES
from homeassistant.helpers.typing import ConfigType
from homeassistant.util.yaml import load_yaml_dict

from .lookup import EPISODE_CHOICES, SEARCH_TYPES, ItemNotFound, JellyfinLookup, describe
from .receiver import DEFAULT_APP_ID, JellyfinReceiver, command_message, play_now_message, plain_id

_LOGGER = logging.getLogger(__name__)

DOMAIN = "jellyfin_cast"
CONF_USER_ID = "user_id"
CONF_APP_ID = "app_id"

CONFIG_SCHEMA = vol.Schema(
    {
        DOMAIN: vol.Schema(
            {
                vol.Required(CONF_URL): cv.url,
                vol.Required(CONF_USER_ID): cv.string,
                vol.Required(CONF_TOKEN): cv.string,
                vol.Optional(CONF_APP_ID, default=DEFAULT_APP_ID): cv.string,
            }
        )
    },
    extra=vol.ALLOW_EXTRA,
)

PLAY_ITEM_SCHEMA = vol.All(
    vol.Schema(
        {
            vol.Required(ATTR_ENTITY_ID): vol.All(cv.entity_ids, vol.Length(min=1, max=1)),
            # The item: its Jellyfin id, or a name for the server to look up.
            vol.Exclusive("item_id", "item"): cv.string,
            vol.Exclusive("name", "item"): cv.string,
            vol.Optional("item_type"): cv.string,
            vol.Optional("episode", default="next_up"): vol.In(EPISODE_CHOICES),
            vol.Optional("library"): cv.string,
            vol.Optional("shuffle", default=False): cv.boolean,
            vol.Optional("audio_stream_index"): vol.Coerce(int),
            vol.Optional("subtitle_stream_index"): vol.Coerce(int),
            vol.Optional("start_position_ticks"): vol.All(vol.Coerce(int), vol.Range(min=0)),
            vol.Optional("max_bitrate"): vol.All(vol.Coerce(int), vol.Range(min=100000)),
            vol.Optional("receiver_name"): cv.string,
            vol.Optional("timeout", default=45): vol.All(vol.Coerce(float), vol.Range(min=5, max=180)),
        }
    ),
    cv.has_at_least_one_key("item_id", "name"),
)

SKIP_SCHEMA = vol.Schema(
    {
        vol.Required(ATTR_ENTITY_ID): vol.All(cv.entity_ids, vol.Length(min=1, max=1)),
        vol.Optional("timeout", default=15): vol.All(vol.Coerce(float), vol.Range(min=2, max=60)),
    }
)
SKIPS = {"next_track": "NextTrack", "previous_track": "PreviousTrack"}


async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    """Register jellyfin_cast.play_item, next_track and previous_track."""
    conf = config.get(DOMAIN)
    if conf is None:
        return True
    receivers: dict[str, JellyfinReceiver] = {}
    # The receiver name each TV was last cast with: commands must send the same one, since the
    # receiver takes it as the Jellyfin session's device name on every message.
    receiver_names: dict[str, str] = {}

    def default_name(entity_id: str) -> str:
        state = hass.states.get(entity_id)
        return f"{state.attributes.get(ATTR_FRIENDLY_NAME, entity_id) if state else entity_id} Jellyfin"

    def receiver_for(entity_id: str) -> JellyfinReceiver | None:
        component = hass.data.get(DATA_INSTANCES, {}).get("media_player")
        entity = component.get_entity(entity_id) if component else None
        if entity is None or entity.platform is None or entity.platform.platform_name != "cast":
            raise ServiceValidationError(f"{entity_id} is not a Google Cast media player")
        chromecast = getattr(entity, "_chromecast", None)
        if chromecast is None:
            return None
        known = receivers.get(entity_id)
        if known is None or known.chromecast is not chromecast:
            # A new connection object after a reconnect gets its own controller.
            known = receivers[entity_id] = JellyfinReceiver(chromecast, conf[CONF_APP_ID])
        return known

    lookup = JellyfinLookup(async_get_clientsession(hass), conf[CONF_URL], conf[CONF_USER_ID], conf[CONF_TOKEN])

    async def resolve(data: dict[str, Any], item_type: str | None) -> tuple[dict[str, Any], list[str]]:
        """What to play: the answer's item part and the queue's item ids, or ok False and an error."""
        try:
            if "name" in data:
                queue, how = await lookup.find(data["name"], item_type, data["episode"], data.get("library"),
                                               data["shuffle"])
            else:  # an item_id, looked up only to shuffle it if it's a show
                queue, how = await lookup.by_id(data["item_id"], data["shuffle"])
        except ItemNotFound as err:
            return {"ok": False, "error": str(err), "candidates": err.candidates}, []
        except aiohttp.ClientResponseError as err:
            hint = " (the token was refused)" if err.status in (401, 403) else ""
            return {"ok": False, "error": f"Jellyfin lookup failed: HTTP {err.status}{hint}"}, []
        except (aiohttp.ClientError, TimeoutError) as err:
            return {"ok": False, "error": f"Jellyfin lookup failed: {err!r}"}, []
        return describe(queue[0]) | how, [plain_id(item["Id"]) for item in queue]

    async def play_item(call: ServiceCall) -> ServiceResponse:
        entity_id = call.data[ATTR_ENTITY_ID][0]
        by_name = "name" in call.data
        item_type = call.data.get("item_type")
        if "library" in call.data and not by_name:
            raise ServiceValidationError("library narrows a name lookup; it can't be used with item_id")
        if by_name and item_type is not None:
            # A search filter here, in any case ("movie"); only these four can be searched for.
            item_type = {kind.lower(): kind for kind in SEARCH_TYPES}.get(item_type.lower())
            if item_type is None:
                raise ServiceValidationError(
                    f"item_type {call.data['item_type']!r} can't be searched by name; "
                    f"use one of {', '.join(SEARCH_TYPES)}"
                )
        receiver = receiver_for(entity_id)
        answer: dict[str, Any] = {"entity_id": entity_id}
        answer |= {"query": call.data["name"]} if by_name else {"item_id": plain_id(call.data["item_id"])}
        queue: list[str] = []
        if receiver is None:
            answer |= {"ok": False, "error": f"{entity_id} is not connected"}
        elif by_name or call.data["shuffle"]:
            found, queue = await resolve(call.data, item_type)
            answer |= found
            if found.get("ok") is not False and (by_name or "series_id" in found or item_type is None):
                item_type = found["type"]  # what was found (a show plays as an Episode)
        if receiver is not None and answer.get("ok") is not False:
            item_id = answer["item_id"]
            name = receiver_names[entity_id] = call.data.get("receiver_name") or default_name(entity_id)
            message = play_now_message(
                server_url=conf[CONF_URL],
                user_id=conf[CONF_USER_ID],
                token=conf[CONF_TOKEN],
                receiver_name=name,
                item_id=item_id,
                item_type=item_type or "Video",
                audio_stream_index=call.data.get("audio_stream_index"),
                subtitle_stream_index=call.data.get("subtitle_stream_index"),
                start_position_ticks=call.data.get("start_position_ticks"),
                max_bitrate=call.data.get("max_bitrate"),
                queue=queue if len(queue) > 1 else None,
            )
            answer |= {"receiver_name": name}
            answer |= await hass.async_add_executor_job(
                receiver.play, message, item_id, call.data["timeout"]
            )
        what = answer.get("item_id") or answer.get("query")
        if answer.get("ok"):
            _LOGGER.debug("Jellyfin receiver plays %s on %s: %s", what, entity_id, answer)
        else:
            _LOGGER.warning("Jellyfin receiver could not play %s on %s: %s", what, entity_id, answer.get("error"))
        return answer if call.return_response else None

    hass.services.async_register(
        DOMAIN,
        "play_item",
        play_item,
        schema=PLAY_ITEM_SCHEMA,
        supports_response=SupportsResponse.OPTIONAL,
    )

    async def skip(call: ServiceCall) -> ServiceResponse:
        """next_track / previous_track: move through the receiver's own queue."""
        entity_id = call.data[ATTR_ENTITY_ID][0]
        receiver = receiver_for(entity_id)
        answer: dict[str, Any] = {"entity_id": entity_id}
        if receiver is None:
            answer |= {"ok": False, "error": f"{entity_id} is not connected"}
        else:
            message = command_message(
                command=SKIPS[call.service], server_url=conf[CONF_URL], user_id=conf[CONF_USER_ID],
                token=conf[CONF_TOKEN], receiver_name=receiver_names.get(entity_id) or default_name(entity_id),
            )
            answer |= await hass.async_add_executor_job(receiver.skip, message, call.data["timeout"])
            if answer.get("ok") and answer.get("item_id"):
                try:  # what it skipped to, for a script or a voice answer; best effort
                    found = await lookup.describe_id(answer["item_id"])
                    answer |= {key: value for key, value in found.items() if key != "item_id"}
                except Exception as err:  # noqa: BLE001 - the skip worked, the details are a bonus
                    _LOGGER.debug("No details for %s: %r", answer["item_id"], err)
        if not answer.get("ok"):
            _LOGGER.warning("Jellyfin receiver %s on %s: %s", call.service, entity_id, answer.get("error"))
        return answer if call.return_response else None

    for service in SKIPS:
        hass.services.async_register(DOMAIN, service, skip, schema=SKIP_SCHEMA,
                                     supports_response=SupportsResponse.OPTIONAL)

    # The UI's TV picker: services.yaml can only filter on the cast integration, which also
    # brings speakers, speaker groups and cast-enabled receivers (device class speaker). The
    # description is published here with just the cast players that can show video, and
    # again whenever a media player is added, removed or changed.
    def load_descriptions() -> dict[str, Any]:
        """The services from services.yaml, their selectors normalized as Home Assistant does."""
        services = load_yaml_dict(str(Path(__file__).parent / "services.yaml"))
        for service in services.values():
            for field in service["fields"].values():
                if "selector" in field:
                    field["selector"] = validate_selector(field["selector"])
        return services

    descriptions = await hass.async_add_executor_job(load_descriptions)

    @callback
    def describe_services(_event: Event | None = None) -> None:
        tvs = sorted(
            entry.entity_id
            for entry in er.async_get(hass).entities.values()
            if entry.domain == "media_player" and entry.platform == "cast" and not entry.disabled_by
            and (entry.device_class or entry.original_device_class) != "speaker"
        )
        for service, description in descriptions.items():
            schema = copy.deepcopy(description)
            if tvs:  # none yet: keep the plain cast filter rather than an empty list
                picker = schema["fields"]["entity_id"]["selector"]
                picker["entity"]["include_entities"] = tvs
                schema["fields"]["entity_id"]["selector"] = validate_selector(picker)
            async_set_service_schema(hass, DOMAIN, service, schema)

    @callback
    def media_player_changed(event_data: er.EventEntityRegistryUpdatedData) -> bool:
        return event_data["entity_id"].startswith("media_player.")

    describe_services()
    hass.bus.async_listen(er.EVENT_ENTITY_REGISTRY_UPDATED, describe_services, event_filter=media_player_changed)
    return True
