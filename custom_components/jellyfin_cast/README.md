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

Launches the receiver if needed, sends it the item, and waits until the TV actually plays it. The item is either a **name**, which the integration looks up on your server, or an **item_id**.

```yaml
action: jellyfin_cast.play_item
data:
  entity_id: media_player.living_room_tv
  name: Futurama
response_variable: cast
```

| Field | Required | Notes |
|---|---|---|
| `entity_id` | yes | The Google Cast media player (the cast entity itself, not a universal/group one). In the UI, the picker only lists cast players that can show video |
| `name` | one of these two | Name of a movie, show, episode or video (see *Playing by name*) |
| `item_id` | one of these two | Jellyfin item id, with or without dashes (see *Finding the item_id*) |
| `item_type` | no | With a name: only look for `Movie`, `Series` (shows), `Episode` or `Video`. With an item_id: its type, `Video` (default), `Episode` or `Movie` |
| `library` | no | With a name: only look in this library, e.g. `Movies` or `TV Shows` |
| `episode` | no | For a show found by name: `next_up` (default) or `random` |
| `shuffle` | no | For a show: play its episodes in random order until stopped (see *Shuffling a show*) |
| `audio_stream_index` | no | Jellyfin stream index of the audio track |
| `subtitle_stream_index` | no | Jellyfin stream index of the subtitles, `-1` for none |
| `start_position_ticks` | no | Start position (10,000,000 ticks per second) |
| `max_bitrate` | no | Bitrate cap in bit/s (default: the receiver measures the network) |
| `receiver_name` | no | Name of the Jellyfin session (default: the TV's name + " Jellyfin"); keep it stable per TV |
| `timeout` | no | Seconds to wait for playback (default 45) |

On success the response holds `ok: true`, `state`, `play_method` (DirectPlay, DirectStream or Transcode), `play_session`, `content_id` (the stream URL, with the token masked) and `seconds`. On failure it holds `ok: false` and an `error` (nothing by that name, the receiver's own error, the TV refusing the stream, or a timeout), so a script can fall back to another way of casting.

### Playing by name

The integration asks your server as the token's user, so it only finds what that user can see:

- Case, accents and punctuation don't matter: `how its made` finds *How It's Made*.
- An exact name wins. If there is none, it takes a title that contains the words you gave: `matrix reloaded` finds *The Matrix Reloaded*. Episodes only count for those looser matches if you set `item_type: Episode`, because their titles are everywhere.
- A **show** plays one of its episodes: **Next Up** for the token's user (the next unwatched one, or the first episode once it was all watched), or a random one with `episode: random`. Since the receiver reports playback under that same user, Next Up moves along as you watch.
- When two items share the name (*Fargo* the movie and *Fargo* the show), a movie or a show goes before a video, and those before an episode. After that it follows the server's order. The others are listed in the response so you can pick one with `item_type`, `library` or `item_id`.
- `library` keeps the search inside one of your libraries, named as in Jellyfin (case doesn't matter): `name: Fargo` with `library: TV Shows` plays the show, with `library: Movies` the film. It's handy when a library holds one kind of thing, like a library of background videos. An unknown library answers `ok: false` with your library names as `candidates`.

The response also says what it found:

```yaml
ok: true
query: Futurama
item_id: 0d3f…                 # what was played
name: Space Pilot 3000
type: Episode
series: Futurama
season: 1
episode: 1
series_id: 5e1e…
episode_choice: next_up
matched: exact                 # or partial
# library: TV Shows            # when you gave one
# other_matches: [{item_id, name, type, year}, …] when the name wasn't unique
# ...plus state, play_method, play_session, content_id, seconds
```

If nothing matches, `ok` is `false` and `candidates` lists what the server suggested instead, which is handy for a voice assistant to answer with.

### Shuffling a show

```yaml
action: jellyfin_cast.play_item
data:
  entity_id: media_player.living_room_tv
  name: Futurama
  shuffle: true
```

With `shuffle: true`, a show (found by name, or given by its own `item_id`) plays all its episodes in random order, then again in a new random order, and so on, until something else plays or you stop it. The integration sends the receiver one queue of 200 episodes: every episode once before any repeats, never the same one twice in a row, about 70 hours of a 22-minute show. A long show gives 200 different episodes. Skipping on the TV or in Jellyfin moves through that queue.

The response has `episode_choice: shuffle`, `queue_length` and the first episode. `shuffle` replaces `episode`, and anything that isn't a show (a movie, an episode, a video) just plays as usual.

Unlike the receiver's own Shuffle command, which queues 50 random episodes and then stops, this one doesn't run out on a short show.

## Finding the `item_id`

You only need one to pin an exact item, for example a dashboard button that should always start the same movie, or one of two shows with the same name.

- **In the Jellyfin web app:** open the movie or episode. The id is in the address bar: `http://jellyfin.local:8096/web/#/details?id=1ffa07c043d35bab9a1c090831a41d13&serverId=…`. A show or a season has an id too, but those aren't playable. With an item_id, pick an episode, or play the show by name.
- **From a name lookup:** play it once by name and take `item_id` (or `series_id`, or one of `other_matches`) from the response.
- **With the API:** `GET /Items?searchTerm=<name>&includeItemTypes=Movie,Series&recursive=true` returns `Items` with their `Id`. My own scripts do that with `rest_command`s. See [`includes/rest_command.yaml`](../../includes/rest_command.yaml) and [`scripts.yaml`](../../scripts.yaml) in this repo, e.g. `jellyfin_play_random_episode_on_a_tv`, which also falls back to the Default Media Receiver when the receiver fails.

## Good to know

- The TV picker in the action editor lists only the cast players that can show video: Chromecasts, Google TVs, TVs with Chromecast built in, Nest Hubs. Speakers, speaker groups and cast-enabled audio receivers are left out (the Cast integration gives those device class `speaker`; set another device class on one if you want it listed). The list follows new and removed cast devices. Scripts and automations can still target any cast player.

- Each TV shows up in Jellyfin as a *Chromecast* session named "<TV name> Jellyfin", so the Jellyfin integration may add a media player for it.
- Some older devices can't play what the receiver chooses. My 1st-gen Nest Hub failed on a 4K movie. I send those to the Default Media Receiver instead, and use the error answer for the rest.
- Restarting Home Assistant stops whatever it cast: Home Assistant is the only sender, and the receiver quits when its last sender leaves.
- `receiver.py` only needs `pychromecast`, so you can try it against a real TV outside Home Assistant.
