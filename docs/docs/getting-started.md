# Getting Started

Welcome to the Thor Vision developer docs! This guide will walk you through building and deploying the app from source.

---

## Building from Source (coming soon)

Before you begin, ensure the following tools and dependencies are installed on your system:

Build Tools:

- [**Visual Studio**](https://visualstudio.microsoft.com/vs/features/cplusplus/) - C/C++ IDE and compiler for Windows
- [**git**](https://git-scm.com/) - Version control system
- [**CMake**](https://cmake.org/) - Cross-platform build system generator
- [**Ninja**](https://ninja-build.org/) - Small build system with a focus on speed
- [**Conan**](https://conan.io/) - Software package manager for C++
- [**Qt 6**](https://www.qt.io/product/qt6) - GUI framework for the app

XDAQ Libraries:

- [**`libxvc`**](https://github.com/kontex-neuro/libxvc) - Required for streaming cameras
- [**`xdaqmetadata`**](https://github.com/kontex-neuro/xdaqmetadata) - Required for parsing [XDAQ Metadata](xdaq-metadata.md)

---

### Clone source code and prepare libraries

Clone source code of Thor Vision and go to the project directory:
```bash
git clone https://github.com/kontex-neuro/ThorVision.git
cd ThorVision
```

Clone source code of `libxvc`:
```bash
git clone https://github.com/kontex-neuro/libxvc.git
```

Clone source code of `xdaqmetadata`:
```bash
git clone https://github.com/kontex-neuro/xdaqmetadata.git
```

Follow the build instruction on both `libxvc` and `xdaqmetadata`.

---

### Build the app

Follow these steps to build the app from source:

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
Replace `<profile>` with the Conan profile from your environment, To see more about how to create [Conan profile](https://docs.conan.io/2/reference/config_files/profiles.html).
///

---

## Building the docs

Before you begin, ensure the following tools are installed on your system:

- [Python](https://www.python.org/)
- [MkDocs](https://www.mkdocs.org/) with [`mkdocs-material`](https://squidfunk.github.io/mkdocs-material/), [`pymdown-extensions`](https://facelessuser.github.io/pymdown-extensions/) and [`mkdocstrings`](https://mkdocstrings.github.io/)

First generate build files using `CMake` with the `-DBUILD_DOC=ON` option enabled. Then compile the target `doc`, for example:

```bash
cmake --build build/Release --preset conan-release --target doc
```

This will generate documentation in `<project_directory>/build/Release/site`.

---