#!/usr/bin/env python3
"""Package the Unreal source, raw vehicle models and road data without local secrets."""
import argparse
import hashlib
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EXCLUDED_DIRS = {
    ".git", ".vs", "__pycache__", "Binaries", "DerivedDataCache",
    "Intermediate", "Saved", "Artifacts", "node_modules",
}
EXCLUDED_SUFFIXES = {".pyc", ".apk", ".aab", ".keystore", ".jks", ".p12", ".pem", ".key"}
INCLUDED_TOP_LEVEL = {"Config", "Content", "Source", "SourceAssets", "Tools", "Tests", "Docs", ".github"}


def source_files():
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        relative = path.relative_to(ROOT)
        if any(part in EXCLUDED_DIRS for part in relative.parts):
            continue
        if path.suffix.lower() in EXCLUDED_SUFFIXES or path.name.startswith(".env"):
            continue
        if len(relative.parts) == 1 or relative.parts[0] in INCLUDED_TOP_LEVEL:
            yield path, relative


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    destination = args.output.resolve()
    if destination.is_relative_to(ROOT):
        parser.error("Place the output outside the source directory.")
    destination.parent.mkdir(parents=True, exist_ok=True)
    count = 0
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path, relative in source_files():
            info = zipfile.ZipInfo("DelhiDrive/" + relative.as_posix(), (2026, 11, 15, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o100755 if path.suffix == ".sh" else 0o100644) << 16
            archive.writestr(info, path.read_bytes())
            count += 1
    digest = hashlib.sha256(destination.read_bytes()).hexdigest()
    destination.with_suffix(destination.suffix + ".sha256").write_text(
        f"{digest}  {destination.name}\n", encoding="utf-8")
    print(f"Packaged {count} files: {destination} ({destination.stat().st_size:,} bytes)")
    print("SHA256:", digest)
    print("Source archive only. This command does not build an APK.")


if __name__ == "__main__":
    main()
