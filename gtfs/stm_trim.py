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

It also writes the route shapes JSON the tablets' bus maps draw their traces
from (www/map-polylines.js, `transit_trace`): every route pattern (shape) of the
trips through the watched stops, simplified, with its stops and their distance
along the shape, the usual pattern per watched stop and route, and the trips
that run a different one. Trip ids change with each STM GTFS release; stale ones
fall back to the usual pattern, so rerun it after a release to stay exact.

Usage (stdlib only):
  python3 stm_trim.py [--src URL|zip] [--out PATH] [--shapes-out PATH|''] [STOP_ID ...]
"""

import argparse
import collections
import csv
import io
import json
import math
import os
import shutil
import sys
import tempfile
import urllib.request
import zipfile

STM_GTFS_URL = "https://www.stm.info/sites/default/files/gtfs/gtfs_stm.zip"
DEFAULT_OUT = "/config/www/gtfs/stm_trimmed.zip"
DEFAULT_SHAPES_OUT = "/config/www/gtfs/stm_shapes.json"
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
SIMPLIFY_M = 2.0  # shape simplification tolerance (metres)


def rows(z, name):
    with z.open(name) as f:
        yield from csv.DictReader(io.TextIOWrapper(f, encoding="utf-8-sig", newline=""))


def simplify(pts, tol, xy):
    """Douglas-Peucker on [lat, lon] points; xy() maps a point to local metres."""
    if len(pts) < 3:
        return list(pts)
    keep = [False] * len(pts)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        (ax, ay), (bx, by) = xy(pts[a]), xy(pts[b])
        vx, vy = bx - ax, by - ay
        ln = math.hypot(vx, vy) or 1e-9
        i, dmax = 0, -1.0
        for k in range(a + 1, b):
            px, py = xy(pts[k])
            d = abs(vx * (ay - py) - vy * (ax - px)) / ln
            if d > dmax:
                i, dmax = k, d
        if dmax > tol:
            keep[i] = True
            stack += [(a, i), (i, b)]
    return [p for p, k in zip(pts, keep) if k]


def shapes_json(z, at, stop_seqs, stops_rows, feed_version):
    """Route shapes for the trips through the watched stops (see the module docstring).

    at: trip_id -> watched stops it serves; stop_seqs: representative trip -> [(stop_sequence, stop_id)].
    """
    trips = {}
    for r in rows(z, "trips.txt"):
        if r["trip_id"] in at:
            trips[r["trip_id"]] = (r["route_id"], r["direction_id"], r["shape_id"], r["trip_headsign"])
    counts = collections.defaultdict(collections.Counter)  # (stop, route) -> Counter(shape)
    for t, served in at.items():
        route, _, shape, _ = trips[t]
        for s in served:
            counts[(s, route)][shape] += 1
    default = collections.defaultdict(dict)
    for (s, route), c in sorted(counts.items()):
        default[s][route] = c.most_common(1)[0][0]
    other = {t: trips[t][2] for t, served in at.items()
             if any(trips[t][2] != default[s][trips[t][0]] for s in served)}

    reps = {}  # shape -> representative trip (one that has its stop sequence)
    for t, (route, d, shape, hs) in trips.items():
        if t in stop_seqs and shape not in reps:
            reps[shape] = t
    raw = collections.defaultdict(list)
    for r in rows(z, "shapes.txt"):
        if r["shape_id"] in reps:
            raw[r["shape_id"]].append((int(r["shape_pt_sequence"]), float(r["shape_pt_lat"]), float(r["shape_pt_lon"])))
    stop_at = {r["stop_id"]: r for r in stops_rows}

    patterns = {}
    for shape, t in sorted(reps.items()):
        route, d, _, hs = trips[t]
        pts = [(la, lo) for _, la, lo in sorted(raw[shape])]
        lat0 = math.radians(pts[0][0])

        def xy(p, lat0=lat0, o=pts[0]):
            return ((p[1] - o[1]) * 111320 * math.cos(lat0), (p[0] - o[0]) * 110574)

        pts = simplify(pts, SIMPLIFY_M, xy)
        cum = [0.0]
        for a, b in zip(pts, pts[1:]):
            (ax, ay), (bx, by) = xy(a), xy(b)
            cum.append(cum[-1] + math.hypot(bx - ax, by - ay))
        stops, seg = [], 0  # stops in trip order, projected forward along the shape (loops stay in order)
        for _, sid in sorted(stop_seqs[t]):
            s = stop_at.get(sid)
            if not s:
                continue
            p = (float(s["stop_lat"]), float(s["stop_lon"]))
            px, py = xy(p)
            best = None
            for k in range(seg, len(pts) - 1):
                (ax, ay), (bx, by) = xy(pts[k]), xy(pts[k + 1])
                vx, vy = bx - ax, by - ay
                l2 = vx * vx + vy * vy or 1e-9
                u = max(0.0, min(1.0, ((px - ax) * vx + (py - ay) * vy) / l2))
                dist = math.hypot(px - (ax + u * vx), py - (ay + u * vy))
                if best is None or dist < best[0] - 1e-6:
                    best = (dist, k, u)
                if best[0] < 25 and dist > best[0] + 250:  # past the nearest pass: stop searching
                    break
            _, k, u = best
            seg = k
            stops.append([sid, round(p[0], 6), round(p[1], 6), s["stop_name"], round(cum[k] + u * (cum[k + 1] - cum[k]), 1)])
        patterns[shape] = {"route": route, "dir": d, "headsign": hs,
                           "pts": [[round(la, 6), round(lo, 6)] for la, lo in pts], "stops": stops}
    return {"feed_version": feed_version, "patterns": patterns, "default": default, "trips": other}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--src", default=STM_GTFS_URL, help="GTFS zip URL or path")
    ap.add_argument("--out", default=DEFAULT_OUT, help="trimmed zip to write")
    ap.add_argument("--shapes-out", default=DEFAULT_SHAPES_OUT, help="route shapes JSON to write ('' to skip)")
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
            si, ti, qi = cols.index("stop_id"), cols.index("trip_id"), cols.index("stop_sequence")

            def stop_times():
                """Yield (trip_id, stop_id, stop_sequence) per stop_times.txt row (byte scan)."""
                with z.open("stop_times.txt") as f:
                    f.readline()
                    for line in f:
                        parts = line.split(b",", max(si, ti, qi) + 1)
                        yield parts[ti].strip().strip(b'"'), parts[si].strip().strip(b'"'), parts[qi]

            # pass 1: trips through the watched stops; pass 2: all their stops
            # (+ the stop sequence of one trip per route pattern for the shapes JSON)
            watched_b = {s.encode() for s in watched}
            at = collections.defaultdict(set)
            for t, s, _ in stop_times():
                if s in watched_b:
                    at[t].add(s.decode())
            trips = set(at)
            reps = set()
            if args.shapes_out:
                seen = set()
                for r in rows(z, "trips.txt"):
                    if r["trip_id"].encode() in trips and r["shape_id"] not in seen:
                        seen.add(r["shape_id"])
                        reps.add(r["trip_id"].encode())
            keep = set(watched_b)
            stop_seqs = collections.defaultdict(list)
            for t, s, q in stop_times():
                if t in trips:
                    keep.add(s)
                    if t in reps:
                        stop_seqs[t.decode()].append((int(q.strip().strip(b'"')), s.decode()))
            keep = {s.decode() for s in keep}

            with z.open("stops.txt") as f:
                rows_ = list(csv.reader(io.TextIOWrapper(f, encoding="utf-8-sig", newline="")))
            sid = rows_[0].index("stop_id")
            if "parent_station" in rows_[0]:
                pi = rows_[0].index("parent_station")
                keep |= {r[pi] for r in rows_[1:] if r[sid] in keep and r[pi]}
            stops_out = io.StringIO()
            w = csv.writer(stops_out, lineterminator="\r\n")
            w.writerow(rows_[0])
            w.writerows(r for r in rows_[1:] if r[sid] in keep)

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

            if args.shapes_out:
                feed = next(rows(z, "feed_info.txt"), {}).get("feed_version", "")
                hdr = rows_[0]
                stops_rows = [dict(zip(hdr, r)) for r in rows_[1:] if r[sid] in keep]
                at_s = {t.decode(): v for t, v in at.items()}
                data = shapes_json(z, at_s, stop_seqs, stops_rows, feed)
                os.makedirs(os.path.dirname(os.path.abspath(args.shapes_out)), exist_ok=True)
                tmp_json = args.shapes_out + ".tmp"
                with open(tmp_json, "w", encoding="utf-8") as f:
                    json.dump(data, f, ensure_ascii=False, separators=(",", ":"))
                os.replace(tmp_json, args.shapes_out)
                print(
                    f"{args.shapes_out}: {len(data['patterns'])} route patterns, "
                    f"{sum(len(p['pts']) for p in data['patterns'].values())} shape points, "
                    f"{len(data['trips'])} trips off their usual pattern, "
                    f"{os.path.getsize(args.shapes_out) / 1e3:.0f} kB"
                )
        return 0
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
