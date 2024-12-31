from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
from os.path import join


class ThorVision(ConanFile):
    name = "ThorVision"
    version = "0.0.3"
    settings = "os", "compiler", "build_type", "arch"
    generators = "VirtualRunEnv"
    license = "LGPL-3.0-or-later"
    url = "https://github.com/kontex-neuro/XDAQ-VC.git"
    description = "XDAQ Desktop Video Capture App"

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.25.0 <3.30.0]")
        self.tool_requires("ninja/[>=1.12.0]")
        # self.requires("catch2/3.5.0")

    def requirements(self):
        self.requires("fmt/10.2.1")
        self.requires("spdlog/1.13.0")
        self.requires("nlohmann_json/3.11.3")
        # self.requires("json-schema-validator/2.3.0")
        self.requires("libxvc/0.0.3")
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
                if self.settings.os == "Windows":
                    for bindir in dep.cpp_info.bindirs:
                        copy(self, "*.dll", bindir, join(self.build_folder, "xdaqvc"))
                elif self.settings.os == "Macos":
                    frameworks_dir = join(
                        self.build_folder,
                        "xdaqvc",
                        "XDAQ-VC.app",
                        "Contents",
                        "Frameworks",
                    )
                    copy(
                        self,
                        "*.dylib",
                        dep.cpp_info.libdirs[0],
                        frameworks_dir,
                    )

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
