# Jellyfin Cast

A small Home Assistant integration that plays Jellyfin movies, episodes and videos on Google Cast devices through the **official Jellyfin receiver** ("Jellyfin for Google Cast"), the same way the Jellyfin apps do when you press the cast button.

## Why

Home Assistant can cast a Jellyfin stream URL to the Default Media Receiver, but then you get Google's generic player and whatever stream you asked for. On my TVs the sound sometimes drifted out of sync that way.

The official Jellyfin receiver plays much better (it picks the stream itself: the file as it is when the TV can take it, otherwise HLS in fMP4), but Home Assistant can only *launch* a receiver app. The Jellyfin receiver then sits there until a sender tells it what to play. This integration is that sender.

It reuses the cast entity's own connection (like DashCast does), so nothing else connects to the TV.

## Install

1. Copy this `jellyfin_cast` folder into `config/custom_components/`.
2. The Google Cast integration must be set up (the TVs are its `media_player` entities).
3. Add the YAML below and restart Home Assistant.

```yaml
# configuration.yaml
jellyfin_cast:
  url: http://jellyfin.local:8096        # your Jellyfin server, as the TVs can reach it
  user_id: <Jellyfin id of the token's user>
  token: !secret jellyfin_cast_token
  # app_id: F007D354                     # optional, default = the Stable Jellyfin receiver
```

### The token

It must be a **user access token**, not an API key: the receiver asks the server for `/Users/Me`, which an API key can't answer.

What I did:
- Made a dedicated Jellyfin user for Home Assistant (hidden from the login screen), with access to the libraries it should play. Its watch history stays out of your own *Continue Watching*.
- Got its token by turning Quick Connect on for a moment, signing that user in with a code, then turning Quick Connect off again. Signing in through the API (`/Users/AuthenticateByName`) works too.
- In that user's playback settings, turned **off** "Play next episode automatically". Otherwise playing one episode queues the rest of the season.
- Set its subtitle and audio language preferences. The receiver uses that user's settings.

`user_id` is that user's id: it's in the URL of the user's page in the Jellyfin dashboard.

## Action: `jellyfin_cast.play_item`

Launches the receiver if needed, sends it the item, and waits until the TV actually plays it.

| Field | Required | Notes |
|---|---|---|
| `entity_id` | yes | The Google Cast media player (the cast entity itself, not a universal/group one) |
| `item_id` | yes | Jellyfin item id (with or without dashes) |
| `item_type` | no | `Video` (default), `Episode` or `Movie` |
| `audio_stream_index` | no | Jellyfin stream index of the audio track |
| `subtitle_stream_index` | no | Jellyfin stream index of the subtitles, `-1` for none |
| `start_position_ticks` | no | Start position (10,000,000 ticks per second) |
| `max_bitrate` | no | Bitrate cap in bit/s (default: the receiver measures the network) |
| `receiver_name` | no | Name of the Jellyfin session (default: the TV's name + " Jellyfin"); keep it stable per TV |
| `timeout` | no | Seconds to wait for playback (default 45) |

It can return a response:

```yaml
action: jellyfin_cast.play_item
data:
  entity_id: media_player.living_room_tv
  item_id: 1ffa07c043d35bab9a1c090831a41d13
  item_type: Episode
response_variable: cast
```

On success `cast` holds `ok: true`, `state`, `play_method` (DirectPlay, DirectStream or Transcode), `play_session`, `content_id` (the stream URL, with the token masked) and `seconds`. On failure it holds `ok: false` and an `error` (the receiver's own error, the TV refusing the stream, or a timeout), so a script can fall back to another way of casting.

## Good to know

- Each TV shows up in Jellyfin as a *Chromecast* session named "<TV name> Jellyfin", so the Jellyfin integration may add a media player for it.
- Some older devices can't play what the receiver chooses. My 1st-gen Nest Hub failed on a 4K movie. I send those to the Default Media Receiver instead, and use the error answer for the rest.
- Restarting Home Assistant stops whatever it cast: Home Assistant is the only sender, and the receiver quits when its last sender leaves.
- `receiver.py` only needs `pychromecast`, so you can try it against a real TV outside Home Assistant.
