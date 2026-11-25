#!/usr/bin/env python3
"""Validate source inputs or fail early before invoking Unreal Android packaging."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import sys
import zipfile
from verify_android_package import verify_delivery


ROOT = Path(__file__).resolve().parents[1]
CITY_NAMES = ("delhi", "chandigarh")


def validate_buildings(path, city):
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("city") != city or data.get("license") != "ODbL-1.0":
        raise ValueError(f"{path.name}: missing city or OSM license")
    for key in ("source", "attribution", "snapshot", "coverage"):
        if not data.get(key):
            raise ValueError(f"{path.name}: missing provenance field {key}")
    buildings = data.get("buildings")
    if not isinstance(buildings, list) or not buildings:
        raise ValueError(f"{path.name}: no building footprints")
    for building in buildings:
        points, triangles = building.get("p", []), building.get("roof", [])
        if len(points) < 3 or any(len(p) != 2 or any(not isinstance(v, (int, float)) or not math.isfinite(v) for v in p) for p in points):
            raise ValueError(f"{path.name}: malformed building footprint")
        if not triangles or len(triangles) % 3 or any(type(i) is not int or not 0 <= i < len(points) for i in triangles):
            raise ValueError(f"{path.name}: invalid roof triangle indices")
        height = building.get("height_cm")
        if not isinstance(height, (int, float)) or not math.isfinite(height) or height <= 0:
            raise ValueError(f"{path.name}: invalid building height")
    return len(buildings)


def validate_art_sources():
    vehicle_root = ROOT / "SourceAssets/Vehicles"
    manifest = json.loads((vehicle_root / "prepared_manifest.json").read_text())
    cars = manifest.get("assets", [])
    if len(cars) != 12 or len({entry.get("id") for entry in cars}) != 12:
        raise ValueError("Expected twelve uniquely identified prepared vehicle sources")
    cooked_paths = []
    for entry in cars:
        path = (vehicle_root / entry["file"]).resolve()
        if not path.is_relative_to(vehicle_root.resolve()) or not path.is_file():
            raise ValueError("Missing or invalid prepared car source: " + entry["id"])
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError("Prepared car checksum mismatch: " + entry["id"])
        cooked_paths.append(asset_file(entry["asset_path"], ".uasset"))
    environment_root = ROOT / "SourceAssets/Environment"
    surfaces = json.loads((environment_root / "surfaces.json").read_text())
    if {entry.get("material") for entry in surfaces} != {"Asphalt", "Ground", "Facade", "Roof"}:
        raise ValueError("Expected Asphalt, Ground, Facade and Roof environment surfaces")
    for surface in surfaces:
        if surface.get("license") != "CC0-1.0" or set(surface["textures"]) != {"Color", "Normal", "Rough"}:
            raise ValueError("Environment source license or texture roles are missing")
        for texture in surface["textures"].values():
            path = (environment_root / texture["file"]).resolve()
            if not path.is_relative_to(environment_root.resolve()) or not path.is_file():
                raise ValueError("Missing or invalid environment source texture")
            if hashlib.sha256(path.read_bytes()).hexdigest() != texture["sha256"]:
                raise ValueError("Environment texture checksum mismatch: " + texture["file"])
        cooked_paths.append(ROOT / "Content/Environment" / ("M_" + surface["material"] + ".uasset"))
    derived = json.loads((environment_root / "facade_bays.json").read_text())
    daylight = json.loads((environment_root / "daylight.json").read_text())
    for entry in [*derived.values(), daylight]:
        if hashlib.sha256((environment_root / entry["file"]).read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError("Lighting or facade texture checksum mismatch: " + entry["file"])
    cooked_paths.append(ROOT / "Content/Environment/T_Daylight.uasset")
    for city in CITY_NAMES:
        name = "T_Map_" + city
        image = ROOT / "SourceAssets/Cartography" / (name + ".png")
        if not image.read_bytes().startswith(bytes.fromhex("89504e470d0a1a0a")):
            raise ValueError("Invalid district map: " + str(image))
        cooked_paths.append(ROOT / "Content/Environment" / (name + ".uasset"))
    return cooked_paths


def validate_road_data(path, city=None):
    data = json.loads(path.read_text(encoding="utf-8"))
    roads = data.get("roads", [])
    if not isinstance(roads, list) or not roads:
        raise ValueError(f"{path.name}: no roads")
    def number(value):
        return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)
    for road in roads:
        if not number(road.get("w")) or road["w"] <= 0:
            raise ValueError(f"{path.name}: road without positive numeric width")
        if not isinstance(road.get("h"), str) or not road["h"] or len(road.get("p", [])) < 2:
            raise ValueError(f"{path.name}: road without highway type or two points")
        if any(not isinstance(p, list) or len(p) != 2 or not all(number(v) for v in p) for p in road["p"]):
            raise ValueError(f"{path.name}: invalid road coordinates")
    if city:
        if data.get("schema_version") != 2 or data.get("name") != city:
            raise ValueError(f"{path.name}: expected schema_version 2 and city name {city}")
        center = data.get("center", {})
        if not number(center.get("lat")) or not number(center.get("lon")):
            raise ValueError(f"{path.name}: missing geographic center")
        if not (-90 <= center["lat"] <= 90 and -180 <= center["lon"] <= 180):
            raise ValueError(f"{path.name}: invalid geographic center")
        radius = data.get("radius_m")
        if not number(radius) or radius <= 0:
            raise ValueError(f"{path.name}: missing positive coverage radius")
        if any(math.hypot(*point) > radius * 100 + 0.2 for road in roads for point in road["p"]):
            raise ValueError(f"{path.name}: road point outside documented coverage radius")
        metadata = data.get("metadata", {})
        if metadata.get("source") != "OpenStreetMap via Overpass API" or metadata.get("license") != "ODbL-1.0":
            raise ValueError(f"{path.name}: missing OpenStreetMap source/license metadata")
        for key in ("attribution", "copyright_url", "endpoint", "retrieved_at_utc", "projection",
                    "geometry", "condition_coverage", "gameplay_synthetic"):
            if not metadata.get(key):
                raise ValueError(f"{path.name}: missing provenance field {key}")
        if metadata.get("counts", {}).get("road_parts") != len(roads):
            raise ValueError(f"{path.name}: metadata road count differs from actual geometry")
    return len(roads)


def asset_file(reference, suffix):
    package = reference.split(".", 1)[0]
    if not package.startswith("/Game/"):
        raise ValueError("Expected a /Game/ asset reference: " + reference)
    return ROOT / "Content" / (package[len("/Game/"):] + suffix)


def read_setting(path, name):
    match = re.search(r"^" + re.escape(name) + r"=(.+)$", path.read_text(), re.M)
    if not match:
        raise ValueError(f"{path.name}: missing {name}")
    return match.group(1).strip()


def check_apks(directory, apksigner=None):
    try:
        print(json.dumps(verify_delivery(directory, apksigner), indent=2))
        return 0
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print("ERROR: " + str(exc), file=sys.stderr)
        return 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ue-root", default=os.environ.get("UE_ROOT"))
    parser.add_argument("--source-only", action="store_true", help="Check source data/model hashes; never claims engine build readiness")
    parser.add_argument("--check-apks", type=Path, help="Verify signed Unreal ARM64 delivery and enforce combined 3,000,000,000-byte limit")
    parser.add_argument("--apksigner", help="Explicit Android build-tools apksigner path")
    args = parser.parse_args()
    if args.check_apks:
        return check_apks(args.check_apks, args.apksigner)

    errors = []
    prerequisites = []
    try:
        project = json.loads((ROOT / "DelhiDrive.uproject").read_text())
        if project.get("EngineAssociation") != "5.5":
            errors.append("This source configuration targets UE 5.5; review engine/toolchain compatibility.")
        validate_road_data(ROOT / "Content/Data/sample_grid.json")
        for city in CITY_NAMES:
            count = validate_road_data(ROOT / "Content/Data" / (city + ".json"), city)
            print(f"ROAD DATA: {city}, {count} road parts; source/license/coverage metadata checked.")
            buildings = validate_buildings(ROOT / "Content/Data" / (city + "_buildings.json"), city)
            print(f"BUILDING DATA: {city}, {buildings} footprints; roof triangles and provenance checked.")
        imported_paths = validate_art_sources()
        print("ART SOURCES: twelve prepared cars and four environment surfaces verified by SHA256.")
        engine_config = ROOT / "Config/DefaultEngine.ini"
        game_config = ROOT / "Config/DefaultGame.ini"
        default_map = read_setting(engine_config, "GameDefaultMap")
        map_paths = {asset_file(default_map, ".umap")}
        map_paths.update(ROOT / "Content/Maps" / ("Lvl_" + city.title() + ".umap") for city in CITY_NAMES)
        for map_path in sorted(map_paths):
            if not map_path.is_file() or map_path.stat().st_size == 0:
                prerequisites.append(f"City map missing or empty: {map_path.relative_to(ROOT)}. With UE_ROOT set, run python Tools/setup_map.py.")
        missing_imports = [str(path.relative_to(ROOT)) for path in imported_paths if not path.is_file() or path.stat().st_size == 0]
        if missing_imports:
            prerequisites.append("Prepared runtime assets require UE import: " + ", ".join(missing_imports) + ". Run Tools/setup_map.py.")
        packaging = game_config.read_text()
        for required in ('+DirectoriesToAlwaysCook=(Path="/Game/Cars")',
                         '+DirectoriesToAlwaysCook=(Path="/Game/Environment")',
                         '+DirectoriesToAlwaysStageAsUFS=(Path="Data")',
                         '+MapsToCook=(FilePath="/Game/Maps/Lvl_Delhi")',
                         '+MapsToCook=(FilePath="/Game/Maps/Lvl_Chandigarh")'):
            if required not in packaging:
                errors.append("Missing packaging entry: " + required)
        if '+DirectoriesToAlwaysStageAsNonUFS=(Path="Data")' in packaging:
            errors.append("Data must be staged as UFS for Unreal pak-backed JSON reads on Android.")
    except (OSError, ValueError, TypeError, KeyError) as exc:
        errors.append(str(exc))

    if not args.source_only:
        if not args.ue_root:
            prerequisites.append("UE_ROOT is not set. Point it to an Unreal Engine 5.5 installation.")
        else:
            engine = Path(args.ue_root)
            uat = engine / "Engine/Build/BatchFiles" / ("RunUAT.bat" if os.name == "nt" else "RunUAT.sh")
            if not uat.is_file():
                prerequisites.append(f"Unreal automation tool missing: {uat}")
            version_file = engine / "Engine/Build/Build.version"
            if version_file.is_file():
                try:
                    version = json.loads(version_file.read_text())
                    if (version.get("MajorVersion"), version.get("MinorVersion")) != (5, 5):
                        prerequisites.append("Use UE 5.5 or explicitly update and validate this project's toolchain settings.")
                except (OSError, ValueError) as exc:
                    errors.append(f"Could not read Unreal version: {exc}")

    for error in errors:
        print("ERROR: " + error, file=sys.stderr)
    for missing in prerequisites:
        print(("UNAVAILABLE: " if args.source_only else "ERROR: ") + missing)
    if errors or (prerequisites and not args.source_only):
        print("Preflight failed; no Unreal build was attempted.")
        return 1
    if args.source_only:
        print("Source inputs validated. C++ compilation, imported UE assets, cooking and device performance remain unverified.")
    else:
        print("Input files present. Unreal Turnkey must verify SDK/NDK before compilation and cooking.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
