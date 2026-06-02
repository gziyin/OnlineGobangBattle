# M2 Phase 2: user_table.hpp - 用户数据访问层 - 实现计划 (v1.2 修订版)

> **项目**: C++ 在线五子棋对战系统  
> **阶段**: M2 Phase 2 用户数据访问层  
> **制定日期**: 2026-04-09  
> **修订日期**: 2026-04-09 (根据代码审查反馈)  
> **前置条件**: M2 Phase 1 数据库连接池已完成 ✅

---

## 一、背景

### 1.1 为什么要做 Phase 2
- Phase 1 只提供了原始 MySQL 连接能力，业务层无法直接使用
- 需要封装用户表的增删改查操作，隔离 SQL 细节
- 为 Phase 3 的密码加密和 Phase 4 的 HTTP API 提供数据基础

### 1.2 代码审查反馈及修复

根据代码审查反馈，本计划已修复以下问题：

| # | 问题 | 修复方案 |
|---|------|----------|
| 1 | `update_score_win/lose` 分两次调用，非原子 | 新增 `update_score_match()` 事务接口，**移除已过时接口** |
| 2 | `password_hash VARCHAR(256)` 可能不够 | 改为 `VARCHAR(512)` |
| 3 | 测试清表用 `DELETE` + `ALTER` | 改为 `TRUNCATE TABLE` |
| 4 | `select_by_username` 返回 `password_hash` 可能泄露 | 拆分为 `select_for_auth()` 和 `select_by_username()` |
| 5 | `loser_delta` 参数语义歧义 | 改名为 `loser_penalty`，注释明确"传正数，内部取减" |

---

## 二、预期成果

1. `user_table.hpp` - 用户数据访问层（DAL，v1.2 修订版）
2. `test_user_table.cpp` - 单元测试（5 项测试）
3. CMakeLists.txt 更新
4. 数据库 user 表已创建（`password_hash VARCHAR(512)`）

---

## 三、实现方案

### 3.1 前置准备

#### 3.1.1 数据库环境验证
在虚拟机中执行：
```bash
# 检查 MySQL 服务
systemctl status mysqld --no-pager | head -5

# 检查数据库和表
mysql -u gobang -p'yourpassword' -e 'USE gobang_db; SHOW TABLES; DESCRIBE user;'
```

#### 3.1.2 确认 user 表结构（已修正）

```sql
-- Phase 2 前必须执行此 SQL
-- 注意：password_hash 改为 VARCHAR(512) 以容纳 PBKDF2 完整输出
CREATE DATABASE IF NOT EXISTS gobang_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE gobang_db;

CREATE TABLE user (
    id INT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(32) UNIQUE NOT NULL COMMENT '用户名',
    password_hash VARCHAR(512) NOT NULL COMMENT 'PBKDF2 密码哈希 (algorithm$iterations$salt$hash)',
    score INT UNSIGNED DEFAULT 1500 COMMENT '天梯分数',
    total_count INT UNSIGNED DEFAULT 0 COMMENT '总场次',
    win_count INT UNSIGNED DEFAULT 0 COMMENT '胜场',
    status TINYINT DEFAULT 0 COMMENT '0-离线 1-大厅 2-房间',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_score (score),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

**字段长度说明**:
- `password_hash` 使用 `VARCHAR(512)` 而非 `VARCHAR(256)`
- PBKDF2 输出格式：`$pbkdf2-sha256$iterations$salt$hash`
- 典型长度示例：`$pbkdf2-sha256$29000$randomsaltbase64$hashbase64` 约 100-150 字符
- 预留 512 字符可支持更高迭代次数和更长 salt

---

### 3.2 user_table.hpp 实现

#### 3.2.1 类设计（已修正）

```cpp
#pragma once
#include <string>
#include <cstdint>
#include <json/json.h>

#include "db.hpp"
#include "logger.hpp"
#include "util.hpp"

namespace gobang {

/**
 * @brief 用户表数据访问层（DAL）
 *
 * 负责用户数据的 CRUD 操作，不做业务逻辑验证
 * 密码验证由调用方使用 security 模块处理
 * 
 * 注意：
 * - 所有接口仅供内部模块调用，不直接暴露给外部
 * - 涉及多表更新的操作用于事务保证原子性
 */
class UserTable {
public:
    /**
     * @brief 初始化，注入 DBPool
     */
    void init(DBPool* pool);

    // ==================== 写操作 ====================

    /**
     * @brief 用户注册：插入新用户
     * @param username 用户名（3-32 位字母数字）
     * @param password_hash PBKDF2 加密后的密码哈希
     * @return 成功返回 user_id (>0)，失败返回 0
     *
     * 失败原因：用户名已存在 (ER_DUP_ENTRY)、数据库错误
     */
    int64_t insert(const std::string& username, const std::string& password_hash);

    /**
     * @brief 更新用户状态
     * @param user_id 用户 ID
     * @param status 状态码（0-离线 1-大厅 2-房间 3-游戏中）
     * @return 成功返回 true，失败返回 false
     */
    bool update_status(int64_t user_id, int status);

    /**
     * @brief 比赛结束后更新双方分数（事务操作，保证原子性）
     * @param winner_id 获胜者 ID
     * @param loser_id 失败者 ID
     * @param winner_delta 获胜者分数变化，**传正数**（通常 +25）
     * @param loser_penalty 失败者扣分，**传正数，内部自动取减**（通常 -15，最低不低于 0）
     * @return 成功返回 true，失败返回 false（回滚）
     *
     * 注意：此接口使用事务，要么双方都更新成功，要么都失败
     * 
     * 示例：update_score_match(1001, 1002, 25, 15);
     *   → winner 分数 +25，loser 分数 -15（不会变成负数）
     */
    bool update_score_match(int64_t winner_id, int64_t loser_id,
                            int winner_delta = 25, int loser_penalty = 15);

    // ==================== 读操作 ====================

    /**
     * @brief 根据用户名查询用户（仅供认证模块使用，包含 password_hash）
     * @param username 用户名
     * @param out 输出参数，包含完整用户记录
     * @return 成功返回 true，用户不存在返回 false
     *
     * out 包含字段：id, username, password_hash, score, total_count, win_count, status
     * 
     * ⚠️ 注意：此接口返回 password_hash，仅供 security 模块验证密码时使用
     */
    bool select_for_auth(const std::string& username, Json::Value& out);

    /**
     * @brief 根据用户名查询用户（公开版本，不包含 password_hash）
     * @param username 用户名
     * @param out 输出参数，包含公开用户信息
     * @return 成功返回 true，用户不存在返回 false
     *
     * out 包含字段：id, username, score, total_count, win_count, status
     * 
     * ✅ 安全：此接口不返回 password_hash，可安全用于用户资料展示
     */
    bool select_by_username(const std::string& username, Json::Value& out);

    /**
     * @brief 根据 ID 查询用户（公开版本，不包含 password_hash）
     * @param user_id 用户 ID
     * @param out 输出参数，包含公开用户信息
     * @return 成功返回 true，用户不存在返回 false
     */
    bool select_by_id(int64_t user_id, Json::Value& out);

private:
    DBPool* _pool = nullptr;
};

} // namespace gobang
```

**接口设计说明**:
- **移除了 `update_score_win` / `update_score_lose`**：过时接口直接删除，不留 `deprecated`，避免混淆
- **`loser_penalty` 替代 `loser_delta`**：参数名更直观，注释明确"传正数，内部取减"

#### 3.2.2 关键实现细节

**insert 实现要点**：
- 使用预处理语句防止 SQL 注入
- 捕获 `ER_DUP_ENTRY` 错误（用户名重复）
- 返回 `mysql_insert_id()` 获取新生成的 user_id

**select_for_auth 实现要点**：
- 明确标注仅供认证模块使用
- 返回包含 password_hash 的完整记录
- 在注释中标明安全警告

**select_by_username / select_by_id 实现要点**：
- 不返回 password_hash 字段
- 可安全用于用户资料展示
- SQL 查询明确列出需要的字段

**update_score_match 实现要点**：
```cpp
bool update_score_match(int64_t winner_id, int64_t loser_id,
                        int winner_delta, int loser_penalty) {
    auto conn = _pool->get_connection();
    if (!conn) return false;

    // 开启事务
    mysql_query(conn.get(), "START TRANSACTION");

    // 更新获胜者分数：score + winner_delta，使用 GREATEST 防止负数
    std::string winner_sql = 
        "UPDATE user SET score = GREATEST(0, score + ?), "
        "win_count = win_count + 1, total_count = total_count + 1 "
        "WHERE id = ?";
    
    auto stmt_winner = mysql_stmt_init(conn.get());
    mysql_stmt_prepare(stmt_winner, winner_sql.c_str(), winner_sql.size());
    // ... 绑定参数 (winner_delta, winner_id) 并执行
    int64_t winner_affected = mysql_stmt_affected_rows(stmt_winner);

    // 更新失败者分数：score - loser_penalty，使用 GREATEST 防止负数
    // 注意：loser_penalty 是正数，SQL 中直接减去
    std::string loser_sql =
        "UPDATE user SET score = GREATEST(0, score - ?), "
        "total_count = total_count + 1 "
        "WHERE id = ?";
    
    auto stmt_loser = mysql_stmt_init(conn.get());
    mysql_stmt_prepare(stmt_loser, loser_sql.c_str(), loser_sql.size());
    // ... 绑定参数 (loser_penalty, loser_id) 并执行
    int64_t loser_affected = mysql_stmt_affected_rows(stmt_loser);

    // 检查受影响行数，任何失败都回滚
    if (winner_affected != 1 || loser_affected != 1) {
        mysql_query(conn.get(), "ROLLBACK");
        return false;
    }

    mysql_query(conn.get(), "COMMIT");
    return true;
}
```

---

### 3.3 CMakeLists.txt 修改

在 `source/CMakeLists.txt` 中添加：

```cmake
# ─── 测试目标：test_user_table ──────────────────────────────────
add_executable(test_user_table tests/test_user_table.cpp)

target_link_libraries(test_user_table
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    Threads::Threads
)

set_target_properties(test_user_table PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)
```

---

### 3.4 测试用例设计

#### 3.4.1 测试文件：tests/test_user_table.cpp

| 测试 # | 名称 | 测试内容 | 断言数 |
|--------|------|----------|--------|
| 1 | 用户注册测试 | 插入新用户，验证返回 ID > 0 | 2 |
| 2 | 重复用户名测试 | 相同用户名再次注册应失败 | 1 |
| 3 | 认证查询测试 | select_for_auth 返回 password_hash | 2 |
| 4 | 公开查询测试 | select_by_username 不返回 password_hash | 1 |
| 5 | 分数更新测试 | update_score_match 同时更新双方 | 6 |

#### 3.4.2 测试前置条件（已修正）

```cpp
// 测试开始前执行：
// 1. 初始化 DBPool
// 2. 清空 user 表：使用 TRUNCATE TABLE（一步到位，重置 AUTO_INCREMENT）
auto conn = pool_.get_connection();
mysql_query(conn.get(), "TRUNCATE TABLE user");
```

**修正说明**:
- 原计划：`DELETE FROM user` + `ALTER TABLE user AUTO_INCREMENT = 1`
- 问题：两步之间可能有并发写入风险
- 修正：`TRUNCATE TABLE user` 一步完成清空 + 重置自增

#### 3.4.3 测试代码结构（已补全断言）

```cpp
#include "user_table.hpp"
#include "db.hpp"
#include "util.hpp"

class UserTableTest {
protected:
    void SetUp() override {
        pool_.init(cfg);
        user_table_.init(&pool_);
        
        // 清空表（修正后）
        auto conn = pool_.get_connection();
        mysql_query(conn.get(), "TRUNCATE TABLE user");
    }
    
    DBPool pool_;
    UserTable user_table_;
};

TEST(UserTableTest, Insert_Success) {
    int64_t id = user_table_.insert("testuser", "hash_xxx");
    ASSERT_GT(id, 0);  // 断言 1: 返回有效 ID
}

TEST(UserTableTest, SelectForAuth_ReturnsHash) {
    Json::Value out;
    bool ok = user_table_.select_for_auth("testuser", out);
    ASSERT_TRUE(ok);                                    // 断言 1
    ASSERT_TRUE(out.isMember("password_hash"));         // 断言 2: 应该包含 hash
}

TEST(UserTableTest, SelectByUsername_NoHash) {
    Json::Value out;
    bool ok = user_table_.select_by_username("testuser", out);
    ASSERT_TRUE(ok);                                    // 断言 1
    ASSERT_FALSE(out.isMember("password_hash"));        // 断言 2: 不应包含 hash
}

TEST(UserTableTest, UpdateScoreMatch_Atomic) {
    // 创建两个测试用户
    int64_t winner_id = user_table_.insert("winner", "hash1");
    int64_t loser_id = user_table_.insert("loser", "hash2");
    
    // 查询初始分数
    Json::Value winner_before, loser_before;
    user_table_.select_by_id(winner_id, winner_before);
    user_table_.select_by_id(loser_id, loser_before);
    int initial_winner_score = winner_before["score"].asInt();
    int initial_loser_score = loser_before["score"].asInt();
    
    // 执行分数更新
    bool ok = user_table_.update_score_match(winner_id, loser_id, 25, 15);
    ASSERT_TRUE(ok);  // 断言 1: 操作成功
    
    // 验证更新结果
    Json::Value winner_after, loser_after;
    user_table_.select_by_id(winner_id, winner_after);
    user_table_.select_by_id(loser_id, loser_after);
    
    ASSERT_EQ(winner_after["score"].asInt(), initial_winner_score + 25);   // 断言 2: winner +25
    ASSERT_EQ(winner_after["win_count"].asInt(), 1);                       // 断言 3: win_count +1
    ASSERT_EQ(winner_after["total_count"].asInt(), 1);                     // 断言 4: total_count +1
    ASSERT_EQ(loser_after["score"].asInt(), initial_loser_score - 15);     // 断言 5: loser -15
    ASSERT_EQ(loser_after["total_count"].asInt(), 1);                      // 断言 6: loser total_count +1
}
```

**断言说明**:
- 测试 5 共 6 个断言，全部明确写出
- 验证 winner 的 score、win_count、total_count（3 项）
- 验证 loser 的 score、total_count（2 项）
- 验证操作成功（1 项）

---

## 四、验收标准

| 标准 | 要求 | 验证方式 |
|------|------|----------|
| 编译通过 | 无警告、无错误 | `cmake --build .` |
| 测试 1 通过 | 注册成功返回有效 ID | 断言 `id > 0` |
| 测试 2 通过 | 重复用户名返回 0 | 断言 `ret == 0` |
| 测试 3 通过 | select_for_auth 返回 hash | 验证 `out["password_hash"]` 存在 |
| 测试 4 通过 | select_by_username 不返回 hash | 验证 `out["password_hash"]` 不存在 |
| 测试 5 通过 | update_score_match 原子更新 | 验证 6 个断言全部通过 |
| 代码规范 | 遵循 C++11、RAII、命名一致 | 代码审查 |

---

## 五、关键文件路径

| 文件 | 路径 | 状态 |
|------|------|------|
| `user_table.hpp` | `source/include/user_table.hpp` | 待创建 |
| `test_user_table.cpp` | `source/tests/test_user_table.cpp` | 待创建 |
| `CMakeLists.txt` | `source/CMakeLists.txt` | 待修改 |
| `db.hpp` | `source/include/db.hpp` | 已存在 ✅ |
| `util.hpp` | `source/include/util.hpp` | 已存在 ✅ |

---

## 六、复用现有代码

| 模块 | 用途 |
|------|------|
| `DBPool::ConnGuard` | RAII 方式获取数据库连接 |
| `util::Config` | 读取配置文件 |
| `util::json_to_str` | JSON 序列化 |
| `LOG_INFO/WARN/ERROR` | 日志记录 |

---

## 七、风险与注意

| 风险 | 概率 | 影响 | 应对措施 |
|------|------|------|----------|
| SQL 注入风险 | 低 | 高 | 全部使用预处理语句 |
| 用户名重复处理 | 中 | 中 | 捕获 `ER_DUP_ENTRY` 错误码 |
| MySQL 服务未启动 | 低 | 高 | 测试前检查 `systemctl status mysqld` |
| 表结构不匹配 | 中 | 中 | Phase 2 前先执行建表 SQL（VARCHAR(512)） |
| 事务未正确提交 | 低 | 高 | 检查 `COMMIT`/`ROLLBACK` 逻辑 |

---

## 八、Phase 3 注意事项

**OpenSSL 依赖验证**:
```bash
# Rocky Linux 9 上检查
pkg-config --exists openssl && echo "已安装" || echo "未安装"

# 如未安装，执行：
sudo dnf install openssl-devel
```

**jwt-cpp 下载**:
```bash
# jwt-cpp 是 header-only 库
# 从 https://github.com/Thalhammer/jwt-cpp/releases 下载
# 解压后把 include/ 复制到 source/include/3rdparty/jwt-cpp/
```

---

## 九、验证步骤

### 编译验证
```bash
cd source/build
cmake .. && cmake --build .
# 应成功生成 test_user_table 目标
```

### 运行测试
```bash
./bin/test_user_table
# 输出：5 项测试全部通过
```

### 数据库验证
```sql
-- 验证表结构（password_hash 应为 VARCHAR(512)）
DESCRIBE user;

-- 验证数据
SELECT id, username, score, LEFT(password_hash, 20) as hash_prefix FROM user;
```

---

## 十、变更历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| v1.0 | 2026-04-09 | 初始版本 |
| v1.1 | 2026-04-09 | 根据代码审查反馈修复：原子性、字段长度、清表逻辑、接口分离 |
| v1.2 | 2026-04-09 | 删除 deprecated 接口、补全测试断言、参数改名 `loser_penalty` |

---

*文档版本：v1.2 (修订版)*  
*制定日期：2026-04-09*  
*修订日期：2026-04-09 (根据代码审查反馈 v1.2)*
