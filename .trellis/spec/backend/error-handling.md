# 错误处理

> 错误处理方式

---

## 概述

本项目采用**快速失败、记录并返回**模式。没有自定义异常类。错误通过 JSON 响应（REST）或 error 事件消息（WebSocket）传递给客户端。

---

## 错误类型

未定义自定义错误类型。错误在两个层面处理：

1. **内部错误**（DB 失败、空指针）：用 `LOG_ERROR` 记录，函数返回 `false` 或 `0`
2. **客户端错误**：以 JSON 格式返回错误码和消息

---

## 错误处理模式

### 模式 1：记录并返回 false（内部/DB 错误）

用于 DAL 类和内部函数：

```cpp
if (!conn) {
    LOG_ERROR("UserTable: insert 在 init 之前被调用");
    return 0;  // 或 false
}
```

参考：`source/include/user_table.hpp`（每个方法）

### 模式 2：校验后返回错误 JSON（客户端错误）

用于请求处理器：

```cpp
if (username.length() < 3 || username.length() > 32) {
    response["success"] = false;
    response["message"] = "用户名长度必须在 3-32 字符之间";
    return response;
}
```

参考：`source/include/auth_handler.hpp` —— `handle_register()`、`handle_login()`

### 模式 3：致命初始化错误抛异常

仅在启动阶段使用（连接池初始化、日志器初始化）：

```cpp
if (!conn) {
    throw std::runtime_error("DBPool: 创建连接失败");
}
```

参考：`source/include/db.hpp` —— `init()`

---

## API 错误响应

### REST（HTTP）响应

标准 JSON 格式：

```json
{"success": true, "message": "注册成功", "user_id": 123}
{"success": false, "message": "用户名已存在"}
```

始终包含 `success`（bool）和 `message`（string）。成功时的附加字段因接口而异。

参考：`source/include/auth_handler.hpp`

### WebSocket 错误响应

标准 JSON 格式：

```json
{"event": "error", "data": {"code": 4001, "message": "invalid token"}}
```

错误码：
| 含义 | 错误码 |
|------|--------|
| 无效 JSON | 4000 |
| 无效/缺失 token，未认证 | 4001 |
| 未知事件 | 4002 |
| 用户状态不正确（如不在大厅） | 4003 |
| 入队失败 | 4004 |
| 取消失败 | 4005 |

参考：`source/include/websocket_handler.hpp` —— `make_error()`

---

## 禁止做法

- **请求处理器中抛异常** —— 使用返回值携带错误信息
- **向客户端暴露内部错误详情** —— 记录详情，返回通用消息
- **用 `assert()` 做运行时校验** —— 用 if/else + 正确的错误响应
- **忽略 `mysql_stmt_*` 的返回值**

---

## 常见错误

- **返回错误响应前忘记设置 `response["success"] = false`**
- **对预期失败使用 ERROR 级别日志**（如用户不存在应该是 WARN 或 DEBUG，不是 ERROR）
- **错误路径上不关闭 mysql_stmt** —— 每个 `mysql_stmt_init` 之后的 `return` 都必须调用 `mysql_stmt_close`
