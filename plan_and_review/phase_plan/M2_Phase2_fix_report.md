# M2 Phase 2 测试问题修复复盘报告

> **项目**: C++ 在线五子棋对战系统
> **阶段**: M2 Phase 2 用户数据访问层
> **日期**: 2026-04-12
> **状态**: ✅ 测试全部通过

---

## 一、问题概述

M2 Phase 2 开发完成后，运行 `test_user_table` 测试时出现错误：
- **错误现象**: `score`、`total_count`、`win_count` 显示为随机内存值（如 `140733193389557`）
- **影响范围**: 测试 4（select_by_username）、测试 5（select_by_id）

---

## 二、问题根因

### 核心问题：MySQL 绑定类型不匹配

| 类型 | 大小 | 说明 |
|------|------|------|
| MySQL `INT UNSIGNED` | 32 位 | 数据库字段类型 |
| `MYSQL_TYPE_LONG` | 32 位 | MySQL C API 类型常量 |
| `unsigned long`（代码使用） | **64 位** | Linux x86-64 是 8 字节 |

**问题机制**：
```
MYSQL 写入: 4 字节 (32位)  →  buffer 高 4 字节未初始化 →  读取时得到随机值
```

### 错误假设排查（走了弯路）

1. **Json::Value 类型问题** - 表面现象，先修复了 JSON 转换，但没解决根本问题
2. **`replace_all` 不够精确** - 只改了 `id` 一行，其他变量没改到

---

## 三、解决方案

### 修复代码（user_table.hpp）

将三个查询函数中的变量类型从 `unsigned long` 改为 `unsigned int`：

```cpp
// 修改前（错误）
unsigned long id;
unsigned long score;
unsigned long total_count;
unsigned long win_count;

// 修改后（正确）
unsigned int id;
unsigned int score;
unsigned int total_count;
unsigned int win_count;
```

### 涉及的函数（全部修复）

| 函数 | 状态 | 说明 |
|------|------|------|
| select_for_auth | ✅ 已修复 | 测试 3 使用 |
| select_by_username | ✅ 已修复 | 测试 4 使用 |
| select_by_id | ✅ 已修复 | 测试 5 使用 |

### 提交历史

| 提交 | 说明 |
|------|------|
| `9352201` | Fix: 修复 select_by_username 和 select_by_id 的变量类型 |
| `3c8c711` | Fix: 修复 MySQL 绑定类型不匹配问题 |
| `dcbaae9` | Fix: 使用 asLargestUInt() 读取无符号整数 |
| `e6478e4` | Fix: 修复测试代码中 Json::Value 类型读取方式 |
| `82c6e9a` | Fix: 修复 Json::Value 整数类型转换越界问题 |

---

## 四、关键知识点

### 1. MySQL C API 绑定规则

> **凡是 MYSQL_TYPE_LONG → 对应 C++ 变量必须是 `int` 或 `unsigned int`（32位），不能用 `unsigned long`（64位）**

| MYSQL_TYPE_xxx | C++ 对应类型 |
|----------------|-------------|
| MYSQL_TYPE_LONG | `int` / `unsigned int` |
| MYSQL_TYPE_LONGLONG | `int64_t` / `uint64_t` |
| MYSQL_TYPE_STRING | `char[]` + `unsigned long length` |

### 2. 三个函数是独立的

每个查询函数都有自己的局部变量声明，必须单独修复：
- `select_for_auth` - 认证查询
- `select_by_username` - 用户名查询（公开）
- `select_by_id` - ID 查询（公开）

### 3. 随机大数字的第一反应

看到类似 `140733193389557` 的随机大数字，立即想到：
- **MYSQL_BIND 的 buffer 变量类型比 MySQL 列类型宽了一倍**
- 高位字节未初始化

### 4. Json::Value 读取方式

- 读取无符号整数使用 `.asLargestUInt()` 最安全
- 读取有符号整数使用 `.asInt64()` 或 `.asLargestInt()`

---

## 五、经验教训

| 教训 | 说明 |
|------|------|
| 先找根因 | 先解决根本问题（类型不匹配），再处理表面现象（JSON 转换） |
| 检查所有函数 | 确认所有相关的函数都已修复，不要遗漏 |
| `replace_all` 局限 | 精确字符串匹配，只能改完全相同的行 |
| 变量类型匹配 | MySQL C API 绑定时，变量类型必须与 MYSQL_TYPE_xxx 严格匹配 |

---

## 六、验证结果

```bash
[guoziyin@localhost build]$ ./bin/test_user_table
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

---

## 七、下一步

M2 Phase 2 完成 ✅

进入 **Phase 3：密码加密模块**
- 安装 OpenSSL 开发库
- 下载 jwt-cpp（header-only 库）
- 实现 PBKDF2 密码哈希

---

*文档版本：v1.0*
*制定日期：2026-04-12*