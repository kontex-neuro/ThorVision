# Video Capture

### Third-party

* Qt 6 ([LGPL](http://doc.qt.io/qt-6/lgpl.html))

### Build instructions
    conan install . -b missing -pr:a default -s build_type=Release
    cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release
    cmake --build build/Release --preset conan-release

### Deploy instructions
    # Put gstreamer runtime library installer gstreamer-1.0-*.msi to xdaqvc folder
    cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="."
    cmake --build build/Release --preset conan-release --target package