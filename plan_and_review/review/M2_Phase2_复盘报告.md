# M2 Phase 2: user_table.hpp - 用户数据访问层 - 复盘报告

> **项目**: C++ 在线五子棋对战系统
> **阶段**: M2 Phase 2 用户数据访问层
> **完成日期**: 2026-04-12
> **报告人**: AI 助手

---

## 一、执行摘要

### 1.1 里程碑状态

| 阶段 | 名称 | 状态 | 完成日期 |
|------|------|------|----------|
| ✅ M1 | 环境搭建与基础框架 | 已完成 | 2026-03-31 |
| ✅ M2-P1 | 数据库连接池 | 已完成 | 2026-04-09 |
| ✅ M2-P2 | 用户数据访问层 | **已完成** | 2026-04-12 |
| ⏳ M2-P3 | 密码加密与 JWT | 待开始 | - |
| ⏳ M2-P4 | HTTP 认证 API | 待开始 | - |

### 1.2 核心交付物

| 文件 | 行数 | 功能 |
|------|------|------|
| `source/include/user_table.hpp` | ~760 行 | UserTable 用户数据访问层（全内联实现） |
| `source/tests/test_user_table.cpp` | ~190 行 | 单元测试（5 项测试，13 个断言） |
| `plan_and_review/phase_plan/M2_Phase2_user_table_plan.md` | ~92 行 | 实现计划（v1.2 修订版） |

### 1.3 关键指标

- **代码行数**: ~950 行（不含注释和空行）
- **测试覆盖**: 5 个测试项，13 个断言，13 通过 ✅
- **构建问题**: 5 个（全部修复）
- **修复提交**: 9 个提交

---

## 二、目标达成情况

### 2.1 计划目标

| 目标 | 计划 | 实际 | 状态 |
|------|------|------|------|
| UserTable 类实现 | 完成 | 完成 | ✅ |
| insert 注册接口 | 完成 | 完成 | ✅ |
| select_for_auth 认证查询 | 完成 | 完成 | ✅ |
| select_by_username 公开查询 | 完成 | 完成 | ✅ |
| select_by_id ID 查询 | 完成 | 完成 | ✅ |
| update_score_match 原子更新 | 完成 | 完成 | ✅ |
| 事务保证原子性 | 完成 | 完成 | ✅ |
| 测试覆盖 | 5 项 | 5 项 | ✅ |

### 2.2 验收标准

| 标准 | 要求 | 结果 |
|------|------|------|
| 用户注册 | 返回有效 ID | ✅ 通过 |
| 重复用户名 | 返回 0 | ✅ 通过 |
| 认证查询 | 返回 password_hash | ✅ 通过 |
| 公开查询 | 不返回 password_hash | ✅ 通过 |
| 分数更新 | 原子更新双方分数 | ✅ 通过 |
| 代码规范 | 遵循 C++11、RAII | ✅ 通过 |

---

## 三、技术方案详解

### 3.1 UserTable 类设计

#### 接口设计

```cpp
class UserTable {
public:
    void init(DBPool* pool);
    
    // 写操作
    int64_t insert(const std::string& username, const std::string& password_hash);
    bool update_status(int64_t user_id, int status);
    bool update_score_match(int64_t winner_id, int64_t loser_id,
                            int winner_delta = 25, int loser_penalty = 15);
    
    // 读操作
    bool select_for_auth(const std::string& username, Json::Value& out);
    bool select_by_username(const std::string& username, Json::Value& out);
    bool select_by_id(int64_t user_id, Json::Value& out);
    
private:
    DBPool* _pool = nullptr;
};
```

#### 接口设计说明

| 接口 | 用途 | 安全特性 |
|------|------|----------|
| `select_for_auth` | 仅供认证模块使用 | 返回 password_hash |
| `select_by_username` | 公开查询 | 不返回 password_hash |
| `select_by_id` | 公开查询 | 不返回 password_hash |
| `update_score_match` | 比赛后更新分数 | 事务保证原子性 |

**接口分离设计**：
- 原计划 v1.0 只有一个 `select` 接口
- 代码审查发现安全风险：公开查询返回 password_hash 可能导致信息泄露
- v1.2 修订为两个独立接口：`select_for_auth`（内部）和 `select_by_username`（公开）

---

### 3.2 关键实现细节

#### insert 实现

```cpp
int64_t insert(const std::string& username, const std::string& password_hash) {
    // 预处理语句防止 SQL 注入
    const std::string sql = "INSERT INTO user (username, password_hash) VALUES (?, ?)";
    
    // 绑定参数
    MYSQL_BIND params[2];
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer_type = MYSQL_TYPE_STRING;
    
    // 捕获 ER_DUP_ENTRY 错误（用户名重复）
    if (mysql_stmt_execute(stmt) != 0) {
        int err = mysql_stmt_errno(stmt);
        if (err == ER_DUP_ENTRY) {
            LOG_WARN("用户名已存在：" << username);
        }
        return 0;
    }
    
    return mysql_insert_id(conn.get());  // 返回新生成的 user_id
}
```

#### update_score_match 实现

```cpp
bool update_score_match(int64_t winner_id, int64_t loser_id,
                        int winner_delta, int loser_penalty) {
    // 开启事务
    mysql_query(conn.get(), "START TRANSACTION");
    
    // 更新获胜者：score + winner_delta，使用 GREATEST 防止负数
    const std::string winner_sql =
        "UPDATE user SET score = GREATEST(0, score + ?), "
        "win_count = win_count + 1, total_count = total_count + 1 "
        "WHERE id = ?";
    
    // 更新失败者：score - loser_penalty，使用 GREATEST 防止负数
    const std::string loser_sql =
        "UPDATE user SET score = GREATEST(0, score - ?), "
        "total_count = total_count + 1 "
        "WHERE id = ?";
    
    // 任何一方失败都回滚
    if (winner_affected != 1 || loser_affected != 1) {
        mysql_query(conn.get(), "ROLLBACK");
        return false;
    }
    
    mysql_query(conn.get(), "COMMIT");
    return true;
}
```

#### select_for_auth 实现

```cpp
bool select_for_auth(const std::string& username, Json::Value& out) {
    // 查询包含 password_hash 的完整记录
    const std::string sql =
        "SELECT id, username, password_hash, score, total_count, win_count, status "
        "FROM user WHERE username = ?";
    
    // 绑定结果（7 个字段）
    MYSQL_BIND result[7];
    // id, username, password_hash, score, total_count, win_count, status
    
    // 填充 JSON 输出
    out["id"] = (Json::Int64)id;
    out["username"] = username_buf;
    out["password_hash"] = password_hash_buf;  // 包含 password_hash
    // ...
}
```

#### select_by_username 实现

```cpp
bool select_by_username(const std::string& username, Json::Value& out) {
    // 查询不包含 password_hash 的公开记录
    const std::string sql =
        "SELECT id, username, score, total_count, win_count, status "
        "FROM user WHERE username = ?";
    
    // 绑定结果（6 个字段，不含 password_hash）
    MYSQL_BIND result[6];
    // id, username, score, total_count, win_count, status
    
    // 填充 JSON 输出（不包含 password_hash）
    out["id"] = (Json::Int64)id;
    out["username"] = username_buf;
    // 不包含 password_hash ✅
}
```

---

### 3.3 测试用例设计

#### 测试覆盖

| 测试 # | 名称 | 测试内容 | 断言数 |
|--------|------|----------|--------|
| 1 | 用户注册测试 | 插入新用户，验证返回 ID > 0 | 1 |
| 2 | 重复用户名测试 | 相同用户名再次注册应失败 | 1 |
| 3 | 认证查询测试 | select_for_auth 返回 password_hash | 2 |
| 4 | 公开查询测试 | select_by_username 不返回 password_hash | 2 |
| 5 | 分数更新测试 | update_score_match 同时更新双方 | 7 |

#### 测试代码结构

```cpp
// 前置准备：清空表
mysql_query(conn.get(), "TRUNCATE TABLE user");

// 测试 1：用户注册
int64_t id = user_table.insert("testuser", "hash_pbkdf2_sha256_xxx");
TEST_ASSERT(id > 0, "注册成功应返回有效 ID (>0)");

// 测试 2：重复用户名
int64_t ret = user_table.insert("testuser", "hash_another");
TEST_ASSERT(ret == 0, "重复用户名应返回 0");

// 测试 3：认证查询
Json::Value out;
bool ok = user_table.select_for_auth("testuser", out);
TEST_ASSERT(ok == true, "select_for_auth 应找到已注册的用户");
TEST_ASSERT(out.isMember("password_hash"), "应返回 password_hash 字段");

// 测试 4：公开查询
bool ok = user_table.select_by_username("testuser", out);
TEST_ASSERT(ok == true, "select_by_username 应找到已注册的用户");
TEST_ASSERT(!out.isMember("password_hash"), "不应返回 password_hash 字段");

// 测试 5：分数更新
bool ok = user_table.update_score_match(winner_id, loser_id, 25, 15);
// 验证 7 个断言：操作成功、winner +25、winner win_count +1、
// winner total_count +1、loser -15、loser total_count +1
```

---

## 四、问题追踪与修复

### 4.1 问题汇总

| # | 问题描述 | 严重性 | 发现阶段 | 修复耗时 |
|---|----------|--------|----------|----------|
| 1 | Json::Value 整数类型转换越界 | 🟡 测试失败 | 运行测试 | 5 min |
| 2 | 测试代码中 asInt() 类型不匹配 | 🟡 测试失败 | 运行测试 | 5 min |
| 3 | select_by_username 和 select_by_id 变量类型不匹配 | 🟡 测试失败 | 运行测试 | 10 min |
| 4 | MYSQL_BIND is_null 参数类型错误 | 🔴 编译警告 | 代码审查 | 5 min |
| 5 | is_null_false 变量定义位置错误 | 🟡 测试失败 | 运行测试 | 5 min |

### 4.2 根因分析

**问题 1：Json::Value 整数类型转换越界**
- **原因**: `Json::Value` 使用 `asInt()` 读取 `unsigned int` 变量，类型不匹配
- **修复**: 使用 `asLargestUInt()` 读取无符号整数
- **代码提交**: `dcbaae9 Fix: 使用 asLargestUInt() 读取无符号整数`

**问题 2：测试代码中 asInt() 类型不匹配**
- **原因**: 测试代码中使用 `asInt()` 读取 `unsigned int` 类型
- **修复**: 测试代码改用 `asLargestUInt()` 
- **代码提交**: `e6478e4 Fix: 修复测试代码中 Json::Value 类型读取方式`

**问题 3：select_by_username 和 select_by_id 变量类型不匹配**
- **原因**: MySQL `INT UNSIGNED` 是 32 位，但代码中使用 `unsigned long`（64 位）
- **修复**: 将变量类型从 `unsigned long` 改为 `unsigned int`
- **代码提交**: 
  - `3c8c711 Fix: 修复 MySQL 绑定类型不匹配问题`
  - `9352201 Fix: 修复 select_by_username 和 select_by_id 的变量类型`

**问题 4：MYSQL_BIND is_null 参数类型错误**
- **原因**: `is_null` 参数应该指向 `bool`，但使用了 `unsigned long`
- **修复**: 定义 `bool is_null_false = false;` 并使用 `&is_null_false`
- **代码提交**: `e72ca49 Fix: 修复 user_table.hpp 中 MYSQL_BIND is_null 参数类型`

**问题 5：is_null_false 变量定义位置错误**
- **原因**: `is_null_false` 定义在数组之后，作用域有问题
- **修复**: 将 `is_null_false` 定义移到数组声明之前
- **代码提交**: `bede317 Fix: 修复 select_by_username 和 select_by_id 中 is_null_false 变量定义`

### 4.3 修复时间线

```
[2026-04-12 修复时间线]
1. d428447 Feat: M2 Phase 2 用户数据访问层实现（初始实现）
2. 82c6e9a Fix: 修复 Json::Value 整数类型转换越界问题
3. e6478e4 Fix: 修复测试代码中 Json::Value 类型读取方式
4. cc6a07d Fix: 修复 Json::Value 类型转换语法
5. e72ca49 Fix: 修复 user_table.hpp 中 MYSQL_BIND is_null 参数类型
6. bede317 Fix: 修复 select_by_username 和 select_by_id 中 is_null_false 变量定义
7. 3c8c711 Fix: 修复 MySQL 绑定类型不匹配问题
8. 9352201 Fix: 修复 select_by_username 和 select_by_id 的变量类型（最终修复）
9. 运行测试：13 个断言全部通过 ✅
```

---

## 五、代码质量分析

### 5.1 代码度量

| 指标 | 数值 | 评价 |
|------|------|------|
| 总行数 | ~950 | 适中 |
| 函数数量 | 7（UserTable 类） | 精简 |
| 最大函数行数 | ~130（`update_score_match`） | 可接受 |
| 注释密度 | ~30% | 良好 |
| 测试覆盖 | 5 项 | 核心场景覆盖 |

### 5.2 设计模式应用

| 模式 | 位置 | 收益 |
|------|------|------|
| RAII | `DBPool::ConnGuard` | 连接自动归还，防止泄露 |
| 依赖注入 | `init(DBPool* pool)` | 便于测试和扩展 |
| 接口分离 | `select_for_auth` vs `select_by_username` | 安全性提升 |
| 事务 | `update_score_match` | 原子性保证 |

### 5.3 代码规范遵循

- ✅ C++11 标准
- ✅ RAII 资源管理
- ✅ 预处理语句防止 SQL 注入
- ✅ 命名一致性（`snake_case` 函数）
- ✅ 错误处理（日志记录 + 返回值）

---

## 六、核心知识点总结

### 6.1 MySQL C API 绑定规则

> **凡是 MYSQL_TYPE_LONG → 对应 C++ 变量必须是 `int` 或 `unsigned int`（32 位），不能用 `unsigned long`（64 位）**

| MYSQL_TYPE_xxx | C++ 对应类型 |
|----------------|-------------|
| MYSQL_TYPE_LONG | `int` / `unsigned int` |
| MYSQL_TYPE_LONGLONG | `int64_t` / `uint64_t` |
| MYSQL_TYPE_STRING | `char[]` + `unsigned long length` |
| MYSQL_TYPE_TINY | `int` / `char` |

### 6.2 事务保证原子性

```cpp
// 开启事务
mysql_query(conn.get(), "START TRANSACTION");

// 执行多个 SQL 操作
mysql_stmt_execute(stmt_winner);
mysql_stmt_execute(stmt_loser);

// 任何一方失败都回滚
if (winner_affected != 1 || loser_affected != 1) {
    mysql_query(conn.get(), "ROLLBACK");
    return false;
}

// 提交事务
mysql_query(conn.get(), "COMMIT");
```

### 6.3 接口分离设计

| 接口 | 返回字段 | 用途 |
|------|----------|------|
| `select_for_auth` | id, username, **password_hash**, score, total_count, win_count, status | 仅供认证模块验证密码 |
| `select_by_username` | id, username, score, total_count, win_count, status | 用户资料展示（公开） |
| `select_by_id` | id, username, score, total_count, win_count, status | 用户资料展示（公开） |

### 6.4 GREATEST 防止负数

```sql
-- 使用 GREATEST 确保分数不会变成负数
UPDATE user SET score = GREATEST(0, score + 25) WHERE id = ?;
```

### 6.5 MYSQL_BIND is_null 参数

```cpp
// is_null 必须指向 bool 类型
bool is_null_false = false;
result[i].is_null = &is_null_false;  // ✅ 正确

// 错误示例
unsigned long is_null = 0;  // ❌ 类型不匹配
```

### 6.6 Json::Value 读取方式

| 类型 | 读取方法 |
|------|----------|
| 无符号整数 | `.asLargestUInt()` |
| 有符号整数 | `.asInt64()` 或 `.asLargestInt()` |
| 字符串 | `.asString()` |
| 布尔值 | `.asBool()` |

---

## 七、经验教训

### 7.1 做得好的地方

1. **接口分离设计**
   - 代码审查发现安全风险
   - 及时将 `select` 接口拆分为 `select_for_auth` 和 `select_by_username`
   - 防止 password_hash 泄露

2. **事务保证原子性**
   - `update_score_match` 使用事务
   - 要么双方都更新成功，要么都失败
   - 符合业务需求

3. **问题修复迅速**
   - 发现问题当天修复
   - 连续 9 个提交，快速迭代
   - 最终 13 个断言全部通过

4. **测试断言明确**
   - 测试 5 有 7 个断言，覆盖所有更新字段
   - 明确验证 winner 和 loser 的分数变化

### 7.2 需要改进的地方

1. **变量类型匹配**
   - MySQL `INT UNSIGNED` 是 32 位
   - 初始实现使用 `unsigned long`（64 位）导致绑定错误
   - **改进**: 严格按照 MySQL C API 文档选择变量类型

2. **代码审查流程**
   - 多个问题在运行测试后才发现
   - **改进**: 代码实现完成后先审查，再编译测试

3. **文档先行**
   - 计划 v1.2 在实现前已修订
   - 但仍有细节问题未考虑到
   - **改进**: 计划文档更加详细，包括变量类型

---

## 八、风险与应对

### 8.1 当前风险

| 风险 | 概率 | 影响 | 应对措施 |
|------|------|------|----------|
| SQL 注入风险 | 低 | 高 | 全部使用预处理语句 ✅ |
| 用户名重复处理 | 中 | 中 | 捕获 ER_DUP_ENTRY 错误码 ✅ |
| MySQL 服务未启动 | 低 | 高 | 测试前检查 systemctl status mysqld |
| 事务未正确提交 | 低 | 高 | 检查 COMMIT/ROLLBACK 逻辑 ✅ |

### 8.2 Phase 3 前置条件

- [x] MySQL 服务已启动
- [x] 数据库 `gobang_db` 已创建
- [x] `user` 表已创建（`password_hash VARCHAR(512)`）
- [x] UserTable 实现完成 ✅
- [x] 测试全部通过 ✅

---

## 九、Phase 3 计划预览

### 9.1 待实现模块

| 模块 | 功能 | 预估工时 |
|------|------|----------|
| `security.hpp` | PBKDF2 密码加密 + JWT | 3-4h |
| HTTP 注册 API | /register 接口 | 1-2h |
| HTTP 登录 API | /login 接口 | 1-2h |

### 9.2 验收标准

- [ ] PBKDF2 密码哈希生成（iterations=29000）
- [ ] JWT Token 生成和验证
- [ ] 用户注册流程（密码加密 → 插入数据库）
- [ ] 用户登录流程（验证密码 → 生成 Token）

---

## 十、附录

### 10.1 文件清单

```
source/
├── include/
│   ├── db.hpp              # 数据库连接池（复用）
│   ├── logger.hpp          # 日志模块（复用）
│   ├── util.hpp            # 工具模块（复用）
│   └── user_table.hpp      # 用户数据访问层（新增）
├── tests/
│   └── test_user_table.cpp # 用户数据访问层测试（新增）
└── CMakeLists.txt          # 构建配置（复用）
```

### 10.2 编译与测试命令

```bash
# 虚拟机中执行
cd /home/guoziyin/Project/OnlineGobangBattle
git pull

cd source/build
cmake .. && cmake --build .

# 运行测试
./bin/test_user_table
```

### 10.3 测试结果

```
=== M2 Phase 2: 用户数据访问层测试 ===

[测试 1] 用户注册测试...
  [PASS] 注册成功应返回有效 ID (>0)

[测试 2] 重复用户名测试...
  [PASS] 重复用户名应返回 0

[测试 3] 认证查询测试（select_for_auth）...
  [PASS] select_for_auth 应找到已注册的用户
  [PASS] select_for_auth 应返回 password_hash 字段

[测试 4] 公开查询测试（select_by_username）...
  [PASS] select_by_username 应找到已注册的用户
  [PASS] select_by_username 不应返回 password_hash 字段

[测试 5] 分数更新测试（update_score_match）...
  [PASS] 创建两个测试用户应成功
  [PASS] update_score_match 应成功
  [PASS] winner 分数应 +25
  [PASS] winner win_count 应 +1
  [PASS] winner total_count 应 +1
  [PASS] loser 分数应 -15
  [PASS] loser total_count 应 +1

=== 测试完成 ===
通过：13, 失败：0
✅ 所有测试通过！
```

### 10.4 提交历史

| 提交 | 说明 |
|------|------|
| `9352201` | Fix: 修复 select_by_username 和 select_by_id 的变量类型 |
| `3c8c711` | Fix: 修复 MySQL 绑定类型不匹配问题 |
| `dcbaae9` | Fix: 使用 asLargestUInt() 读取无符号整数 |
| `cc6a07d` | Fix: 修复 Json::Value 类型转换语法 |
| `e6478e4` | Fix: 修复测试代码中 Json::Value 类型读取方式 |
| `bede317` | Fix: 修复 select_by_username 和 select_by_id 中 is_null_false 变量定义 |
| `e72ca49` | Fix: 修复 user_table.hpp 中 MYSQL_BIND is_null 参数类型 |
| `82c6e9a` | Fix: 修复 Json::Value 整数类型转换越界问题 |
| `d428447` | Feat: M2 Phase 2 用户数据访问层实现 |

---

**报告版本**: v1.0  
**生成日期**: 2026-04-12  
**下次更新**: M2 Phase 3 完成后