# Deployment

To successfully deploy the app, follow the steps outlined in [Build the app](getting-started.md#build-the-app). Then, generate the build files with the `-DCMAKE_INSTALL_PREFIX="./<folder>` option enabled, and compile the target with `package`.

1. Generate the build files with a custom installation directory:
```console
cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="./<folder>"
```

2. Build and package the app:
```console
cmake --build build/Release --preset conan-release --target package
```