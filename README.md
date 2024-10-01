# Video Capture

### Third-party

* Qt 6 ([LGPL](http://doc.qt.io/qt-6/lgpl.html))

### Build instructions

``` bash
conan install . -b missing -pr:a default -s build_type=Release
cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release --preset conan-release
```