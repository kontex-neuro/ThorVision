# Build the FrameLab desktop on macOS (arm64)

This is the development source build for Apple silicon. It mirrors
[the Windows build](windows-build.md): libxvc is compiled from the commit pinned in
[`sources.json`](../build-support/windows/sources.json) (or a branch that contains it),
the native checks run, and the agent bridge is installed, built and tested. It does
not produce a signed or notarised `.dmg`; the tag-triggered release workflow still
does that with the official GStreamer framework.

## Prerequisites

- macOS 14 or later on Apple silicon, with the Xcode Command Line Tools
  (verified with Apple clang 17 on macOS 15 (Darwin 24.6)).
- Homebrew packages:

  ```bash
  brew install cmake ninja conan meson pkgconf node \
      qtbase qtdeclarative qtsvg qtwebsockets qtshadertools gstreamer
  ```

  Verified with Qt 6.12.0, GStreamer 1.28.9, Conan 2.34.0 and Node 26.12.1.
- GitHub access to `kontex-neuro/libxvc`.

No `sudo` is needed. The official `GStreamer.framework` route used by CI also works
for configuration (`-Dgst_dir`), but it has not been exercised by this script.

## Build and check

```bash
git clone --branch feat/framelab https://github.com/kontex-neuro/libxvc.git ../libxvc
scripts/build-macos.sh --libxvc-source ../libxvc
```

The script:

1. Resolves the native dependencies in a build-local Conan cache
   (`build/macos/conan-cache`) from [`build-support/macos`](../build-support/macos).
   The xdaqmetadata 0.3.1 macOS arm64 archive is verified by SHA-256.
2. Builds the Qt GStreamer sink (`qml6glsink`, `libgstqml6.dylib`) from the
   gst-plugins-good release that matches the installed GStreamer, after checking the
   tarball against the SHA-256 published by gstreamer.freedesktop.org. Neither Homebrew
   nor the official macOS framework ships this plugin, and it must be linked against
   the same Qt as the application.
3. Configures and builds ThorVision with `THORVISION_LIBXVC_SOURCE`, then runs
   `recording-filename-test` and `extension-client-smoke --api-transport-test`.
4. Runs `npm ci`, `npm run build` and `npm test` in `agent-bridge` (skip with
   `--skip-bridge`).

It writes `build/macos/build-receipt.json` and copies a launcher beside it.

## Start

```bash
build/macos/run-thorvision.sh            # checks executable, sink and runtime only
build/macos/run-thorvision.sh --launch   # opens ThorVision
```

The sink is copied into `ThorVision.app/Contents/PlugIns/gstreamer`, which
`main.cc` already prefers. The launcher sets `DYLD_FALLBACK_LIBRARY_PATH` to the
Homebrew `lib` directory, because the prebuilt `libxdaqmetadata.dylib` finds glib,
gobject and libintl through `@rpath` (the official framework layout).

## macOS-specific notes

- Use the individual Qt kegs (`/opt/homebrew/opt/qtbase`, …) as `CMAKE_PREFIX_PATH`,
  not `/opt/homebrew`. Otherwise `-isystem /opt/homebrew/include` precedes the Conan
  include paths and spdlog compiles against Homebrew's newer `fmt`.
- The ConanCenter `cpr` recipe accepts only `darwinssl` (Secure Transport) on Apple
  platforms, and libcurl removed that backend in 8.15.0. The macOS dependency set
  therefore holds libcurl at 8.12.1, while Windows uses 8.17.0 with OpenSSL.
- Qt's default macOS Quick Controls style is native and ignores the custom
  `background`/`contentItem` the UI sets, so macOS now uses Fusion, as Windows does.

## Not covered

No `.dmg` packaging, code signing or notarisation, and no recording or SSD workflow
test. Device integration tests (`[.integration]`) were not run.
