# 日志规范

> 日志记录方式

---

## 概述

项目使用自定义异步日志单例 `gobang::Logger`（定义在 `source/include/logger.hpp`）。后台消费者线程通过有界队列（最大 10000 条）批量写入文件，支持自动轮转。业务线程以非阻塞方式推送日志。

---

## 日志级别

| 级别 | 使用场景 |
|------|---------|
| `DEBUG` | 连接池状态、查询详情、心跳 ping |
| `INFO` | 用户注册、登录、匹配成功、组件初始化 |
| `WARN` | 登录失败、无效 token、状态不匹配、断开连接 |
| `ERROR` | DB 连接失败、stmt_prepare 失败、空指针、初始化失败 |

原则：如果是预期情况（用户不存在、用户名重复），用 WARN。如果是 bug 或系统故障，用 ERROR。

---

## 日志格式

```
[2026-03-31 10:00:00] [INFO ] [auth_handler.hpp:80] Auth: 用户注册成功，username=test, user_id=1
```

格式：`[时间戳] [级别] [文件名:行号] 消息`

- 时间戳：`YYYY-MM-DD HH:MM:SS`
- 级别：固定 5 字符宽度（`DEBUG`、`INFO `、`WARN `、`ERROR`）
- 文件名：仅文件名（非完整路径），从 `__FILE__` 自动提取

---

## 使用方式

使用宏 —— 不要直接调用 `Logger::instance().log()`：

```cpp
LOG_DEBUG("DBPool: 获取连接成功（剩余: " << _pool.size() << "）");
LOG_INFO("Auth: 用户登录成功，username=" << username << ", user_id=" << user_id);
LOG_WARN("WebSocketHandler: 无效 token");
LOG_ERROR("UserTable: mysql_stmt_prepare 失败: " << mysql_stmt_error(stmt));
```

宏自动捕获 `__FILE__` 和 `__LINE__`，使用 `ostringstream` 实现流式消息拼接。

---

## 配置

通过 `server.conf`：

```
log_file = logs/server.log
log_level = DEBUG
log_max_bytes = 10485760
```

- `log_file`：输出路径
- `log_level`：最低输出级别（DEBUG/INFO/WARN/ERROR）
- `log_max_bytes`：轮转阈值（默认 10MB）。超出后旧文件重命名为 `server.log.20260525_143000.bak`

---

## 应该记录什么

- **每个客户端操作**：注册、登录、匹配开始/取消、游戏事件
- **每个初始化步骤**：组件初始化、配置加载
- **每个错误**：DB 失败、无效输入、状态违规
- **状态转换**：用户上线/下线、状态变更

---

## 不应该记录什么

- **密码**（明文或哈希）—— 永远不记录 `password` 或 `password_hash`
- **JWT 密钥** —— 永远不记录 secret key
- **完整 JWT token** —— 只记录"token 已验证"，不记录 token 本身
- **高频心跳数据** —— ping 用 DEBUG 级别

---

## 常见错误

- **用 `std::cout` 代替 LOG 宏** —— 始终用 LOG_* 保持一致性和异步缓冲
- **对业务逻辑失败用 ERROR 级别** —— "用户名已存在"是 WARN，不是 ERROR
- **无条件构造昂贵的日志消息** —— 宏会先检查级别再格式化，但如果消息本身构造代价高，应避免

---

## 生命周期注意事项（测试/多次启动场景）

`gobang::Logger` 是异步单例，内部包含后台线程。需要特别注意：

- **禁止重复调用 `Logger::instance().init(...)`**：若 `init` 内部对 `std::thread` 做赋值/重建，重复 init 可能触发 `std::terminate`。
- **测试推荐做法**：
  - 用 `std::once_flag + std::call_once` 保证只初始化一次；
  - 或在测试可执行文件的 `main()` 中初始化一次，后续用例不再调用 `init`。
