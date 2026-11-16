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


ROOT = Path(__file__).resolve().parents[1]
CITY_NAMES = ("delhi", "chandigarh")


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


def check_apks(directory):
    apks = sorted(Path(directory).rglob("*.apk"))
    if not apks:
        print(f"ERROR: packaging produced no APK under {directory}", file=sys.stderr)
        return 1
    for path in apks:
        try:
            with zipfile.ZipFile(path) as archive:
                names = archive.namelist()
                if "AndroidManifest.xml" not in names or "classes.dex" not in names:
                    raise ValueError("APK lacks Android manifest or executable dex")
                if not any(n.startswith("lib/arm64-v8a/") and n.endswith(".so") for n in names):
                    raise ValueError("APK lacks ARM64 native libraries")
            digest = hashlib.sha256()
            with path.open("rb") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(block)
            print(f"APK: {path.resolve()} ({path.stat().st_size:,} bytes)")
            print(f"SHA256: {digest.hexdigest()}")
        except (OSError, ValueError, zipfile.BadZipFile) as exc:
            print(f"ERROR: {path}: {exc}", file=sys.stderr)
            return 1
    print("Archive structure checked; install and test on a physical Android phone.")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ue-root", default=os.environ.get("UE_ROOT"))
    parser.add_argument("--source-only", action="store_true", help="Check text inputs only; never claims build readiness")
    parser.add_argument("--check-apks", type=Path, help="Check packaged archive structure and print SHA256 hashes")
    args = parser.parse_args()
    if args.check_apks:
        return check_apks(args.check_apks)

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
        engine_config = ROOT / "Config/DefaultEngine.ini"
        game_config = ROOT / "Config/DefaultGame.ini"
        default_map = read_setting(engine_config, "GameDefaultMap")
        map_paths = {asset_file(default_map, ".umap")}
        map_paths.update(ROOT / "Content/Maps" / ("Lvl_" + city.title() + ".umap") for city in CITY_NAMES)
        car_path = asset_file(read_setting(game_config, "FallbackCarPath"), ".uasset")
        for map_path in sorted(map_paths):
            if not map_path.is_file() or map_path.stat().st_size == 0:
                prerequisites.append(f"City map missing or empty: {map_path.relative_to(ROOT)}. With UE_ROOT set, run python Tools/setup_map.py.")
        if not car_path.is_file() or car_path.stat().st_size == 0:
            prerequisites.append(f"Missing fallback car: {car_path.relative_to(ROOT)}. Create a configured DriveVehicle Blueprint.")
        packaging = game_config.read_text()
        for required in ('+DirectoriesToAlwaysCook=(Path="/Game/Cars")',
                         '+DirectoriesToAlwaysStageAsNonUFS=(Path="Data")',
                         '+MapsToCook=(FilePath="/Game/Maps/Lvl_Delhi")',
                         '+MapsToCook=(FilePath="/Game/Maps/Lvl_Chandigarh")'):
            if required not in packaging:
                errors.append("Missing packaging entry: " + required)
    except (OSError, ValueError, TypeError) as exc:
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
        print("Text inputs validated. C++ compilation, assets, cooking and device performance remain unverified.")
    else:
        print("Input files present. Unreal Turnkey must verify SDK/NDK before compilation and cooking.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
