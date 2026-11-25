#!/usr/bin/env python3
"""Import the twelve prepared, single-mesh vehicle bodies in Unreal Editor 5.5.

System Python: python Tools/import_vehicle_assets.py --validate-only
Unreal commandlet: -run=pythonscript -script=Tools/import_vehicle_assets.py
Default invocation imports every catalog car without dialogs. It never requires
vehicle Blueprints or skeletal rigs: native DriveVehicle physics owns collision.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "SourceAssets/Vehicles/prepared_manifest.json"
REPORT = ROOT / "Saved/VehicleAssetImport.json"
UNSUPPORTED_COMPRESSION = {"EXT_meshopt_compression", "KHR_draco_mesh_compression"}


def read_glb_document(path):
    data = Path(path).read_bytes()
    if len(data) < 20:
        raise ValueError(f"Truncated GLB: {path}")
    magic, version, length = struct.unpack_from("<4sII", data)
    chunk_length, chunk_type = struct.unpack_from("<I4s", data, 12)
    if magic != b"glTF" or version != 2 or length != len(data) or chunk_type != b"JSON" or chunk_length > length - 20:
        raise ValueError(f"Invalid self-contained GLB 2.0: {path}")
    document = json.loads(data[20:20 + chunk_length].decode("utf-8").rstrip("\x00 \t\r\n"))
    unsupported = UNSUPPORTED_COMPRESSION.intersection(document.get("extensionsUsed", []) + document.get("extensionsRequired", []))
    if unsupported:
        raise ValueError(f"Unsupported import compression: {sorted(unsupported)}")
    for resource in document.get("buffers", []) + document.get("images", []):
        uri = resource.get("uri", "")
        if uri and not uri.startswith("data:"):
            raise ValueError(f"External resource in prepared GLB: {uri}")
    return document


def expected_catalog_ids():
    catalog = (ROOT / "Source/DelhiDrive/CarCatalog.cpp").read_text(encoding="utf-8")
    return set(re.findall(r'\{\s*TEXT\("((?:lux|sup)_[a-z0-9_]+)"\)', catalog))


def glb_bounds(document):
    low, high = [math.inf] * 3, [-math.inf] * 3
    for primitive in document["meshes"][0]["primitives"]:
        accessor = document["accessors"][primitive["attributes"]["POSITION"]]
        for i in range(3):
            low[i] = min(low[i], float(accessor["min"][i]))
            high[i] = max(high[i], float(accessor["max"][i]))
    return low, high


def validate_manifest(manifest_path=DEFAULT_MANIFEST):
    """Host-safe validation; returns [(entry, absolute prepared GLB path)]."""
    manifest_path = Path(manifest_path).resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1 or not isinstance(manifest.get("assets"), list):
        raise ValueError("Expected prepared manifest schema_version 1 with assets[]")
    entries, seen = [], set()
    for entry in manifest["assets"]:
        identifier = entry.get("id", "")
        if not re.fullmatch(r"(?:lux|sup)_[a-z0-9_]+", identifier) or identifier in seen:
            raise ValueError(f"Invalid/duplicate catalog model id: {identifier}")
        seen.add(identifier)
        mesh_name = "SM_" + identifier
        expected_path = f"/Game/Cars/Models/{mesh_name}.{mesh_name}"
        if entry.get("asset_path") != expected_path or entry.get("mesh_name") != mesh_name:
            raise ValueError(f"Wrong runtime path for {identifier}: expected {expected_path}")
        source = Path(entry["file"])
        path = (manifest_path.parent / source).resolve()
        if source.is_absolute() or not path.is_relative_to(manifest_path.parent) or path.suffix != ".glb":
            raise ValueError(f"Invalid local prepared source path: {source}")
        data = path.read_bytes()
        if len(data) != entry["bytes"] or hashlib.sha256(data).hexdigest() != entry["sha256"]:
            raise ValueError(f"Prepared asset integrity mismatch: {identifier}")
        document = read_glb_document(path)
        if len(document.get("meshes", [])) != 1 or len(document.get("nodes", [])) != 1 or document.get("skins"):
            raise ValueError(f"{identifier}: expected one baked static mesh and one identity node")
        node = document["nodes"][0]
        if node.get("mesh") != 0 or any(k in node for k in ("matrix", "translation", "rotation", "scale")):
            raise ValueError(f"{identifier}: scene transforms must be baked into vertices")
        low, high = glb_bounds(document)
        size = [high[i] - low[i] for i in range(3)]
        if not all(math.isfinite(v) for v in low + high):
            raise ValueError(f"{identifier}: nonfinite bounds")
        # Prepared glTF deliberately retains +X car-forward and uses glTF +Y-up.
        if abs(size[0] - float(entry["length_m"])) > .015 or not 1.4 < size[2] < 2.8 or not .8 < size[1] < 2.8:
            raise ValueError(f"{identifier}: wrong prepared axes/size {size}; expected X length {entry['length_m']} m")
        if abs(low[1]) > .015 or abs(low[0] + high[0]) > .025 or abs(low[2] + high[2]) > .025:
            raise ValueError(f"{identifier}: model must be centered horizontally and grounded at Y=0")
        if not 0 < int(entry["triangles"]) <= 60000:
            raise ValueError(f"{identifier}: prepared model exceeds mobile triangle budget")
        entries.append((entry, path))
    expected = expected_catalog_ids()
    if not expected or seen != expected:
        raise ValueError(f"Prepared models do not match catalog; missing={sorted(expected-seen)}, extra={sorted(seen-expected)}")
    return entries


def verify_mesh(mesh, entry, unreal):
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"Expected a StaticMesh at {entry['asset_path']}")
    bounds = mesh.get_bounds()
    origin, extent = bounds.origin, bounds.box_extent
    size = [float(extent.x * 2), float(extent.y * 2), float(extent.z * 2)]
    expected_length = float(entry["length_m"]) * 100
    if abs(size[0] - expected_length) > max(3, expected_length * .025) or not 140 < size[1] < 280 or not 80 < size[2] < 280:
        raise RuntimeError(f"Unreal import axes/units failed for {entry['id']}: {size} cm; expected +X length {expected_length} cm")
    if abs(origin.x) > 3 or abs(origin.y) > 3 or abs(origin.z - extent.z) > 3:
        raise RuntimeError(f"Unreal import origin is not centered/grounded for {entry['id']}")
    return {"size_cm": size, "origin_cm": [origin.x, origin.y, origin.z], "axis_unit_check": "passed"}


def optimize_mesh(mesh, unreal):
    """Optional engine LODs supplement the already bounded source mesh complexity."""
    notes = []
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    try:
        options = unreal.EditorScriptingMeshReductionOptions()
        options.set_editor_property("auto_compute_lod_screen_size", False)
        settings = []
        for fraction, screen in ((1.0, 1.0), (.50, .35), (.20, .12)):
            setting = unreal.EditorScriptingMeshReductionSettings()
            setting.set_editor_property("percent_triangles", fraction)
            setting.set_editor_property("screen_size", screen)
            settings.append(setting)
        options.set_editor_property("reduction_settings", settings)
        count = subsystem.set_lods(mesh, options)
        notes.append(f"Unreal generated {count} LODs")
    except Exception as exc:
        notes.append("Engine LOD generation unavailable; bounded source LOD remains usable: " + str(exc))
    try:
        subsystem.remove_collisions(mesh)
    except Exception as exc:
        notes.append("Collision cleanup note: " + str(exc))
    try:
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", nanite)
    except Exception as exc:
        notes.append("Nanite setting note: " + str(exc))
    return notes


def import_vehicle(entry, path, replace_existing, previous, unreal):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    target = entry["asset_path"]
    target_package = target.split(".")[0]
    old = assets.load_asset(target) if assets.does_asset_exist(target) else None
    if old and not replace_existing:
        if previous.get("source_sha256") != entry["sha256"]:
            raise RuntimeError(f"{target} exists without a matching source hash. Reimport intentionally with --replace-existing.")
        checked = verify_mesh(old, entry, unreal)
        return {"status": "preserved_verified", **checked}
    # Per-car staging avoids material/texture name collisions between artists.
    destination = f"/Game/Cars/GeneratedSources/{entry['id']}_{entry['sha256'][:10]}"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(path))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", entry["mesh_name"])
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    objects = list(task.get_objects())
    meshes = [obj for obj in objects if isinstance(obj, unreal.StaticMesh)]
    if len(meshes) != 1:
        raise RuntimeError(f"Expected exactly one imported StaticMesh for {entry['id']}; got {len(meshes)}. Check Interchange importer logs.")
    mesh = meshes[0]
    checked = verify_mesh(mesh, entry, unreal)
    notes = optimize_mesh(mesh, unreal)
    if old:
        if not assets.delete_asset(target):
            raise RuntimeError("Could not replace prior generated target: " + target)
    if not assets.rename_asset(mesh.get_path_name(), target_package):
        raise RuntimeError("Could not assign stable runtime model path: " + target)
    saved = assets.load_asset(target)
    checked = verify_mesh(saved, entry, unreal)
    if not assets.save_loaded_asset(saved, only_if_is_dirty=False):
        raise RuntimeError("Could not save imported model: " + target)
    if not assets.save_directory(destination, only_if_is_dirty=True, recursive=True):
        raise RuntimeError("Could not save model material dependencies: " + destination)
    return {"status": "imported", **checked, "notes": notes}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument("--replace-existing", action="store_true")
    args = parser.parse_args(argv)
    try:
        entries = validate_manifest(args.manifest)
    except Exception as exc:
        if not args.validate_only:
            REPORT.parent.mkdir(parents=True, exist_ok=True)
            REPORT.write_text(json.dumps({"schema_version": 2, "success": False, "assets": [], "error": str(exc)}, indent=2) + "\n")
        raise
    if args.validate_only:
        for entry, path in entries:
            print(f"VALID PREPARED MODEL: {entry['id']} | {entry['triangles']} triangles | {path.name}")
        print("Prepared source validation passed. This does not test Unreal import or gameplay.")
        return 0
    import unreal
    previous = {}
    if REPORT.exists():
        try:
            previous = {item["id"]: item for item in json.loads(REPORT.read_text()).get("assets", [])}
        except (OSError, ValueError, KeyError):
            pass
    # Write a failure report first so crashes cannot leave a stale success marker.
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    results = []
    report = {"schema_version": 2, "success": False, "assets": results}
    REPORT.write_text(json.dumps(report, indent=2) + "\n")
    try:
        for entry, path in entries:
            result = {"id": entry["id"], "asset_path": entry["asset_path"], "source_sha256": entry["sha256"]}
            try:
                result.update(import_vehicle(entry, path, args.replace_existing, previous.get(entry["id"], {}), unreal))
            except Exception as exc:
                result.update(status="failed", error=str(exc))
                unreal.log_error(f"Vehicle import failed: {entry['id']}: {exc}")
            results.append(result)
            REPORT.write_text(json.dumps(report, indent=2) + "\n")
        report["success"] = len(results) == len(entries) and all(r["status"] in ("imported", "preserved_verified") for r in results)
        REPORT.write_text(json.dumps(report, indent=2) + "\n")
        if not report["success"]:
            raise RuntimeError("Vehicle import failed; see Saved/VehicleAssetImport.json")
        unreal.log("All twelve native-physics vehicle visuals imported and verified.")
        return 0
    finally:
        REPORT.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
