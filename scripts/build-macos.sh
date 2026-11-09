#!/usr/bin/env bash
# Development source build of the FrameLab desktop on macOS arm64.
#
# Mirrors scripts/build-windows.ps1: libxvc is compiled from the pinned commit in
# build-support/windows/sources.json, the native filename and ExtensionClient checks run,
# and the agent bridge is installed, built and tested. Qt and GStreamer come from Homebrew;
# the Qt GStreamer sink (qml6glsink) is not shipped by Homebrew or the official macOS
# framework, so it is compiled here from the gst-plugins-good release matching the
# installed GStreamer, with its published SHA-256 verified.
#
# Usage: scripts/build-macos.sh --libxvc-source DIR [--build-dir DIR] [--conan-home DIR]
#                               [--jobs N] [--skip-bridge]
# --conan-home reuses an existing Conan cache instead of <build-dir>/conan-cache.
set -euo pipefail

source_dir=$(cd "$(dirname "$0")/.." && pwd)
support="$source_dir/build-support/macos"
build_dir="$source_dir/build/macos"
libxvc_source=""
conan_home=""
jobs=8
skip_bridge=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --libxvc-source) libxvc_source=$2; shift 2 ;;
        --build-dir) build_dir=$2; shift 2 ;;
        --conan-home) conan_home=$2; shift 2 ;;
        --jobs) jobs=$2; shift 2 ;;
        --skip-bridge) skip_bridge=1; shift ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done
[[ -n "$libxvc_source" ]] || { echo "--libxvc-source is required" >&2; exit 2; }
libxvc_source=$(cd "$libxvc_source" && pwd)
mkdir -p "$build_dir"; build_dir=$(cd "$build_dir" && pwd)
if [[ -n "$conan_home" ]]; then mkdir -p "$conan_home"; conan_home=$(cd "$conan_home" && pwd); fi

[[ "$(uname -s)/$(uname -m)" == "Darwin/arm64" ]] || { echo "This script targets macOS arm64" >&2; exit 1; }
brew_prefix=$(brew --prefix)
for tool in cmake ninja conan meson pkg-config node npm; do
    command -v "$tool" >/dev/null || { echo "Missing $tool (brew install cmake ninja conan meson pkgconf node)" >&2; exit 1; }
done
qt_kegs=(qtbase qtdeclarative qtsvg qtwebsockets qtshadertools)
qt_prefix=""
for keg in "${qt_kegs[@]}"; do
    [[ -d "$brew_prefix/opt/$keg" ]] || { echo "Missing Homebrew $keg" >&2; exit 1; }
    qt_prefix+="$brew_prefix/opt/$keg;"
done
pins="$source_dir/build-support/windows/sources.json"
expected_libxvc=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["libxvc"]["commit"])' "$pins")
actual_libxvc=$(git -C "$libxvc_source" rev-parse HEAD)
if [[ "$actual_libxvc" != "$expected_libxvc" ]]; then
    # The macOS branch of libxvc may carry extra commits on top of the pin.
    git -C "$libxvc_source" merge-base --is-ancestor "$expected_libxvc" HEAD \
        || { echo "libxvc $actual_libxvc does not contain pinned $expected_libxvc" >&2; exit 1; }
fi
export PKG_CONFIG_PATH="$brew_prefix/lib/pkgconfig"
gst_version=$(pkg-config --modversion gstreamer-1.0)
qt_version=$("$brew_prefix/opt/qtbase/bin/qtpaths" --qt-version)

# 1. Native dependencies in a build-local Conan cache.
export CONAN_HOME="${conan_home:-$build_dir/conan-cache}"
mkdir -p "$CONAN_HOME"
[[ -f "$CONAN_HOME/global.conf" ]] || printf 'core.download:retry=15\ncore.download:retry_wait=20\n' > "$CONAN_HOME/global.conf"
conan remote list | grep -q conancenter || conan remote add conancenter https://center2.conan.io
conan export "$support/xdaqmetadata"
conan install "$support/dependencies.py" -pr:a "$support/apple-clang-release" \
    --output-folder "$build_dir/dependencies" --build=missing -c "tools.build:jobs=$jobs" \
    --format=json > "$build_dir/conan-graph.json"

# 2. Qt GStreamer sink from the gst-plugins-good release that matches the installed GStreamer.
plugin_dir="$build_dir/gstreamer-plugins"
plugin="$plugin_dir/libgstqml6.dylib"
if [[ ! -f "$plugin" ]]; then
    downloads="$build_dir/downloads"; mkdir -p "$downloads"
    tarball="gst-plugins-good-$gst_version.tar.xz"
    url="https://gstreamer.freedesktop.org/src/gst-plugins-good/$tarball"
    curl -fsSL -o "$downloads/$tarball" "$url"
    expected=$(curl -fsSL "$url.sha256sum" | awk '{print $1}')
    actual=$(shasum -a 256 "$downloads/$tarball" | awk '{print $1}')
    [[ "$expected" == "$actual" ]] || { echo "$tarball SHA-256 mismatch" >&2; exit 1; }
    rm -rf "$downloads/gst-plugins-good-$gst_version"
    tar -xf "$downloads/$tarball" -C "$downloads"
    meson setup "$build_dir/gst-qml6" "$downloads/gst-plugins-good-$gst_version" \
        -Dauto_features=disabled -Dqt6=enabled -Dqt-method=qmake -Dbuildtype=release \
        -Dexamples=disabled -Dtests=disabled -Ddoc=disabled --reconfigure
    ninja -C "$build_dir/gst-qml6"
    mkdir -p "$plugin_dir"
    cp "$build_dir/gst-qml6/ext/qt6/libgstqml6.dylib" "$plugin"
fi
GST_PLUGIN_PATH="$plugin_dir" gst-inspect-1.0 qml6glsink >/dev/null \
    || { echo "The built qml6glsink plugin does not load" >&2; exit 1; }

# 3. Desktop application with libxvc compiled from source.
cmake -S "$source_dir" -B "$build_dir/app" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_TOOLCHAIN_FILE=$build_dir/dependencies/conan_toolchain.cmake" \
    "-DTHORVISION_LIBXVC_SOURCE=$libxvc_source" \
    "-DCMAKE_PREFIX_PATH=$qt_prefix" "-DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=$qt_prefix" \
    "-DPython3_EXECUTABLE=$(command -v python3)" "-Dgst_dir=$brew_prefix" -DTHORVISION_ENABLE_UI_SMOKE=OFF
cmake --build "$build_dir/app" --target ThorVision recording-filename-test extension-client-smoke --parallel "$jobs"

app="$build_dir/app/ThorVision.app"
# main.cc prefers <bundle>/Contents/PlugIns/gstreamer, so the sink travels with the bundle.
mkdir -p "$app/Contents/PlugIns/gstreamer"
cp "$plugin" "$app/Contents/PlugIns/gstreamer/"
# The prebuilt metadata library resolves glib/gobject/intl through @rpath (official
# framework layout). Homebrew keeps them in separate kegs linked into <prefix>/lib.
export DYLD_FALLBACK_LIBRARY_PATH="$brew_prefix/lib"
"$build_dir/app/thorvision/recording-filename-test"
QT_QPA_PLATFORM=offscreen "$build_dir/app/thorvision/extension-client-smoke" --api-transport-test

# 4. Agent bridge.
if [[ $skip_bridge -eq 0 ]]; then
    (cd "$source_dir/agent-bridge" && npm ci --ignore-scripts --no-audit --no-fund && npm run build && npm test)
fi

sha() { shasum -a 256 "$1" | awk '{print $1}'; }
metadata=$(find "$build_dir/dependencies" -maxdepth 1 -name 'libxdaqmetadata.dylib' | head -1)
python3 - "$build_dir/build-receipt.json" <<EOF
import json, sys
json.dump({
    "desktopCommit": "$(git -C "$source_dir" rev-parse HEAD)",
    "desktopTrackedChanges": """$(git -C "$source_dir" status --porcelain --untracked-files=no)""",
    "libxvcCommit": "$actual_libxvc",
    "executableSha256": "$(sha "$app/Contents/MacOS/ThorVision")",
    "metadataDylibSha256": "$(sha "$metadata")",
    "gstreamerQtPluginSha256": "$(sha "$plugin")",
    "qt": "$qt_version", "gstreamer": "$gst_version", "qtPrefix": "$qt_prefix",
    "brewPrefix": "$brew_prefix", "node": "$(node --version)",
    "bridgeIncluded": $([[ $skip_bridge -eq 0 ]] && echo True || echo False),
}, open(sys.argv[1], "w"), indent=2)
EOF
cp "$source_dir/scripts/run-macos.sh" "$build_dir/run-thorvision.sh"
echo "Built $app"
echo "Build receipt: $build_dir/build-receipt.json"
echo "Start it with: $build_dir/run-thorvision.sh --launch"
