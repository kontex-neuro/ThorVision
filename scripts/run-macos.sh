#!/usr/bin/env bash
# Checks and starts a ThorVision build produced by scripts/build-macos.sh.
# Copied beside the build as run-thorvision.sh. Pass --launch to open the application.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
receipt="$here/build-receipt.json"
[[ -f "$receipt" ]] || { echo "No build-receipt.json beside this script" >&2; exit 1; }
field() { python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))[sys.argv[2]])' "$receipt" "$1"; }

app="$here/app/ThorVision.app"
plugin="$app/Contents/PlugIns/gstreamer/libgstqml6.dylib"
brew_prefix=$(field brewPrefix)
[[ "$(shasum -a 256 "$app/Contents/MacOS/ThorVision" | awk '{print $1}')" == "$(field executableSha256)" ]] \
    || { echo "ThorVision executable differs from the build receipt" >&2; exit 1; }
[[ "$(shasum -a 256 "$plugin" | awk '{print $1}')" == "$(field gstreamerQtPluginSha256)" ]] \
    || { echo "The Qt GStreamer sink differs from the build receipt" >&2; exit 1; }

export DYLD_FALLBACK_LIBRARY_PATH="$brew_prefix/lib"
export THORVISION_NODE="${THORVISION_NODE:-$(command -v node)}"
GST_PLUGIN_PATH="$(dirname "$plugin")" "$brew_prefix/bin/gst-inspect-1.0" qml6glsink >/dev/null \
    || { echo "The Qt GStreamer sink cannot load" >&2; exit 1; }

if [[ "${1:-}" != "--launch" ]]; then
    echo "Native runtime and Qt sink are available. Pass --launch to open ThorVision."
    exit 0
fi
exec "$app/Contents/MacOS/ThorVision" "${@:2}"
