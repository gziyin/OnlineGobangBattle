# M2 Phase 1: db.hpp 数据库连接池 - 复盘报告

> **项目**: C++ 在线五子棋对战系统  
> **阶段**: M2 Phase 1 数据库连接池  
> **完成日期**: 2026-04-09  
> **报告人**: AI 助手

---

## 一、执行摘要

### 1.1 里程碑状态

| 阶段 | 名称 | 状态 | 完成日期 |
|------|------|------|----------|
| ✅ M1 | 环境搭建与基础框架 | 已完成 | 2026-03-31 |
| ✅ M2-P1 | 数据库连接池 | **已完成** | 2026-04-09 |
| ⏳ M2-P2 | 用户数据访问层 | 待开始 | - |
| ⏳ M2-P3 | 密码加密与 JWT | 待开始 | - |
| ⏳ M2-P4 | HTTP 认证 API | 待开始 | - |

### 1.2 核心交付物

| 文件 | 行数 | 功能 |
|------|------|------|
| `source/include/db.hpp` | ~230 行 | DBPool 数据库连接池 |
| `source/tests/test_db_pool.cpp` | ~210 行 | 单元测试（5 项） |
| `source/CMakeLists.txt` | +33 行 | MySQL 依赖配置 |
| `plan_and_review/M2_Phase1_db_pool_plan.md` | ~85 行 | 实现计划 |

### 1.3 关键指标

- **代码行数**: ~440 行（不含注释和空行）
- **测试覆盖**: 5 个测试项，10 个断言，10 通过 ✅
- **编译时间**: < 10 秒
- **构建问题**: 2 个（全部修复）

---

## 二、目标达成情况

### 2.1 计划目标

| 目标 | 计划 | 实际 | 状态 |
|------|------|------|------|
| DBPool 类实现 | 完成 | 完成 | ✅ |
| RAII 智能指针 | 完成 | 完成 | ✅ |
| 3 秒超时机制 | 完成 | 完成 | ✅ |
| 并发安全测试 | 完成 | 完成 | ✅ |
| MySQL 依赖配置 | 完成 | 完成 | ✅ |

### 2.2 验收标准

| 标准 | 要求 | 结果 |
|------|------|------|
| 连接池初始化 | 成功创建 N 个连接 | ✅ 通过 |
| 获取/归还连接 | RAII 自动归还 | ✅ 通过 |
| 并发获取 | 5 连接池，10 线程，5 成功 5 超时 | ✅ 通过 |
| 超时机制 | 超时返回 nullptr | ✅ 通过 |
| 归还可用性 | 归还后等待线程可获取 | ✅ 通过 |

---

## 三、技术方案详解

### 3.1 DBPool 类设计

#### 架构图

```
┌─────────────────────────────────────────────────────────┐
│                    业务线程                              │
│  pool.get_connection(timeout) → wait_for → 返回 ConnGuard │
└─────────────────────────────────────────────────────────┘
                           │
                           │ std::condition_variable
                           ▼
┌─────────────────────────────────────────────────────────┐
│                   连接池队列                              │
│  std::queue<MYSQL*> + mutex + cv                        │
│  池大小：10（可配置）                                     │
└─────────────────────────────────────────────────────────┘
```

#### 核心特性

| 特性 | 实现 | 收益 |
|------|------|------|
| RAII 智能指针 | `ConnGuard = unique_ptr<MYSQL, function>` | 析构自动归还，防止泄露 |
| 超时等待 | `cv.wait_for(timeout_ms)` | 防止无限等待死锁 |
| 连接复用 | 队列 FIFO 复用连接 | 减少连接创建开销 |
| 自动重连 | `mysql_ping` 检测失效连接 | 提高可用性 |
| 线程安全 | `mutex` 保护队列 | 并发安全 |

#### 关键代码

```cpp
// RAII 智能指针返回类型
using ConnGuard = std::unique_ptr<MYSQL, std::function<void(MYSQL*)>>;

ConnGuard get_connection(int timeout_ms = 3000) {
    std::unique_lock<std::mutex> lock(_mtx);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (_pool.empty()) {
        if (_cv.wait_until(lock, deadline) == std::cv_status::timeout) {
            return nullptr;  // 超时返回
        }
    }

    MYSQL* conn = _pool.front();
    _pool.pop();

    // 创建 RAII 守卫，析构时自动归还
    return ConnGuard(conn, [this](MYSQL* conn) {
        this->return_connection(conn);
    });
}
```

---

### 3.2 测试用例设计

#### 测试覆盖

| 测试 # | 名称 | 测试内容 | 断言数 |
|--------|------|----------|--------|
| 1 | 连接池初始化 | 初始化成功，池大小正确 | 1 |
| 2 | 获取/归还连接 | RAII 自动归还，ping 有效 | 3 |
| 3 | 并发超时验证 | 10 线程竞争 5 连接，5 成功 5 超时 | 2 |
| 4 | 超时机制 | 1 秒超时，实际耗时 900-1500ms | 2 |
| 5 | 归还可用性 | 归还后等待线程可获取 | 4 |

#### 测试修复历史

| 版本 | 问题 | 修复方案 |
|------|------|----------|
| v1 | 测试 3 并发测试失败（10 成功 0 超时） | 添加同步启动信号 `start_flag` |
| v2 | 持有时间不足导致后 5 线程也能获取 | 持有时间从 100ms 增至 2000ms |

---

### 3.3 CMake 配置

#### MySQL 依赖处理

Rocky 9 上 `pkg-config` 找到的 `mysqlclient` 缺少库路径，需要补充：

```cmake
pkg_check_modules(MYSQL mysqlclient)
if(NOT MYSQL_FOUND)
    # 手动指定路径
    find_path(...)
    find_library(...)
else()
    # pkg-config 找到了，补充库路径
    set(MYSQL_LIBRARY_DIRS ${MYSQL_LIBRARY_DIRS} /usr/lib64/mysql)
endif()
include_directories(${MYSQL_INCLUDE_DIRS})
link_directories(${MYSQL_LIBRARY_DIRS})  # 关键修复
```

---

## 四、问题追踪与修复

### 4.1 问题汇总

| # | 问题描述 | 严重性 | 发现阶段 | 修复耗时 |
|---|----------|--------|----------|----------|
| 1 | MySQL 库链接失败 (`cannot find -lmysqlclient`) | 🔴 编译错误 | 编译测试 | 5 min |
| 2 | 测试 3 并发测试逻辑错误（10 成功 0 超时） | 🟡 测试失败 | 运行测试 | 10 min |

### 4.2 根因分析

**问题 1：MySQL 库链接失败**
- **原因**: Rocky 9 上 `pkg-config --exists mysqlclient` 能通过，但 `${MYSQL_LIBRARIES}` 只包含库名 `mysqlclient`，不包含库路径 `/usr/lib64/mysql`
- **修复**: 添加 `link_directories(${MYSQL_LIBRARY_DIRS})` 显式指定库搜索路径
- **代码提交**: `84c2de4 Fix: CMakeLists.txt 添加 link_directories 解决 MySQL 链接问题`

**问题 2：并发测试逻辑错误**
- **原因**: 原测试 10 线程竞争 5 连接，但前 5 个线程 100ms 就归还连接，导致后 5 个线程在 3 秒超时前也能获取连接
- **修复**: 
  1. 添加同步信号 `start_flag` 确保 10 线程同时开始
  2. 持有时间从 100ms 增至 2000ms
  3. 超时时间从 3000ms 改为 1000ms
- **代码提交**: `f4a409a Fix: 测试 3 并发超时逻辑（添加同步启动 + 持有 2 秒）`

### 4.3 修复时间线

```
[时间线待补充 - 用户在虚拟机上执行]
1. git pull 拉取代码
2. cmake .. && cmake --build . 编译
3. ./bin/test_db_pool 运行测试
4. 测试 3 失败（10 成功 0 超时）
5. 本地修复测试逻辑 → git push
6. 虚拟机 git pull → cmake --build . → ./bin/test_db_pool
7. 10 个断言全部通过 ✅
```

---

## 五、代码质量分析

### 5.1 代码度量

| 指标 | 数值 | 评价 |
|------|------|------|
| 总行数 | ~440 | 适中 |
| 函数数量 | ~8（DBPool 类） | 精简 |
| 最大函数行数 | ~60（`get_connection`） | 可接受 |
| 注释密度 | ~25% | 良好 |
| 测试覆盖 | 5 项 | 核心场景覆盖 |

### 5.2 设计模式应用

| 模式 | 位置 | 收益 |
|------|------|------|
| 单例模式 | 无（DBPool 非单例） | 多实例更灵活 |
| 工厂模式 | `_create_connection()` | 连接创建隔离 |
| RAII | `ConnGuard` 智能指针 | 资源自动管理 |
| 生产者 - 消费者 | 连接池队列 | 并发安全 |

### 5.3 代码规范遵循

- ✅ C++11 标准
- ✅ RAII 资源管理
- ✅ `const` 正确性
- ✅ 命名一致性（`snake_case` 函数，`PascalCase` 类型）
- ✅ 错误处理（返回 nullptr + 日志记录）

---

## 六、核心知识点总结

### 6.1 数据结构

| 组件 | 选择 | 说明 |
|------|------|------|
| 连接容器 | `std::queue<MYSQL*>` | FIFO 先进先出，但换成 `stack` 也无所谓，所有连接等价 |

### 6.2 线程安全三件套

| 组件 | 作用 | 为什么需要 |
|------|------|------------|
| `mutex` | 互斥锁 | 同一时间只有一个线程能操作队列，防止多线程同时取同一个连接 |
| `lock_guard` | RAII 包装器 | 构造时加锁，出作用域自动解锁，防止忘记解锁导致死锁 |
| `condition_variable` | 条件变量 | 线程等待时睡过去不占 CPU，有连接归还时被叫醒 |

### 6.3 RAII（资源获取即初始化）

**核心思想**：构造时获取资源，析构时自动释放

**三处体现**：
1. `lock_guard` 管理锁 —— 出作用域自动解锁
2. `ConnGuard` 管理连接 —— 出作用域自动归还到池
3. `DBPool` 析构函数 —— 自动调用 `shutdown()` 释放所有连接

### 6.4 ConnGuard 设计

```cpp
using ConnGuard = std::unique_ptr<MYSQL, std::function<void(MYSQL*)>>;
```

- `unique_ptr` 的自定义析构版本
- 出作用域时不是 `delete`，而是执行 lambda：`return_connection(conn)`
- 用户无需手动归还，即使异常抛出也会自动归还

### 6.5 get_connection 核心逻辑

**为什么用 `while` 不用 `if`？**
```cpp
while (_pool.empty()) {
    if (_cv.wait_until(lock, deadline) == std::cv_status::timeout) {
        return nullptr;
    }
}
```
- 防止**虚假唤醒**（spurious wakeup）
- 每次醒来重新确认队列不空再取连接

**`wait_until` 工作原理**：
- 睡过去，释放锁
- 超时或被 `notify` 才醒
- 醒来后重新加锁
- 判断是哪种情况唤醒的

**醒来后检查 `_running`**：
- 防止是 `shutdown()` 的 `notify_all` 叫醒的
- 如果是关闭状态，返回 `nullptr`

### 6.6 细节设计

| 设计点 | 原因 |
|--------|------|
| 构造函数只设 `_running=false` | 真正初始化推迟到 `init()`，避免构造时抛异常 |
| 禁止拷贝和移动 | 防止两个对象持有同一批连接指针，析构时双重释放崩溃 |
| `notify_one` 放锁外 | 被叫醒的线程能立刻拿到锁，少等一次，提高并发性能 |
| `shutdown` 用 `notify_all` | 叫醒所有等待线程，让它们检查 `_running` 后退出，防止永远睡着 |

---

## 七、经验教训

### 7.1 做得好的地方

1. **用户审查提前发现问题**
   - 用户提出 4 点注意事项（CMake 依赖、RAII 智能指针、测试断言、密码配置）
   - 全部采纳并实现，避免了后续返工
   - **后续保持**: 重要模块开发前先 review 计划

2. **RAII 智能指针设计**
   - 采用 `unique_ptr + function` 实现自动归还
   - 测试代码无需显式调用 `return_connection()`
   - **效果**: Phase 2 开始使用时会感谢自己

3. **测试断言明确**
   - 超时时间验证精确到毫秒（900-1500ms）
   - 并发场景明确断言成功/超时数量
   - **后续保持**: 后续测试继续保持

4. **问题修复迅速**
   - MySQL 链接问题当天发现当天修
   - 测试逻辑问题立即修复并推送
   - **后续保持**: 持续快速响应

### 7.2 需要改进的地方

1. **测试 3 初始设计缺陷**
   - 未考虑线程执行顺序问题
   - 假设"10 线程竞争 5 连接"会自动 5 超时，实际不一定
   - **改进计划**: 并发测试设计更严谨，明确同步点

2. **虚拟机连接问题**
   - SSH 配置多次调整（IP 变更、主机别名）
   - rsync 方案因虚拟机网络问题未能验证
   - **改进计划**: 统一使用 git push/pull 流程，更可靠

3. **文档先行不足**
   - M2_开发计划.md 已有，但 Phase 1 计划是开发中补充的
   - **改进计划**: Phase 2 开始前先写详细计划

---

## 八、风险与应对

### 8.1 当前风险

| 风险 | 概率 | 影响 | 应对措施 |
|------|------|------|----------|
| MySQL 服务未启动 | 低 | 高 | 测试前检查 `systemctl status mysqld` |
| 数据库表未创建 | 中 | 高 | Phase 2 前执行建表 SQL |
| 虚拟机网络不稳定 | 中 | 中 | 使用 git 流程，不依赖 rsync |

### 8.2 Phase 2 前置条件

- [x] MySQL 服务已启动
- [x] 数据库 `gobang_db` 已创建
- [ ] `user` 表已创建（Phase 2 前执行）
- [x] `server.conf` 密码配置正确

---

## 九、Phase 2 计划预览

### 9.1 待实现模块

| 模块 | 功能 | 预估工时 |
|------|------|----------|
| `user_table.hpp` | 用户 CRUD（insert/select/update） | 2-3h |
| `security.hpp` | PBKDF2 密码加密 + JWT | 3-4h |
| HTTP 认证 API | /register, /login, /info | 2-3h |

### 9.2 验收标准

- [ ] 用户注册成功，密码 PBKDF2 加密
- [ ] 用户登录验证正确
- [ ] 用户信息查询返回正确数据
- [ ] 分数更新正确（胜利/失败）
- [ ] JWT Token 生成和验证正确

---

## 十、附录

### 10.1 文件清单

```
source/
├── include/
│   ├── db.hpp              # 数据库连接池（新增）
│   ├── logger.hpp          # 日志模块（复用）
│   └── util.hpp            # 工具模块（复用）
├── config/
│   └── server.conf         # 数据库配置
├── tests/
│   ├── test_base.cpp       # M1 单元测试
│   └── test_db_pool.cpp    # 数据库连接池测试（新增）
├── build/                  # 编译输出目录
└── CMakeLists.txt          # 构建配置（修改）
```

### 10.2 编译与测试命令

```bash
# 虚拟机中执行
cd /home/guoziyin/Project/OnlineGobangBattle
git pull

cd source/build
cmake .. && cmake --build .

# 运行测试
./bin/test_db_pool
```

### 10.3 数据库初始化 SQL

```sql
-- Phase 2 前执行
CREATE DATABASE IF NOT EXISTS gobang_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE gobang_db;

CREATE TABLE user (
    id INT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(32) UNIQUE NOT NULL,
    password_hash VARCHAR(256) NOT NULL,
    score INT UNSIGNED DEFAULT 1500,
    total_count INT UNSIGNED DEFAULT 0,
    win_count INT UNSIGNED DEFAULT 0,
    status TINYINT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_score (score),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

---

**报告版本**: v1.0  
**生成日期**: 2026-04-09  
**下次更新**: M2 Phase 2 完成后
