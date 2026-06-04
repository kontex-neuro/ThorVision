from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
from os.path import join


class ThorVision(ConanFile):
    name = "ThorVision"
    settings = "os", "compiler", "build_type", "arch"
    generators = "VirtualRunEnv"
    license = "LGPL-3.0-or-later"
    url = "https://github.com/kontex-neuro/ThorVision.git"
    description = "ThorVision Desktop Video Capture GUI App"

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.25.0 <3.30.0]")
        self.tool_requires("ninja/[>=1.12.0]")

    def requirements(self):
        self.requires("spdlog/1.13.0")
        self.requires("nlohmann_json/3.11.3")
        self.requires("libxvc/0.3.2")
        self.requires("xdaqmetadata/0.2.0")
        self.requires("oatpp/1.3.0.latest")

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
                    copy(self, "*.dll", dep.cpp_info.bindir, self.build_folder)
                elif self.settings.os == "Macos":
                    frameworks_dir = join(
                        self.build_folder,
                        "ThorVision.app",
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
