"""Find a Jellyfin item by name, the way someone would ask for it ("play Futurama").

No Home Assistant imports: the server calls only need aiohttp. It asks the server as the
token's user, so it only finds what that user may see.
"""

from __future__ import annotations

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

    async def find(self, name: str, item_type: str | None = None,
                   episode: str = "next_up") -> tuple[dict[str, Any], dict[str, Any]]:
        """The item to play for a name, and how it was found.

        item_type limits the search (Movie, Series, Episode or Video). A show gives one of
        its episodes: Next Up for the token's user (the first episode once it was all
        watched), or a random one. Raises ItemNotFound, or aiohttp errors.
        """
        wanted = compact(name)
        if not wanted:
            raise ItemNotFound(f"Nothing to look for in {name!r}", [])
        types = ",".join([item_type] if item_type else SEARCH_TYPES)
        found = await self._items(searchTerm=name.strip(), includeItemTypes=types, limit=25)
        exact = [item for item in found if compact(item.get("Name", "")) == wanted]
        if not exact:
            # The server's search (or a search plugin such as Meilisearch) can miss a new item
            # or rank it out of the first page; ask the database for names starting with it.
            start = LEADING_ARTICLE.sub("", name.strip())
            more = await self._items(nameStartsWith=start, includeItemTypes=types, limit=25)
            seen = {item["Id"] for item in found}
            found += [item for item in more if item["Id"] not in seen]
            exact = [item for item in found if compact(item.get("Name", "")) == wanted]

        how: dict[str, Any] = {}
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
                raise ItemNotFound(f"Nothing in Jellyfin is called {name!r}",
                                   [item.get("Name", "") for item in found[:5]])
            chosen, how["matched"] = partial[0], "partial"

        if chosen.get("Type") == "Series":
            how["series_id"] = plain_id(chosen["Id"])
            how["episode_choice"] = episode
            chosen = await self.episode_of(chosen, episode)
        return chosen, how

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
