# 🚀 快速开始指南

## 5 分钟快速开始

### 前置条件
- Visual Studio 2022+ 或其他 C++20 编译器
- CMake 3.20+
- Conan 2.0+
- PostgreSQL 12+

---

## 步骤 1: 环境准备

```bash
# 安装 Conan（如果未安装）
pip install conan

# 安装 PostgreSQL（如果未安装）
# Windows: https://www.postgresql.org/download/windows/
```

---

## 步骤 2: 克隆和进入项目

```bash
cd d:\workspace\xpp
```

---

## 步骤 3: 编译

```bash
# 一行命令完整编译（推荐）
rm -rf build && conan install . --output-folder=build --build=missing -s compiler.cppstd=20 && cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release -j4
```

或按步骤编译：

```bash
# 1. 清理
rm -rf build

# 2. 安装依赖
conan install . --output-folder=build --build=missing -s compiler.cppstd=20

# 3. 生成 CMake
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# 4. 编译
cmake --build build --config Release -j4
```

✅ **编译成功**: 看到 `[100%] Built target xpp` 字样

---

## 步骤 4: 初始化数据库

```bash
# 在 pgAdmin 或命令行中执行
psql -U postgres -f config/init_db.sql
```

---

## 步骤 5: 运行应用

### 运行主服务器

```bash
./build/Release/xpp.exe
```

**预期输出**:
```
[2026-01-09 10:30:45] [info] === XPP WeChat Backend Starting ===
[2026-01-09 10:30:45] [info] Memory cache initialized
[2026-01-09 10:30:45] [info] Server starting on 0.0.0.0:8080
```

### 测试 API

在另一个终端：

```bash
# 健康检查
curl http://localhost:8080/health

# 响应应该是
# {"status":"ok","timestamp":1673275845125}
```

---

## 快速 API 测试

### 1. 注册用户

```bash
curl -X POST http://localhost:8080/api/auth/register \
  -H "Content-Type: application/json" \
  -d '{
    "username": "alice",
    "password": "pwd123",
    "email": "alice@example.com"
  }'
```

### 2. 登录

```bash
curl -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "alice",
    "password": "pwd123"
  }'
```

**响应**（保存 token）:
```json
{
  "token": "eyJhbGciOiJIUzI1NiIs...",
  "user": {
    "id": 1,
    "username": "alice",
    "email": "alice@example.com"
  }
}
```

### 3. 获取当前用户

```bash
# 替换 <TOKEN> 为上面的 token
curl http://localhost:8080/api/auth/me \
  -H "Authorization: Bearer <TOKEN>"
```

### 4. 登出

```bash
curl -X POST http://localhost:8080/api/auth/logout \
  -H "Authorization: Bearer <TOKEN>"
```

---

## ⚡ 快速命令参考

```bash
# 编译
cmake --build build --config Release -j4

# 运行
./build/Release/xpp.exe

# 测试
./build/Release/test_simple.exe

# 清理
rm -rf build

# 查看日志
tail -f logs/xpp.log
```

---

## 📚 文档导航

需要更多信息？

| 想要... | 查看文档 |
|--------|--------|
| 详细编译步骤 | `BUILD_GUIDE.md` |
| 项目架构 | `CLAUDE.md` |
| MemoryCache 细节 | `MEMORY_CACHE_API.md` |
| 项目结构 | `PROJECT_STRUCTURE.md` |
| 迁移总结 | `MIGRATION_SUMMARY.md` |

---

## ✅ 完成清单

- [ ] 安装 Conan 和 CMake
- [ ] 克隆项目
- [ ] 编译项目
- [ ] 初始化 PostgreSQL 数据库
- [ ] 运行服务器
- [ ] 测试 API 端点
- [ ] 查看日志文件

---

## 🆘 故障排除

### 编译失败

```bash
# 完整清理和重新编译
rm -rf build
conan install . --output-folder=build --build=missing -s compiler.cppstd=20
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j4
```

### 数据库连接失败

```bash
# 检查 PostgreSQL 是否运行
psql -U postgres -c "SELECT 1"

# 如果找不到 psql，添加到 PATH
# PostgreSQL 默认安装在 C:\Program Files\PostgreSQL\...\bin
```

### 服务器无法启动

1. 检查 8080 端口是否被占用
2. 检查日志文件：`logs/xpp.log`
3. 确保 PostgreSQL 和 config.yaml 配置正确

---

## 🎯 下一步

1. **查看源代码**: `src/modules/user/` 了解项目结构
2. **阅读 CLAUDE.md**: 学习架构和模式
3. **创建新端点**: 基于 auth_controller 的示例
4. **部署**: 参考 BUILD_GUIDE.md

---

## 💡 常见问题

**Q: 需要 Redis 吗？**
A: 不需要！使用内存缓存，无需外部依赖。

**Q: 支持多进程吗？**
A: 目前不支持。如需多进程支持，请迁移到 Redis（见 MIGRATION_SUMMARY.md）。

**Q: 会话数据会丢失吗？**
A: 是的。服务器重启后会话数据丢失。对于持久化，请切换到 Redis。

**Q: 如何修改端口？**
A: 编辑 `config/config.yaml` 中的 `server.port`。

**Q: 如何启用 HTTPS？**
A: 这需要在 Drogon 配置中启用，参考 Drogon 文档。

---

## 📞 需要帮助？

- 检查 `logs/xpp.log` 中的错误信息
- 查看 `BUILD_GUIDE.md` 中的故障排除部分
- 阅读 `CLAUDE.md` 了解项目架构
- 参考 `MEMORY_CACHE_API.md` 学习缓存 API

---

恭喜！🎉 你已经准备好开始开发了！

快乐编码！ 💻
