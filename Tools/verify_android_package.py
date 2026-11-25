#!/usr/bin/env python3
"""Verify signed Unreal Android delivery files; never substitutes for device tests."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zipfile


MAX_DELIVERY_BYTES = 3_000_000_000
PAYLOAD_SUFFIXES = {".apk", ".obb", ".pak", ".utoc", ".ucas", ".aab"}
PAK_MAGIC = b"\xe1\x12\x6f\x5a"


def collect_delivery(directory):
    directory = Path(directory).resolve()
    files = sorted(path for path in directory.rglob("*")
                   if path.is_file() and path.suffix.lower() in PAYLOAD_SUFFIXES)
    apks = [path for path in files if path.suffix.lower() == ".apk"]
    if not apks:
        raise ValueError(f"No APK produced under {directory}")
    total = sum(path.stat().st_size for path in files)
    if total > MAX_DELIVERY_BYTES:
        raise ValueError(f"Combined APK/OBB/game payload is {total:,} bytes; limit is {MAX_DELIVERY_BYTES:,} bytes")
    return directory, files, apks, total


def has_pak_footer(stream, size):
    # FPakInfo includes its magic, version, index offset/size and SHA1 near EOF.
    # The exact footer size varies across supported Unreal pak versions.
    if size < 44:
        return False
    stream.seek(max(0, size - 4096))
    tail = stream.read()
    offset = 0
    while True:
        offset = tail.find(PAK_MAGIC, offset)
        if offset < 0:
            return False
        if offset + 44 <= len(tail):
            version, index_offset, index_size = struct.unpack_from("<Iqq", tail, offset + 4)
            if 1 <= version <= 12 and index_offset >= 0 and index_size > 0 and index_offset + index_size < size:
                return True
        offset += 1


def archive_has_cooked_payload(archive, depth=0):
    names = set(archive.namelist())
    for name in names:
        info = archive.getinfo(name)
        if name.lower().endswith(".pak"):
            with archive.open(name) as stream:
                if has_pak_footer(stream, info.file_size):
                    return True
        # IoStore is supported for an explicitly changed UE packaging profile.
        if name.lower().endswith(".utoc") and name[:-5] + ".ucas" in names:
            with archive.open(name) as stream:
                magic = stream.read(16)
            if magic.startswith(b"-==--==--==--==-") and archive.getinfo(name[:-5] + ".ucas").file_size > 0:
                return True
        # UE embeds expansion data as assets/main.obb.png when all data is in
        # the APK. It may be a zip containing paks or directly contain a pak.
        if depth == 0 and name.lower().endswith((".obb", ".obb.png")):
            with archive.open(name) as stream:
                if has_pak_footer(stream, info.file_size):
                    return True
                stream.seek(0)
                try:
                    with zipfile.ZipFile(stream) as expansion:
                        if archive_has_cooked_payload(expansion, depth=1):
                            return True
                except zipfile.BadZipFile:
                    pass
    return False


def external_payload_valid(files):
    for path in files:
        if path.suffix.lower() not in {".obb", ".pak"}:
            continue
        with path.open("rb") as stream:
            if has_pak_footer(stream, path.stat().st_size):
                return True
            stream.seek(0)
            try:
                with zipfile.ZipFile(stream) as archive:
                    if archive_has_cooked_payload(archive):
                        return True
            except zipfile.BadZipFile:
                pass
    for toc in files:
        if toc.suffix.lower() == ".utoc" and toc.with_suffix(".ucas") in files:
            with toc.open("rb") as stream:
                if stream.read(16).startswith(b"-==--==--==--==-") and toc.with_suffix(".ucas").stat().st_size > 0:
                    return True
    return False


def inspect_unreal_apk(path, external_payload=False):
    with zipfile.ZipFile(path) as archive:
        names = set(archive.namelist())
        required = {"AndroidManifest.xml", "classes.dex", "lib/arm64-v8a/libUnreal.so"}
        missing = required - names
        if missing:
            raise ValueError(f"{path.name}: missing actual UE5 ARM64 package entries: {', '.join(sorted(missing))}")
        if any(name.startswith("lib/") and name.endswith("libgodot_android.so") for name in names):
            raise ValueError(f"{path.name}: contains a Godot engine library, not the requested Unreal build")
        library = archive.getinfo("lib/arm64-v8a/libUnreal.so")
        with archive.open(library) as stream:
            header = stream.read(64)
        if (len(header) != 64 or header[:6] != b"\x7fELF\x02\x01" or
                struct.unpack_from("<H", header, 18)[0] != 183 or library.file_size < 1_000_000):
            raise ValueError(f"{path.name}: libUnreal.so is not a plausible ARM64 ELF engine library")
        with archive.open("classes.dex") as stream:
            if not stream.read(8).startswith(b"dex\n"):
                raise ValueError(f"{path.name}: executable dex header is invalid")
        with archive.open("AndroidManifest.xml") as stream:
            if stream.read(4) != b"\x03\x00\x08\x00":
                raise ValueError(f"{path.name}: compiled Android manifest is invalid")
        if not archive_has_cooked_payload(archive) and not external_payload:
            raise ValueError(f"{path.name}: no Unreal pak/IoStore game payload found in the APK or delivery")
        bad_file = archive.testzip()
        if bad_file:
            raise ValueError(f"{path.name}: corrupt APK entry: {bad_file}")


def find_apksigner(explicit=None):
    if explicit:
        path = Path(explicit).expanduser()
        if not path.is_file():
            raise ValueError(f"apksigner not found: {path}")
        return path
    located = shutil.which("apksigner") or shutil.which("apksigner.bat")
    if located:
        return Path(located)
    candidates = []
    sdk_roots = [Path.home() / "Android/Sdk", Path.home() / "Library/Android/sdk"]
    if os.environ.get("LOCALAPPDATA"):
        sdk_roots.append(Path(os.environ["LOCALAPPDATA"]) / "Android/Sdk")
    for variable in ("ANDROID_HOME", "ANDROID_SDK_ROOT"):
        if os.environ.get(variable):
            sdk_roots.append(Path(os.environ[variable]))
    for sdk_root in sdk_roots:
        candidates.extend((sdk_root / "build-tools").glob("*/apksigner.bat" if os.name == "nt" else "*/apksigner"))
    if candidates:
        return max(candidates, key=lambda path: tuple(int(value) for value in re.findall(r"\d+", path.parent.name)))
    raise ValueError("apksigner is unavailable. Set ANDROID_HOME/ANDROID_SDK_ROOT or pass --apksigner; signature verification is required.")


def verify_delivery(directory, apksigner=None):
    directory, files, apks, total = collect_delivery(directory)
    external = external_payload_valid(files)
    for path in apks:
        inspect_unreal_apk(path, external_payload=external)
    signer = find_apksigner(apksigner)
    results = []
    for path in apks:
        result = subprocess.run([str(signer), "verify", "--verbose", "--print-certs", str(path)],
                                capture_output=True, text=True)
        if result.returncode:
            raise ValueError(f"{path.name}: Android APK signature verification failed:\n{result.stdout}{result.stderr}")
        hasher = hashlib.sha256()
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                hasher.update(block)
        results.append({"path": path.relative_to(directory).as_posix(), "bytes": path.stat().st_size,
                        "sha256": hasher.hexdigest(), "signature_verified": True,
                        "engine_payload": "Unreal Engine 5 ARM64 with cooked game data"})
    report = {"maximum_delivery_bytes": MAX_DELIVERY_BYTES, "total_delivery_bytes": total,
              "delivery_files": [path.relative_to(directory).as_posix() for path in files], "apks": results,
              "scope": "Package size, ZIP/ELF/cooked-payload structure and APK cryptographic signature. Android gameplay/performance remain separate tests."}
    (directory / "Android-verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--apksigner")
    args = parser.parse_args()
    try:
        report = verify_delivery(args.directory, args.apksigner)
        print(json.dumps(report, indent=2))
        return 0
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print("ERROR: " + str(exc))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
