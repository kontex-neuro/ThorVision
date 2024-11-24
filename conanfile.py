from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
from os.path import join


class xdaqvc(ConanFile):
    name = "xdaqvc"
    version = "0.0.1"
    settings = "os", "compiler", "build_type", "arch"
    generators = "VirtualRunEnv"
    license = ""
    url = ""
    description = "XDAQ Video Capture Desktop"

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.25.0 <3.30.0]")
        self.tool_requires("ninja/[>=1.12.0]")
        # self.requires("catch2/3.5.0")

    def requirements(self):
        self.requires("fmt/10.2.1")
        self.requires("spdlog/1.13.0")
        self.requires("nlohmann_json/3.11.3")
        # self.requires("json-schema-validator/2.3.0")
        self.requires("libxvc/0.0.1")
        self.requires("xdaqmetadata/0.0.1")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generator = "Ninja"
        tc.generate()

        for dep in self.dependencies.values():
            if dep.ref.name == "xdaqmetadata":
                for bindir in dep.cpp_info.bindirs:
                    copy(self, "*.dll", bindir, join(self.build_folder, "xdaqvc"))

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()