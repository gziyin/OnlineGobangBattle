/**
 * @file websocket_smoke.cpp
 * @brief M3 WebSocket 接线层最小 smoke 测试
 *
 * 目标：验证 websocket_handler.hpp + matcher.hpp + connection_manager.hpp
 *       第一次进入真实编译链，确保接线闭环可编译。
 */

#include <iostream>
#include <signal.h>

#include "connection_manager.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "websocket_handler.hpp"
#include "logger.hpp"

using websocketpp::connection_hdl;

// 全局实例
gobang::WebsocketServer g_server;
gobang::ConnectionManager g_conn_mgr;
gobang::OnlineManager g_online_mgr;
gobang::Matcher g_matcher;
gobang::WebSocketHandler g_ws_handler;

// 信号处理
void on_signal(int sig) {
    (void)sig;
    std::cout << "\nShutting down..." << std::endl;
    g_matcher.stop();
    g_server.stop();
    exit(0);
}

int main() {
    std::cout << "=== M3 WebSocket Smoke Test ===" << std::endl;

    // 初始化日志
    gobang::Logger::instance().init("websocket_smoke.log");

    // 初始化各组件
    g_conn_mgr.init(&g_server);
    g_matcher.init(&g_online_mgr);
    g_matcher.start();

    // WebSocketHandler 初始化（注入 server）
    g_ws_handler.init(&g_conn_mgr, &g_online_mgr, &g_matcher, &g_server);

    // 配置 server
    g_server.init_asio();
    g_server.set_reuse_addr(true);

    // 设置验证处理器 - 允许 /ws 路径
    g_server.set_validate_handler([](connection_hdl hdl) {
        auto con = g_server.get_con_from_hdl(hdl);
        std::string uri = con->get_resource();
        std::cout << "[validate] URI: " << uri << std::endl;
        return uri == "/ws";
    });

    // 设置 HTTP 处理器 - 非 WebSocket 请求返回信息
    g_server.set_http_handler([](connection_hdl hdl) {
        auto con = g_server.get_con_from_hdl(hdl);
        std::cout << "[http] Non-WebSocket request to: " << con->get_resource() << std::endl;
        con->set_body("WebSocket server is running. Use /ws endpoint.");
        con->set_status(websocketpp::http::status_code::ok);
    });

    // 设置失败日志
    g_server.set_fail_handler([](connection_hdl hdl) {
        auto con = g_server.get_con_from_hdl(hdl);
        std::cout << "[fail] Connection failed: " << con->get_ec().message() << std::endl;
    });

    // 设置打开日志
    g_server.set_open_handler([&](connection_hdl hdl) {
        std::cout << "[open] WebSocket connection opened" << std::endl;
        g_ws_handler.on_open(hdl);
    });

    // 设置关闭日志
    g_server.set_close_handler([&](connection_hdl hdl) {
        std::cout << "[close] WebSocket connection closed" << std::endl;
        g_ws_handler.on_close(hdl);
    });

    // 设置消息处理器
    g_server.set_message_handler([&](connection_hdl hdl, gobang::WebsocketServer::message_ptr msg) {
        g_ws_handler.on_message(hdl, msg->get_payload());
    });

    // 监听端口
    try {
        g_server.listen(8080);
        std::cout << "Listening on port 8080..." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Listen failed: " << e.what() << std::endl;
        std::cerr << "Port 8080 may be in use. Smoke test passes compilation check." << std::endl;
        g_matcher.stop();
        return 0;  // 编译验证通过即可
    }

    // 注册信号处理
    signal(SIGINT, on_signal);

    // 启动接收连接
    g_server.start_accept();

    std::cout << "WebSocket server started. Press Ctrl+C to stop." << std::endl;
    std::cout << "Smoke test: all components assembled successfully." << std::endl;

    // 运行事件循环
    g_server.run();

    g_matcher.stop();
    return 0;
}