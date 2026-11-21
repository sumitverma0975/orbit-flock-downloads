#!/usr/bin/env python3
"""Verify the signed offline release APK and write a public checksum/report."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--build-tools", required=True, type=Path)
    args = parser.parse_args()
    apk = args.apk.resolve()
    build_tools = args.build_tools.resolve()
    signature = subprocess.check_output(
        [str(build_tools / "apksigner"), "verify", "--verbose", "--print-certs", str(apk)], text=True)
    subprocess.run([str(build_tools / "zipalign"), "-c", "-P", "16", "4", str(apk)], check=True)
    badging = subprocess.check_output([str(build_tools / "aapt"), "dump", "badging", str(apk)], text=True)
    permissions = subprocess.check_output([str(build_tools / "aapt"), "dump", "permissions", str(apk)], text=True)
    manifest = subprocess.check_output(
        [str(build_tools / "aapt"), "dump", "xmltree", str(apk), "AndroidManifest.xml"], text=True)
    if "name='com.sumit.delhidrive.mobile'" not in badging:
        raise ValueError("Unexpected Android package name")
    if "application-debuggable" in badging:
        raise ValueError("Release APK must not be debuggable")
    if "android.intent.category.LAUNCHER" not in manifest:
        raise ValueError("APK has no launcher entry")
    forbidden = ("android.permission.INTERNET", "android.permission.RECORD_AUDIO",
                 "android.permission.READ_EXTERNAL_STORAGE", "android.permission.WRITE_EXTERNAL_STORAGE")
    if any(permission in permissions for permission in forbidden):
        raise ValueError("Offline game unexpectedly requests network, microphone, or storage access")
    with zipfile.ZipFile(apk) as archive:
        names = archive.namelist()
        if "AndroidManifest.xml" not in names or "classes.dex" not in names:
            raise ValueError("APK is missing its manifest or executable code")
        for abi in ("arm64-v8a", "armeabi-v7a"):
            if f"lib/{abi}/libgodot_android.so" not in names:
                raise ValueError("Missing native game engine library for " + abi)
        if not any(name.startswith("assets/") and name.endswith("project.binary") for name in names):
            raise ValueError("APK does not contain the exported game project")
        if any(name.endswith((".keystore", ".jks")) or "private-signing" in name for name in names):
            raise ValueError("APK contains a private signing file")
        if any("/SourceAssets/" in name for name in names):
            raise ValueError("APK contains duplicate raw model sources that should be excluded")
        bad_file = archive.testzip()
        if bad_file:
            raise ValueError("Corrupt ZIP entry: " + bad_file)
    digest = hashlib.sha256(apk.read_bytes()).hexdigest()
    report = {
        "filename": apk.name,
        "bytes": apk.stat().st_size,
        "sha256": digest,
        "package": "com.sumit.delhidrive.mobile",
        "architectures": ["arm64-v8a", "armeabi-v7a"],
        "min_sdk": re.search(r"sdkVersion:'(\d+)'", badging).group(1),
        "target_sdk": re.search(r"targetSdkVersion:'(\d+)'", badging).group(1),
        "signature_verified": True,
        "alignment_16kb_verified": True,
        "signing_certificate_sha256": re.search(r"certificate SHA-256 digest: ([0-9a-f]+)", signature).group(1),
        "requested_permissions": [line for line in permissions.splitlines() if line.startswith("uses-permission")],
        "verification_scope": "Signature, package metadata and archive integrity. Device gameplay testing is separate.",
    }
    apk.with_suffix(apk.suffix + ".sha256").write_text(f"{digest}  {apk.name}\n", encoding="utf-8")
    apk.with_suffix(apk.suffix + ".verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
