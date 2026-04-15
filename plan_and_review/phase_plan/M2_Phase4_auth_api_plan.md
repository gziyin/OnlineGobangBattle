# M2 Phase 4: HTTP API 整合 - 用户注册/登录/认证 - 实现计划

> **项目**: C++ 在线五子棋对战系统
> **阶段**: M2 Phase 4 HTTP API 整合
> **制定日期**: 2026-04-15
> **前置条件**: M2 Phase 1 数据库连接池 ✅, M2 Phase 2 用户数据访问层 ✅, M2 Phase 3 安全模块 ✅

---

## 一、背景

### 1.1 为什么要做 Phase 4

- Phase 1 提供了数据库连接能力 (db.hpp)
- Phase 2 提供了用户数据的 CRUD 操作 (user_table.hpp)
- Phase 3 提供了密码加密和 JWT 认证 (security.hpp)
- **Phase 4 需要提供 HTTP API 接口**：
  - 用户注册 API
  - 用户登录 API
  - 用户信息查询 API（需要 JWT 认证）
- 完成 M2 阶段所有功能，实现完整的用户模块

### 1.2 技术选型

| 功能 | 选型 | 理由 |
|------|------|------|
| 业务逻辑层 | 纯 C++ 函数 | Phase 4 专注业务逻辑，HTTP 路由留到 M3 WebSocket 阶段 |
| 请求/响应格式 | JSON | 前后端通用格式 |
| 认证方式 | JWT Bearer Token | 无状态，Phase 3 已实现 |

---

## 二、预期成果

1. `source/include/auth_handler.hpp` - 认证处理器（注册/登录逻辑）
2. `source/include/auth_middleware.hpp` - JWT 认证中间件
3. `source/tests/test_auth_api.cpp` - API 集成测试（6 项测试）
4. `source/CMakeLists.txt` - 添加 test_auth_api 测试目标

---

## 三、前置准备

### 3.1 环境验证

在虚拟机中执行：

```bash
# 1. 检查前置 Phase 已完成
ls source/include/db.hpp        # 应存在
ls source/include/user_table.hpp # 应存在
ls source/include/security.hpp   # 应存在

# 2. 检查 MySQL 服务
systemctl status mysqld --no-pager | head -3
```

### 3.2 UserTable 接口说明

> **注意**：Phase 2 已实现的 `UserTable` 提供两个查询接口，Phase 4 需要区分使用：
>
> | 接口 | 返回字段 | 使用场景 |
> |------|----------|----------|
> | `select_for_auth()` | 包含 `password_hash` | **登录验证**时使用，需要获取哈希进行密码比对 |
> | `select_by_username()` | 不包含 `password_hash` | **注册检查**或**公开信息展示**时使用，避免密码泄露 |
>
> `select_for_auth` 与 `select_by_username` 的区别：前者在查询结果中包含 password_hash 字段，专用于登录密码验证场景。

---

## 四、实现方案

### 4.1 auth_handler.hpp 接口设计

```cpp
#pragma once
#include <string>
#include "db.hpp"
#include "user_table.hpp"
#include "security.hpp"
#include <json/json.h>

namespace gobang {
namespace auth {

/**
 * @brief 用户注册请求处理
 * @param user_table 用户表操作对象
 * @param username 用户名
 * @param password 原始密码
 * @return JSON 响应 {success: bool, message: string, user_id: number}
 *
 * 流程：
 * 1. 参数校验（用户名长度 3-32，密码长度 6-128）
 * 2. 检查用户名是否已存在
 * 3. PBKDF2 加密密码
 * 4. 插入用户表
 * 5. 返回结果
 */
Json::Value handle_register(UserTable& user_table,
                            const std::string& username,
                            const std::string& password);

/**
 * @brief 用户登录请求处理
 * @param user_table 用户表操作对象
 * @param username 用户名
 * @param password 原始密码
 * @return JSON 响应 {success: bool, message: string, token: string, user: {...}}
 *
 * 流程：
 * 1. 参数校验
 * 2. 查询用户（使用 select_for_auth，包含 password_hash）
 * 3. PBKDF2 验证密码
 * 4. 生成 JWT Token
 * 5. 返回 token 和用户信息（不包含 password_hash）
 */
Json::Value handle_login(UserTable& user_table,
                         const std::string& username,
                         const std::string& password);

} // namespace auth
} // namespace gobang
```

### 4.2 auth_middleware.hpp 接口设计

```cpp
#pragma once
#include <string>
#include <json/json.h>
#include "security.hpp"

namespace gobang {
namespace auth {

/**
 * @brief 从 HTTP Authorization Header 中提取并验证 JWT
 * @param auth_header HTTP Header 中的 Authorization 值
 * @return 验证成功返回 user_id，失败返回 0
 *
 * Header 格式：Authorization: Bearer <jwt_token>
 */
int64_t extract_user_id_from_token(const std::string& auth_header);

/**
 * @brief 检查请求是否已认证
 * @param auth_header HTTP Authorization Header
 * @return 已认证返回 true，未认证返回 false
 */
bool is_authenticated(const std::string& auth_header);

} // namespace auth
} // namespace gobang
```

### 4.3 实现细节

#### 4.3.1 handle_register 实现

```cpp
Json::Value handle_register(UserTable& user_table,
                            const std::string& username,
                            const std::string& password) {
    Json::Value response;
    
    // 1. 参数校验
    if (username.length() < 3 || username.length() > 32) {
        response["success"] = false;
        response["message"] = "用户名长度必须在 3-32 字符之间";
        return response;
    }
    if (password.length() < 6 || password.length() > 128) {
        response["success"] = false;
        response["message"] = "密码长度必须在 6-128 字符之间";
        return response;
    }
    
    // 2. 检查用户名是否已存在
    Json::Value existing_user;
    if (user_table.select_by_username(username, existing_user)) {
        response["success"] = false;
        response["message"] = "用户名已存在";
        return response;
    }
    
    // 3. PBKDF2 加密密码
    std::string password_hash = security::pbkdf2_hash(password);
    if (password_hash.empty()) {
        response["success"] = false;
        response["message"] = "密码加密失败";
        return response;
    }
    
    // 4. 插入用户表（TOCTOU 防护：insert 失败后再次检查用户名是否存在）
    int64_t user_id = user_table.insert(username, password_hash);
    if (user_id == 0) {
        // 并发情况下：先检查后插入可能存在竞态，insert 因 UNIQUE 约束失败
        // 再次检查用户名是否存在，给用户准确的错误信息
        Json::Value existing_user2;
        if (user_table.select_by_username(username, existing_user2)) {
            response["success"] = false;
            response["message"] = "用户名已存在";
        } else {
            response["success"] = false;
            response["message"] = "用户注册失败";
        }
        return response;
    }
    
    // 5. 返回成功
    response["success"] = true;
    response["message"] = "注册成功";
    response["user_id"] = static_cast<Json::Int64>(user_id);
    LOG_INFO("Auth: 用户注册成功, username=" << username << ", user_id=" << user_id);
    return response;
}
```

#### 4.3.2 handle_login 实现

```cpp
Json::Value handle_login(UserTable& user_table,
                         const std::string& username,
                         const std::string& password) {
    Json::Value response;
    
    // 1. 参数校验
    if (username.empty() || password.empty()) {
        response["success"] = false;
        response["message"] = "用户名和密码不能为空";
        return response;
    }
    
    // 2. 查询用户（使用 select_for_auth 获取 password_hash）
    Json::Value user;
    if (!user_table.select_for_auth(username, user)) {
        response["success"] = false;
        response["message"] = "用户名或密码错误";
        return response;
    }
    
    // 3. PBKDF2 验证密码
    std::string stored_hash = user["password_hash"].asString();
    if (!security::pbkdf2_verify(password, stored_hash)) {
        response["success"] = false;
        response["message"] = "用户名或密码错误";
        LOG_WARN("Auth: 登录密码错误, username=" << username);
        return response;
    }
    
    // 4. 生成 JWT Token
    int64_t user_id = user["id"].asInt64();
    std::string token = security::jwt_generate(user_id);
    if (token.empty()) {
        response["success"] = false;
        response["message"] = "Token 生成失败";
        return response;
    }
    
    // 5. 返回结果（移除 password_hash）
    response["success"] = true;
    response["message"] = "登录成功";
    response["token"] = token;
    
    Json::Value user_info;
    user_info["id"] = static_cast<Json::Int64>(user_id);
    user_info["username"] = user["username"];
    user_info["score"] = user["score"];
    user_info["total_count"] = user["total_count"];
    user_info["win_count"] = user["win_count"];
    user_info["status"] = user["status"];
    response["user"] = user_info;
    
    LOG_INFO("Auth: 用户登录成功, username=" << username << ", user_id=" << user_id);
    return response;
}
```

#### 4.3.3 auth_middleware 实现

```cpp
int64_t extract_user_id_from_token(const std::string& auth_header) {
    // Header 格式：Bearer <token>
    const std::string bearer_prefix = "Bearer ";
    
    if (auth_header.length() < bearer_prefix.length()) {
        return 0;
    }
    
    // 检查前缀（大小写不敏感）
    // 注：RFC 6750 规定 Bearer 应大小写敏感，此处为宽松实现，兼容小写
    std::string prefix = auth_header.substr(0, bearer_prefix.length());
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
    
    if (prefix != "bearer ") {
        return 0;
    }
    
    // 提取 token
    std::string token = auth_header.substr(bearer_prefix.length());
    
    // 验证 token
    return security::jwt_verify(token);
}

bool is_authenticated(const std::string& auth_header) {
    return extract_user_id_from_token(auth_header) > 0;
}
```

### 4.4 CMakeLists.txt 修改

在 `source/CMakeLists.txt` 中添加：

```cmake
# ─── 测试目标：test_auth_api ────────────────────────────────
add_executable(test_auth_api tests/test_auth_api.cpp)

target_link_libraries(test_auth_api
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
)

set_target_properties(test_auth_api PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)
```

---

## 五、测试用例设计

### 5.1 测试文件：tests/test_auth_api.cpp

| 测试 # | 名称 | 测试内容 | 断言数 |
|--------|------|----------|--------|
| 1 | 注册_成功 | 正常注册新用户 | 3 |
| 2 | 注册_重复用户名 | 重复注册应失败 | 2 |
| 3 | 注册_参数校验 | 用户名/密码长度校验 | 4 |
| 4 | 登录_成功 | 正确密码登录成功并返回 token | 4 |
| 5 | 登录_密码错误 | 错误密码登录失败 | 2 |
| 6 | Token 验证 | 有效 token 提取 user_id | 3 |

### 5.2 测试代码结构

```cpp
#include "auth_handler.hpp"
#include "auth_middleware.hpp"
#include "db.hpp"
#include "user_table.hpp"
#include "util.hpp"
#include <gtest/gtest.h>

class AuthApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化 DBPool
        gobang::util::Config cfg = gobang::util::load_config("config/server.conf");
        pool_.init(cfg);
        user_table_.init(&pool_);
        
        // 清空 user 表
        auto conn = pool_.get_connection();
        int ret = mysql_query(conn.get(), "TRUNCATE TABLE user");
        ASSERT_EQ(ret, 0) << "TRUNCATE TABLE failed: " << mysql_error(conn.get());
    }
    
    DBPool pool_;
    UserTable user_table_;
};

TEST_F(AuthApiTest, Register_Success) {
    Json::Value result = gobang::auth::handle_register(
        user_table_, "testuser", "password123");
    
    ASSERT_TRUE(result["success"].asBool());              // 断言 1
    ASSERT_EQ(result["message"].asString(), "注册成功");   // 断言 2
    ASSERT_GT(result["user_id"].asInt64(), 0);           // 断言 3
}

TEST_F(AuthApiTest, Register_DuplicateUsername) {
    // 第一次注册
    gobang::auth::handle_register(user_table_, "testuser", "password123");
    
    // 重复注册
    Json::Value result = gobang::auth::handle_register(
        user_table_, "testuser", "password456");
    
    ASSERT_FALSE(result["success"].asBool());             // 断言 1
    ASSERT_EQ(result["message"].asString(), "用户名已存在");  // 断言 2
}

TEST_F(AuthApiTest, Register_ParameterValidation) {
    // 用户名太短
    Json::Value r1 = gobang::auth::handle_register(user_table_, "ab", "password123");
    ASSERT_FALSE(r1["success"].asBool());  // 断言 1
    
    // 用户名太长
    Json::Value r2 = gobang::auth::handle_register(
        user_table_, std::string(33, 'a'), "password123");
    ASSERT_FALSE(r2["success"].asBool());  // 断言 2
    
    // 密码太短
    Json::Value r3 = gobang::auth::handle_register(user_table_, "testuser", "12345");
    ASSERT_FALSE(r3["success"].asBool());  // 断言 3
    
    // 密码太长
    Json::Value r4 = gobang::auth::handle_register(
        user_table_, "testuser", std::string(129, 'a'));
    ASSERT_FALSE(r4["success"].asBool());  // 断言 4
}

TEST_F(AuthApiTest, Login_Success) {
    // 先注册
    gobang::auth::handle_register(user_table_, "testuser", "password123");
    
    // 登录
    Json::Value result = gobang::auth::handle_login(
        user_table_, "testuser", "password123");
    
    ASSERT_TRUE(result["success"].asBool());              // 断言 1
    ASSERT_FALSE(result["token"].asString().empty());    // 断言 2
    ASSERT_EQ(result["user"]["username"].asString(), "testuser");  // 断言 3
    ASSERT_FALSE(result["user"].isMember("password_hash"));  // 断言 4: 不返回密码
}

TEST_F(AuthApiTest, Login_WrongPassword) {
    // 先注册
    gobang::auth::handle_register(user_table_, "testuser", "password123");
    
    // 错误密码登录
    Json::Value result = gobang::auth::handle_login(
        user_table_, "testuser", "wrongpassword");
    
    ASSERT_FALSE(result["success"].asBool());                 // 断言 1
    ASSERT_EQ(result["message"].asString(), "用户名或密码错误");  // 断言 2
}

TEST_F(AuthApiTest, Token_Verify) {
    // 注册并登录获取 token
    gobang::auth::handle_register(user_table_, "testuser", "password123");
    Json::Value login_result = gobang::auth::handle_login(
        user_table_, "testuser", "password123");
    
    std::string token = login_result["token"].asString();
    std::string auth_header = "Bearer " + token;
    
    // 验证 token
    int64_t user_id = gobang::auth::extract_user_id_from_token(auth_header);
    
    ASSERT_GT(user_id, 0);                                          // 断言 1
    ASSERT_EQ(user_id, login_result["user"]["id"].asInt64());     // 断言 2
    ASSERT_TRUE(gobang::auth::is_authenticated(auth_header));      // 断言 3
}
```

---

## 六、验收标准

| 标准 | 要求 | 验证方式 |
|------|------|----------|
| 编译通过 | 无警告、无错误 | `cmake --build .` |
| 测试 1 通过 | 注册成功返回 user_id | 断言 `success=true`, `user_id>0` |
| 测试 2 通过 | 重复用户名注册失败 | 断言 `success=false` |
| 测试 3 通过 | 参数校验正确 | 4 种边界情况都失败 |
| 测试 4 通过 | 登录成功返回 token 和用户信息 | 断言 `token` 非空，用户信息正确 |
| 测试 5 通过 | 错误密码登录失败 | 断言 `success=false` |
| 测试 6 通过 | Token 验证正确提取 user_id | 断言返回的 user_id 正确 |
| 代码规范 | 遵循 C++11、错误处理完善 | 代码审查 |

---

## 七、关键文件路径

| 文件 | 路径 | 状态 |
|------|------|------|
| `auth_handler.hpp` | `source/include/auth_handler.hpp` | 待创建 |
| `auth_middleware.hpp` | `source/include/auth_middleware.hpp` | 待创建 |
| `test_auth_api.cpp` | `source/tests/test_auth_api.cpp` | 待创建 |
| `CMakeLists.txt` | `source/CMakeLists.txt` | 待修改 |
| `db.hpp` | `source/include/db.hpp` | 已存在 ✅ |
| `user_table.hpp` | `source/include/user_table.hpp` | 已存在 ✅ |
| `security.hpp` | `source/include/security.hpp` | 已存在 ✅ |

---

## 八、复用现有代码

| 模块 | 用途 |
|------|------|
| `DBPool::ConnGuard` | RAII 方式获取数据库连接 |
| `UserTable` | 用户数据增删改查 |
| `security::pbkdf2_hash/verify` | 密码加密验证 |
| `security::jwt_generate/verify` | Token 生成验证 |
| `util::Config` | 读取配置文件 |
| `LOG_INFO/WARN/ERROR` | 日志记录 |

---

## 九、风险与注意

| 风险 | 概率 | 影响 | 应对措施 |
|------|------|------|----------|
| 前置 Phase 未完成 | 低 | **高** | 确认 Phase 1/2/3 测试通过 |
| SQL 注入风险 | 低 | **高** | 复用 Phase 2 的预处理语句 |
| 密码泄露 | 低 | **高** | 确保不返回 password_hash |
| Token 伪造 | 低 | **高** | 复用 Phase 3 的 jwt_verify |
| 并发注册冲突(TOCTOU) | 低 | 中 | insert 失败后重查，给出准确错误信息 |

---

## 十、验证步骤

### 编译验证
```bash
cd source/build
cmake .. && cmake --build .
# 应成功生成 test_auth_api 目标
```

### 运行测试
```bash
./bin/test_auth_api
# 输出：6 项测试全部通过
```

### 数据库验证
```sql
-- 验证用户已创建
SELECT id, username, score FROM user;

-- 验证密码哈希格式正确（应包含冒号分隔的 salt:hash）
SELECT LEFT(password_hash, 30) FROM user LIMIT 1;
```

---

## 十一、变更历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| v1.0 | 2026-04-15 | 初始版本 |
| v1.1 | 2026-04-15 | 删除 cpp-httplib 技术选型；修复 user_id 类型统一使用 Json::Int64；添加 TOCTOU 处理逻辑；添加 select_for_auth/select_by_username 说明；添加 Bearer 大小写处理注释；测试 SetUp 添加 TRUNCATE 错误检查 |

---

*文档版本：v1.1*
*制定日期：2026-04-15*
*修订日期：2026-04-15*
