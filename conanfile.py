from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy

class NstdConan(ConanFile):
    name = "nstd"
    version = "0.1.0"
    description = "A C++ library resembling the standard library"
    license = "MIT"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    exports_sources = "CMakeLists.txt", "include/*", "tests/*"
    package_type = "header-library"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["INSTALL_GTEST"] = False
        tc.cache_variables["INSTALL_GMOCK"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if not self.conf.get("tools.build:skip_test", default=False):
            cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_id(self):
        self.info.clear()

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.set_property("cmake_file_name", "nstd")
        self.cpp_info.set_property("cmake_target_name", "nstd::nstd")

