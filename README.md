# XDAQ Video Capture Desktop

### Third-party

* Qt 6 ([LGPL](http://doc.qt.io/qt-6/lgpl.html))
* CMake ([BSD 3-clause License](https://github.com/Kitware/CMake/blob/master/Copyright.txt))
* Ninja ([Apache License 2.0](https://github.com/ninja-build/ninja/blob/master/COPYING))

### Windows/MacOS Build instructions
    conan install . -b missing -pr:a <profile> -s build_type=Release
    cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release
    cmake --build build/Release --preset conan-release

### Windows/MacOS Deploy instructions
    conan install . -b missing -pr:a <profile> -s build_type=Release
    cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="./<folder>" 
    cmake --build build/Release --preset conan-release --target package