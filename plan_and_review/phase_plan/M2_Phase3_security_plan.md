# M2 Phase 3: security.hpp - 密码加密与 JWT 认证 - 实现计划

> **项目**: C++ 在线五子棋对战系统
> **阶段**: M2 Phase 3 安全模块（密码加密与 JWT）
> **制定日期**: 2026-04-13
> **修订日期**: 2026-04-13（根据代码审查反馈）
> **前置条件**: M2 Phase 1 数据库连接池已完成 ✅，M2 Phase 2 用户数据访问层已完成 ✅

---

## 一、背景

### 1.1 为什么要做 Phase 3

- Phase 1 提供了数据库连接能力
- Phase 2 提供了用户数据的 CRUD 操作
- **Phase 3 需要提供安全保障**：
  - 用户密码不能明文存储，需要加密
  - HTTP API 需要身份认证，需要 JWT Token
- 为 Phase 4 的 HTTP API（注册/登录/查询）提供安全基础

### 1.2 技术选型

| 功能 | 选型 | 理由 |
|------|------|------|
| 密码加密 | **OpenSSL PBKDF2** | Rocky 9 自带，0 依赖风险，标准算法 |
| JWT 库 | **jwt-cpp** | header-only，无需编译，社区活跃 |
| JWT 算法 | **HS256** | 对称加密，简单可靠 |
| JSON 后端 | **nlohmann-json** | 主流选择，Rocky 9 可能需要安装 |

---

## 二、预期成果

1. `source/include/security.hpp` - 安全模块头文件
2. `source/include/jwt-cpp/` - jwt-cpp 第三方库
3. `source/tests/test_security.cpp` - 单元测试（8 项测试）
4. `source/CMakeLists.txt` - 添加 OpenSSL + jwt-cpp 依赖
5. `source/config/server.conf` - 添加 JWT_SECRET 配置项

---

## 三、前置准备

### 3.1 环境验证

在虚拟机中执行：

```bash
# 1. 检查 OpenSSL 开发库（PBKDF2 用）
pkg-config --exists openssl && echo "openssl: 已安装" || echo "openssl: 未安装"

# 2. 检查 jwt-cpp 头文件
find /usr/include /usr/local/include -name "jwt*" 2>/dev/null | head -5

# 3. 检查 nlohmann-json（jwt-cpp v5 默认依赖）
find /usr/include /usr/local/include -name "nlohmann" -type d 2>/dev/null
# 或者
find /usr/include /usr/local/include -name "json.hpp" 2>/dev/null | head -3

# 4. 检查 picojson（jwt-cpp v5 备选依赖，可能冲突）
find /usr/include /usr/local/include -name "picojson*" 2>/dev/null | head -5
```

### 3.2 依赖安装

如未安装，执行：

```bash
# Rocky Linux 9
sudo dnf install openssl-devel

# nlohmann-json (需要先安装 EPEL)
sudo dnf install epel-release -y
sudo dnf install nlohmann-json-devel
# 或者从源码安装：https://github.com/nlohmann/json

# jwt-cpp 是 header-only 库，从以下地址下载：
# https://github.com/Thalhammer/jwt-cpp/releases
# 推荐版本：v5.x（稳定版本）
# ⚠️ 注意：下载 with nlohmann-json 版本，或者确保系统已安装 nlohmann-json
```

---

## 四、实现方案

### 4.1 security.hpp 接口设计

```cpp
#pragma once
#include <string>
#include <cstdint>

#include "util.hpp"  // 用于读取配置文件

namespace gobang {
namespace security {

// JWT 配置通过 gobang::util::Config 统一管理
// jwt_generate / jwt_verify 中直接访问 cfg.jwt_secret / cfg.jwt_issuer
// 详见"四.3 util.hpp Config 扩展" 部分

/**
 * @brief PBKDF2 密码加密
 * @param password 原始密码
 * @return 加密后的哈希字符串（格式：salt:hash，base64 编码），失败返回空字符串
 *
 * 参数配置：
 * - 迭代次数：10000 次
 * - 盐长度：16 字节
 * - 密钥长度：32 字节
 * - 哈希算法：SHA256
 *
 * 输出格式（总长约 100 字符）：
 *   base64(salt):base64(hash)
 */
std::string pbkdf2_hash(const std::string& password);

/**
 * @brief PBKDF2 密码验证
 * @param password 原始密码
 * @param hash 加密后的哈希（格式：salt:hash）
 * @return 验证通过返回 true，失败返回 false
 */
bool pbkdf2_verify(const std::string& password, const std::string& hash);

/**
 * @brief JWT Token 生成
 * @param user_id 用户 ID
 * @param expire_sec 过期时间（秒），默认 86400（24 小时）
 * @return JWT 字符串，失败返回空字符串
 *
 * Token 包含：
 * - user_id: 用户 ID（int64）
 * - iat: 签发时间（issued at）
 * - exp: 过期时间（expiration）
 *
 * Payload 示例：
 *   {"user_id": 123, "iat": 1713000000, "exp": 1713086400}
 */
std::string jwt_generate(int64_t user_id, int64_t expire_sec = 86400);

/**
 * @brief JWT Token 验证
 * @param token JWT 字符串
 * @return 验证成功返回 user_id，失败返回 0
 *
 * 验证项：
 * - 签名正确（HS256）
 * - 未过期（exp > now）
 * - 无效 token 返回 0
 */
int64_t jwt_verify(const std::string& token);

} // namespace security
} // namespace gobang
```

### 4.2 实现细节

#### 4.2.1 Base64 编解码辅助函数

```cpp
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

// 参数配置
const int PBKDF2_ITERATIONS = 10000;
const int SALT_LENGTH = 16;
const int KEY_LENGTH = 32;

/**
 * @brief Base64 编码
 */
static std::string base64_encode(const unsigned char* data, size_t len) {
    BIO* bio = nullptr;
    BIO* b64 = nullptr;
    BUF_MEM* bufferPtr = nullptr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_write(bio, data, len);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    return result;
}

/**
 * @brief Base64 解码（完整实现，处理换行符）
 */
static std::vector<unsigned char> base64_decode(const std::string& encoded) {
    BIO* bio = nullptr;
    BIO* b64 = nullptr;
    size_t len = encoded.length();

    // 分配输出缓冲区（最大可能长度）
    std::vector<unsigned char> buffer(len);

    bio = BIO_new_mem_buf(encoded.data(), len);
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);  // 不处理换行符
    bio = BIO_push(b64, bio);

    int decoded_len = BIO_read(bio, buffer.data(), len);
    buffer.resize(decoded_len > 0 ? decoded_len : 0);

    BIO_free_all(bio);
    return buffer;
}

std::string pbkdf2_hash(const std::string& password) {
    // 1. 生成随机盐
    unsigned char salt[SALT_LENGTH];
    if (RAND_bytes(salt, SALT_LENGTH) != 1) {
        LOG_ERROR("Security: RAND_bytes failed");
        return "";
    }

    // 2. PBKDF2 密钥派生
    unsigned char hash[KEY_LENGTH];
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), password.size(),
            salt, SALT_LENGTH,
            PBKDF2_ITERATIONS,
            EVP_sha256(),
            KEY_LENGTH,
            hash) != 1) {
        LOG_ERROR("Security: PKCS5_PBKDF2_HMAC failed");
        return "";
    }

    // 3. 编码输出：salt:hash
    std::string salt_b64 = base64_encode(salt, SALT_LENGTH);
    std::string hash_b64 = base64_encode(hash, KEY_LENGTH);
    return salt_b64 + ":" + hash_b64;
}

bool pbkdf2_verify(const std::string& password, const std::string& hash_str) {
    // 1. 解析 salt:hash
    auto pos = hash_str.find(':');
    if (pos == std::string::npos) {
        LOG_WARN("Security: invalid hash format (missing ':')");
        return false;
    }

    std::string salt_b64 = hash_str.substr(0, pos);
    std::string hash_b64 = hash_str.substr(pos + 1);

    // 2. Base64 解码 salt
    std::vector<unsigned char> salt = base64_decode(salt_b64);
    if (salt.size() != SALT_LENGTH) {
        LOG_WARN("Security: invalid salt length");
        return false;
    }

    // 3. Base64 解码存储的 hash
    std::vector<unsigned char> stored_hash = base64_decode(hash_b64);
    if (stored_hash.size() != KEY_LENGTH) {
        LOG_WARN("Security: invalid hash length");
        return false;
    }

    // 4. 重新计算哈希
    unsigned char new_hash[KEY_LENGTH];
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), password.size(),
            salt.data(), salt.size(),
            PBKDF2_ITERATIONS,
            EVP_sha256(),
            KEY_LENGTH,
            new_hash) != 1) {
        LOG_ERROR("Security: PKCS5_PBKDF2_HMAC verify failed");
        return false;
    }

    // 5. 常数时间比较（防止时序攻击）
    if (CRYPTO_memcmp(new_hash, stored_hash.data(), KEY_LENGTH) != 0) {
        LOG_DEBUG("Security: password verification failed");
        return false;
    }

    return true;
}
```

#### 4.2.2 JWT 实现（使用 jwt-cpp v5 verifier 模式）

```cpp
#include "jwt-cpp/jwt.h"

std::string jwt_generate(int64_t user_id, int64_t expire_sec) {
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::seconds(expire_sec);

    // 从 Config 读取 JWT 配置
    static gobang::util::Config cfg = gobang::util::load_config("config/server.conf");
    std::string secret = cfg.jwt_secret;
    std::string issuer = cfg.jwt_issuer;

    auto token = jwt::create()
        .set_issuer(issuer)
        .set_type("JWS")
        .set_payload_claim("user_id", jwt::claim(std::to_string(user_id)))
        .set_issued_at(now)
        .set_expires_at(exp)
        .sign(jwt::algorithm::hs256{secret});

    LOG_DEBUG("Security: JWT generated for user_id=" << user_id);
    return token;
}

int64_t jwt_verify(const std::string& token) {
    try {
        // 从 Config 读取 JWT 配置
        static gobang::util::Config cfg = gobang::util::load_config("config/server.conf");
        std::string secret = cfg.jwt_secret;
        std::string issuer = cfg.jwt_issuer;

        // jwt-cpp v5 正确写法：使用 verifier 验证
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .with_issuer(issuer);

        // 解码 token（不验证）
        auto decoded = jwt::decode(token);

        // 验证签名和 claims
        verifier.verify(decoded);

        // 获取 user_id
        auto user_id_claim = decoded.get_payload_claim("user_id");
        if (user_id_claim.is_string()) {
            int64_t user_id = std::stoll(user_id_claim.as_string());
            LOG_DEBUG("Security: JWT verified, user_id=" << user_id);
            return user_id;
        }

        LOG_WARN("Security: JWT claim 'user_id' not found or invalid type");
        return 0;

    } catch (const jwt::error::token_verification_exception& e) {
        // 过期、签名错误、issuer 不对，都走这里
        LOG_DEBUG("Security: JWT verify failed - " << e.what());
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Security: JWT verify error - " << e.what());
        return 0;
    } catch (...) {
        LOG_ERROR("Security: JWT verify unknown error");
        return 0;
    }
}
```

---

### 4.3 CMakeLists.txt 修改

在 `source/CMakeLists.txt` 中添加 OpenSSL 和 jwt-cpp 依赖：

```cmake
# OpenSSL 库
pkg_check_modules(OPENSSL openssl)
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "OpenSSL: found at ${OPENSSL_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR "OpenSSL: not found. Please install openssl-devel")
endif()

# jwt-cpp（header-only 库）
# 假设放在 source/include/jwt-cpp/
# 注意：这里指向 include/ 目录，不是 jwt-cpp/ 内部
set(JWT_CPP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
if(EXISTS ${JWT_CPP_INCLUDE_DIR})
    include_directories(${JWT_CPP_INCLUDE_DIR})
    message(STATUS "jwt-cpp: found at ${JWT_CPP_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "jwt-cpp: not found at ${JWT_CPP_INCLUDE_DIR}")
endif()

# ─── 测试目标：test_security ────────────────────────────────
add_executable(test_security tests/test_security.cpp)

target_link_libraries(test_security
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
)

set_target_properties(test_security PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)
```

### 4.4 util.hpp Config 扩展

在 `source/include/util.hpp` 的 Config 结构体中添加 JWT 配置字段：

```cpp
struct Config {
    // ... 现有字段 ...

    // JWT 配置（Phase 3 新增）
    std::string jwt_secret  = "CHANGE_ME";
    std::string jwt_issuer  = "online_gobang";
    int64_t     jwt_expire  = 86400;
};
```

在 `load_config()` 函数中添加解析逻辑：

```cpp
else if (key == "jwt_secret")            cfg.jwt_secret         = val;
else if (key == "jwt_issuer")            cfg.jwt_issuer         = val;
else if (key == "jwt_expire")            cfg.jwt_expire         = std::stoll(val);
```

### 4.5 配置文件更新

在 `source/config/server.conf` 中添加 JWT 配置项：

```ini
# ── JWT 配置 ─────────────────────────────
jwt_secret   = "CHANGE_ME_USE_RANDOM_STRING_AT_LEAST_32_CHARS"
jwt_issuer   = "online_gobang"
jwt_expire   = 86400
```

> **注意**：jwt_secret 是敏感配置，**不要提交到 Gitee**，已在 `.gitignore` 中忽略。

---

## 五、测试用例设计

### 5.1 测试文件：tests/test_security.cpp

| 测试 # | 名称 | 测试内容 | 断言数 |
|--------|------|----------|--------|
| 1 | PBKDF2 加密_不同密码 | 不同密码生成不同 hash | 2 |
| 2 | PBKDF2 加密_相同密码 | 相同密码因 salt 不同生成不同 hash | 1 |
| 3 | PBKDF2 验证_正确密码 | 正确密码验证通过 | 1 |
| 4 | PBKDF2 验证_错误密码 | 错误密码验证失败 | 1 |
| 5 | JWT 生成_格式正确 | 生成有效 JWT 字符串 | 1 |
| 6 | JWT 验证_有效 token | 有效 token 验证通过并返回正确 user_id | 2 |
| 7 | JWT 验证_过期 token | 过期 token 验证失败返回 0 | 1 |
| 8 | JWT 验证_伪造 token | 伪造 token 验证失败返回 0 | 1 |

### 5.2 测试代码结构

```cpp
#include "security.hpp"
#include <gtest/gtest.h>

class SecurityTest : public ::testing::Test {
protected:
    // 共享的测试资源
};

TEST(SecurityTest, PBKDF2_DifferentPasswords) {
    std::string hash1 = gobang::security::pbkdf2_hash("password123");
    std::string hash2 = gobang::security::pbkdf2_hash("password456");

    ASSERT_FALSE(hash1.empty());   // 断言 1: 哈希不为空
    ASSERT_NE(hash1, hash2);       // 断言 2: 不同密码生成不同哈希
}

TEST(SecurityTest, PBKDF2_SamePasswordDifferentSalt) {
    std::string hash1 = gobang::security::pbkdf2_hash("password123");
    std::string hash2 = gobang::security::pbkdf2_hash("password123");

    ASSERT_NE(hash1, hash2);       // 断言 1: 相同密码因 salt 不同生成不同哈希
}

TEST(SecurityTest, PBKDF2_Verify_CorrectPassword) {
    std::string password = "my_secure_password";
    std::string hash = gobang::security::pbkdf2_hash(password);

    ASSERT_TRUE(gobang::security::pbkdf2_verify(password, hash));  // 断言 1: 正确密码通过
}

TEST(SecurityTest, PBKDF2_Verify_WrongPassword) {
    std::string password = "my_secure_password";
    std::string hash = gobang::security::pbkdf2_hash(password);

    ASSERT_FALSE(gobang::security::pbkdf2_verify("wrong_password", hash));  // 断言 1: 错误密码拒绝
}

TEST(SecurityTest, JWT_Generate_ValidFormat) {
    std::string token = gobang::security::jwt_generate(12345);

    ASSERT_FALSE(token.empty());                    // 断言 1: token 不为空
    // JWT 格式：header.payload.signature，三段用 "." 分隔
    size_t first = token.find('.');
    ASSERT_NE(first, std::string::npos);           // 断言 2: 至少有一个点
    ASSERT_NE(token.find('.', first + 1), std::string::npos);  // 断言 3: 有第二个点
}

TEST(SecurityTest, JWT_Verify_ValidToken) {
    int64_t user_id = 12345;
    std::string token = gobang::security::jwt_generate(user_id, 3600);

    int64_t verified_id = gobang::security::jwt_verify(token);
    ASSERT_NE(verified_id, 0);        // 断言 1: 验证成功返回非 0
    ASSERT_EQ(verified_id, user_id);  // 断言 2: 返回正确的 user_id
}

TEST(SecurityTest, JWT_Verify_ExpiredToken) {
    int64_t user_id = 12345;
    // 生成 1 秒后过期的 token
    std::string token = gobang::security::jwt_generate(user_id, 1);

    // 等待过期
    std::this_thread::sleep_for(std::chrono::seconds(2));

    int64_t verified_id = gobang::security::jwt_verify(token);
    ASSERT_EQ(verified_id, 0);  // 断言 1: 过期 token 返回 0
}

TEST(SecurityTest, JWT_Verify_ForgedToken) {
    std::string fake_token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                             "eyJ1c2VyX2lkIjoiMTIzNDUiLCJpYXQiOjE3MTMwMDAwMDAsImV4cCI6MTcxMzA0NjAwMH0."
                             "fake_signature";

    int64_t verified_id = gobang::security::jwt_verify(fake_token);
    ASSERT_EQ(verified_id, 0);  // 断言 1: 伪造 token 返回 0
}
```

---

## 六、验收标准

| 标准 | 要求 | 验证方式 |
|------|------|----------|
| 编译通过 | 无警告、无错误 | `cmake --build .` |
| 测试 1 通过 | 不同密码生成不同哈希 | 断言 `hash1 != hash2` |
| 测试 2 通过 | 相同密码因 salt 不同生成不同哈希 | 断言 `hash1 != hash2` |
| 测试 3 通过 | 正确密码验证通过 | 断言返回 `true` |
| 测试 4 通过 | 错误密码验证失败 | 断言返回 `false` |
| 测试 5 通过 | JWT 格式正确 | 断言包含 `.` 分隔符 |
| 测试 6 通过 | 有效 token 验证返回正确 user_id | 断言 `verified_id == user_id` |
| 测试 7 通过 | 过期 token 验证失败 | 断言返回 `0` |
| 测试 8 通过 | 伪造 token 验证失败 | 断言返回 `0` |
| 代码规范 | 遵循 C++11、安全编码实践 | 代码审查 |

---

## 七、关键文件路径

| 文件 | 路径 | 状态 |
|------|------|------|
| `security.hpp` | `source/include/security.hpp` | 待创建 |
| `test_security.cpp` | `source/tests/test_security.cpp` | 待创建 |
| `jwt-cpp/` | `source/include/jwt-cpp/` | 待下载 |
| `CMakeLists.txt` | `source/CMakeLists.txt` | 待修改 |
| `server.conf` | `source/config/server.conf` | 待修改（添加 jwt 配置） |

---

## 八、复用现有代码

| 模块 | 用途 |
|------|------|
| `logger.hpp` | 日志记录 |
| `util.hpp` | 配置文件读取 |
| `db.hpp` | 数据库连接（后续 Phase 4 使用） |
| `user_table.hpp` | 用户数据访问（后续 Phase 4 使用） |

---

## 九、风险与注意

| 风险 | 概率 | 影响 | 应对措施 |
|------|------|------|----------|
| jwt-cpp JSON 后端缺失 | **高** | **高** | 确认 nlohmann-json 已安装，或使用带 nlohmann-json 的 jwt-cpp 发行版 |
| jwt-cpp 版本兼容性 | 中 | 高 | 使用已知兼容版本 v5.x |
| OpenSSL 版本差异 | 低 | 中 | 使用标准 API，确保兼容性 |
| 常数时间比较 | 低 | 高 | 使用 `CRYPTO_memcmp` 防止时序攻击 |
| Windows 兼容性 | 低 | 中 | 使用 `#ifdef _WIN32` 条件编译 |
| Token 泄漏 | 中 | 高 | 过期时间合理设置（默认 24 小时） |
| JWT_SECRET 硬编码 | 中 | 高 | 从 server.conf 读取，禁止提交到 Gitee |

---

## 十、Phase 4 注意事项

**Phase 4 需要实现 HTTP API**，需要以下集成：

```cpp
// 用户注册流程
1. 接收客户端请求 {username, password}
2. 调用 security::pbkdf2_hash(password) 加密密码
3. 调用 user_table::insert(username, password_hash) 存储用户
4. 返回 user_id 给客户端

// 用户登录流程
1. 接收客户端请求 {username, password}
2. 调用 user_table::select_for_auth(username) 获取用户记录
3. 调用 security::pbkdf2_verify(password, stored_hash) 验证密码
4. 验证通过后调用 security::jwt_generate(user_id) 生成 token
5. 返回 {token, user_info} 给客户端

// 认证流程
1. 从 HTTP Header 提取 JWT：Authorization: Bearer <token>
2. 调用 security::jwt_verify(token) 验证 token
3. 验证通过获取 user_id，后续业务使用
```

---

## 十一、验证步骤

### 编译验证
```bash
cd source/build
cmake .. && cmake --build .
# 应成功生成 test_security 目标
```

### 运行测试
```bash
./bin/test_security
# 输出：8 项测试全部通过
```

### 手动验证（可选）
```cpp
// 测试密码加密
std::string hash = gobang::security::pbkdf2_hash("test123");
std::cout << "Hash: " << hash << std::endl;

// 测试 JWT
std::string token = gobang::security::jwt_generate(12345);
std::cout << "Token: " << token << std::endl;

int64_t user_id = gobang::security::jwt_verify(token);
std::cout << "User ID: " << user_id << std::endl;
```

---

## 十二、变更历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| v1.0 | 2026-04-13 | 初始版本 |
| v1.1 | 2026-04-13 | 修复审查问题：jwt-cpp v5 verifier API、完整 base64_decode 实现、CMakeLists 添加 jwt-cpp 路径、从配置文件读取 JWT_SECRET、测试断言加强、添加 nlohmann-json 依赖检查 |
| v1.2 | 2026-04-13 | 修复二次审查问题：jwt 异常类名修正、CMakeLists 路径修正（指向 include/）、删除 helper 函数改用 static Config、添加 util.hpp Config 字段扩展、添加 EPEL 安装步骤 |

---

## 附录 A：环境检查脚本

在虚拟机中创建并运行以下脚本 `scripts/check_m3_env.sh`：

```bash
#!/bin/bash
echo "=== M3 (Phase 3) 环境检查 ==="
echo ""

echo "1. OpenSSL 开发库:"
pkg-config --exists openssl && echo "  ✅ 已安装" || echo "  ❌ 未安装"
ls -la /usr/include/openssl/opensslconf.h 2>/dev/null | head -1

echo ""
echo "2. nlohmann-json (jwt-cpp 依赖):"
find /usr/include /usr/local/include -name "nlohmann" -type d 2>/dev/null | head -3
find /usr/include /usr/local/include -name "json.hpp" 2>/dev/null | head -3

echo ""
echo "3. picojson (可能与 nlohmann-json 冲突):"
find /usr/include /usr/local/include -name "picojson*" 2>/dev/null | head -3

echo ""
echo "4. jwt-cpp 头文件:"
find /usr/include /usr/local/include -name "jwt*" -type f 2>/dev/null | head -5

echo ""
echo "5. 当前 server.conf JWT 配置:"
grep -A5 "\[jwt\]" source/config/server.conf 2>/dev/null || echo "  ❌ 未配置 JWT"
```

如果 nlohmann-json 未安装，执行：

```bash
# Rocky Linux 9
sudo dnf install epel-release -y
sudo dnf install nlohmann-json-devel

# 如果没有可用包，从源码安装：
# git clone https://github.com/nlohmann/json.git
# cd json && mkdir build && cd build
# cmake .. -DJSON_BuildTests=OFF -DCMAKE_INSTALL_PREFIX=/usr/local
# sudo make install
```

---

*文档版本：v1.1*
*制定日期：2026-04-13*
*修订日期：2026-04-13（根据代码审查反馈）*