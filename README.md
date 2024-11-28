# XDAQ Video Capture Desktop

### Third-party

* Qt 6 ([LGPL](http://doc.qt.io/qt-6/lgpl.html))

### Build instructions
    conan install . -b missing -pr:a <profile> -s build_type=Release
    cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release
    cmake --build build/Release --preset conan-release

### Windows Deploy instructions
    conan install . -b missing -pr:a <profile> -s build_type=Release
    cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="." -DGSTREAMER_INSTALLER_PATH="path\to\gstreamer-runtime-msvc-library.msi"
    # TODO: For now, need to manually update gstreamer-runtime-msvc-library.msi in cmake/call_gstreamer_installer.iss
    cmake --build build/Release --preset conan-release --target package

### Macos Deploy instructions
    conan install . -b missing -pr:a <profile> -s build_type=Release
    cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="."
    cmake --build build/Release --preset conan-release --target package