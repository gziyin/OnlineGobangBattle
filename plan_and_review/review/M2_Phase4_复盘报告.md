# M2 Phase 4: auth_handler.hpp - 用户认证 HTTP API - 复盘报告

> **项目**: C++ 在线五子棋对战系统
> **阶段**: M2 Phase 4 用户认证 HTTP API
> **完成日期**: 2026-04-15
> **报告人**: AI 助手

---

## 一、执行摘要

### 1.1 里程碑状态

| 阶段 | 名称 | 状态 | 完成日期 |
|------|------|------|----------|
| ✅ M1 | 环境搭建与基础框架 | 已完成 | 2026-03-31 |
| ✅ M2-P1 | 数据库连接池 | 已完成 | 2026-04-09 |
| ✅ M2-P2 | 用户数据访问层 | 已完成 | 2026-04-12 |
| ✅ M2-P3 | 密码加密与 JWT | 已完成 | 2026-04-14 |
| ✅ M2-P4 | HTTP 认证 API | **已完成** | 2026-04-15 |

### 1.2 核心交付物

| 文件 | 行数 | 功能 |
|------|------|------|
| `source/include/auth_handler.hpp` | ~145 行 | 注册/登录业务逻辑处理 |
| `source/include/auth_middleware.hpp` | ~50 行 | JWT 认证中间件 |
| `source/tests/test_auth_api.cpp` | ~108 行 | API 集成测试（6 项测试） |
| `source/CMakeLists.txt` | 修改 ~15 行 | 添加 test_auth_api 测试目标 |

### 1.3 关键指标

- **代码行数**: ~318 行（新增 + 修改）
- **测试覆盖**: 6 个测试项，18 个断言，6 通过 ✅
- **构建问题**: 2 个（全部修复）
- **修复提交**: 2 个提交

---

## 二、目标达成情况

### 2.1 计划目标

| 目标 | 计划 | 实际 | 状态 |
|------|------|------|------|
| 用户注册 API | 完成 | 完成 | ✅ |
| 用户登录 API | 完成 | 完成 | ✅ |
| JWT 认证中间件 | 完成 | 完成 | ✅ |
| 参数校验 | 完成 | 完成 | ✅ |
| TOCTOU 防护 | 完成 | 完成 | ✅ |
| 测试覆盖 | 6 项 | 6 项 | ✅ |

### 2.2 验收标准

| 标准 | 要求 | 结果 |
|------|------|------|
| 注册成功 | 返回 user_id | ✅ 通过 |
| 重复用户名 | 注册失败 | ✅ 通过 |
| 参数校验 | 4 种边界情况 | ✅ 通过 |
| 登录成功 | 返回 token 和用户信息 | ✅ 通过 |
| 密码错误 | 登录失败 | ✅ 通过 |
| Token 验证 | 正确提取 user_id | ✅ 通过 |

---

## 三、技术方案详解

### 3.1 auth_handler.hpp 接口设计

#### 接口概览

```cpp
namespace gobang {
namespace auth {

/**
 * @brief 用户注册请求处理
 * @return JSON 响应 {success: bool, message: string, user_id: number}
 */
Json::Value handle_register(UserTable& user_table,
                            const std::string& username,
                            const std::string& password);

/**
 * @brief 用户登录请求处理
 * @return JSON 响应 {success: bool, message: string, token: string, user: {...}}
 */
Json::Value handle_login(UserTable& user_table,
                         const std::string& username,
                         const std::string& password);

} // namespace auth
} // namespace gobang
```

---

### 3.2 关键实现细节

#### handle_register 实现

```cpp
inline Json::Value handle_register(UserTable& user_table,
                                   const std::string& username,
                                   const std::string& password) {
    Json::Value response;

    // 1. 参数校验（用户名 3-32 字符，密码 6-128 字符）
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

    // 2. 检查用户名是否已存在（使用 select_by_username，不返回 password_hash）
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

    // 4. 插入用户表（TOCTOU 防护）
    int64_t user_id = user_table.insert(username, password_hash);
    if (user_id == 0) {
        // 并发情况下：insert 因 UNIQUE 约束失败后再次检查
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
    LOG_INFO("Auth: 用户注册成功，username=" << username << ", user_id=" << user_id);
    return response;
}
```

**安全特性**:
- 参数校验：防止用户名/密码过短或过长
- TOCTOU 防护：insert 失败后重查，区分 UNIQUE 冲突和其他错误
- 密码加密：复用 Phase 3 的 PBKDF2 加密

#### handle_login 实现

```cpp
inline Json::Value handle_login(UserTable& user_table,
                                const std::string& username,
                                const std::string& password) {
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
        LOG_WARN("Auth: 登录密码错误，username=" << username);
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

    // 5. 返回结果（不包含 password_hash）
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

    LOG_INFO("Auth: 用户登录成功，username=" << username << ", user_id=" << user_id);
    return response;
}
```

**安全特性**:
- 模糊错误信息：用户名不存在和密码错误返回相同提示
- 密码不泄露：返回的用户信息不包含 password_hash
- JWT 认证：复用 Phase 3 的 jwt_generate

---

### 3.3 auth_middleware.hpp 接口设计

```cpp
namespace gobang {
namespace auth {

/**
 * @brief 从 HTTP Authorization Header 中提取并验证 JWT
 * @param auth_header HTTP Header 中的 Authorization 值
 * @return 验证成功返回 user_id，失败返回 0
 */
inline int64_t extract_user_id_from_token(const std::string& auth_header);

/**
 * @brief 检查请求是否已认证
 * @return 已认证返回 true
 */
inline bool is_authenticated(const std::string& auth_header);

} // namespace auth
} // namespace gobang
```

#### 实现细节

```cpp
inline int64_t extract_user_id_from_token(const std::string& auth_header) {
    const std::string bearer_prefix = "Bearer ";

    if (auth_header.length() < bearer_prefix.length()) {
        return 0;
    }

    // 检查前缀（大小写不敏感，兼容小写）
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

inline bool is_authenticated(const std::string& auth_header) {
    return extract_user_id_from_token(auth_header) > 0;
}
```

**特性**:
- Bearer Token 标准格式支持
- 大小写不敏感（兼容小写 bearer）
- 复用 Phase 3 的 jwt_verify

---

### 3.4 测试用例设计

| 测试 # | 名称 | 测试内容 | 断言数 |
|--------|------|----------|--------|
| 1 | Register_Success | 正常注册新用户 | 3 |
| 2 | Register_DuplicateUsername | 重复注册应失败 | 2 |
| 3 | Register_ParameterValidation | 用户名/密码长度校验 | 4 |
| 4 | Login_Success | 正确密码登录成功并返回 token | 4 |
| 5 | Login_WrongPassword | 错误密码登录失败 | 2 |
| 6 | Token_Verify | 有效 token 提取 user_id | 3 |

---

## 四、问题追踪与修复

### 4.1 问题汇总

| # | 问题描述 | 严重性 | 发现阶段 | 修复耗时 |
|---|----------|--------|----------|----------|
| 1 | DBPool/UserTable 类型未识别 | 🔴 编译错误 | 编译 | 5 min |
| 2 | undefined reference to `main` | 🔴 链接错误 | 链接 | 5 min |

### 4.2 根因分析与修复

**问题 1：DBPool/UserTable 类型未识别**

- **原因**: test_auth_api.cpp 中未包含 `config.h` 和未使用 `using namespace gobang;` 声明
- **现象**: 编译器无法识别 `DBPool` 和 `UserTable` 类型
- **修复**: 添加 `#include "config.h"` 和 `using namespace gobang;`

```cpp
// 修复前
#include "auth_handler.hpp"
#include "auth_middleware.hpp"
#include "db.hpp"
#include "user_table.hpp"
#include "util.hpp"
#include <gtest/gtest.h>

// 修复后
#include "auth_handler.hpp"
#include "auth_middleware.hpp"
#include "db.hpp"
#include "user_table.hpp"
#include "util.hpp"
#include "config.h"  // 新增
#include <gtest/gtest.h>

using namespace gobang;  // 新增
```

**问题 2：undefined reference to `main`**

- **原因**: GTest 测试需要 main 函数入口，test_auth_api.cpp 未添加
- **现象**: 链接器报错 `undefined reference to main`
- **修复**: 添加 GTest main 函数

```cpp
// ==================== 主函数 ====================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

### 4.3 修复时间线

```
[2026-04-15 修复时间线]
1. 初始实现 auth_handler.hpp、auth_middleware.hpp、test_auth_api.cpp
2. 提交 git: 0fd708a feat: 添加用户认证 HTTP API（Phase 4）
3. 编译错误：类型未识别 → 添加 config.h 和 using namespace gobang
4. 提交 git: f300fad fix: 修复 test_auth_api.cpp 编译错误
5. 链接错误：undefined reference to main → 添加 main 函数
6. 提交 git: 3eb8146 fix: 添加 main 函数到 test_auth_api.cpp
7. 运行测试：6 项测试全部通过 ✅
```

---

## 五、代码质量分析

### 5.1 代码度量

| 指标 | 数值 | 评价 |
|------|------|------|
| 总行数 | ~318（新增 + 修改） | 精简 |
| 函数数量 | 4（handle_register、handle_login、extract_user_id_from_token、is_authenticated） | 精简 |
| 最大函数行数 | ~75 | 良好 |
| 注释密度 | ~30% | 适中 |
| 测试覆盖 | 6 项 | 核心场景覆盖 |

### 5.2 设计模式应用

| 模式 | 位置 | 收益 |
|------|------|------|
| 内联实现 | header-only | 无需编译单元，编译期优化 |
| RAII | DBPool::ConnGuard | 自动释放数据库连接 |
| TOCTOU 防护 | insert 失败后重查 | 并发注册冲突处理 |

### 5.3 代码规范遵循

- ✅ C++11 标准
- ✅ header-only 实现（全内联）
- ✅ 安全编码（模糊错误信息、密码不泄露）
- ✅ 命名一致性（`snake_case` 函数）
- ✅ 日志记录（分级日志：INFO/WARN）

---

## 六、核心知识点总结

### 6.1 TOCTOU 防护

TOCTOU (Time Of Check To Time Of Use) 是一种并发竞态条件：

```cpp
// ❌ 错误做法：检查后直接插入，存在竞态
if (!user_exists(username)) {
    insert_user(username, hash);  // 并发时可能违反 UNIQUE 约束
}

// ✅ 正确做法：insert 失败后重查，给出准确错误
int64_t user_id = insert(username, hash);
if (user_id == 0) {
    // 再次检查，区分 UNIQUE 冲突和其他错误
    if (user_exists(username)) {
        return "用户名已存在";
    } else {
        return "用户注册失败";
    }
}
```

### 6.2 接口区分使用

| 接口 | 返回字段 | 使用场景 |
|------|----------|----------|
| `select_for_auth()` | 包含 password_hash | 登录验证 |
| `select_by_username()` | 不包含 password_hash | 注册检查、公开信息展示 |

### 6.3 安全编码实践

| 风险 | 应对措施 |
|------|----------|
| 用户名枚举 | 模糊错误信息（用户名不存在/密码错误返回相同提示） |
| 密码泄露 | 返回信息不包含 password_hash |
| 并发注册冲突 | TOCTOU 防护（insert 失败后重查） |
| 参数注入 | 严格的长度校验 |

---

## 七、经验教训

### 7.1 做得好的地方

1. **计划驱动开发**
   - 严格按照计划文档实现
   - 接口设计与计划一致
   - 测试用例覆盖所有场景

2. **复用现有代码**
   - 完全复用 Phase 2 的 UserTable
   - 完全复用 Phase 3 的 security 模块
   - 模块化设计带来高效开发

3. **安全编码**
   - 模糊错误信息防止枚举
   - TOCTOU 防护处理并发
   - 密码哈希不泄露

### 7.2 需要改进的地方

1. **测试文件完整性**
   - 初始版本遗漏 main 函数
   - 遗漏 config.h 包含
   - **改进**: 参考其他测试文件模板，确保结构完整

2. **命名空间处理**
   - 测试文件中需要显式使用 `using namespace gobang;`
   - **改进**: 在测试文件模板中统一处理

---

## 八、风险与应对

### 8.1 当前风险

| 风险 | 概率 | 影响 | 应对措施 |
|------|------|------|----------|
| 用户名枚举攻击 | 低 | 中 | 模糊错误信息 ✅ |
| 并发注册冲突 | 低 | 中 | TOCTOU 防护 ✅ |
| 密码哈希泄露 | 低 | 高 | 不返回 password_hash ✅ |
| Token 伪造 | 低 | 高 | 复用 Phase 3 的 jwt_verify ✅ |

### 8.2 M2 阶段完成状态

- [x] Phase 1: 数据库连接池 ✅
- [x] Phase 2: 用户数据访问层 ✅
- [x] Phase 3: 密码加密与 JWT ✅
- [x] Phase 4: HTTP 认证 API ✅

---

## 九、M3 计划预览

### 9.1 待实现模块

| 模块 | 功能 | 预估工时 |
|------|------|----------|
| HTTP 服务器 | cpp-httplib 或 crow | 2-3h |
| 路由集成 | 注册/登录 API 绑定 | 1-2h |
| 中间件集成 | JWT 认证拦截 | 1-2h |
| WebSocket 支持 | 游戏对战通信 | 3-4h |

### 9.2 Phase 4 集成到 HTTP 服务器

```cpp
// 伪代码示例
#include "auth_handler.hpp"
#include "auth_middleware.hpp"

// 注册 API
server.Post("/api/register", [](const Request& req, Response& res) {
    Json::Value body = Json::parse(req.body);
    Json::Value result = gobang::auth::handle_register(
        user_table,
        body["username"].asString(),
        body["password"].asString()
    );
    res.set_content(result.toStyledString(), "application/json");
});

// 登录 API
server.Post("/api/login", [](const Request& req, Response& res) {
    // 类似 handle_register 处理
});

// 需要认证的 API
server.Get("/api/user/info", [](const Request& req, Response& res) {
    std::string auth_header = req.get_header_value("Authorization");
    if (!gobang::auth::is_authenticated(auth_header)) {
        res.status = 401;
        return;
    }
    // 继续处理业务逻辑
});
```

---

## 十、附录

### 10.1 文件清单

```
source/
├── include/
│   ├── db.hpp              # 数据库连接池（复用）
│   ├── logger.hpp          # 日志模块（复用）
│   ├── util.hpp            # 工具模块（复用）
│   ├── user_table.hpp      # 用户数据访问层（复用）
│   ├── security.hpp        # 安全模块（复用）
│   ├── auth_handler.hpp    # 认证处理器（新增）
│   └── auth_middleware.hpp # 认证中间件（新增）
├── tests/
│   └── test_auth_api.cpp   # API 集成测试（新增）
└── CMakeLists.txt          # 构建配置（扩展 test_auth_api）
```

### 10.2 编译与测试命令

```bash
# 虚拟机中执行
cd /home/guoziyin/Project/OnlineGobangBattle
git pull

cd source/build
cmake .. && cmake --build .

# 运行测试
./bin/test_auth_api
```

### 10.3 测试结果

```
[==========] Running 6 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 6 tests from AuthApiTest
[ RUN      ] AuthApiTest.Register_Success
[       OK ] AuthApiTest.Register_Success (45 ms)
[ RUN      ] AuthApiTest.Register_DuplicateUsername
[       OK ] AuthApiTest.Register_DuplicateUsername (38 ms)
[ RUN      ] AuthApiTest.Register_ParameterValidation
[       OK ] AuthApiTest.Register_ParameterValidation (0 ms)
[ RUN      ] AuthApiTest.Login_Success
[       OK ] AuthApiTest.Login_Success (52 ms)
[ RUN      ] AuthApiTest.Login_WrongPassword
[       OK ] AuthApiTest.Login_WrongPassword (41 ms)
[ RUN      ] AuthApiTest.Token_Verify
[       OK ] AuthApiTest.Token_Verify (48 ms)
[----------] 6 tests from AuthApiTest (224 ms total)

[----------] Global test environment tear-down.
[==========] 6 tests from 1 test suite ran. (224 ms total)
[  PASSED  ] 6 tests.
```

### 10.4 关键提交

| 提交 | 说明 |
|------|------|
| `0fd708a` | feat: 添加用户认证 HTTP API（Phase 4） |
| `f300fad` | fix: 修复 test_auth_api.cpp 编译错误 |
| `3eb8146` | fix: 添加 main 函数到 test_auth_api.cpp |

---

**报告版本**: v1.0  
**生成日期**: 2026-04-15  
**下次更新**: M3 阶段完成后