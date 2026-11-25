"""Bake simple closed OSM building outlines and roofs into the two playable city districts."""
import argparse
import json
import math
from pathlib import Path
import re
from fetch_osm_roads import PRESETS, fetch, project

DATA = Path(__file__).resolve().parents[1] / "Content/Data"

def cross(a, b, c):
    return (b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0])

def triangulate(ring):
    # Ear clipping on a CCW simple ring. A convex fan would fill courtyards and recesses.
    active = list(range(len(ring)))
    triangles = []
    while len(active) > 2:
        for i, b in enumerate(active):
            a, c = active[i-1], active[(i+1) % len(active)]
            if cross(ring[a],ring[b],ring[c]) <= 0:
                continue
            if any(min(cross(ring[a],ring[b],ring[p]), cross(ring[b],ring[c],ring[p]),
                       cross(ring[c],ring[a],ring[p])) >= 0 for p in active if p not in (a,b,c)):
                continue
            triangles.extend([a,c,b])  # Unreal front face
            active.pop(i)
            break
        else:
            return []
    return triangles

def height(tags, osm_id):
    raw = tags.get("height","").strip().lower()
    match = re.fullmatch(r"(\d+(?:\.\d+)?)\s*(m|ft)?",raw)
    if match:
        meters = float(match[1]) * (0.3048 if match[2]=="ft" else 1)
        if 2 <= meters <= 220:
            return round(meters * 100), "height"
    try:
        levels = float(tags.get("building:levels", ""))
        if 1 <= levels <= 60:
            return round(levels * 310), "levels"
    except ValueError:
        pass
    floors = 3 + osm_id % 4 if tags.get("building") in {"office","commercial","apartments"} else 1 + osm_id % 3
    return floors * 310, "estimated"

def convert(city, response, endpoint):
    lat, lon = PRESETS[city]
    buildings = []
    for way in response["elements"]:
        tags = way.get("tags",{})
        nodes = way.get("nodes",[])
        if len(nodes)<4 or nodes[0]!=nodes[-1] or tags.get("building") in ("roof","no","construction","ruins"):
            continue
        ring = []
        for p in way["geometry"][:-1]:
            xy = [round(v) for v in project(p["lat"],p["lon"],lat,lon)]
            if not ring or math.dist(xy,ring[-1]) >= 5:
                ring.append(xy)
        if len(ring)<3 or len(set(map(tuple,ring)))!=len(ring):
            continue
        area = sum(a[0]*b[1]-a[1]*b[0] for a,b in zip(ring,ring[1:]+ring[:1]))
        if abs(area)<20000:
            continue
        if area<0:
            ring.reverse()
        triangles = triangulate(ring)
        if not triangles:
            continue
        h,basis = height(tags,way["id"])
        buildings.append({"id": way["id"], "p":ring, "roof":triangles,"height_cm":h,"height_source":basis})
    if not buildings:
        raise ValueError("No usable building outlines")
    return {"city":city,"center":PRESETS[city],"radius_m":1200,"source":endpoint,
            "attribution":"© OpenStreetMap contributors","license":"ODbL-1.0",
            "snapshot":response.get("osm3s",{}).get("timestamp_osm_base"),
            "coverage":"Simple closed building ways intersecting the district. Multipolygons and interiors are not reconstructed. Untagged heights, materials and windows are illustrative.",
            "buildings":buildings}

if __name__=="__main__":
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("city",choices=PRESETS)
    ap.add_argument("--saved-response",type=Path)
    args=ap.parse_args()
    if args.saved_response:
        saved=json.loads(args.saved_response.read_text())
        response,endpoint=saved["response"],saved["endpoint"]
    else:
        lat,lon=PRESETS[args.city]
        response,endpoint=fetch(f'[out:json][timeout:45];way["building"](around:1200,{lat},{lon});out geom;')
    result=convert(args.city,response,endpoint)
    path=DATA/(args.city+"_buildings.json")
    path.with_suffix(".tmp").write_text(json.dumps(result,separators=(",",":")))
    path.with_suffix(".tmp").replace(path)
    print(args.city,len(result["buildings"]),"footprints")
