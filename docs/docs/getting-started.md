# Getting Started

Welcome, Developers!

This guide will walk you through building and deploying the GUI app from source.

---

## Building from Source (coming soon)

Before you begin, ensure the following tools and dependencies are installed on your system:

Build Tools:

- [**CMake**](https://cmake.org/) - A cross-platform build system generator
- [**Ninja**](https://ninja-build.org/) - A small build system with a focus on speed
- [**Conan**](https://conan.io/) - A package manager for C++
- [**Qt 6**](https://www.qt.io/product/qt6) - The GUI framework for the application

XDAQ Libraries:

- [**`libxvc`**](https://github.com/kontex-neuro/libxvc) - Required for streaming cameras
- [**`xdaqmetadata`**](https://github.com/kontex-neuro/xdaqmetadata) - Required for parsing [XDAQ Metadata](metadata.md)

Follow these steps to build the application from source:

1. Install dependencies using Conan
```bash
conan install . -b missing -pr:a <profile> -s build_type=Release
```

2. Generate the build files with CMake
```bash
cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release
```

3. Build the project
```bash
cmake --build build/Release --preset conan-release
```

/// note | Note 
Replace `<profile>` with the Conan profile from your environment
///

---

## Building the docs

Before you begin, ensure the following tools are installed on your system:

- [Python](https://www.python.org/)
- [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/)

First generate build files using `cmake` with the `-DBUILD_DOC=ON` option enabled. Then build the target `doc` to generate HTML documentation in `build/Release/site`:

```bash
cmake --build build/Release --preset conan-release --target doc
```

---