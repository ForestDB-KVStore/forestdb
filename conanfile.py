import os

from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake, CMakeToolchain
from conan.tools.files import copy,save,load
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

    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*", "tools/*", "option/*", "utils/*", "LICENSE"

    def set_version(self):
        if self.version:
            return
        
        git = Git(self, folder=self.recipe_folder)
        version = git.run(cmd="describe --tags --long")
        # the string returned by the git command above looks like this:
        # v1.2.3-4-g5bfa09b

        # if the tag starts with "v" - strip it
        version = version[1:] if version.startswith("v") else version

        # replace the second dash with "." in order to ensure proper semantic version comparison
        items = version.split("-", 2)
        assert len(items) == 3, f"Unexpected version format: {version}"
        version = items[0] + "-" + items[1] + "." + items[2]
            
        # the final version string will look like this:
        # "1.2.3-4.g5bfa09b"
        # 1 is the major version
        # 2 is the minor version
        # 3 is the patch version
        # 4.g5bfa09b is the pre-release version wich will also be parsed 
        #  and 4 will be the major version of the pre-release version
        self.version = version

    def requirements(self):
        if self.options.with_snappy:
            self.requires("snappy/[~1]")

        if self.options.with_jemalloc:
            self.requires("jemalloc/[*]")

    def _computeCommitHash(self):
        hash_file = os.path.join(self.recipe_folder, "COMMIT_HASH")
        if (os.path.exists(hash_file)):
            hash = load(self,path=hash_file)
            self.output.info(f"Fetched commit hash from {hash_file}")
        else: # we are building from local source, i.e. in editable mode
            git = Git(self, folder=self.recipe_folder)
            hash = git.get_commit()
            diff = git.run(cmd="diff --stat")
            if diff:
                hash +="-dirty"
            self.output.info(f"Fetched commit hash {hash} from local Git in {self.recipe_folder}")
        return hash

    def export(self):
        save(self, path=os.path.join(self.export_folder, "COMMIT_HASH"), content=self._computeCommitHash())

    def layout(self):
        cmake_layout(self, generator="CMakeDeps")
        self.cpp.package.libs = [f"lib{self.name}.so" if self.options.shared else f"lib{self.name}.a"]
        self.cpp.package.defines = self.cpp.build.defines = ["_FDB_COMMIT_HASH=%s" % self._computeCommitHash()]

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

