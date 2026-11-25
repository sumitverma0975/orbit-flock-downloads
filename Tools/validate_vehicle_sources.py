#!/usr/bin/env python3
"""Check included GLB bytes/provenance; this does not test Unreal import or rigging."""
import hashlib
import json
from pathlib import Path
import struct
import sys

BASE = Path(__file__).resolve().parents[1] / "SourceAssets/Vehicles"


def validate_file(filename, expected_sha, expected_bytes=None):
    path = (BASE / filename).resolve()
    if not path.is_relative_to(BASE.resolve()):
        raise ValueError("Manifest file escapes vehicle source directory")
    data = path.read_bytes()
    if expected_bytes is not None and len(data) != expected_bytes:
        raise ValueError(f"{filename}: unexpected byte count")
    if hashlib.sha256(data).hexdigest() != expected_sha:
        raise ValueError(f"{filename}: SHA256 mismatch")
    if len(data) < 20:
        raise ValueError(f"{filename}: truncated GLB")
    magic, version, length = struct.unpack_from("<4sII", data)
    if magic != b"glTF" or version != 2 or length != len(data):
        raise ValueError(f"{filename}: invalid GLB header")
    json_length, chunk_type = struct.unpack_from("<II", data, 12)
    if chunk_type != 0x4E4F534A or 20 + json_length > len(data):
        raise ValueError(f"{filename}: missing/truncated GLB JSON")
    model = json.loads(data[20:20 + json_length])
    if not model.get("meshes") or not model.get("scenes"):
        raise ValueError(f"{filename}: no model scene or geometry")
    for resource in model.get("buffers", []) + model.get("images", []):
        uri = resource.get("uri", "")
        if uri and not uri.startswith("data:"):
            raise ValueError(f"{filename}: missing bundled resource {uri}")
    return len(data)


def main():
    try:
        manifest = json.loads((BASE / "asset_manifest.json").read_text(encoding="utf-8"))
        assets = manifest["assets"]
        if len(assets) < 3 or len({a["id"] for a in assets}) != len(assets):
            raise ValueError("Expected at least three uniquely named vehicle source models")
        for asset in assets:
            size = validate_file(asset["source_file"], asset["sha256"], asset["bytes"])
            validate_file(asset["source_original_file"], asset["source_sha256"])
            for field in ("author", "original_url", "license_url", "provenance_file"):
                if not asset.get(field):
                    raise ValueError(f"{asset['id']}: attribution missing {field}")
            if not (BASE / asset["provenance_file"]).is_file():
                raise ValueError(f"{asset['id']}: provenance snapshot missing")
            print(f"PASS: {asset['display_name']}: GLB, SHA256, bundled resources, attribution ({size:,} bytes)")
        from import_vehicle_assets import validate_manifest
        prepared = validate_manifest()
        for entry, path in prepared:
            print(f"PASS: prepared {entry['id']}: catalog path, axes, origin, scale, SHA256, {entry['triangles']:,} triangles")
        print("Original and prepared source integrity passed. Unreal import and native-physics runtime remain untested.")
        return 0
    except (OSError, ValueError, KeyError, struct.error) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
