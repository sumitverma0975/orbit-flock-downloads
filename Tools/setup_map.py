#!/usr/bin/env python3
"""Build the UE editor target, then create missing Delhi and Chandigarh .umaps."""
import argparse
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ue-root", default=os.environ.get("UE_ROOT"))
    parser.add_argument("--skip-editor-build", action="store_true", help="Use an already compiled DelhiDriveEditor module")
    args = parser.parse_args()
    target_maps = [ROOT / "Content/Maps" / name for name in ("Lvl_Delhi.umap", "Lvl_Chandigarh.umap")]
    for target_map in target_maps:
        if target_map.is_file() and target_map.stat().st_size == 0:
            print(f"ERROR: existing map is empty: {target_map}. Restore it before running setup.", file=sys.stderr)
            return 1
    if all(target_map.is_file() for target_map in target_maps):
        print("Preserving existing city maps:", ", ".join(str(path) for path in target_maps))
        return 0
    if not args.ue_root:
        print("ERROR: set UE_ROOT to an installed Unreal Engine 5.5 directory.", file=sys.stderr)
        return 1
    engine = Path(args.ue_root)
    host = "Win64" if sys.platform == "win32" else "Mac" if sys.platform == "darwin" else "Linux"
    batch = engine / "Engine/Build/BatchFiles"
    build = batch / "Build.bat" if host == "Win64" else batch / host / "Build.sh"
    binaries = engine / "Engine/Binaries" / host
    editor = binaries / ("UnrealEditor-Cmd.exe" if host == "Win64" else "UnrealEditor-Cmd")
    if host == "Mac":
        editor = binaries / "UnrealEditor.app/Contents/MacOS/UnrealEditor"
    if not editor.is_file() or (not args.skip_editor_build and not build.is_file()):
        print(f"ERROR: Unreal editor/toolchain unavailable under {engine}.", file=sys.stderr)
        return 1
    project = ROOT / "DelhiDrive.uproject"
    try:
        if not args.skip_editor_build:
            command = [str(build), "DelhiDriveEditor", host, "Development", f"-Project={project}", "-WaitMutex"]
            if host != "Win64":
                command.insert(0, "bash")
            subprocess.run(command, check=True, cwd=ROOT)
        subprocess.run([
            str(editor), str(project), "-run=pythonscript",
            f"-script={ROOT / 'Tools/create_map_in_editor.py'}", "-unattended",
            "-nop4", "-nosplash", "-NullRHI", "-stdout", "-FullStdOutLogOutput",
        ], check=True, cwd=ROOT)
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: map generation failed: {exc}", file=sys.stderr)
        return 1
    missing_maps = [str(path) for path in target_maps if not path.is_file() or path.stat().st_size == 0]
    if missing_maps:
        print("ERROR: Unreal did not save the expected nonempty city maps: " + ", ".join(missing_maps), file=sys.stderr)
        return 1
    print("City maps present:", ", ".join(str(path) for path in target_maps))
    print("Open both in Unreal Editor to validate lighting, roads and the configured vehicle.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
