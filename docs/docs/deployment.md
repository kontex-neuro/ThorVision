# Deployment (coming soon)

To deploy the application, follow these instructions:

1. Install dependencies using Conan:
```bash
conan install . -b missing -pr:a <profile> -s build_type=Release
```

2. Generate the build files with a custom installation directory:
```bash
cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="./<folder>"
```

3. Build and package the application:
```bash
cmake --build build/Release --preset conan-release --target package
```

/// note | Note 
Replace `<profile>` with the Conan profile from your environment and `<folder>` with your desired installation directory. To see more about how to create [Conan profile](https://docs.conan.io/2/reference/config_files/profiles.html).
///