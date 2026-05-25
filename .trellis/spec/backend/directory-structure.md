# 后端目录结构

> 后端代码的组织方式

---

## 概述

本项目是 C++ 在线五子棋对战游戏服务器。所有服务端代码均为**纯头文件实现**（`.hpp` 文件内含 `inline` 实现），当前阶段不做 `.hpp` / `.cpp` 拆分。

---

## 目录布局

```
source/
├── CMakeLists.txt              # 构建系统（CMake 3.10+, C++11）
├── config.h.in                 # CMake 生成的配置头文件模板
├── config/
│   ├── server.conf             # 运行时配置（已 gitignore）
│   └── server.conf.example     # 配置示例
├── include/                    # 所有后端代码（纯头文件）
│   ├── auth_handler.hpp        # 注册/登录请求处理
│   ├── auth_middleware.hpp      # 从 Authorization 头提取 JWT
│   ├── block_queue.hpp         # 线程安全阻塞队列（模板）
│   ├── connection_manager.hpp  # WebSocket 连接与用户的映射管理
│   ├── db.hpp                  # MySQL 连接池（RAII）
│   ├── logger.hpp              # 异步日志单例
│   ├── matcher.hpp             # 匹配引擎
│   ├── matcher_interface.hpp   # 匹配器抽象接口
│   ├── online.hpp              # 在线用户状态管理
│   ├── security.hpp            # PBKDF2 密码哈希 + JWT（jwt-cpp）
│   ├── user_table.hpp          # 用户表数据访问层（DAL）
│   ├── util.hpp                # JSON 工具、Config 解析、字符串工具
│   └── websocket_handler.hpp   # WebSocket 事件分发器
├── logs/                       # 运行时日志输出（已 gitignore）
└── tests/                      # 所有测试文件
    ├── test_base.cpp           # 基础冒烟测试（手动 assert）
    ├── test_db_pool.cpp        # 连接池集成测试
    ├── test_user_table.cpp     # 用户表集成测试
    ├── test_security.cpp       # PBKDF2 + JWT 单元测试
    ├── test_auth_api.cpp       # 认证 API 集成测试
    ├── test_online.cpp         # 在线管理器单元测试
    ├── test_block_queue.cpp    # 阻塞队列单元测试
    ├── test_matcher.cpp        # 匹配器单元测试
    ├── test_connection_manager.cpp
    ├── test_websocket_m3_flow.cpp  # 端到端 WebSocket 流程测试
    └── websocket_smoke.cpp     # 完整服务器组装（冒烟 + 可运行服务器）
```

---

## 模块组织

- **所有后端代码**放在 `source/include/` 下，以 `.hpp` 为扩展名
- **每个模块**是一个独立的 `.hpp` 文件，包含类声明 + `inline` 实现
- **命名空间**：所有代码在 `gobang::` 下，子命名空间如 `gobang::auth::`、`gobang::util::`
- **测试**放在 `source/tests/`，每个模块对应一个 `test_<模块名>.cpp`
- **配置模板**放在 `source/config/server.conf.example`

---

## 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| 头文件 | `snake_case.hpp` | `user_table.hpp`、`auth_handler.hpp` |
| 测试文件 | `test_<模块名>.cpp` | `test_matcher.cpp` |
| 命名空间 | `gobang::` | `gobang::auth::handle_login()` |
| 类名 | `PascalCase` | `UserTable`、`WebSocketHandler` |
| 方法名 | `snake_case` | `handle_register()`、`get_connection()` |
| 私有成员 | `_前缀` | `_pool`、`_mtx`、`_running` |
| 常量 | `UPPER_SNAKE_CASE` | `MAX_QUEUE_SIZE`、`ER_DUP_ENTRY` |

---

## 新增模块步骤

1. 创建 `source/include/<模块名>.hpp`
2. 在 `namespace gobang { ... }` 下编写类
3. 所有方法实现标记为 `inline`，写在 `.hpp` 文件内
4. 创建 `source/tests/test_<模块名>.cpp`，使用 GTest
5. 在 `CMakeLists.txt` 中用 `add_gobang_test()` + `register_gobang_ctest()` 注册

---

## 参考示例

- 模块组织参考：`source/include/user_table.hpp` —— 接口与 inline 实现分离清晰
- 测试注册参考：`source/CMakeLists.txt` 第 129-209 行的 `add_gobang_test()` + `register_gobang_ctest()` 模式
