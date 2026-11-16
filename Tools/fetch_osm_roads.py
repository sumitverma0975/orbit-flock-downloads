#!/usr/bin/env python3
"""Fetch bounded OpenStreetMap roads for Delhi Drive (no third-party Python packages).

Examples:
  python Tools/fetch_osm_roads.py delhi --radius 1200
  python Tools/fetch_osm_roads.py chandigarh --radius 1200
  python Tools/fetch_osm_roads.py --lat 28.5562 --lon 77.1000 --name igi

Geometry uses Unreal centimetres, X east and Y south. OSM tags are observations
in a community database, not a survey of current road conditions. This tool never
infers a real pothole from a road class or bad surface. Random gameplay hazards,
traffic, buildings and train schedules are separate synthetic game content.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

PRESETS = {"delhi": (28.6315, 77.2167), "chandigarh": (30.7333, 76.7794)}
WIDTH_M = {
    "motorway": 10.0, "motorway_link": 5.0, "trunk": 10.0, "trunk_link": 5.0,
    "primary": 10.0, "primary_link": 5.0, "secondary": 8.0, "secondary_link": 5.0,
    "tertiary": 7.0, "tertiary_link": 4.0, "unclassified": 6.0,
    "residential": 5.5, "living_street": 4.5, "service": 4.0,
}
OVERPASS_ENDPOINTS = (
    "https://overpass-api.de/api/interpreter",
    "https://overpass.kumi.systems/api/interpreter",
    "https://overpass.private.coffee/api/interpreter",
)
ROAD_TAGS = ("name", "name:en", "ref", "surface", "smoothness", "tracktype", "lanes",
             "width", "oneway", "maxspeed", "access", "motor_vehicle", "motorcar",
             "bridge", "tunnel", "layer", "lit", "sidewalk", "hazard", "pothole")
FEATURE_TAGS = ("name", "barrier", "access", "motor_vehicle", "motorcar", "bicycle",
                "foot", "railway", "crossing", "crossing:barrier", "highway",
                "traffic_calming", "hazard", "pothole", "surface", "smoothness")


def build_query(lat: float, lon: float, radius: int) -> str:
    area = f"(around:{radius},{lat},{lon})"
    classes = "|".join(WIDTH_M)
    return (
        '[out:json][timeout:90];('
        f'way["highway"~"^({classes})$"]{area};'
        f'node["railway"="level_crossing"]{area};'
        f'node["barrier"]{area};'
        f'node["highway"="crossing"]{area};'
        f'node["traffic_calming"]{area};'
        f'node["hazard"~"^(pothole|potholes)$"]{area};'
        f'node["pothole"="yes"]{area};'
        ');out geom;'
    )


def fetch(query: str, endpoint: str | None = None) -> tuple[dict, str]:
    """Try public mirrors once each; report failure rather than writing fake data."""
    failures = []
    for url in ((endpoint,) if endpoint else OVERPASS_ENDPOINTS):
        request = urllib.request.Request(
            url + "?" + urllib.parse.urlencode({"data": query}),
            headers={"User-Agent": "DelhiDrive-road-tools/2.0 (OpenStreetMap game extract)"},
        )
        try:
            with urllib.request.urlopen(request, timeout=110) as response:
                document = json.load(response)
            if not isinstance(document.get("elements"), list) or document.get("remark"):
                raise ValueError(document.get("remark", "Missing Overpass elements array"))
            return document, url
        except (OSError, ValueError, urllib.error.URLError) as exc:
            failures.append(f"{url}: {exc}")
            print(f"Overpass attempt failed: {failures[-1]}", file=sys.stderr)
    raise RuntimeError("All Overpass endpoints failed. Retry later.\n" + "\n".join(failures))


def project(lat: float, lon: float, lat0: float, lon0: float) -> list[float]:
    return [round((lon - lon0) * 111320.0 * math.cos(math.radians(lat0)) * 100.0, 1),
            round(-(lat - lat0) * 110540.0 * 100.0, 1)]


def road_width(tags: dict) -> tuple[float, str]:
    """Only explicit width is a mapped width; lanes/class are documented estimates."""
    match = re.fullmatch(r"\s*(\d+(?:\.\d+)?)\s*(m|metres?|meters?|ft|feet)?\s*", tags.get("width", ""), re.I)
    if match:
        width = float(match.group(1))
        if (match.group(2) or "").lower() in ("ft", "feet"):
            width *= 0.3048
        if 1.5 <= width <= 40:
            return round(width, 2), "osm_width_tag"
    lanes = tags.get("lanes", "")
    if str(lanes).isdigit() and 1 <= int(lanes) <= 12:
        return round(int(lanes) * 3.3, 2), "estimate_from_lanes"
    return WIDTH_M[tags["highway"]], "estimate_from_road_class"


def _clip_segment(a: list[float], b: list[float], radius_cm: float):
    """Analytic line/circle clipping; keeps through-roads with both ends outside."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    aa = dx * dx + dy * dy
    if aa < 0.01:
        return None
    bb = 2 * (a[0] * dx + a[1] * dy)
    cc = a[0] * a[0] + a[1] * a[1] - radius_cm * radius_cm
    discriminant = bb * bb - 4 * aa * cc
    if discriminant < 0:
        return None
    root = math.sqrt(max(0, discriminant))
    lo = max(0.0, (-bb - root) / (2 * aa))
    hi = min(1.0, (-bb + root) / (2 * aa))
    if hi - lo <= 0.0000001:
        return None
    return [[round(a[0] + t * dx, 1), round(a[1] + t * dy, 1)] for t in (lo, hi)]


def clip_polyline(points: list[list[float]], radius_cm: float) -> list[list[list[float]]]:
    parts: list[list[list[float]]] = []
    current: list[list[float]] = []
    for a, b in zip(points, points[1:]):
        clipped = _clip_segment(a, b, radius_cm)
        if clipped is None:
            if len(current) >= 2:
                parts.append(current)
            current = []
            continue
        start, end = clipped
        if current and math.dist(current[-1], start) <= 0.2:
            if math.dist(current[-1], end) > 0.2:
                current.append(end)
        else:
            if len(current) >= 2:
                parts.append(current)
            current = [start, end]
    if len(current) >= 2:
        parts.append(current)
    return parts


def convert(document: dict, lat: float, lon: float, radius: int, *, name: str,
            keep_bridges: bool = False, endpoint: str = "offline fixture",
            retrieved_at: str | None = None) -> dict:
    roads, crossings, features = [], [], []
    excluded = {"elevated_or_bridge": 0, "tunnel": 0, "restricted_motor_access": 0}
    for element in document.get("elements", []):
        tags = element.get("tags", {})
        if element.get("type") == "node" and "lat" in element and "lon" in element:
            point = project(element["lat"], element["lon"], lat, lon)
            if math.hypot(*point) > radius * 100 + 1:
                continue
            kinds = []
            if tags.get("railway") == "level_crossing":
                crossings.append(point)
                kinds.append("railway_level_crossing")
            if tags.get("barrier"):
                kinds.append("barrier")
            if tags.get("highway") == "crossing":
                kinds.append("pedestrian_crossing")
            if tags.get("traffic_calming"):
                kinds.append("traffic_calming")
            if tags.get("hazard") in ("pothole", "potholes") or tags.get("pothole") == "yes":
                kinds.append("pothole")
            for kind in kinds:
                features.append({"kind": kind, "osm_type": "node", "osm_id": element["id"],
                                 "p": point, "tags": {k: tags[k] for k in FEATURE_TAGS if k in tags},
                                 "source": "explicit_osm_tag", "current_condition_verified": False})
            continue
        if element.get("type") != "way" or tags.get("highway") not in WIDTH_M:
            continue
        if tags.get("tunnel") not in (None, "no", "0"):
            excluded["tunnel"] += 1
            continue
        try:
            elevated = float(tags.get("layer", "0")) > 0
        except ValueError:
            elevated = False
        if not keep_bridges and (tags.get("bridge") not in (None, "no", "0") or elevated):
            excluded["elevated_or_bridge"] += 1
            continue
        # Access tags are ordered from general to motorcar-specific; the most
        # specific value wins (e.g. access=no + motorcar=yes remains driveable).
        access = tags.get("motorcar", tags.get("motor_vehicle", tags.get("access", "yes")))
        if access in ("no", "private"):
            excluded["restricted_motor_access"] += 1
            continue
        width, width_source = road_width(tags)
        points = [project(p["lat"], p["lon"], lat, lon) for p in element.get("geometry", [])
                  if "lat" in p and "lon" in p]
        for part, clipped in enumerate(clip_polyline(points, radius * 100)):
            road = {"h": tags["highway"], "w": width, "p": clipped, "osm_id": element["id"],
                    "part": part, "width_source": width_source,
                    "tags": {k: tags[k] for k in ROAD_TAGS if k in tags}}
            roads.append(road)
    if not roads:
        raise ValueError("No drivable OSM road geometry remains; no output file written")
    osm_timestamp = document.get("osm3s", {}).get("timestamp_osm_base")
    return {
        "schema_version": 2,
        "name": name,
        "center": {"lat": lat, "lon": lon},
        "radius_m": radius,
        "roads": roads,
        "crossings": crossings,
        "features": features,
        "metadata": {
            "source": "OpenStreetMap via Overpass API",
            "attribution": "© OpenStreetMap contributors",
            "copyright_url": "https://www.openstreetmap.org/copyright",
            "license": "ODbL-1.0",
            "license_url": "https://opendatacommons.org/licenses/odbl/1-0/",
            "endpoint": endpoint,
            "retrieved_at_utc": retrieved_at or datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "osm_database_timestamp_utc": osm_timestamp,
            "projection": "Local equirectangular; centimetres; X=east, Y=south, Z=flat",
            "geometry": "Actual OSM road polylines clipped to radius; roads are flattened, not surveyed elevation",
            "widths": "OSM width when tagged, otherwise estimates from lanes or road class",
            "observed_features": "features contains only explicit OSM tags; their presence today is unverified",
            "condition_coverage": "Incomplete community mapping; missing tags never establish smoothness, safety or absence of hazards",
            "gameplay_synthetic": ["random potholes", "traffic vehicles", "buildings", "mission checkpoints", "train schedules"],
            "rendering_note": "Road geometry and mapped railway crossings drive the base map. Feature tags are retained, with only implemented feature kinds rendered; presence in this file does not imply full simulation.",
            "excluded_way_counts": excluded,
            "bridge_policy": "kept_flat" if keep_bridges else "excluded_to_avoid_flattened_grade_separation",
            "counts": {"road_parts": len(roads), "railway_crossings": len(crossings), "explicit_features": len(features)},
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("preset", nargs="?", choices=sorted(PRESETS))
    parser.add_argument("--lat", type=float)
    parser.add_argument("--lon", type=float)
    parser.add_argument("--name", help="output name, without .json")
    parser.add_argument("--radius", type=int, default=1200, help="radius in metres (default 1200; maximum 5000)")
    parser.add_argument("--keep-bridges", action="store_true", help="flatten bridges; may cause false at-grade intersections")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--input", type=Path, help="convert a saved Overpass response without network access")
    parser.add_argument("--endpoint", choices=OVERPASS_ENDPOINTS)
    args = parser.parse_args()
    if args.preset:
        lat, lon = PRESETS[args.preset]
        name = args.name or args.preset
    elif args.lat is not None and args.lon is not None:
        lat, lon = args.lat, args.lon
        name = args.name or "custom"
    else:
        parser.error("provide a city preset or --lat and --lon")
    if not (-90 < lat < 90 and -180 <= lon <= 180) or not 100 <= args.radius <= 5000:
        parser.error("coordinates must be valid and radius must be 100–5000 metres")
    if not re.fullmatch(r"[A-Za-z0-9_-]+", name):
        parser.error("name must contain letters, numbers, underscores or hyphens")
    output = args.out or Path(__file__).resolve().parent.parent / "Content" / "Data" / (name + ".json")
    try:
        if args.input:
            document = json.loads(args.input.read_text(encoding="utf-8"))
            endpoint = "saved Overpass response: " + args.input.name
        else:
            print(f"Fetching {name}: ({lat}, {lon}), radius {args.radius}m", file=sys.stderr)
            document, endpoint = fetch(build_query(lat, lon, args.radius), args.endpoint)
        result = convert(document, lat, lon, args.radius, name=name,
                         keep_bridges=args.keep_bridges, endpoint=endpoint)
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = output.with_suffix(output.suffix + ".tmp")
        temporary.write_text(json.dumps(result, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8")
        temporary.replace(output)
    except (OSError, ValueError, RuntimeError) as exc:
        parser.exit(1, f"Map extraction failed: {exc}\n")
    print(f"Saved {output}: {len(result['roads'])} road parts, {len(result['crossings'])} railway crossings, {len(result['features'])} explicitly mapped features", file=sys.stderr)
    print("OSM geometry is real; procedural scenery and random gameplay potholes are synthetic.", file=sys.stderr)


if __name__ == "__main__":
    main()
