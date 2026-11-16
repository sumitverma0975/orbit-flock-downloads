#!/usr/bin/env bash
# Requires UE 5.5 with Android support, configured SDK/NDK and the missing game assets.
# Usage: UE_ROOT="/path/to/UE_5.5" bash Tools/build_apk.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$HERE/../DelhiDrive.uproject"
OUT="${DELHIDRIVE_BUILD_OUTPUT:-$HERE/../Artifacts/Android-$(date -u +%Y%m%dT%H%M%SZ)}"

if [[ -n "${UE_ROOT:-}" ]]; then
  python3 "$HERE/setup_map.py" --ue-root "$UE_ROOT"
fi
python3 "$HERE/preflight.py" --ue-root "${UE_ROOT:-}"
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" Turnkey -command=VerifySdk -platform=Android -unattended

"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJ" -noP4 -utf8output \
  -platform=Android -cookflavor=ASTC -clientconfig=Shipping -unattended \
  -build -cook -stage -pak -compressed -package -archive -archivedirectory="$OUT"

python3 "$HERE/preflight.py" --check-apks "$OUT"
