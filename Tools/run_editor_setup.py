"""Unreal Python commandlet wrapper: mark success only after a setup script completes."""
import json
import os
from pathlib import Path
import runpy
import sys
import traceback


ROOT = Path(__file__).resolve().parents[1]
ALLOWED = {"import_vehicle_assets.py", "import_environment.py", "create_map_in_editor.py"}
name = os.environ.get("DELHIDRIVE_SETUP_SCRIPT", "")
if name not in ALLOWED:
    raise RuntimeError("Unknown DelhiDrive editor setup task")
result = ROOT / "Saved/Setup" / (Path(name).stem + ".json")
result.parent.mkdir(parents=True, exist_ok=True)
script = ROOT / "Tools" / name
try:
    # PythonScriptCommandlet may populate argv with engine flags. Import scripts
    # receive only their own filename, exactly as an unattended no-argument run.
    sys.argv = [str(script)]
    runpy.run_path(str(script), run_name="__main__")
except BaseException:
    result.write_text(json.dumps({"success": False, "script": name,
                                  "error": traceback.format_exc()}, indent=2), encoding="utf-8")
    raise
else:
    result.write_text(json.dumps({"success": True, "script": name}, indent=2), encoding="utf-8")
