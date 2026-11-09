"""Locked native dependencies for the Windows source build.

libxvc is built from the Git revision in sources.json, not a mutable dev package.
"""
from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain
from conan.tools.files import copy


class WindowsDependencies(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    default_options = {
        "boost/*:header_only": True,
        "cpr/*:with_ssl": "winssl",
        "libcurl/*:with_ssl": "openssl",
    }

    def build_requirements(self):
        self.tool_requires("cmake/3.29.7")
        self.tool_requires("ninja/1.13.2")

    def requirements(self):
        self.requires("boost/1.81.0")
        self.requires("spdlog/1.13.0")
        self.requires("nlohmann_json/3.11.3")
        self.requires("json-schema-validator/2.3.0")
        self.requires("cpr/1.14.2")
        self.requires("xdaqmetadata/0.3.1")
        self.requires("oatpp/1.3.0.latest")
        self.requires("libcurl/8.17.0", override=True)
        self.requires("openssl/3.4.1", override=True)
        self.requires("zlib/1.3.1", override=True)

    def generate(self):
        CMakeDeps(self).generate()
        toolchain = CMakeToolchain(self)
        toolchain.generator = "Ninja"
        toolchain.user_presets_path = False
        toolchain.generate()
        metadata = self.dependencies["xdaqmetadata"]
        copy(self, "*.dll", metadata.cpp_info.bindirs[0], self.generators_folder)
