# Pinned Qt GStreamer plugin

`gstqt6d3d11.dll` is the Windows x64 plugin required by ThorVision's
`qml6d3d11sink` video renderer. This is the exact prebuilt dependency used by the
inspected `build/AgentWorkflow/ThorVision.exe` on 2026-11-09.

- Size: 102,912 bytes.
- SHA-256: `4a51060df057cbf2e0d44708571a0d1ee901fbb3164b5e9ebeb7c394743499ff`.
- Embedded GStreamer version: `1.26.8.1`.
- Embedded Qt build path: Qt `6.7.2`, `msvc2019_64`.
- The original compiler version and exact plugin source revision are unknown.
- `LGPL-2+.txt` and `README-LICENSE-INFO.txt` preserve the installed
  gst-plugins-bad notices. This plugin's source headers specify LGPL version 2
  or later.

The [upstream plugin directory](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/tree/5609d59151e9767f330f963ad648c2d7001fd547/subprojects/gst-plugins-bad/ext/qt6d3d11)
is a known source reference. It is not asserted to be the source revision of
this DLL.

The desktop build script checks the binary's hash before use and configures
its plugin search path without changing the system GStreamer installation.
The application source build reuses this immutable binary dependency. It does
not rebuild this plugin or promise byte-for-byte reproduction of third-party
binaries. Replace the plugin only with an explicitly versioned and verified
build, then update its pin in `build-support/windows/sources.json`.
