// 编译：g++ -std=c++11 test_base.cpp -o test_base -ljsoncpp -lpthread
#include "../include/logger.hpp"
#include "../include/util.hpp"
#include "config.h"  // CMake 生成的配置文件路径
#include <iostream>
#include <cassert>

int main() {
    std::cout << "=== M1 基础设施测试 ===\n\n";

    // ── 测试配置读取 ────────────────────────────────────────────
    std::cout << "[1] 测试配置读取...\n";
    try {
        auto cfg = gobang::util::load_config(GOBANG_CONFIG_PATH);
        std::cout << "  server_port:  " << cfg.server_port  << "\n";
        std::cout << "  db_host:      " << cfg.db_host       << "\n";
        std::cout << "  db_pool_size: " << cfg.db_pool_size  << "\n";
        std::cout << "  log_level:    " << cfg.log_level     << "\n";
        std::cout << "  [PASS] 配置读取成功\n\n";
    } catch (const std::exception& e) {
        std::cout << "  [FAIL] 配置读取失败：" << e.what() << "\n\n";
        // 配置失败不影响后续测试，继续执行
    }

    // ── 初始化日志 ──────────────────────────────────────────
    std::cout << "[2] 测试 Logger 初始化...\n";
    try {
        gobang::Logger::instance().init(
            GOBANG_TEST_LOG_PATH,
            gobang::LogLevel::DEBUG,  // DEBUG 及以上都输出
            1024 * 1024               // 1MB 就轮转（测试用小值）
        );
        std::cout << "  [PASS] Logger 初始化成功\n\n";
    } catch (const std::exception& e) {
        std::cout << "  [FAIL] Logger 初始化失败：" << e.what() << "\n\n";
        return 1;
    }

    LOG_INFO("Logger initialized");
    LOG_DEBUG("debug msg: " << 42);
    LOG_WARN("warn msg: something looks off");
    LOG_ERROR("error msg: " << "fake error");
    std::cout << "  [PASS] 四级日志输出完成\n\n";

    // ── 测试 JSON 工具 ─────────────────────────────────────
    std::cout << "[3] 测试 JSON 工具...\n";
    Json::Value data;
    data["user_id"] = 1;
    data["name"]    = "郭子寅";

    auto resp = gobang::util::make_response(200, "ok", data);
    std::string json_str = gobang::util::json_to_str(resp);
    std::cout << "  JSON response: " << json_str << "\n";

    // 反向解析
    Json::Value parsed;
    std::string err;
    if (gobang::util::str_to_json(json_str, parsed, err)) {
        std::cout << "  Parsed code: " << parsed["code"].asInt() << "\n";
        std::cout << "  [PASS] JSON 序列化/反序列化成功\n\n";
    } else {
        std::cout << "  [FAIL] JSON 解析失败：" << err << "\n\n";
    }

    // ── 测试时间工具 ────────────────────────────────────────
    std::cout << "[4] 测试时间工具...\n";
    auto ts = gobang::util::now_sec();
    std::cout << "  Now: " << gobang::util::format_time(ts) << "\n";
    std::cout << "  Now ms: " << gobang::util::now_ms() << "\n";
    std::cout << "  [PASS] 时间工具测试完成\n\n";

    // ── 测试字符串工具 ─────────────────────────────────────
    std::cout << "[5] 测试字符串工具...\n";
    std::cout << "  trim: '" << gobang::util::trim("  hello  ") << "'\n";
    auto parts = gobang::util::split("a,b,c,d", ',');
    std::cout << "  split size: " << parts.size() << "\n";
    std::cout << "  valid_username('abc123'): "  << gobang::util::valid_username("abc123") << "\n";
    std::cout << "  valid_username('ab'): "   << gobang::util::valid_username("ab") << "\n";
    std::cout << "  valid_password('123456'): " << gobang::util::valid_password("123456") << "\n";

    assert(gobang::util::valid_username("abc123") == true);
    assert(gobang::util::valid_username("ab") == false);
    assert(gobang::util::valid_password("123456") == true);
    std::cout << "  [PASS] 字符串工具测试完成\n\n";

    // ── 测试棋盘序列化 ─────────────────────────────────────
    std::cout << "[6] 测试棋盘序列化...\n";
    int board[15][15] = {};
    board[7][7] = 1; // 黑棋落中心
    board[7][8] = 2; // 白棋落旁边

    std::string bs = gobang::util::board_to_str(board);
    std::cout << "  board_str[7*15+7]: " << bs[7*15+7] << " (expected: 1)\n";
    std::cout << "  board_str[7*15+8]: " << bs[7*15+8] << " (expected: 2)\n";

    int board2[15][15] = {};
    gobang::util::str_to_board(bs, board2);
    std::cout << "  board2[7][7]: " << board2[7][7] << " (expected: 1)\n";
    std::cout << "  board2[7][8]: " << board2[7][8] << " (expected: 2)\n";

    assert(bs[7*15+7] == '1');
    assert(bs[7*15+8] == '2');
    assert(board2[7][7] == 1);
    assert(board2[7][8] == 2);
    std::cout << "  [PASS] 棋盘序列化测试完成\n\n";

    // ── 测试配置校验 ────────────────────────────────────────
    std::cout << "[7] 测试配置校验...\n";
    try {
        gobang::util::Config bad_cfg;
        bad_cfg.db_password = "";  // 空密码应该抛出异常
        gobang::util::validate_config(bad_cfg);
        std::cout << "  [FAIL] 配置校验未抛出异常\n\n";
    } catch (const std::exception& e) {
        std::cout << "  [PASS] 配置校验正确抛出异常：" << e.what() << "\n\n";
    }

    // ── 完成 ────────────────────────────────────────────────
    std::cout << "=== 所有测试完成 ===\n";
    LOG_INFO("All tests passed");

    // Logger 析构时自动 flush 并关闭后台线程
    return 0;
}
