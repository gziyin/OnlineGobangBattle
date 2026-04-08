// 编译：cmake && cmake --build .
// 测试数据库连接池功能
#include "../include/db.hpp"
#include "../include/util.hpp"
#include "config.h"

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include <atomic>

using namespace gobang;

// 全局测试计数器
std::atomic<int> g_tests_passed{0};
std::atomic<int> g_tests_failed{0};

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            std::cout << "  [PASS] " << message << "\n"; \
            g_tests_passed++; \
        } else { \
            std::cout << "  [FAIL] " << message << "\n"; \
            g_tests_failed++; \
        } \
    } while(0)

int main() {
    std::cout << "=== M2 Phase 1: 数据库连接池测试 ===\n\n";

    // 加载配置
    util::Config cfg;
    try {
        cfg = util::load_config(GOBANG_CONFIG_PATH);
        std::cout << "[INFO] 配置加载成功：" << cfg.db_host << ":" << cfg.db_port << "/" << cfg.db_name << "\n\n";
    } catch (const std::exception& e) {
        std::cout << "[WARN] 配置加载失败：" << e.what() << "\n";
        std::cout << "[INFO] 使用默认配置继续测试...\n\n";
        cfg.db_host = "127.0.0.1";
        cfg.db_port = 3306;
        cfg.db_user = "root";
        cfg.db_password = "yourpassword"; // 需要替换为实际密码
        cfg.db_name = "gobang_db";
        cfg.db_pool_size = 5;
    }

    // ── 测试 1: 连接池初始化 ──────────────────────────────────────
    std::cout << "[测试 1] 连接池初始化...\n";
    try {
        DBPool pool;
        pool.init(cfg);
        TEST_ASSERT(pool.available_connections() == (size_t)cfg.db_pool_size,
                    "连接池初始化后应有 " + std::to_string(cfg.db_pool_size) + " 个可用连接");
        std::cout << "  [INFO] 池大小：" << pool.available_connections() << "\n\n";
    } catch (const std::exception& e) {
        std::cout << "  [FAIL] 连接池初始化失败：" << e.what() << "\n\n";
        g_tests_failed++;
        // 初始化失败，后续测试无法进行
        std::cout << "=== 测试终止：无法连接数据库 ===\n";
        std::cout << "请确保:\n";
        std::cout << "  1. MySQL 服务已启动\n";
        std::cout << "  2. 数据库 gobang_db 已创建\n";
        std::cout << "  3. server.conf 配置正确\n\n";
        return 1;
    }

    // ── 测试 2: 获取/归还连接 ────────────────────────────────────
    std::cout << "[测试 2] 获取/归还连接...\n";
    {
        DBPool pool;
        pool.init(cfg);

        // 获取一个连接
        auto conn1 = pool.get_connection();
        TEST_ASSERT(conn1 != nullptr, "获取连接应成功");

        if (conn1) {
            // 验证连接有效（ping）
            int ping_result = mysql_ping(conn1.get());
            TEST_ASSERT(ping_result == 0, "连接 ping 测试应成功");

            // 检查获取后池大小减 1
            TEST_ASSERT(pool.available_connections() == (size_t)cfg.db_pool_size - 1,
                        "获取连接后池大小应减 1");
        }

        // 归还连接（作用域结束自动归还）
    }
    std::cout << "  [INFO] 连接已归还（RAII 自动归还）\n\n";

    // ── 测试 3: 并发获取连接（无超时） ─────────────────────────────
    std::cout << "[测试 3] 并发获取连接（无超时）...\n";
    {
        DBPool pool;
        cfg.db_pool_size = 5; // 小池大小，便于测试并发
        pool.init(cfg);

        std::atomic<int> success_count{0};
        std::atomic<int> null_count{0};
        std::mutex log_mtx;

        // 10 个线程同时获取连接，池大小为 5
        std::vector<std::thread> threads;
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&pool, &success_count, &null_count, &log_mtx, i]() {
                auto conn = pool.get_connection(3000); // 3 秒超时
                if (conn) {
                    success_count++;
                    std::lock_guard<std::mutex> lock(log_mtx);
                    std::cout << "  [DEBUG] 线程 " << i << " 获取连接成功\n";
                    // 模拟使用连接
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    // 作用域结束自动归还
                } else {
                    null_count++;
                    std::lock_guard<std::mutex> lock(log_mtx);
                    std::cout << "  [DEBUG] 线程 " << i << " 获取连接超时\n";
                }
            });
        }

        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }

        TEST_ASSERT(success_count == 5, "应有 5 个线程成功获取连接（池大小）");
        TEST_ASSERT(null_count == 5, "应有 5 个线程超时（10 线程 - 5 连接）");
        std::cout << "  [INFO] 成功：" << success_count << ", 超时：" << null_count << "\n\n";
    }

    // ── 测试 4: 超时机制验证 ─────────────────────────────────────
    std::cout << "[测试 4] 超时机制验证（1 秒超时）...\n";
    {
        DBPool pool;
        cfg.db_pool_size = 1; // 只有 1 个连接
        pool.init(cfg);

        // 占用唯一的连接
        auto conn_holder = pool.get_connection();
        TEST_ASSERT(conn_holder != nullptr, "第一个连接应成功获取");

        // 记录开始时间
        auto start = std::chrono::steady_clock::now();

        // 尝试获取第二个连接（应该超时）
        auto conn_timeout = pool.get_connection(1000); // 1 秒超时

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        TEST_ASSERT(conn_timeout == nullptr, "第二个连接应返回 nullptr（超时）");
        TEST_ASSERT(elapsed >= 900 && elapsed <= 1500,
                    "超时时间应在 900-1500ms 之间（实际：" + std::to_string(elapsed) + "ms)");
        std::cout << "  [INFO] 实际超时时间：" << elapsed << "ms\n\n";
    }

    // ── 测试 5: 归还后可用性验证 ──────────────────────────────────
    std::cout << "[测试 5] 归还后可用性验证...\n";
    {
        DBPool pool;
        cfg.db_pool_size = 2;
        pool.init(cfg);

        std::atomic<bool> thread_got_connection{false};
        std::atomic<bool> done{false};

        // 启动一个线程等待连接
        std::thread waiter([&pool, &thread_got_connection, &done]() {
            auto conn = pool.get_connection(3000);
            if (conn) {
                thread_got_connection = true;
                std::cout << "  [DEBUG] 等待线程获取到连接\n";
            }
            done = true;
        });

        // 主线程占用所有连接
        auto conn1 = pool.get_connection();
        auto conn2 = pool.get_connection();
        TEST_ASSERT(conn1 && conn2, "主线程应获取 2 个连接");

        // 等待 100ms 确保 waiter 线程开始等待
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 归还一个连接
        conn1.reset(); // 显式归还

        // 等待 waiter 线程获取连接
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        TEST_ASSERT(thread_got_connection == true, "归还后等待线程应能获取连接");
        TEST_ASSERT(done == true, "等待线程应完成");

        waiter.join();
        std::cout << "  [INFO] 归还机制验证通过\n\n";
    }

    // ── 完成 ────────────────────────────────────────────────
    std::cout << "=== 测试完成 ===\n";
    std::cout << "通过：" << g_tests_passed << ", 失败：" << g_tests_failed << "\n";

    if (g_tests_failed == 0) {
        std::cout << "✅ 所有测试通过！\n";
        return 0;
    } else {
        std::cout << "❌ 有 " << g_tests_failed << " 个测试失败\n";
        return 1;
    }
}
