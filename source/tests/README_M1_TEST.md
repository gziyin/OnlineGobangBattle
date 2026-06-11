# M1 基础设施 - 编译与测试指南

> **注意（2026-06-11）**：本文为 M1 阶段历史文档，路径与依赖说明已过时。  
> **当前请以** [`documents/build-and-run-guide.md`](../../documents/build-and-run-guide.md) **与根目录** [`README.md`](../../README.md) **为准**（含 `gobang_server`、`gobang_core`、完整 CTest 标签）。

## 环境要求

- **编译器**: g++ 7+ (支持 C++11)
- **依赖库**: JsonCpp
- **CMake**: 3.10+ (可选，用于自动构建)

---

## 方式一：使用 CMake（推荐）

### Linux / Ubuntu

```bash
# 1. 安装依赖
sudo apt-get install libjsoncpp-dev cmake build-essential

# 2. 创建 build 目录
cd "d:/code/C++ - Online Gobang Battle"
mkdir -p build && cd build

# 3. 配置并编译
cmake ..
make

# 4. 运行测试
./bin/test_base
```

### Windows (MinGW)

```bash
# 1. 安装依赖（使用 vcpkg 或手动安装）
# JsonCpp: https://github.com/open-source-parsers/jsoncpp

# 2. 创建 build 目录
mkdir -p build && cd build

# 3. 配置 CMake（指定 MinGW 编译器）
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# 4. 编译
mingw32-make

# 5. 运行测试
.\bin\test_base.exe
```

---

## 方式二：手动编译（无 CMake）

### Linux

```bash
cd "d:/code/C++ - Online Gobang Battle"

g++ -std=c++11 -I./include \
    tests/test_base.cpp \
    -o test_base \
    -ljsoncpp -lpthread

./test_base
```

### Windows (MinGW)

```bash
cd "d:/code/C++ - Online Gobang Battle"

# 假设 JsonCpp 安装在 E:/Dev/jsoncpp
g++ -std=c++11 -I./include -IE:/Dev/jsoncpp/include \
    tests/test_base.cpp \
    -o test_base.exe \
    -LE:/Dev/jsoncpp/lib -ljsoncpp -lws2_32

test_base.exe
```

---

## 预期输出

```
=== M1 基础设施测试 ===

[1] 测试配置读取...
  server_port:  8080
  db_host:      127.0.0.1
  db_pool_size: 10
  log_level:    INFO
  [PASS] 配置读取成功

[2] 测试 Logger 初始化...
  [PASS] Logger 初始化成功

  [PASS] 四级日志输出完成

[3] 测试 JSON 工具...
  JSON response: {"code":200,"data":{"name":"郭子寅","user_id":1},"message":"ok"}
  Parsed code: 200
  [PASS] JSON 序列化/反序列化成功

[4] 测试时间工具...
  Now: 2026-03-31 10:00:00
  Now ms: 1743386400000
  [PASS] 时间工具测试完成

[5] 测试字符串工具...
  trim: 'hello'
  split size: 4
  valid_username('abc123'): 1
  valid_username('ab'): 0
  valid_password('123456'): 1
  [PASS] 字符串工具测试完成

[6] 测试棋盘序列化...
  board_str[7*15+7]: 1 (expected: 1)
  board_str[7*15+8]: 2 (expected: 2)
  board2[7][7]: 1 (expected: 1)
  board2[7][8]: 2 (expected: 2)
  [PASS] 棋盘序列化测试完成

[7] 测试配置校验...
  [PASS] 配置校验正确抛出异常：Config error: db_password is required

=== 所有测试完成 ===
```

---

## 日志文件检查

测试完成后，检查 `logs/` 目录：

```bash
# Linux
cat logs/test.log

# Windows
type logs\test.log
```

日志格式示例：
```
[2026-03-31 10:00:00] [INFO ] [test_base.cpp:35] Logger initialized
[2026-03-31 10:00:00] [DEBUG] [test_base.cpp:36] debug msg: 42
[2026-03-31 10:00:00] [WARN ] [test_base.cpp:37] warn msg: something looks off
[2026-03-31 10:00:00] [ERROR] [test_base.cpp:38] error msg: fake error
```

---

## 常见问题

### 1. `fatal error: json/json.h: No such file or directory`

**解决**: JsonCpp 未安装或头文件路径未指定

```bash
# Ubuntu
sudo apt-get install libjsoncpp-dev

# Windows (vcpkg)
vcpkg install jsoncpp
```

### 2. `undefined reference to 'Json::Value::...'`

**解决**: 链接时未指定 JsonCpp 库

```bash
# 确保编译命令包含 -ljsoncpp
g++ ... -o test -ljsoncpp
```

### 3. `undefined reference to 'pthread_create'` (Linux)

**解决**: 未链接 pthread 库

```bash
# 确保编译命令包含 -lpthread
g++ ... -o test -ljsoncpp -lpthread
```

### 4. 配置文件加载失败

**解决**: 检查配置文件路径是否正确

```bash
# 确保从 tests/ 目录运行时，相对路径是 ../config/server.conf
# 或者使用绝对路径
```

---

## 下一步

测试通过后，进入 **M2：数据库与用户模块**

- [ ] 安装 MySQL 8.0
- [ ] 创建数据库和用户
- [ ] 实现 `db.hpp`（连接池 + UserTable）
- [ ] 实现 `security.hpp`（bcrypt + JWT）
