"""Stage the signed, self-contained UE Shipping APK for a direct file download."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

from verify_android_package import MAX_DELIVERY_BYTES, inspect_unreal_apk


def stage(archive, destination):
    report_path = archive / "Android-verification.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    apks = report["apks"]
    if len(apks) != 1 or not apks[0]["signature_verified"]:
        raise ValueError("Expected a single signed game APK in the build report")
    entry = apks[0]
    source = (archive / entry["path"]).resolve()
    if source.suffix.lower() != ".apk" or not source.is_relative_to(archive.resolve()):
        raise ValueError("Invalid APK path in the build report")
    if report["delivery_files"] != [entry["path"]]:
        raise ValueError("Direct download requires the game data to be packaged inside the APK")
    size = source.stat().st_size
    if not 0 < size <= MAX_DELIVERY_BYTES or size != entry["bytes"] or size != report["total_delivery_bytes"]:
        raise ValueError("APK size disagrees with verified delivery or exceeds the 3 GB limit")
    inspect_unreal_apk(source, external_payload=False)
    destination.mkdir(parents=True, exist_ok=False)
    try:
        digest = hashlib.sha256()
        with source.open("rb") as src, (destination / "DelhiDrive.apk").open("xb") as dst:
            for block in iter(lambda: src.read(1024 * 1024), b""):
                digest.update(block)
                dst.write(block)
        if digest.hexdigest() != entry["sha256"]:
            raise ValueError("APK checksum changed after signing verification")
        (destination / "SHA256SUMS").write_text(digest.hexdigest() + "  DelhiDrive.apk\n", encoding="ascii")
        shutil.copyfile(report_path, destination / report_path.name)
    except BaseException:
        shutil.rmtree(destination)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        stage(args.archive, args.destination)
    except (OSError, ValueError, KeyError) as error:
        sys.exit(str(error))
