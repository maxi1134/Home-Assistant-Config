"""Find a Jellyfin item by name, the way someone would ask for it ("play Futurama").

No Home Assistant imports: the server calls only need aiohttp. It asks the server as the
token's user, so it only finds what that user may see.
"""

from __future__ import annotations

import random
import re
import unicodedata
from typing import Any

import aiohttp

from .receiver import plain_id

SEARCH_TYPES = ("Movie", "Series", "Episode", "Video")
EPISODE_CHOICES = ("next_up", "random")
# Among exact matches, a movie or a show wins over a video, and those over an episode
# (episode titles such as "Pilot" are everywhere). Otherwise the server's order stands.
TYPE_RANK = {"Movie": 0, "Series": 0, "Video": 1, "Episode": 2}
# Jellyfin's sort names drop these, and nameStartsWith compares against the sort name.
LEADING_ARTICLE = re.compile(r"^(the|a|an)\s+", re.IGNORECASE)
FIELDS = "ProductionYear,SeriesName,ParentIndexNumber,IndexNumber"
# A shuffled show is sent as one queue: its episodes in random order, reshuffled each time
# they have all been queued. 200 is about 70 hours of 22-minute episodes, so it plays until
# something stops it, and keeps the cast message well under its 64 kB limit.
SHUFFLE_QUEUE = 200


class ItemNotFound(Exception):
    """Nothing matched; candidates lists what the server suggested instead."""

    def __init__(self, message: str, candidates: list[str]) -> None:
        super().__init__(message)
        self.candidates = candidates


def words(text: str) -> str:
    """Case, accents and punctuation ignored: "How It's Made!" -> "how its made"."""
    text = unicodedata.normalize("NFKD", text or "")
    text = "".join(ch for ch in text if not unicodedata.combining(ch)).casefold()
    text = re.sub(r"['’‘`]", "", text)
    return " ".join(re.sub(r"[^\w]+", " ", text).split())


def compact(text: str) -> str:
    """words() without the spaces, so "Spider-Man" and "spiderman" are the same name."""
    return words(text).replace(" ", "")


def describe(item: dict[str, Any]) -> dict[str, Any]:
    """What a script or a voice answer needs to know about an item."""
    out: dict[str, Any] = {"item_id": plain_id(item["Id"]), "name": item.get("Name"), "type": item.get("Type")}
    if item.get("ProductionYear"):
        out["year"] = item["ProductionYear"]
    if item.get("Type") == "Episode":
        out |= {"series": item.get("SeriesName"), "season": item.get("ParentIndexNumber"),
                "episode": item.get("IndexNumber")}
    return out


class JellyfinLookup:
    """Name -> playable item, with the token and user the receiver gets."""

    def __init__(self, session: aiohttp.ClientSession, server_url: str, user_id: str, token: str) -> None:
        self._session = session
        self._base = server_url.rstrip("/")
        self._user_id = user_id
        self._headers = {"Authorization": f'MediaBrowser Token="{token}"'}

    async def _get(self, path: str, **params: Any) -> dict[str, Any]:
        query = {"userId": self._user_id, "enableImages": "false", "enableUserData": "false"}
        query |= {key: str(value) for key, value in params.items() if value is not None}
        async with self._session.get(
            f"{self._base}{path}", params=query, headers=self._headers,
            timeout=aiohttp.ClientTimeout(total=15),
        ) as response:
            response.raise_for_status()
            return await response.json()

    async def _items(self, **params: Any) -> list[dict[str, Any]]:
        return (await self._get("/Items", recursive="true", fields=FIELDS, **params)).get("Items") or []

    async def library(self, name: str) -> dict[str, Any]:
        """The user's library with that name ("Movies", "tv shows"), compared like item names."""
        views = (await self._get("/UserViews")).get("Items") or []
        for view in views:
            if compact(view.get("Name", "")) == compact(name):
                return view
        raise ItemNotFound(f"No library called {name!r}", [view.get("Name", "") for view in views])

    async def find(self, name: str, item_type: str | None = None, episode: str = "next_up",
                   library: str | None = None, shuffle: bool = False) -> tuple[list[dict[str, Any]], dict[str, Any]]:
        """What to play for a name (a queue of one item, or a shuffled show), and how it was found.

        item_type limits the search (Movie, Series, Episode or Video), library to one of the
        user's libraries, by name. A show gives one of its episodes: Next Up for the token's
        user (the first episode once it was all watched), a random one, or with shuffle all
        of them in random order. Raises ItemNotFound, or aiohttp errors.
        """
        wanted = compact(name)
        if not wanted:
            raise ItemNotFound(f"Nothing to look for in {name!r}", [])
        how: dict[str, Any] = {}
        scope: dict[str, Any] = {"includeItemTypes": ",".join([item_type] if item_type else SEARCH_TYPES)}
        if library:
            view = await self.library(library)
            scope["parentId"] = view["Id"]
            how["library"] = view.get("Name")
        found = await self._items(searchTerm=name.strip(), limit=25, **scope)
        exact = [item for item in found if compact(item.get("Name", "")) == wanted]
        if not exact:
            # The server's search (or a search plugin such as Meilisearch) can miss a new item
            # or rank it out of the first page; ask the database for names starting with it.
            start = LEADING_ARTICLE.sub("", name.strip())
            more = await self._items(nameStartsWith=start, limit=25, **scope)
            seen = {item["Id"] for item in found}
            found += [item for item in more if item["Id"] not in seen]
            exact = [item for item in found if compact(item.get("Name", "")) == wanted]

        if exact:
            exact.sort(key=lambda item: TYPE_RANK.get(item.get("Type"), 3))  # stable: server order next
            chosen, how["matched"] = exact[0], "exact"
            if len(exact) > 1:
                # Same name twice (a movie and a show, two versions of a show): say which others
                # there were, so a script can pin one with item_id.
                how["other_matches"] = [describe(item) for item in exact[1:6]]
        else:
            # A name that contains the words asked for, as whole words. Not an episode unless
            # episodes were asked for: their titles are too common ("Lava" would find a
            # cartoon episode before the lava lamp video).
            phrase = f" {words(name)} "
            partial = [item for item in found if phrase in f" {words(item.get('Name', ''))} "
                       and (item.get("Type") != "Episode" or item_type == "Episode")]
            if not partial:
                where = how.get("library") or "Jellyfin"
                raise ItemNotFound(f"Nothing in {where} is called {name!r}",
                                   [item.get("Name", "") for item in found[:5]])
            chosen, how["matched"] = partial[0], "partial"

        return await self.playable(chosen, how, episode, shuffle)

    async def by_id(self, item_id: str, shuffle: bool = False) -> tuple[list[dict[str, Any]], dict[str, Any]]:
        """What to play for an item id; only needed to shuffle a show given by its id."""
        items = await self._items(ids=plain_id(item_id))
        if not items:
            raise ItemNotFound(f"No item with id {item_id!r}", [])
        return await self.playable(items[0], {}, "next_up", shuffle)

    async def describe_id(self, item_id: str) -> dict[str, Any]:
        """describe() of an item by its id, or {} when the server doesn't know it."""
        items = await self._items(ids=plain_id(item_id))
        return describe(items[0]) if items else {}

    async def playable(self, chosen: dict[str, Any], how: dict[str, Any], episode: str,
                       shuffle: bool) -> tuple[list[dict[str, Any]], dict[str, Any]]:
        """A show becomes one episode, or with shuffle a queue of them; anything else plays as is."""
        if chosen.get("Type") != "Series":
            return [chosen], how
        how["series_id"] = plain_id(chosen["Id"])
        if shuffle:
            queue = await self.shuffled(chosen)
            how |= {"episode_choice": "shuffle", "queue_length": len(queue)}
            return queue, how
        how["episode_choice"] = episode
        return [await self.episode_of(chosen, episode)], how

    async def shuffled(self, series: dict[str, Any]) -> list[dict[str, Any]]:
        """Every episode of a show that has a file, in random order, again and again (SHUFFLE_QUEUE)."""
        episodes = await self._items(parentId=series["Id"], includeItemTypes="Episode", isMissing="false")
        if not episodes:
            raise ItemNotFound(f"{series.get('Name')!r} has no episode to play", [])
        queue: list[dict[str, Any]] = []
        while len(queue) < SHUFFLE_QUEUE:
            batch = random.sample(episodes, len(episodes))
            if queue and len(batch) > 1 and batch[0]["Id"] == queue[-1]["Id"]:
                batch[0], batch[-1] = batch[-1], batch[0]  # never the same episode twice in a row
            queue += batch
        return queue[:SHUFFLE_QUEUE]

    async def episode_of(self, series: dict[str, Any], episode: str) -> dict[str, Any]:
        """Next Up (or the first episode) or a random episode of a show."""
        common = {"parentId": series["Id"], "includeItemTypes": "Episode", "isMissing": "false"}
        if episode == "random":
            items = await self._items(sortBy="Random", limit=1, **common)
        else:
            up = await self._get("/Shows/NextUp", seriesId=series["Id"], limit=1, fields=FIELDS,
                                 enableTotalRecordCount="false")
            items = up.get("Items") or []
            if not items:
                # Watched to the end: start over at the first episode, specials (season 0) last.
                every = await self._items(sortBy="ParentIndexNumber,IndexNumber", limit=50, **common)
                items = sorted(every, key=lambda item: item.get("ParentIndexNumber") == 0)
        if not items:
            raise ItemNotFound(f"{series.get('Name')!r} has no episode to play", [])
        return items[0]
