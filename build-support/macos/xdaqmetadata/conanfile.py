"""macOS arm64 metadata package with verified archive bytes.

Adapted from kontex-neuro/kontex-conan at
936ed8f959df84236f5ccb5493edf622c4f7b812, recipes/xdaqmetadata/0.3.1.
Mirrors build-support/windows/xdaqmetadata, pinned to the published macOS archive.
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
        if (str(self.settings.os), str(self.settings.arch), str(self.settings.build_type)) != ("Macos", "armv8", "Release"):
            raise ConanInvalidConfiguration("This locked metadata archive is macOS armv8 Release only")

    def build(self):
        get(self,
            "https://dl-conan.kontex.io/xdaqmetadata/macos-armv8-release-0.3.1.zip",
            sha256="3e329471bf99b536b867295c62f75c29351e95084af2cf6ec6a90176418977c3",
            strip_root=True)

    def package(self):
        for folder in ("include", "lib"):
            copy(self, "*", Path(self.build_folder, folder), Path(self.package_folder, folder),
                 excludes=("cmake/*",))

    def package_info(self):
        self.cpp_info.libs = ["xdaqmetadata"]
        self.cpp_info.set_property("cmake_file_name", "xdaqmetadata")
        self.cpp_info.set_property("cmake_target_name", "xdaqmetadata::xdaqmetadata")
