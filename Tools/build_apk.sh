#!/usr/bin/env bash
# Requires UE 5.5 with Android support and its configured SDK/NDK.
# Usage: UE_ROOT="/path/to/UE_5.5" bash Tools/build_apk.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$HERE/../DelhiDrive.uproject"
OUT="${DELHIDRIVE_BUILD_OUTPUT:-$HERE/../Artifacts/Android-$(date -u +%Y%m%dT%H%M%SZ)}"

python3 "$HERE/preflight.py" --source-only
if [[ -z "${UE_ROOT:-}" ]]; then
  printf 'ERROR: UE_ROOT is not set. Install Unreal Engine 5.5 with Android support; no APK was built.\n' >&2
  exit 1
fi
if [[ ! -f "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" ]]; then
  printf 'ERROR: Unreal RunUAT.sh is missing under UE_ROOT; no APK was built.\n' >&2
  exit 1
fi
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" Turnkey -command=VerifySdk -platform=Android -unattended
python3 "$HERE/setup_map.py" --ue-root "$UE_ROOT"
python3 "$HERE/preflight.py" --ue-root "$UE_ROOT"

"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJ" -noP4 -utf8output \
  -platform=Android -cookflavor=ASTC -clientconfig=Shipping -unattended \
  -build -cook -stage -pak -compressed -package -archive -archivedirectory="$OUT"

python3 "$HERE/preflight.py" --check-apks "$OUT" --apksigner "${DELHIDRIVE_APKSIGNER:-}"
