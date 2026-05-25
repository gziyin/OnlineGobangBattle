# 数据库规范

> 数据库使用模式与约定

---

## 概述

本项目使用**原生 MySQL C API**（`libmysqlclient`）+ **预处理语句**。无 ORM，无查询构建器，无迁移系统。数据库 Schema 手动管理。

---

## 连接管理

使用 `DBPool` 类（`source/include/db.hpp`）进行连接池管理。

```cpp
// 初始化（启动时执行一次）
DBPool pool;
pool.init(cfg);  // cfg 是 util::Config，包含 db_host, db_port 等

// 获取连接（RAII 方式，离开作用域自动归还）
auto conn = pool.get_connection();  // 返回 ConnGuard（unique_ptr<MYSQL>）
if (!conn) {
    LOG_ERROR("获取连接失败");
    return false;
}
// 使用 conn.get() 获取原始 MYSQL* 指针
```

关键规则：
- 始终使用 `pool.get_connection()` —— 不要直接调用 `mysql_init()` / `mysql_real_connect()`
- `ConnGuard` 是 `std::unique_ptr<MYSQL, 自定义删除器>` —— 离开作用域自动归还连接
- 默认超时 3000ms

---

## 查询模式

所有查询使用**预处理语句**（`mysql_stmt_*` API）。模式如下：

```cpp
// 1. 准备
auto stmt = mysql_stmt_init(conn.get());
if (!stmt) { /* 错误处理 */ }
if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
    LOG_ERROR("prepare 失败: " << mysql_stmt_error(stmt));
    mysql_stmt_close(stmt);
    return false;
}

// 2. 绑定参数
MYSQL_BIND params[2];
memset(params, 0, sizeof(params));
params[0].buffer_type = MYSQL_TYPE_STRING;
params[0].buffer = (char*)value.c_str();
params[0].buffer_length = value.size();
// ... 绑定每个参数

if (mysql_stmt_bind_param(stmt, params) != 0) { /* 错误，关闭 stmt */ }

// 3. 执行
if (mysql_stmt_execute(stmt) != 0) {
    int err = mysql_stmt_errno(stmt);
    if (err == ER_DUP_ENTRY) { /* 处理重复 */ }
    /* 错误，关闭 stmt */
}

// 4. SELECT：绑定结果列，然后 mysql_stmt_fetch()
// 5. 始终关闭：mysql_stmt_close(stmt)
```

参考：`source/include/user_table.hpp` —— `insert()`、`select_for_auth()`、`update_score_match()`

---

## 事务处理

手动事务控制，使用原始 SQL：

```cpp
mysql_query(conn.get(), "START TRANSACTION");
// ... 执行预处理语句 ...
// 任何失败：mysql_query(conn.get(), "ROLLBACK"); return false;
// 成功：mysql_query(conn.get(), "COMMIT");
```

参考：`source/include/user_table.hpp` —— `update_score_match()` 是完整的事务示例，含失败回滚。

---

## 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| 表名 | `snake_case`，单数 | `user`（不用 `users`） |
| 列名 | `snake_case` | `user_id`、`password_hash`、`total_count` |
| 主键 | `id`（自增） | `user.id` |
| 外键 | `<引用表>_id` | `user_id` |
| 索引 | `idx_<表>_<列>` | `idx_user_username` |

---

## DAL 层模式

数据访问层类（如 `UserTable`）遵循以下模式：
- 每个表（或相关表组）对应一个类
- `init(DBPool*)` 方法注入连接池
- 方法返回 `bool`（成功/失败）或 `int64_t`（插入的 ID，0 表示失败）
- SELECT 结果通过 `Json::Value& out` 输出参数返回
- DAL 类**不做业务逻辑校验** —— 校验属于 handler 层
- DAL 类通过 `LOG_ERROR` 记录错误，不抛异常

---

## 常见错误

- **忘记关闭 stmt**：每个 `return` 路径前都要调用 `mysql_stmt_close(stmt)`
- **不检查 `get_connection()` 返回值**：超时时可能返回 `nullptr`
- **用 `mysql_query()` 拼接参数**：始终用预处理语句防止 SQL 注入
- **手动归还连接**：用 RAII 的 `ConnGuard` 模式，不要手动调用 `return_connection()`
