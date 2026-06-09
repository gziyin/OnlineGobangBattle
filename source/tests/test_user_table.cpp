// 编译：cmake && cmake --build .
// 测试用户数据访问层（user_table.hpp）
#include "../include/user_table.hpp"
#include "../include/db.hpp"
#include "../include/util.hpp"
#include "test_config.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
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
    std::cout << "=== M2 Phase 2: 用户数据访问层测试 ===\n\n";

    // 加载配置
    util::Config cfg;
    try {
        cfg = testutil::load_test_config();
        std::cout << "[INFO] 测试配置加载成功：" << cfg.db_host << ":" << cfg.db_port << "/"
                  << cfg.db_name << "\n\n";
    } catch (const std::exception& e) {
        std::cout << "[FAIL] 测试配置加载失败：" << e.what() << "\n";
        std::cout << "[INFO] 请复制 source/config/server.conf.test.example 为 "
                     "source/config/server.conf.test 并创建 gobang_db_test\n";
        return 1;
    }

    // 初始化日志（测试用）
    try {
        Logger::instance().init("logs/test.log", LogLevel::DEBUG);
        LOG_INFO("test_user_table: test started");
    } catch (const std::exception& e) {
        std::cout << "[WARN] Logger init failed: " << e.what() << "\n";
    }

    // 初始化 DBPool
    DBPool pool;
    try {
        pool.init(cfg);
        std::cout << "[INFO] DBPool 初始化成功\n\n";
    } catch (const std::exception& e) {
        std::cout << "[FAIL] DBPool 初始化失败：" << e.what() << "\n";
        std::cout << "=== 测试终止：无法连接数据库 ===\n";
        return 1;
    }

    // 初始化 UserTable
    UserTable user_table;
    user_table.init(&pool);

    // ── 前置准备：清空 user 表 ──────────────────────────────────────
    std::cout << "[前置] 清空 user 表...\n";
    {
        auto conn = pool.get_connection();
        if (mysql_query(conn.get(), "TRUNCATE TABLE user") == 0) {
            std::cout << "  [INFO] TRUNCATE TABLE user 成功\n\n";
        } else {
            std::cout << "  [FAIL] TRUNCATE TABLE user 失败：" << mysql_error(conn.get()) << "\n\n";
            g_tests_failed++;
        }
    }

    // ── 测试 1: 用户注册测试 ──────────────────────────────────────
    std::cout << "[测试 1] 用户注册测试...\n";
    {
        int64_t user_id = user_table.insert("testuser", "hash_pbkdf2_sha256_xxx");
        TEST_ASSERT(user_id > 0, "注册成功应返回有效 ID (>0)");
        std::cout << "  [INFO] 注册返回 user_id: " << user_id << "\n\n";
    }

    // ── 测试 2: 重复用户名测试 ────────────────────────────────────
    std::cout << "[测试 2] 重复用户名测试...\n";
    {
        int64_t ret = user_table.insert("testuser", "hash_another");
        TEST_ASSERT(ret == 0, "重复用户名应返回 0");
        std::cout << "  [INFO] 重复注册返回：" << ret << "\n\n";
    }

    // ── 测试 3: 认证查询测试（返回 password_hash） ─────────────────
    std::cout << "[测试 3] 认证查询测试（select_for_auth）...\n";
    {
        Json::Value out;
        bool ok = user_table.select_for_auth("testuser", out);
        TEST_ASSERT(ok == true, "select_for_auth 应找到已注册的用户");

        if (ok) {
            TEST_ASSERT(out.isMember("password_hash"), "select_for_auth 应返回 password_hash 字段");
            std::cout << "  [INFO] password_hash: " << out["password_hash"].asString() << "\n";
        }
        std::cout << "\n";
    }

    // ── 测试 4: 公开查询测试（不返回 password_hash） ────────────────
    std::cout << "[测试 4] 公开查询测试（select_by_username）...\n";
    {
        Json::Value out;
        bool ok = user_table.select_by_username("testuser", out);
        TEST_ASSERT(ok == true, "select_by_username 应找到已注册的用户");

        if (ok) {
            TEST_ASSERT(!out.isMember("password_hash"), "select_by_username 不应返回 password_hash 字段");
            std::cout << "  [INFO] 公开信息：username=" << out["username"].asString()
                      << ", score=" << out["score"].asLargestUInt() << "\n";
        }
        std::cout << "\n";
    }

    // ── 测试 5: 分数更新测试（update_score_match 原子性） ───────────
    std::cout << "[测试 5] 分数更新测试（update_score_match）...\n";
    {
        // 创建两个测试用户
        int64_t winner_id = user_table.insert("winner_user", "hash_winner");
        int64_t loser_id = user_table.insert("loser_user", "hash_loser");
        TEST_ASSERT(winner_id > 0 && loser_id > 0, "创建两个测试用户应成功");

        if (winner_id > 0 && loser_id > 0) {
            // 查询初始分数
            Json::Value winner_before, loser_before;
            user_table.select_by_id(winner_id, winner_before);
            user_table.select_by_id(loser_id, loser_before);
            int initial_winner_score = (int)winner_before["score"].asLargestUInt();
            int initial_loser_score = (int)loser_before["score"].asLargestUInt();
            std::cout << "  [INFO] 初始分数 - winner: " << initial_winner_score
                      << ", loser: " << initial_loser_score << "\n";

            // 执行分数更新
            bool ok = user_table.update_score_match(winner_id, loser_id, 25, 15);
            TEST_ASSERT(ok == true, "update_score_match 应成功");

            if (ok) {
                // 验证更新结果
                Json::Value winner_after, loser_after;
                user_table.select_by_id(winner_id, winner_after);
                user_table.select_by_id(loser_id, loser_after);

                TEST_ASSERT(winner_after["score"].asLargestUInt() == (unsigned long long)initial_winner_score + 25,
                            "winner 分数应 +25");
                TEST_ASSERT(winner_after["win_count"].asLargestUInt() == 1,
                            "winner win_count 应 +1");
                TEST_ASSERT(winner_after["total_count"].asLargestUInt() == 1,
                            "winner total_count 应 +1");
                TEST_ASSERT(loser_after["score"].asLargestUInt() == (unsigned long long)initial_loser_score - 15,
                            "loser 分数应 -15");
                TEST_ASSERT(loser_after["total_count"].asLargestUInt() == 1,
                            "loser total_count 应 +1");

                std::cout << "  [INFO] 更新后 - winner: score=" << winner_after["score"].asLargestUInt()
                          << ", win_count=" << winner_after["win_count"].asLargestUInt()
                          << ", total_count=" << winner_after["total_count"].asLargestUInt() << "\n";
                std::cout << "  [INFO] 更新后 - loser: score=" << loser_after["score"].asLargestUInt()
                          << ", total_count=" << loser_after["total_count"].asLargestUInt() << "\n";
            }
        }
        std::cout << "\n";
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
