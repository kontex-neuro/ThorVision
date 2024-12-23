# XDAQ Desktop Video Capture App

## Platforms
* Windows
* macOS

## Third-party

* Qt 6 ([LGPL](http://doc.qt.io/qt-6/lgpl.html))
* CMake ([New BSD License](https://github.com/Kitware/CMake/blob/master/Copyright.txt))
* Ninja ([Apache License 2.0](https://github.com/ninja-build/ninja/blob/master/COPYING))

## Build instructions
    conan install . -b missing -pr:a <profile> -s build_type=Release
    cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release
    cmake --build build/Release --preset conan-release

## Deploy instructions
    conan install . -b missing -pr:a <profile> -s build_type=Release
    cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="./<folder>" 
    cmake --build build/Release --preset conan-release --target package