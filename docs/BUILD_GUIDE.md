# 构建和运行指南

## 📋 前置需求

### 必需
- C++20 编译器（Visual Studio 2022 或更高版本）
- CMake 3.20+
- Conan 2.0+
- PostgreSQL 12+ (仅用于数据库，Redis 不再需要)

### 安装 Conan（如果未安装）
```bash
pip install conan
```

---

## 🔨 编译步骤

### 1. 清理旧构建（重要！移除了 Redis 依赖）
```bash
cd d:\workspace\xpp
rm -rf build
```

### 2. 安装依赖
```bash
conan install . --output-folder=build --build=missing -s compiler.cppstd=20
```

**输出示例**:
```
Aggregating env files...
Installed: ... spdlog, drogon, nlohmann_json, yaml-cpp, protobuf, libpqxx, boost ...
```

### 3. 生成 CMake 项目
```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
```

### 4. 编译
```bash
cmake --build build --config Release -j4
```

**输出示例**:
```
Building CXX object CMakeFiles/xpp.dir/src/main.cpp.obj
[100%] Built target xpp
[100%] Built target test_simple
```

---

## ▶️ 运行应用

### 前置步骤：初始化数据库

**1. 启动 PostgreSQL**
```bash
# Windows 上通常已作为服务运行
# 或使用 pgAdmin 启动
```

**2. 初始化数据库结构**
```bash
psql -U postgres -f config/init_db.sql
```

或者在 pgAdmin 中执行 `config/init_db.sql` 中的 SQL。

### 运行主服务器

```bash
# 运行可执行文件
./build/Release/xpp.exe

# 或
D:\workspace\xpp\build\Release\xpp.exe
```

**预期输出**:
```
[2026-01-09 10:30:45.123] [info] [main] === XPP WeChat Backend Starting ===
[2026-01-09 10:30:45.124] [info] [core] Database pool initialized: localhost:5432/xpp_db
[2026-01-09 10:30:45.125] [info] [infrastructure] Memory cache initialized (in-process, data will be lost on restart)
[2026-01-09 10:30:45.126] [info] [main] All services initialized successfully
[2026-01-09 10:30:45.127] [info] [main] Server starting on 0.0.0.0:8080
```

### 测试服务器

**在另一个终端**:
```bash
# 测试健康检查端点
curl http://localhost:8080/health

# 预期响应：
# {"status":"ok","timestamp":1673275845125}
```

---

## 🧪 运行测试

### 简单测试（无需数据库）

```bash
./build/Release/test_simple.exe
```

**预期输出**:
```
=== XPP Framework Test ===
1. Testing Logger (console only)...
   ✓ Logger works
2. Testing Config Manager...
   ✓ Config Manager works
3. Testing IoC Container...
   ✓ IoC Container works
4. Testing Event Bus...
   ✓ Event Bus works

=== All Core Components Tested ===
```

---

## 🔧 常见问题

### 问题 1: "Conan: command not found"
**解决方案**:
```bash
pip install conan
# 或添加 Python Scripts 目录到 PATH
```

### 问题 2: "PostgreSQL 连接失败"
**检查清单**:
1. PostgreSQL 是否运行？
   ```bash
   psql -U postgres -c "SELECT 1"
   ```
2. `config/config.yaml` 中数据库配置是否正确？
3. 是否运行了 `config/init_db.sql`？

### 问题 3: "某些文件未找到"
**解决方案**:
```bash
# 完整重新编译
rm -rf build
conan install . --output-folder=build --build=missing -s compiler.cppstd=20
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j4
```

### 问题 4: "CMake 找不到 Ninja"
**解决方案**:
```bash
# 使用 Visual Studio 生成器而不是 Ninja
cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
```

---

## 📁 输出文件位置

编译后生成的文件：
```
build/
├── Release/
│   ├── xpp.exe                 # 主服务器
│   ├── test_simple.exe         # 单元测试
│   └── ...其他库文件
├── conan_toolchain.cmake       # Conan 生成的工具链
└── ...其他 CMake 文件
```

---

## 🚀 开发工作流

### 修改代码后增量编译
```bash
# 只编译变更部分（快速）
cmake --build build --config Release -j4
```

### 清理编译输出
```bash
# 清理但保留依赖
rm -rf build/CMakeFiles build/CMakeCache.txt

# 完全清理（包括依赖）
rm -rf build
```

---

## 📝 配置文件

### `config/config.yaml` - 服务器配置

```yaml
server:
  host: "0.0.0.0"              # 监听地址
  port: 8080                   # 监听端口
  threads: 4                   # 工作线程数
  enable_cors: true            # 启用 CORS

database:
  host: "localhost"
  port: 5432
  database: "xpp_db"
  username: "postgres"
  password: ""
  connection_num: 10

logging:
  level: "info"                # trace, debug, info, warn, error, critical
  log_dir: "logs"
  max_file_size: 10485760      # 10MB
  max_files: 5
```

---

## ✅ 验证安装

编译成功标志：
- ✅ 无编译错误
- ✅ 生成了 `xpp.exe` 和 `test_simple.exe`
- ✅ `test_simple.exe` 通过所有测试
- ✅ 服务器能成功启动并监听 8080 端口

---

## 🎯 下一步

1. **在 VS Code 中调试**：
   - 安装 C++ 扩展
   - 配置 `.vscode/launch.json`（见文档）
   - F5 启动调试

2. **创建新模块**：
   - 参考 `src/modules/user/` 结构
   - 在 `main.cpp` 中注册服务

3. **添加新端点**：
   - 在 Controller 中创建路由处理函数
   - 在 `setup_routes()` 中注册

详见 `CLAUDE.md` 中的完整指南。

---

## 💡 技巧

### 一行命令完整编译
```bash
rm -rf build && conan install . --output-folder=build --build=missing -s compiler.cppstd=20 && cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release -j4
```

### 查看编译详细信息
```bash
cmake --build build --config Release -j4 -- VERBOSE=1
```

### 生成特定目标
```bash
cmake --build build --config Release --target test_simple
```

---

祝你编译成功！🎉
