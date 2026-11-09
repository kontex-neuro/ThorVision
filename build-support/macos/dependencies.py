"""Native dependencies for the macOS arm64 source build.

Mirrors build-support/windows/dependencies.py. libxvc is built from the Git revision in
build-support/windows/sources.json (THORVISION_LIBXVC_SOURCE), not a mutable dev package.
"""
from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain
from conan.tools.files import copy


class MacosDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    default_options = {
        "boost/*:header_only": True,
        # The ConanCenter cpr recipe accepts only darwinssl (Secure Transport) on Apple
        # platforms, and libcurl removed that backend in 8.15.0. libcurl is therefore held
        # at 8.12.1 here instead of the Windows 8.17.0 pin. Team decision pending.
        "cpr/*:with_ssl": "darwinssl",
        "libcurl/*:with_ssl": "darwinssl",
    }

    def requirements(self):
        self.requires("boost/1.81.0")
        self.requires("spdlog/1.13.0")
        self.requires("nlohmann_json/3.11.3")
        self.requires("json-schema-validator/2.3.0")
        self.requires("cpr/1.14.2")
        self.requires("xdaqmetadata/0.3.1")
        self.requires("oatpp/1.3.0.latest")
        self.requires("libcurl/8.12.1", override=True)
        self.requires("zlib/1.3.1", override=True)

    def generate(self):
        CMakeDeps(self).generate()
        toolchain = CMakeToolchain(self)
        toolchain.generator = "Ninja"
        toolchain.user_presets_path = False
        toolchain.generate()
        metadata = self.dependencies["xdaqmetadata"]
        copy(self, "*.dylib", metadata.cpp_info.libdirs[0], self.generators_folder)
