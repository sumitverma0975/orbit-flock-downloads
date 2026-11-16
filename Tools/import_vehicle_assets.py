#!/usr/bin/env python3
"""Validate sources, or import complete static vehicle scenes in UE 5.5 Editor.

System Python: python Tools/import_vehicle_assets.py --validate-only
Unreal Editor: Tools > Execute Python Script > Tools/import_vehicle_assets.py
No skeleton, wheel bones, physics asset or Chaos vehicle Blueprint is generated.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "SourceAssets/Vehicles/asset_manifest.json"
UNSUPPORTED_COMPRESSION = {"EXT_meshopt_compression", "KHR_draco_mesh_compression"}


def read_glb_document(path):
    """Read the GLB JSON chunk without requiring Blender or Unreal."""
    with path.open("rb") as stream:
        header = stream.read(12)
        if len(header) != 12:
            raise ValueError(f"Truncated GLB header: {path.name}")
        magic, version, length = struct.unpack("<4sII", header)
        if magic != b"glTF" or version != 2 or length != path.stat().st_size:
            raise ValueError(f"Invalid GLB 2.0 header or file length: {path.name}")
        chunk_header = stream.read(8)
        if len(chunk_header) != 8:
            raise ValueError(f"Missing GLB JSON chunk: {path.name}")
        chunk_length, chunk_type = struct.unpack("<I4s", chunk_header)
        if chunk_type != b"JSON" or chunk_length > length - 20:
            raise ValueError(f"Invalid GLB JSON chunk: {path.name}")
        document = json.loads(stream.read(chunk_length).decode("utf-8").rstrip("\x00 \t\r\n"))
    unsupported = UNSUPPORTED_COMPRESSION.intersection(
        document.get("extensionsUsed", []) + document.get("extensionsRequired", []))
    if unsupported:
        raise ValueError(f"{path.name} needs decompression before Unreal import: {', '.join(sorted(unsupported))}")
    if not document.get("meshes"):
        raise ValueError(f"GLB contains no mesh geometry: {path.name}")
    return document


def validate_manifest(manifest_path=DEFAULT_MANIFEST):
    """Validate local geometry and its provenance before importing any assets."""
    manifest_path = Path(manifest_path).resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1 or not isinstance(manifest.get("assets"), list):
        raise ValueError("Vehicle manifest must use schema_version 1 and an assets array.")
    if not manifest["assets"]:
        raise ValueError("Vehicle manifest contains no assets.")
    seen = set()
    validated = []
    for entry in manifest["assets"]:
        asset_id = entry.get("id", "")
        if not re.fullmatch(r"[a-z][a-z0-9_]*", asset_id) or asset_id in seen:
            raise ValueError(f"Invalid or duplicate vehicle id: {asset_id!r}")
        seen.add(asset_id)
        expected_destination = "/Game/Cars/Imported/" + asset_id
        if entry.get("import_destination") != expected_destination:
            raise ValueError(f"{asset_id}: import_destination must be {expected_destination}")
        for key in ("display_name", "source_file", "original_url", "author", "license", "sha256"):
            if not entry.get(key):
                raise ValueError(f"{asset_id}: missing {key}")
        relative_source = Path(entry["source_file"])
        path = (manifest_path.parent / relative_source).resolve()
        if relative_source.is_absolute() or not path.is_relative_to(manifest_path.parent):
            raise ValueError(f"{asset_id}: source_file must stay within the manifest directory")
        if not path.is_file() or path.suffix.lower() != ".glb":
            raise ValueError(f"{asset_id}: missing GLB source: {relative_source}")
        hasher = hashlib.sha256()
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                hasher.update(block)
        digest = hasher.hexdigest()
        if digest != entry["sha256"]:
            raise ValueError(f"{asset_id}: source checksum mismatch; restore the file or update verified provenance")
        document = read_glb_document(path)
        if document.get("skins"):
            raise ValueError(f"{asset_id}: this importer expects static vehicle geometry, not a skinned mesh")
        validated.append((entry, path))
    return validated


def describe_objects(objects, unreal):
    return [
        {"path": obj.get_path_name(), "class": obj.get_class().get_name(),
         "static_mesh": isinstance(obj, unreal.StaticMesh)}
        for obj in objects if obj is not None
    ]


def import_vehicle(entry, path, replace_existing, unreal):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    destination = entry["import_destination"]
    existing_paths = assets.list_assets(destination, recursive=True, include_folder=False)
    if existing_paths and not replace_existing:
        objects = [assets.load_asset(object_path) for object_path in existing_paths]
        descriptions = describe_objects(objects, unreal)
        if not any(item["static_mesh"] for item in descriptions):
            raise RuntimeError(f"{destination} already contains assets but no static mesh; inspect the partial import")
        unreal.log_warning("Preserving existing vehicle assets: " + destination)
        return {"status": "preserved_existing", "objects": descriptions,
                "note": "Existing assets were not compared with the source. Use --replace-existing to reimport intentionally."}

    # These GLBs contain several mesh nodes and their local transforms. An
    # AssetImportTask only produces asset objects; it does not recreate their
    # scene hierarchy. Use Interchange's synchronous SCENE import into a new
    # preview world so offsets, rotations, scale and parent relationships survive.
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError("Save the current map before importing; this tool opens a separate preview world")
    preview_base = "/Game/VehicleImportPreviews/Preview_" + entry["id"]
    preview_map, _ = unreal.AssetToolsHelpers.get_asset_tools().create_unique_asset_name(preview_base, "")
    if not unreal.EditorLevelLibrary.new_level(preview_map):
        raise RuntimeError("Could not create import preview world " + preview_map)
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    source = unreal.InterchangeManager.create_source_data(str(path))
    parameters = unreal.ImportAssetParameters()
    parameters.set_editor_property("is_automated", True)
    parameters.set_editor_property("replace_existing", replace_existing)
    if not manager.import_scene(destination, source, parameters):
        raise RuntimeError(f"Interchange scene import failed for {entry['id']}; inspect the Unreal log")
    objects = [assets.load_asset(object_path)
               for object_path in assets.list_assets(destination, recursive=True, include_folder=False)]
    descriptions = describe_objects(objects, unreal)
    if not any(item["static_mesh"] for item in descriptions):
        raise RuntimeError(f"No StaticMesh imported for {entry['id']}; inspect the Unreal Interchange log")
    if not assets.save_directory(destination, only_if_is_dirty=True, recursive=True):
        raise RuntimeError("Could not save imported assets under " + destination)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    mesh_components = sum(len(actor.get_components_by_class(unreal.StaticMeshComponent))
                          for actor in actor_subsystem.get_all_level_actors())
    if mesh_components == 0:
        raise RuntimeError("Scene import produced assets but no static mesh components in the preview world")
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save vehicle hierarchy preview " + preview_map)
    return {"status": "imported", "objects": descriptions, "preview_map": preview_map,
            "scene_static_mesh_components": mesh_components}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument("--asset-id", action="append", help="Import only this id; may be repeated")
    parser.add_argument("--replace-existing", action="store_true", help="Intentionally reimport existing assets")
    args = parser.parse_args(argv)
    entries = validate_manifest(args.manifest)
    if args.asset_id:
        requested = set(args.asset_id)
        unknown = requested - {entry["id"] for entry, _ in entries}
        if unknown:
            raise ValueError("Unknown vehicle id(s): " + ", ".join(sorted(unknown)))
        entries = [(entry, path) for entry, path in entries if entry["id"] in requested]
    if args.validate_only:
        for entry, path in entries:
            print(f"VALID SOURCE: {entry['id']} | {path.name} | {entry['sha256']}")
        print("GLB structure and hashes validated. Unreal import and Chaos vehicle setup remain unverified.")
        return

    try:
        import unreal
    except ImportError as exc:
        raise RuntimeError("Run this script inside Unreal Editor 5.5, or use --validate-only with system Python.") from exc
    if not hasattr(unreal, "InterchangeManager"):
        raise RuntimeError("Enable Unreal's Interchange import support and restart the editor before importing GLB files.")
    results = []
    for entry, path in entries:
        result = {"id": entry["id"], "source_file": entry["source_file"],
                  "sha256": entry["sha256"], "destination": entry["import_destination"]}
        try:
            result.update(import_vehicle(entry, path, args.replace_existing, unreal))
        except Exception as exc:
            result.update(status="failed", error=str(exc))
            unreal.log_error(f"Vehicle import failed for {entry['id']}: {exc}")
        results.append(result)
    report = ROOT / "Saved/VehicleAssetImport.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps({"schema_version": 1, "static_geometry_only": True,
                                  "assets": results}, indent=2) + "\n", encoding="utf-8")
    unreal.log("Vehicle import report: " + str(report))
    if any(result["status"] == "failed" for result in results):
        raise RuntimeError("One or more vehicle imports failed; see Saved/VehicleAssetImport.json")
    unreal.log_warning("Static vehicle geometry imported. Rigging, wheel setup, physics and DriveVehicle Blueprints are still required.")


if __name__ == "__main__":
    main()
