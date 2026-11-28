#!/usr/bin/env python3
"""Build the UE editor target, import runtime assets, then create both city maps."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SETUP_SCRIPTS = ("import_vehicle_assets.py", "import_environment.py", "create_map_in_editor.py")


def run_editor_task(editor, project, name):
    marker = ROOT / "Saved/Setup" / (Path(name).stem + ".json")
    marker.unlink(missing_ok=True)
    environment = dict(os.environ, DELHIDRIVE_SETUP_SCRIPT=name)
    subprocess.run([
        str(editor), str(project), "-run=pythonscript",
        f"-script={ROOT / 'Tools/run_editor_setup.py'}", "-unattended",
        "-nop4", "-nosplash", "-NullRHI", "-stdout", "-FullStdOutLogOutput",
    ], check=True, cwd=ROOT, env=environment)
    if not marker.is_file() or json.loads(marker.read_text()).get("success") is not True:
        raise RuntimeError(f"Unreal task did not complete successfully: {name}; inspect Saved/Setup and the Unreal log")


def required_imported_assets():
    manifest = json.loads((ROOT / "SourceAssets/Vehicles/prepared_manifest.json").read_text())
    paths = [ROOT / "Content" / (entry["asset_path"].split(".", 1)[0][len("/Game/"):] + ".uasset")
             for entry in manifest["assets"]]
    paths.extend(ROOT / "Content/Environment" / ("M_" + name + ".uasset")
                 for name in ("Asphalt", "Ground", "Facade", "Roof"))
    return paths


def editor_executable(engine, host):
    binaries = engine / "Engine/Binaries" / host
    if host == "Win64":
        return binaries / "UnrealEditor-Cmd.exe"
    if host == "Mac":
        return binaries / "UnrealEditor.app/Contents/MacOS/UnrealEditor"
    commandlet = binaries / "UnrealEditor-Cmd"
    return commandlet if commandlet.is_file() else binaries / "UnrealEditor"


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
    if not args.ue_root:
        print("ERROR: set UE_ROOT to an installed Unreal Engine 5.5 directory.", file=sys.stderr)
        return 1
    engine = Path(args.ue_root)
    host = "Win64" if sys.platform == "win32" else "Mac" if sys.platform == "darwin" else "Linux"
    batch = engine / "Engine/Build/BatchFiles"
    build = batch / "Build.bat" if host == "Win64" else batch / host / "Build.sh"
    editor = editor_executable(engine, host)
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
        for name in SETUP_SCRIPTS:
            run_editor_task(editor, project, name)
        vehicle_report = json.loads((ROOT / "Saved/VehicleAssetImport.json").read_text())
        if vehicle_report.get("schema_version") != 2 or vehicle_report.get("success") is not True:
            raise RuntimeError("Vehicle importer did not verify all prepared runtime meshes")
        missing_assets = [str(path.relative_to(ROOT)) for path in required_imported_assets()
                          if not path.is_file() or path.stat().st_size == 0]
        if missing_assets:
            raise RuntimeError("Imported runtime assets missing: " + ", ".join(missing_assets))
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: editor asset/map setup failed: {exc}", file=sys.stderr)
        return 1
    missing_maps = [str(path) for path in target_maps if not path.is_file() or path.stat().st_size == 0]
    if missing_maps:
        print("ERROR: Unreal did not save the expected nonempty city maps: " + ", ".join(missing_maps), file=sys.stderr)
        return 1
    print("City maps present:", ", ".join(str(path) for path in target_maps))
    print("Prepared vehicle meshes and four environment materials are present.")
    print("Open both maps in Unreal Editor to validate lighting, roads and native vehicle handling.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
