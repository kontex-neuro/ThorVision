# GStreamer Build Scripts

This folder contains scripts for building GStreamer with MSVC and building debug versions of the Qt6 D3D11 QML plugin.

## Prerequisites

- **Visual Studio 2022** with C++ workload
- **Qt 6.8.0** (MSVC 2022 64-bit)
- **Python 3**
- **Git**
- **Meson** build system (`pip install meson`)
- **CMake** and **Ninja**
- **GStreamer MSVC runtime** installed (for linking against base libraries)

## Directory Structure

```
scripts/gstreamer/
├── build_gstreamer_msvc.ps1      # Main GStreamer build script
├── README.md                      # This file
└── gstreamer_qml_debug/
    ├── build_debug.ps1            # Debug plugin build script
    ├── CMakeLists.txt             # CMake configuration
    ├── config.h                   # Minimal config header
    └── plugin_init.cpp            # QML plugin entry point
```

## Build Order

1. First, build GStreamer using `build_gstreamer_msvc.ps1`
2. Then, build the QML plugin using `gstreamer_qml_debug/build_debug.ps1`

---

## Part 1: Building GStreamer (`build_gstreamer_msvc.ps1`)

This script builds GStreamer from source using MSVC with debug symbols enabled.

### Usage

Run the script from the GStreamer source directory:

```powershell
cp ./build_gstreamer_msvc.ps1 path/to/gstreamer
cd path/to/gstreamer
./build_gstreamer_msvc.ps1
```

### Hardcoded Paths (MUST BE MODIFIED)

Edit `build_gstreamer_msvc.ps1` and update the following paths to match your system:

| Variable               | Default Value                                                                              | Description                         |
| ---------------------- | ------------------------------------------------------------------------------------------ | ----------------------------------- |
| `$vs_path`             | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat` | Path to Visual Studio vcvarsall.bat |
| `$qt6_path`            | `C:\Qt\6.8.0\msvc2022_64\bin`                                                              | Path to Qt6 MSVC bin directory      |
| `$clean_path` (Python) | `C:\Users\$env:USERNAME\AppData\Local\Programs\Python\Python312`                           | Path to Python installation         |

### Build Configuration

The script configures GStreamer with the following options:

- **Build type**: Debug
- **Library type**: Shared (DLLs)
- **Enabled plugins**:
  - base, good, bad
  - Qt6 (`-Dqt6=enabled`)
  - D3D11 (`-Dgst-plugins-bad:d3d11=enabled`)
- **Disabled plugins** (to speed up build):
  - ugly, vulkan, webrtc, d3d12, libav, devtools, ges, rtsp_server, vaapi, sharp, rs, doc, introspection, python

### Output

The build output is placed in `builddir/` within the GStreamer source directory.

---

## Part 2: Building Debug QML Plugin (`gstreamer_qml_debug/`)

This folder contains files to build a debug version of the `gstqt6d3d11` QML plugin, which provides D3D11-based video rendering for Qt6 QML applications.

### Usage
- Make sure gstreamer_qml_debug/ gstreamer/ should be in the same folder

```powershell
cd scripts/gstreamer/gstreamer_qml_debug
powershell -ExecutionPolicy Bypass -File build_debug.ps1
```

### Hardcoded Paths in `build_debug.ps1` (MUST BE MODIFIED)

| Variable    | Default Value                                                                              | Description                         |
| ----------- | ------------------------------------------------------------------------------------------ | ----------------------------------- |
| `$vs_path`  | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat` | Path to Visual Studio vcvarsall.bat |
| `$qt6_path` | `C:\Qt\6.8.0\msvc2022_64`                                                                  | Path to Qt6 MSVC installation root  |
| `$gst_path` | `C:\gstreamer\1.0\msvc_x86_64`                                                             | Path to installed GStreamer runtime |

### Hardcoded Paths in `CMakeLists.txt` (MUST BE MODIFIED)

| Variable          | Default Value                               | Description                                  |
| ----------------- | ------------------------------------------- | -------------------------------------------- |
| `GST_BUILD_DIR`   | `${CMAKE_SOURCE_DIR}/../gstreamer/builddir` | Path to GStreamer build output (from Part 1) |
| `GST_SRC_DIR`     | `${CMAKE_SOURCE_DIR}/../gstreamer`          | Path to GStreamer source code                |
| `GST_INSTALL_DIR` | `C:/gstreamer/1.0/msvc_x86_64`              | Path to installed GStreamer runtime          |

### Expected Directory Layout

The CMakeLists.txt expects this directory layout relative to `gstreamer_qml_debug/`:

```
../gstreamer/                     # GStreamer source code
├── builddir/                     # Build output from build_gstreamer_msvc.ps1
│   └── subprojects/
│       ├── gst-plugins-bad/
│       │   └── gst-libs/
│       │       └── gst/
│       │           └── d3d11/
│       │               └── gstd3d11-1.0.lib
│       └── gstreamer/
└── subprojects/
    ├── gst-plugins-bad/
    │   ├── ext/
    │   │   └── qt6d3d11/         # Qt6 D3D11 plugin sources
    │   │       ├── gstqt6d3d11videoitem.cpp
    │   │       ├── gstqt6d3d11videoitem.h
    │   │       ├── gstqsg6d3d11node.cpp
    │   │       └── gstqsg6d3d11node.h
    │   └── gst-libs/
    │       └── gst/
    │           └── d3d11/
    └── gstreamer/
        └── libs/
```

### Output

The built QML plugin is output to:

```
build/org/freedesktop/gstreamer/Qt6D3D11VideoItem/
├── gstqt6d3d11.dll
├── gstqt6d3d11.lib
├── gstqt6d3d11.pdb
├── gstqt6d3d11.qmltypes
└── qmldir
```

After building, the plugin is automatically copied to:

```
../gstreamer_qml/org/freedesktop/gstreamer/Qt6D3D11VideoItem/
```

- the whole folder of gstreamer_qml/ is the whole library we need