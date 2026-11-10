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
sends PlayNow and waits until the TV plays the item. The answer has ok, and either the
play method, PlaySessionId and stream URL (token hidden) or an error, so a script can
fall back to another way of casting.

Added 2026-11-09 for Maxi: the Default Media Receiver casts sometimes had the sound out of
sync.
"""

from __future__ import annotations

import logging
from typing import Any

import voluptuous as vol

from homeassistant.const import ATTR_ENTITY_ID, ATTR_FRIENDLY_NAME, CONF_TOKEN, CONF_URL
from homeassistant.core import HomeAssistant, ServiceCall, ServiceResponse, SupportsResponse
from homeassistant.exceptions import ServiceValidationError
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers.entity_component import DATA_INSTANCES
from homeassistant.helpers.typing import ConfigType

from .receiver import DEFAULT_APP_ID, JellyfinReceiver, play_now_message, plain_id

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

PLAY_ITEM_SCHEMA = vol.Schema(
    {
        vol.Required(ATTR_ENTITY_ID): vol.All(cv.entity_ids, vol.Length(min=1, max=1)),
        vol.Required("item_id"): cv.string,
        vol.Optional("item_type", default="Video"): cv.string,
        vol.Optional("audio_stream_index"): vol.Coerce(int),
        vol.Optional("subtitle_stream_index"): vol.Coerce(int),
        vol.Optional("start_position_ticks"): vol.All(vol.Coerce(int), vol.Range(min=0)),
        vol.Optional("max_bitrate"): vol.All(vol.Coerce(int), vol.Range(min=100000)),
        vol.Optional("receiver_name"): cv.string,
        vol.Optional("timeout", default=45): vol.All(vol.Coerce(float), vol.Range(min=5, max=180)),
    }
)


async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    """Register jellyfin_cast.play_item."""
    conf = config.get(DOMAIN)
    if conf is None:
        return True
    receivers: dict[str, JellyfinReceiver] = {}

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

    async def play_item(call: ServiceCall) -> ServiceResponse:
        entity_id = call.data[ATTR_ENTITY_ID][0]
        item_id = plain_id(call.data["item_id"])
        receiver = receiver_for(entity_id)
        answer: dict[str, Any] = {"entity_id": entity_id, "item_id": item_id}
        if receiver is None:
            answer |= {"ok": False, "error": f"{entity_id} is not connected"}
        else:
            state = hass.states.get(entity_id)
            name = call.data.get("receiver_name") or (
                f"{state.attributes.get(ATTR_FRIENDLY_NAME, entity_id) if state else entity_id} Jellyfin"
            )
            message = play_now_message(
                server_url=conf[CONF_URL],
                user_id=conf[CONF_USER_ID],
                token=conf[CONF_TOKEN],
                receiver_name=name,
                item_id=item_id,
                item_type=call.data["item_type"],
                audio_stream_index=call.data.get("audio_stream_index"),
                subtitle_stream_index=call.data.get("subtitle_stream_index"),
                start_position_ticks=call.data.get("start_position_ticks"),
                max_bitrate=call.data.get("max_bitrate"),
            )
            answer |= {"receiver_name": name}
            answer |= await hass.async_add_executor_job(
                receiver.play, message, item_id, call.data["timeout"]
            )
        if answer.get("ok"):
            _LOGGER.debug("Jellyfin receiver plays %s on %s: %s", item_id, entity_id, answer)
        else:
            _LOGGER.warning("Jellyfin receiver could not play %s on %s: %s", item_id, entity_id, answer.get("error"))
        return answer if call.return_response else None

    hass.services.async_register(
        DOMAIN,
        "play_item",
        play_item,
        schema=PLAY_ITEM_SCHEMA,
        supports_response=SupportsResponse.OPTIONAL,
    )
    return True
