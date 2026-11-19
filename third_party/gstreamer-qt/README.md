# ThorVision video preview plugins

ThorVision Desktop uses GStreamer 1.26.8 with Qt 6. The Windows x64
plugin provides `qml6d3d11sink` and the macOS arm64 plugin provides
`qml6glsink`. These files come from the signed ThorVision Desktop 1.3.1
production installers. CMake verifies the plugin hashes before packaging.
macOS signs the copy inside the finished application bundle.

| Platform | Source installer | Installer SHA-256 | Path in installation |
| --- | --- | --- | --- |
| Windows x64 | [ThorVision 1.3.1](https://dl-thorvision.kontex.io/dist/v1.3.1/x64/ThorVision-1.3.1-win64.exe) | `a8229f1b0f728b9082a65c3f2483fcc8369bed867a2280be9d1afa31968a20f8` | `plugins/gstreamer/gstqt6d3d11.dll` |
| macOS arm64 | [ThorVision 1.3.1](https://dl-thorvision.kontex.io/dist/v1.3.1/macos-arm64/ThorVision-1.3.1-Darwin.dmg) | `8affeb070c91c5a33d47bbc515f5166bfc7336976619bb8eb08fe086bdb88c45` | `ThorVision.app/Contents/PlugIns/gstreamer/libgstqml6.dylib` |

The plugin source and licensing notices are available in GStreamer's
[qt6d3d11](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/tree/1.26.8/subprojects/gst-plugins-bad/ext/qt6d3d11)
and [qt6](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/tree/1.26.8/subprojects/gst-plugins-good/ext/qt6)
directories. The LGPL text is in [COPYING](COPYING).
