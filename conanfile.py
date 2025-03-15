import os

from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake, CMakeToolchain
from conan.tools.files import copy
from conan.tools.scm import Git

required_conan_version = ">=2.12.2"

'''
Notes:
Snappy is disabled.
Build static lib only, no shared lib.
Package contains bin forestdb_dump.
'''
class ForestdbConan(ConanFile):
    name = "forestdb"
    license = "Apache License 2.0"
    description = "forestdb kv store"
    homepage = "https://github.com/ForestDB-KVStore/forestdb"

    generators = "CMakeDeps"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True,False],
        "fPIC": [True, False],
        "coverage": [True, False],
        "with_snappy" : [True, False],
        "with_jemalloc" : [True, False],
        "build_tests": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "coverage": False,
        "with_snappy": False,
        "with_jemalloc": False,
        "build_tests": False
    }

    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*", "tools/*", "LICENSE"

    def requirements(self):
        if self.options.with_snappy:
            self.requires("snappy/[~1]")

        if self.options.with_jemalloc:
            self.requires("jemalloc/[*]")

    def layout(self):
        cmake_layout(self, generator="CMakeDeps")
        self.cpp.package.libs = [self.name]

        hash = Git(self).get_commit()
        self.cpp.package.defines = self.cpp.build.defines = ["_FDB_COMMIT_HASH=%s" % hash]

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["WITH_CONAN"] = True
        tc.variables["CONAN_BUILD_COVERAGE"] = False
        tc.variables["CODE_COVERAGE"] = self.options.coverage
        tc.variables["SNAPPY_OPTION"] = self.options.with_snappy
        tc.variables["_JEMALLOC"] = self.options.with_snappy
        tc.variables["BUILD_TESTING"] = self.options.build_tests
        tc.variables["CMAKE_VERBOSE_MAKEFILE"] = False
        tc.variables["CMAKE_EXPORT_COMPILE_COMMANDS"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if self.options.build_tests:
            cmake.ctest()
        copy(self, "compile_commands.json", self.build_folder, self.source_folder, keep_path=False)

    def package(self):
        cmake = CMake(self)
        cmake.install()
        assert copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses")), "Copy failed"

    def package_info(self):
        self.cpp_info.system_libs.extend(["pthread", "m", "dl"])
        if self.settings.os == "Linux":
            self.cpp_info.system_libs.extend(["rt"])