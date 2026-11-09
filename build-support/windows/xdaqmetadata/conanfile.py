"""Windows metadata package with verified archive bytes.

Adapted from kontex-neuro/kontex-conan at
936ed8f959df84236f5ccb5493edf622c4f7b812, recipes/xdaqmetadata/0.3.1.
"""
from pathlib import Path
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy, get


class Metadata(ConanFile):
    name = "xdaqmetadata"
    version = "0.3.1"
    settings = "os", "compiler", "build_type", "arch"
    package_type = "shared-library"
    build_policy = "missing"
    url = "https://github.com/kontex-neuro/xdaqmetadata.git"

    def requirements(self):
        self.requires("spdlog/1.13.0")

    def validate(self):
        if (str(self.settings.os), str(self.settings.arch), str(self.settings.build_type)) != ("Windows", "x86_64", "Release"):
            raise ConanInvalidConfiguration("This locked metadata archive is Windows x86_64 Release only")

    def build(self):
        get(self,
            "https://dl-conan.kontex.io/xdaqmetadata/windows-x86_64-release-0.3.1.zip",
            sha256="62ac329ea6c5f25001f2723563fa3da59fcd7361c7888650e21823ecf2045d01",
            strip_root=True)

    def package(self):
        for folder in ("bin", "include", "lib"):
            copy(self, "*", Path(self.build_folder, folder), Path(self.package_folder, folder))

    def package_info(self):
        self.cpp_info.libs = ["xdaqmetadata"]
        self.cpp_info.set_property("cmake_file_name", "xdaqmetadata")
        self.cpp_info.set_property("cmake_target_name", "xdaqmetadata::xdaqmetadata")
