# 质量规范

> 后端代码质量标准

---

## 概述

项目使用 C++11，纯头文件实现（`.hpp` + `inline`），Google Test 测试框架，CMake + CTest 构建/测试。线程安全通过 `std::mutex` + `std::lock_guard` 实现。

---

## 禁止的模式

1. **裸 `new`/`delete`** —— 使用 RAII 包装（`unique_ptr`、`ConnGuard`）
2. **裸 `MYSQL*`** —— 始终用 `DBPool::get_connection()` 返回的 `ConnGuard`
3. **`std::cout` 输出日志** —— 使用 `LOG_DEBUG/INFO/WARN/ERROR` 宏
4. **请求处理器中抛异常** —— 用返回值携带错误信息；仅致命初始化错误可抛异常
5. **`.cpp` 实现文件** —— 当前阶段所有代码写在 `.hpp` 中（纯头文件约定）
6. **无同步的全局可变状态** —— 每个共享变量都需要 `std::mutex` + `std::lock_guard`
7. **SQL 字符串拼接** —— 始终用预处理语句（`mysql_stmt_*`）
8. **析构函数中抛异常** —— 可能导致 `std::terminate`

---

## 必须遵循的模式

1. **命名空间**：所有代码在 `gobang::` 下（子命名空间如 `gobang::auth::`、`gobang::util::`）
2. **头文件保护**：`#pragma once`
3. **Inline 实现**：所有方法体在 `.hpp` 文件内，标记 `inline`
4. **RAII 管理资源**：连接、锁、文件句柄 —— 全部通过 RAII 管理
5. **`std::lock_guard` 加锁**：不手动 lock/unlock
6. **预处理语句执行 SQL**：不用 `mysql_query()` 拼接字符串
7. **`LOG_*` 宏记录日志**：自动包含文件/行号
8. **`(void)param` 标记未使用参数**：消除编译器警告

---

## 测试要求

### 测试分类（CTest 标签）

| 标签 | 依赖要求 | 示例 |
|------|---------|------|
| `unit` | 无外部依赖（无 MySQL、无网络） | `test_security`、`test_matcher`、`test_online` |
| `integration` | 需要运行中的 MySQL | `test_db_pool`、`test_user_table`、`test_auth_api` |
| `smoke` | 需要配置文件，可能需要 MySQL + 网络 | `test_base`、`websocket_smoke` |

### 测试风格

使用 Google Test 的 `TEST()` 和 `TEST_F()` fixture：

```cpp
#include <gtest/gtest.h>

class MatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        matcher_.init(&online_mgr_);
        matcher_.start();
    }
    void TearDown() override {
        matcher_.stop();
    }
    // 辅助方法和成员...
    Matcher matcher_;
    OnlineManager online_mgr_;
};

TEST_F(MatcherTest, 同分段用户匹配成功) {
    online_mgr_.user_online(1);
    online_mgr_.user_online(2);
    ASSERT_TRUE(matcher_.enqueue(1, 1200));
    ASSERT_TRUE(matcher_.enqueue(2, 1250));
    wait_for_results(1);
    // 断言...
}
```

参考：`source/tests/test_matcher.cpp`

### 在 CMake 中注册测试

```cmake
add_gobang_test(test_security tests/test_security.cpp
    ${JSONCPP_LIBRARIES} ${OPENSSL_LIBRARIES} ${GTEST_LIBRARIES} Threads::Threads
)
register_gobang_ctest(test_security unit)
```

参考：`source/CMakeLists.txt`

### 运行测试

```bash
cmake -S source -B source/build
cmake --build source/build -j
ctest --test-dir source/build -L unit --output-on-failure          # 无需 MySQL
ctest --test-dir source/build -L integration --output-on-failure   # 需要 MySQL
ctest --test-dir source/build -L smoke --output-on-failure
```

---

## 线程安全

- 每个有可变共享状态的类使用 `std::mutex` + `std::lock_guard`
- 不使用裸 `lock()`/`unlock()` —— 始终 RAII
- 后台线程（Matcher、Logger）使用 `std::condition_variable` 信号
- 简单标志用 `std::atomic<bool>`（如 `_running`）

---

## 测试中的线程控制与对象生命周期（CRITICAL）

集成测试涉及 `Logger`、`Matcher`、WebSocket++/ASIO 等多线程组件，**重点防止 `std::terminate` 与随机崩溃**：

- **避免跨用例复用全局可变对象**：不要在 `source/tests/*.cpp` 里复用全局 `WebsocketServer/Matcher/GameController/...` 并反复 `start()/stop()`；推荐每个用例创建独立实例（fixture 成员/本地 server 包装类成员）。
- **Logger 只 init 一次**：`Logger::init` 可能包含线程，重复 init 易触发 `std::terminate`；用 `std::call_once` 或在 `main()` 初始化一次。
- **线程要 stop + join**：测试启动的 server/client/worker 线程必须在 `TearDown()`/`stop()` 中停止并 `join()`；禁止析构时线程仍 `joinable()`。
- **ASSERT_* 的返回类型约束**：禁止在“返回非 void”的 helper 内使用 `ASSERT_*`；改为 `void + out 参数` 或返回 `testing::AssertionResult`。
- **事件语义对齐**：断言要匹配实现（例如胜负步可能只发 `game.over`，不再发 `game.move`）。

---

## 代码审查清单

- [ ] 新 `.hpp` 文件使用 `#pragma once` 和 `namespace gobang`
- [ ] 所有方法标记 `inline`
- [ ] 无裸 `new`/`delete` 或裸 `MYSQL*`
- [ ] 所有 SQL 使用预处理语句
- [ ] 所有共享状态有 mutex 保护
- [ ] 新测试在 CMakeLists.txt 中注册并标注正确标签
- [ ] 使用 `LOG_*` 宏（不用 `std::cout`）
- [ ] 日志中不包含密码或密钥
- [ ] 未使用参数标记 `(void)param`
