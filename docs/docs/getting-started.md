# Getting Started

Welcome to the Thor Vision developer docs! This guide will walk you through building and deploying the app from source.

---

## Building from Source

Before you begin, ensure the following tools and dependencies are installed on your system:

### Install Build Tools

- [**CMake**](https://cmake.org/) - Cross-platform build system generator
    - Download **CMake 3.27** (or higher) installer from [https://cmake.org/download/](https://cmake.org/download/) and install it.
- [**Conan**](https://conan.io/) - Software package manager for C++
- [**git**](https://git-scm.com/) - Version control system
    - Download **git** installer from [https://git-scm.com/downloads/win](https://git-scm.com/downloads/win) and install it.
- [**Ninja**](https://ninja-build.org/) - Small build system with a focus on speed
- [**Python**](https://www.python.org/) - Programming language
    - Download **Python 3.12** (or higher) installer from [https://www.python.org/downloads/](https://www.python.org/downloads/) and install it with adding to **PATH** enabled.
- [**Qt 6**](https://www.qt.io/product/qt6) - GUI framework for the app
    - Download **Qt 6** installer from [https://www.qt.io/download-dev](https://www.qt.io/download-dev) and install it.
- [**Visual Studio**](https://visualstudio.microsoft.com/) - C/C++ IDE and MSVC compiler for Windows
    - Download **Visual Studio 2022** installer from [https://visualstudio.microsoft.com/downloads/](https://visualstudio.microsoft.com/downloads/) and install it.

/// note | Note
The build is done in **Visual Studio 2022 Community** with SDK version **10.0.22621.0** and **Qt 6 Community Edition**.
///

### Install XDAQ Libraries

- [**`xdaqmetadata`**](https://github.com/kontex-neuro/xdaqmetadata) - Required for parsing [XDAQ Metadata](xdaq-metadata.md)
- [**`libxvc`**](https://github.com/kontex-neuro/libxvc) - Required for streaming cameras

#### Build [**xdaqmetadata**](https://github.com/kontex-neuro/xdaqmetadata)

1. Get the source code and go to project directory
```console
git clone https://github.com/kontex-neuro/xdaqmetadata.git
cd xdaqmetadata
```

2. Create python virtual environment `.venv` in project directory and activate it
```console
py -m venv .venv
.venv\Scripts\activate
```

3. Install Conan and ninja in `.venv` via pip
```console
pip install conan ninja
```

4. Install dependencies using Conan
```console
conan install . -b missing -pr:a <profile> -s build_type=Release
```

5. Generate the build files with CMake
```console
cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release
```

6. Build the project
```console
cmake --build build/Release --preset conan-release
```

7. Export as conan package to local cache
```console
conan export-pkg . -pr:a <profile> -s build_type=Release
```

/// note | Note 
Replace `<profile>` with the Conan profile from your environment, To see more about how to create [Conan profile](https://docs.conan.io/2/reference/config_files/profiles.html).

Example Conan profile for Windows:
```console
[settings]
arch=x86_64
compiler=msvc
compiler.cppstd=20
compiler.runtime=dynamic
compiler.version=194
os=Windows
[conf]
tools.cmake.cmaketoolchain:generator=Ninja
```
///

#### Build [**libxvc**](https://github.com/kontex-neuro/libxvc)

1. Get the source code and go to project directory
```console
git clone https://github.com/kontex-neuro/libxvc.git
cd libxvc
```

2. Create python virtual environment `.venv` in project directory and activate it
```console
py -m venv .venv
.venv\Scripts\activate
```

3. Install Conan and ninja in `.venv` via pip
```console
pip install conan ninja
```

4. Install dependencies using Conan
```console
conan install . -b missing -pr:a <profile> -s build_type=Release
```

5. Generate the build files with CMake
```console
cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release
```

6. Build the project
```console
cmake --build build/Release --preset conan-release
```

7. Export as conan package to local cache
```console
conan export-pkg . -pr:a <profile> -s build_type=Release
```

/// warning | Build order
Be sure to build `xdaqmetadata` first then `libxvc`, since `libxvc` is depended on `xdaqmetadata`.
///

---

### Build the app

Follow these steps to build the app from source:

1. Get the source code and go to project directory
```console
git clone https://github.com/kontex-neuro/ThorVision.git
cd ThorVision
```

2. Create python virtual environment `.venv` in project directory and activate it
```console
py -m venv .venv
.venv\Scripts\activate
```

3. Install Conan and ninja in `.venv` via pip
```console
pip install conan ninja
```

4. Install dependencies using Conan
```console
conan install . -b missing -pr:a <profile> -s build_type=Release
```

5. Generate the build files with CMake
```console
cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release
```

6. Build the project
```console
cmake --build build/Release --preset conan-release
```

---

## Building the docs (Optional)

Before you begin, ensure the following tools are installed on your system:

- [Python](https://www.python.org/)
- [MkDocs](https://www.mkdocs.org/) with [`mkdocs-material`](https://squidfunk.github.io/mkdocs-material/), [`pymdown-extensions`](https://facelessuser.github.io/pymdown-extensions/) and [`mkdocstrings`](https://mkdocstrings.github.io/)
    ```console
    pip install mkdocs mkdocs-material pymdown-extensions mkdocstrings
    ```

First generate build files using `CMake` with the `-DBUILD_DOC=ON` option enabled. Then compile the target `doc`, for example:

```console
cmake --build build/Release --preset conan-release --target doc
```

This will generate documentation in `<project_directory>/build/Release/site`.

---