from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, CMakeDeps


class XppConan(ConanFile):
    name = "xpp"
    version = "1.0"
    
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared": False}
    
    # Dependencies
    requires = [
        "spdlog/1.14.1",
        "drogon/1.9.11",
        "nlohmann_json/3.11.3",
        "yaml-cpp/0.8.0",
        "protobuf/3.21.12",
        "sqlite3/3.44.0",
        "boost/1.83.0",
        "gtest/1.17.0",
    ]

    def layout(self):
        """设置输出目录为 debug/ 或 release/"""
        build_type = str(self.settings.build_type).lower()
        self.folders.build = build_type
        self.folders.generators = f"{build_type}/generators"
        self.folders.source = "."

    def generate(self):
        """生成CMakeDeps和CMakeToolchain"""
        tc = CMakeToolchain(self)
        # 配置Ninja生成器
        tc.generator = "Ninja"
        tc.generate()
        
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        """构建项目（可选）"""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
