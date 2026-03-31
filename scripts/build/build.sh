# Delete build directory
rm -rf build

# Build instructions
echo "Conan Install..."
conan install . -b missing -pr:a default -s build_type=Release -c tools.cmake.cmaketoolchain:generator=Ninja

echo "CMake Configure..."
cmake -S . -B build/Release --preset conan-release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake

echo "CMake Build..."
cmake --build build/Release