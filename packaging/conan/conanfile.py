# Conan 2 recipe for tg.
#   conan create packaging/conan --build=missing                       (from the tg checkout)
#   conan create packaging/conan -o "tg/*:freetype=True" --build=missing
# Consumers:  [requires] tg/0.5.0   ->  find_package(tg)  ->  tg::app / tg::tg / tg::soft
import os
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class TgConan(ConanFile):
    name = "tg"
    version = "0.5.0"
    description = "A small retained-mode C++20 GUI framework"
    url = "https://github.com/txxmo/tgui"
    topics = ("gui", "ui", "widgets", "imgui-alternative", "opengl", "direct3d")
    settings = "os", "arch", "compiler", "build_type"
    options = {"fPIC": [True, False], "app": [True, False], "freetype": [True, False], "harfbuzz": [True, False]}
    default_options = {"fPIC": True, "app": True, "freetype": False, "harfbuzz": False}

    def export_sources(self):
        root = os.path.join(self.recipe_folder, "..", "..")
        for pattern in ("CMakeLists.txt", "cmake/*", "include/*", "src/*", "backends/*", "third_party/*", "assets/fonts/LICENSE*", "LICENSE*"):
            copy(self, pattern, root, self.export_sources_folder)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.harfbuzz:
            self.options.freetype = True

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        if self.options.app and self.settings.os != "Windows":
            self.requires("glfw/3.4")
        if self.options.freetype:
            self.requires("freetype/2.13.2")
        if self.options.harfbuzz:
            self.requires("harfbuzz/8.3.0")

    def validate(self):
        from conan.tools.build import check_min_cppstd
        check_min_cppstd(self, 20)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["TG_INSTALL"] = True
        tc.variables["TG_BUILD_EXAMPLES"] = False
        tc.variables["TG_BUILD_HEADLESS"] = False
        tc.variables["TG_WEBVIEW"] = False
        tc.variables["TG_BUILD_APP"] = bool(self.options.app)
        tc.variables["TG_FREETYPE"] = bool(self.options.freetype)
        tc.variables["TG_HARFBUZZ"] = bool(self.options.harfbuzz)
        tc.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()

    def package_info(self):
        # Conan generates the CMake config for consumers (same target names as tg's own config)
        self.cpp_info.set_property("cmake_file_name", "tg")
        self.cpp_info.set_property("cmake_target_name", "tg::tg_all")
        core = self.cpp_info.components["tg"]
        core.set_property("cmake_target_name", "tg::tg")
        core.libs = ["tg"]
        if self.settings.os == "Linux":
            core.system_libs = ["pthread", "util"]
        elif self.settings.os == "Windows":
            core.system_libs = ["ole32", "shell32", "uuid"]
        if self.options.freetype:
            core.requires.append("freetype::freetype")
            core.defines.append("TG_HAS_FREETYPE=1")
        if self.options.harfbuzz:
            core.requires.append("harfbuzz::harfbuzz")
            core.defines.append("TG_HAS_HARFBUZZ=1")
        soft = self.cpp_info.components["soft"]
        soft.set_property("cmake_target_name", "tg::soft")
        soft.libs = ["tg_soft"]
        soft.includedirs = ["include", os.path.join("include", "tg", "backends")]
        soft.requires = ["tg"]
        if self.settings.os == "Windows":
            for name, lib, sys in (("win32", "tg_win32", ["imm32", "ole32", "shell32"]), ("d3d11", "tg_d3d11", ["d3d11", "d3dcompiler"]),
                                   ("d3d9", "tg_d3d9", ["d3d9"])):
                c = self.cpp_info.components[name]
                c.set_property("cmake_target_name", "tg::" + name)
                c.libs = [lib]
                c.includedirs = ["include", os.path.join("include", "tg", "backends")]
                c.system_libs = sys
                c.requires = ["tg"]
        if self.options.app:
            app = self.cpp_info.components["app"]
            app.set_property("cmake_target_name", "tg::app")
            app.libs = ["tg_app"]
            app.includedirs = ["include", os.path.join("include", "tg", "backends")]
            if self.settings.os == "Windows":
                app.requires = ["tg", "win32", "d3d11"]
                app.system_libs = ["dxgi"]
            else:
                app.requires = ["tg", "glfw::glfw"]
                if self.settings.os == "Linux":
                    app.system_libs = ["GL"]
