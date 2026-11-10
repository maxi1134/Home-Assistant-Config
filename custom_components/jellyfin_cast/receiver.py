"""Sender side of the official Jellyfin cast receiver, on an existing Chromecast connection.

Only pychromecast here (no Home Assistant imports), so it can be tried against a real
device from any Python with pychromecast installed.
"""

from __future__ import annotations

import logging
import re
import threading
import time
from typing import Any

from pychromecast import Chromecast
from pychromecast.controllers import BaseController
from pychromecast.controllers.media import MediaStatus, MediaStatusListener

_LOGGER = logging.getLogger(__name__)

NAMESPACE = "urn:x-cast:com.connectsdk"
DEFAULT_APP_ID = "F007D354"  # "Jellyfin for Google Cast", the server's Stable receiver
PLAYING = ("PLAYING", "PAUSED")
TOKEN_IN_URL = re.compile(r"((?:api_key|ApiKey)=)[^&]+", re.IGNORECASE)


def plain_id(item_id: str) -> str:
    """Jellyfin ids appear with and without dashes; compare them without."""
    return item_id.replace("-", "").lower()


def is_item(content_id: str | None, item_id: str) -> bool:
    """True when a stream URL is for this item (/videos/<id>/..., dashed or not)."""
    return f"/{plain_id(item_id)}/" in (content_id or "").replace("-", "").lower()


def hide_token(url: str | None) -> str:
    """The receiver's stream URLs carry the access token; keep it out of answers."""
    return TOKEN_IN_URL.sub(r"\1…", url or "")


class _MediaListener(MediaStatusListener):
    """Wakes the waiting play() on every media status of the Chromecast."""

    def __init__(self, receiver: JellyfinReceiver) -> None:
        self._receiver = receiver

    def new_media_status(self, status: MediaStatus) -> None:
        self._receiver.wake()

    def load_media_failed(self, queue_item_id: int, error_code: int) -> None:
        self._receiver.wake()


class JellyfinReceiver(BaseController):
    """Talks to the Jellyfin receiver on one Chromecast, the way the Jellyfin apps do.

    send_message() launches the receiver first when another app is in front
    (app_must_match: other apps use the same Connect SDK namespace).
    """

    def __init__(self, chromecast: Chromecast, app_id: str = DEFAULT_APP_ID) -> None:
        super().__init__(NAMESPACE, app_id, app_must_match=True)
        self.chromecast = chromecast
        self._cond = threading.Condition()
        self._error: str | None = None
        chromecast.register_handler(self)
        chromecast.media_controller.register_status_listener(_MediaListener(self))

    def wake(self) -> None:
        with self._cond:
            self._cond.notify_all()

    def receive_message(self, _message: Any, data: dict) -> bool:
        """The receiver broadcasts its events (playbackstart, playbackerror, ...) to senders."""
        kind = data.get("type")
        if kind in ("error", "playbackerror", "connectionerror"):
            _LOGGER.debug("Jellyfin receiver on %s: %s %s", self.chromecast.uri, kind, data.get("message"))
            with self._cond:
                self._error = str(data.get("message") or kind)
                self._cond.notify_all()
        return True

    def play(self, message: dict, item_id: str, timeout: float) -> dict[str, Any]:
        """Send a PlayNow and wait until the TV plays that item, or fails.

        Blocking: run it in an executor. Success needs a new media session (or a new
        stream) for the item in PLAYING or PAUSED, so a failed load that leaves the
        previous stream on screen does not count.
        """
        started = time.monotonic()
        before = self.chromecast.media_controller.status
        before_session = before.media_session_id if before and before.content_id else None
        before_content = before.content_id if before else None
        with self._cond:
            self._error = None
        sent = threading.Event()
        outcome: dict[str, Any] = {}

        def on_sent(ok: bool, _response: Any) -> None:
            outcome["sent"] = ok
            sent.set()

        try:
            self.send_message(message, callback_function=on_sent, no_add_request_id=True)
        except Exception as err:  # noqa: BLE001 - NotConnected, ControllerNotRegistered, ...
            return {"ok": False, "error": f"Could not reach the receiver: {err!r}"}
        if not sent.wait(min(20.0, timeout)) or not outcome.get("sent"):
            return {"ok": False, "error": "The Jellyfin receiver did not start on the TV"}

        deadline = started + timeout
        with self._cond:
            while True:
                status = self.chromecast.media_controller.status
                ours = status is not None and is_item(status.content_id, item_id)
                new = ours and (status.media_session_id != before_session or status.content_id != before_content)
                if new and status.player_state in PLAYING:
                    custom = status.media_custom_data or {}
                    return {
                        "ok": True,
                        "state": status.player_state.lower(),
                        "play_method": custom.get("playMethod"),
                        "play_session": custom.get("playSessionId"),
                        "content_id": hide_token(status.content_id),
                        "seconds": round(time.monotonic() - started, 1),
                    }
                if new and status.player_state == "IDLE" and status.idle_reason == "ERROR":
                    return {"ok": False, "error": "The TV could not play the stream the receiver chose",
                            "content_id": hide_token(status.content_id)}
                if self._error:
                    return {"ok": False, "error": f"Jellyfin receiver: {self._error}"}
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return {"ok": False, "error": f"Not playing after {timeout:g} s",
                            "state": status.player_state.lower() if status else None,
                            "content_id": hide_token(status.content_id) if ours else None}
                self._cond.wait(min(remaining, 1.0))


def play_now_message(
    *,
    server_url: str,
    user_id: str,
    token: str,
    receiver_name: str,
    item_id: str,
    item_type: str = "Video",
    audio_stream_index: int | None = None,
    subtitle_stream_index: int | None = None,
    start_position_ticks: int | None = None,
    max_bitrate: int | None = None,
    queue: list[str] | None = None,
) -> dict[str, Any]:
    """The PlayNow message jellyfin-web's Chromecast player sends (plugins/chromecastPlayer).

    The receiver reads the server, token and receiverName; it derives the user from the
    token. receiverName becomes the Jellyfin session's device name (and its device id),
    so keep it stable per TV. queue: item ids (of item_type) to play one after the other
    instead of item_id alone; the receiver plays a list of several as it is, and only
    loads each item when its turn comes.
    """
    options: dict[str, Any] = {
        "items": [{
            "Id": plain_id(one),
            "Type": item_type,
            "MediaType": "Audio" if item_type == "Audio" else "Video",
            "IsFolder": False,
        } for one in (queue or [item_id])],
    }
    if start_position_ticks:
        options["startPositionTicks"] = start_position_ticks
    if audio_stream_index is not None:
        options["audioStreamIndex"] = audio_stream_index
    if subtitle_stream_index is not None:
        options["subtitleStreamIndex"] = subtitle_stream_index
    message: dict[str, Any] = {
        "options": options,
        "command": "PlayNow",
        "userId": user_id,
        "deviceId": "home-assistant-jellyfin-cast",
        "accessToken": token,
        "serverAddress": server_url.rstrip("/"),
        "receiverName": receiver_name,
        "subtitleBurnIn": "",
    }
    if max_bitrate:
        message["maxBitrate"] = max_bitrate
    return message
