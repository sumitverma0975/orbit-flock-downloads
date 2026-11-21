#!/usr/bin/env bash
set -euo pipefail

# Requires Godot 4.6.3 + matching Android export templates, JDK 17, and
# Android SDK build-tools 36. Configure SDK/JDK paths in Godot Editor Settings.
# Keep all signing credentials outside the project and source repository.
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
godot_bin="${GODOT_BIN:-godot}"
apk_path="${1:-${project_dir}/build/DelhiDrive-Mobile.apk}"
sdk_path="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
build_tools_version="${ANDROID_BUILD_TOOLS_VERSION:-36.0.0}"

for signing_var in GODOT_ANDROID_KEYSTORE_RELEASE_PATH GODOT_ANDROID_KEYSTORE_RELEASE_USER GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD; do
    if [[ -z "${!signing_var:-}" ]]; then
        printf 'Missing required signing environment variable: %s\n' "$signing_var" >&2
        exit 1
    fi
done
if [[ ! -f "$GODOT_ANDROID_KEYSTORE_RELEASE_PATH" ]]; then
    printf 'The configured release keystore does not exist.\n' >&2
    exit 1
fi
if [[ ! -f "${project_dir}/project.godot" || ! -f "${project_dir}/export_presets.cfg" ]]; then
    printf 'Run this script from the complete DelhiDriveMobile project.\n' >&2
    exit 1
fi
if [[ -z "$sdk_path" || ! -x "$sdk_path/build-tools/$build_tools_version/apksigner" ]]; then
    printf 'Set ANDROID_HOME to an SDK containing build-tools %s.\n' "$build_tools_version" >&2
    exit 1
fi
mkdir -p -- "$(dirname -- "$apk_path")"
"$godot_bin" --headless --editor --path "$project_dir" --import
"$godot_bin" --headless --path "$project_dir" --export-release Android "$apk_path"

"$sdk_path/build-tools/$build_tools_version/apksigner" verify --verbose "$apk_path"
python3 "$project_dir/tools/validate_apk.py" "$apk_path" --build-tools "$sdk_path/build-tools/$build_tools_version"
printf 'Built signed Android APK: %s\n' "$apk_path"
