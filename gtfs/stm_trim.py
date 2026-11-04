#!/usr/bin/env python3
"""Build a trimmed STM GTFS static zip for the GTFS Realtime integration.

Why: the gtfs_realtime custom integration (bcpearce, gtfs_station_stop 0.11.8)
is built for small feeds. With STM's 69 MB zip it would
- parse all of stop_times.txt (336 MB, 8.5 M rows; ~25 s) on HA's event loop at
  every schedule refresh,
- deep-copy the whole schedule on the event loop at every realtime refresh
  (seconds for STM's 200 k trips; still ~0.3 s with only our trips), and
- scan every trip for each arrival sensor's headsign.

STM's realtime feed carries absolute arrival times and route ids, so the
integration needs only stop names (sensor devices, destinations) and routes
(colour, type). This zip keeps agency, calendar, feed_info and routes whole,
the stops served by the trips through the watched stops, and header-only
trips.txt / stop_times.txt (sensors lose the "headsign" attribute: Sud/Nord/...).
Stops and routes rarely change, so the file stays valid across STM's monthly
GTFS updates; rerun it when the watched stops change.

Usage (stdlib only):
  python3 stm_trim.py [--src URL|zip] [--out PATH] [STOP_ID ...]
"""

import argparse
import csv
import io
import os
import shutil
import sys
import tempfile
import urllib.request
import zipfile

STM_GTFS_URL = "https://www.stm.info/sites/default/files/gtfs/gtfs_stm.zip"
DEFAULT_OUT = "/config/www/gtfs/stm_trimmed.zip"
# Nearest stop of every bus direction near home that reaches a
# metro station (STM GTFS 2026-11-03):
#   52457 De Lorimier / Marie-Anne   25 m   10 Sud -> Papineau
#   52381 Mont-Royal / De Lorimier  195 m   97 Ouest (night 368) -> Mont-Royal
#   52389 Papineau / Marie-Anne     282 m   45 Sud (445, night 359) -> Papineau
#   52383 Mont-Royal / De Lorimier  180 m   97 Est -> Pie-IX (night 368 -> Frontenac)
#   52388 Papineau / Marie-Anne     271 m   45 Nord (night 359) -> Fabre
#   52564 Rachel / De Lorimier      298 m   29 Est -> Préfontaine
DEFAULT_STOPS = ["52457", "52381", "52389", "52383", "52388", "52564"]
KEEP_WHOLE = ["agency.txt", "calendar.txt", "calendar_dates.txt", "feed_info.txt", "routes.txt"]
HEADER_ONLY = ["trips.txt", "stop_times.txt"]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--src", default=STM_GTFS_URL, help="GTFS zip URL or path")
    ap.add_argument("--out", default=DEFAULT_OUT, help="trimmed zip to write")
    ap.add_argument("stops", nargs="*", default=DEFAULT_STOPS)
    args = ap.parse_args()
    watched = set(args.stops)

    tmpdir = tempfile.mkdtemp(prefix="stm_gtfs_")
    try:
        src = args.src
        if "://" in src:
            src = os.path.join(tmpdir, "gtfs_stm.zip")
            with urllib.request.urlopen(args.src, timeout=120) as r, open(src, "wb") as f:
                shutil.copyfileobj(r, f, 1 << 20)

        with zipfile.ZipFile(src) as z:
            with z.open("stop_times.txt") as f:
                cols = f.readline().decode("utf-8-sig").strip().split(",")
            si, ti = cols.index("stop_id"), cols.index("trip_id")

            def stop_times():
                """Yield (trip_id, stop_id) per stop_times.txt row (byte scan)."""
                with z.open("stop_times.txt") as f:
                    f.readline()
                    for line in f:
                        parts = line.split(b",", max(si, ti) + 1)
                        yield parts[ti].strip().strip(b'"'), parts[si].strip().strip(b'"')

            # pass 1: trips through the watched stops; pass 2: all their stops
            watched_b = {s.encode() for s in watched}
            trips = {t for t, s in stop_times() if s in watched_b}
            keep = set(watched_b)
            keep.update(s for t, s in stop_times() if t in trips)
            keep = {s.decode() for s in keep}

            with z.open("stops.txt") as f:
                rows = list(csv.reader(io.TextIOWrapper(f, encoding="utf-8-sig", newline="")))
            sid = rows[0].index("stop_id")
            if "parent_station" in rows[0]:
                pi = rows[0].index("parent_station")
                keep |= {r[pi] for r in rows[1:] if r[sid] in keep and r[pi]}
            stops_out = io.StringIO()
            w = csv.writer(stops_out, lineterminator="\r\n")
            w.writerow(rows[0])
            w.writerows(r for r in rows[1:] if r[sid] in keep)

            os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
            tmp_out = args.out + ".tmp"
            with zipfile.ZipFile(tmp_out, "w", zipfile.ZIP_DEFLATED) as zo:
                names = set(z.namelist())
                for name in KEEP_WHOLE:
                    if name in names:
                        zo.writestr(name, z.read(name))
                for name in HEADER_ONLY:
                    with z.open(name) as f:
                        zo.writestr(name, f.readline())
                zo.writestr("stops.txt", stops_out.getvalue().encode("utf-8"))
            os.replace(tmp_out, args.out)

        print(
            f"{args.out}: {len(watched)} watched stops, {len(trips)} trips through them, "
            f"{len(keep)} stops kept, {os.path.getsize(args.out) / 1e3:.0f} kB"
        )
        return 0
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
