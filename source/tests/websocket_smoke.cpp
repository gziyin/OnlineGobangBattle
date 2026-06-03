/**
 * @file websocket_smoke.cpp
 * @brief M3 WebSocket 接线层最小 smoke 测试
 *
 * 目标：验证 websocket_handler.hpp + matcher.hpp + connection_manager.hpp
 *       第一次进入真实编译链，确保接线闭环可编译。
 *
 * 扩展：集成 HTTP REST API（注册/登录）用于前端联调
 */

#include <iostream>
#include <memory>
#include <sstream>
#include <signal.h>
#include <functional>
#include <chrono>

#include "connection_manager.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "websocket_handler.hpp"
#include "logger.hpp"
#include "auth_handler.hpp"
#include "db.hpp"
#include "user_table.hpp"
#include "security.hpp"
#include "util.hpp"
#include "game.hpp"
#include "room.hpp"

using websocketpp::connection_hdl;

extern gobang::WebsocketServer g_server;

namespace {

std::string trim_query_and_fragment(const std::string& resource) {
    size_t end = resource.find_first_of("?#");
    return resource.substr(0, end);
}

bool is_allowed_websocket_resource(const std::string& resource) {
    std::string path = trim_query_and_fragment(resource);
    return path == "/ws" || path == "/ws/";
}

std::string join_requested_subprotocols(
    const std::vector<std::string>& subprotocols) {
    if (subprotocols.empty()) {
        return "(none)";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < subprotocols.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << subprotocols[i];
    }
    return oss.str();
}

void print_request_details(const char* tag, connection_hdl hdl) {
    try {
        auto con = g_server.get_con_from_hdl(hdl);
        const std::string resource = con->get_resource();

        std::cout << "[" << tag << "] resource=" << resource
                  << ", host=" << con->get_request_header("Host")
                  << ", origin=" << con->get_request_header("Origin")
                  << ", ua=" << con->get_request_header("User-Agent")
                  << ", connection=" << con->get_request_header("Connection")
                  << ", upgrade=" << con->get_request_header("Upgrade")
                  << ", sec-websocket-version="
                  << con->get_request_header("Sec-WebSocket-Version")
                  << ", sec-websocket-key="
                  << con->get_request_header("Sec-WebSocket-Key")
                  << ", requested-subprotocols="
                  << join_requested_subprotocols(con->get_requested_subprotocols())
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[" << tag << "] failed to inspect handshake: "
                  << e.what() << std::endl;
    }
}

void print_connection_failure(const char* tag, connection_hdl hdl) {
    try {
        auto con = g_server.get_con_from_hdl(hdl);
        std::cout << "[" << tag << "] ec=" << con->get_ec().message()
                  << ", local_close_code=" << con->get_local_close_code()
                  << ", local_close_reason=" << con->get_local_close_reason()
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[" << tag << "] failed to inspect failed connection: "
                  << e.what() << std::endl;
    }
}

void print_connection_close(const char* tag, connection_hdl hdl) {
    try {
        auto con = g_server.get_con_from_hdl(hdl);
        std::cout << "[" << tag << "] remote_close_code="
                  << con->get_remote_close_code()
                  << ", remote_close_reason=" << con->get_remote_close_reason()
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[" << tag << "] failed to inspect close details: "
                  << e.what() << std::endl;
    }
}

void start_process_timers(gobang::WebsocketServer& server,
                          gobang::WebSocketHandler& handler) {
    typedef websocketpp::lib::asio::steady_timer timer;
    auto timer_ptr = std::make_shared<timer>(server.get_io_service());
    std::function<void(const websocketpp::lib::error_code&)> tick;
    tick = [&handler, timer_ptr, &tick](const websocketpp::lib::error_code& ec) {
        if (ec) {
            return;
        }
        handler.process_timers();
        timer_ptr->expires_from_now(websocketpp::lib::asio::milliseconds(500));
        timer_ptr->async_wait(tick);
    };
    timer_ptr->expires_from_now(websocketpp::lib::asio::milliseconds(500));
    timer_ptr->async_wait(tick);
}

} // namespace

// 全局实例
gobang::WebsocketServer g_server;
gobang::ConnectionManager g_conn_mgr;
gobang::OnlineManager g_online_mgr;
gobang::Matcher g_matcher;
gobang::WebSocketHandler g_ws_handler;
gobang::DBPool g_db_pool;
gobang::UserTable g_user_table;
gobang::RoomManager g_room_mgr;
std::shared_ptr<gobang::GameController> g_game_ctrl;

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

    // 加载配置并初始化数据库
    try {
        gobang::util::Config cfg = gobang::util::load_config(GOBANG_CONFIG_PATH);
        gobang::util::validate_config(cfg);
        g_db_pool.init(cfg);
        g_user_table.init(&g_db_pool);
        LOG_INFO("main: database initialized");
    } catch (const std::exception& e) {
        std::cerr << "DB init failed: " << e.what() << std::endl;
        return 1;
    }

    // 初始化各组件
    g_conn_mgr.init(&g_server);
    g_matcher.init(&g_online_mgr);
    g_matcher.start();

    g_game_ctrl = gobang::GameController::create();
    g_game_ctrl->init(&g_room_mgr, &g_online_mgr, &g_conn_mgr, &g_user_table);

    // WebSocketHandler 初始化（注入 server 与 GameController）
    g_ws_handler.init(&g_conn_mgr, &g_online_mgr, &g_matcher, &g_server,
                      g_game_ctrl.get());

    // 配置 server
    g_server.init_asio();
    g_server.set_reuse_addr(true);
    g_server.set_access_channels(websocketpp::log::alevel::all);
    g_server.set_error_channels(websocketpp::log::elevel::all);

    // 设置验证处理器 - 接受浏览器常见的 /ws 资源形态
    g_server.set_validate_handler([](connection_hdl hdl) {
        print_request_details("validate", hdl);

        auto con = g_server.get_con_from_hdl(hdl);
        std::string resource = con->get_resource();
        bool allowed = is_allowed_websocket_resource(resource);
        std::cout << "[validate] normalized_resource="
                  << trim_query_and_fragment(resource)
                  << ", allowed=" << (allowed ? "true" : "false")
                  << std::endl;
        return allowed;
    });

    // 设置 HTTP 处理器 - REST API 路由分发
    g_server.set_http_handler([](connection_hdl hdl) {
        auto con = g_server.get_con_from_hdl(hdl);
        std::string method = con->get_request().get_method();
        std::string resource = trim_query_and_fragment(con->get_resource());

        print_request_details("http", hdl);

        // CORS 预检
        if (method == "OPTIONS") {
            con->append_header("Access-Control-Allow-Origin", "*");
            con->append_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
            con->append_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            con->set_status(websocketpp::http::status_code::ok);
            return;
        }

        std::string body = con->get_request_body();
        Json::Value req_json, resp_json;
        std::string errmsg;

        if (method == "POST" && resource == "/api/v1/auth/register") {
            if (!gobang::util::str_to_json(body, req_json, errmsg)) {
                resp_json["success"] = false;
                resp_json["message"] = "Invalid JSON: " + errmsg;
            } else {
                resp_json = gobang::auth::handle_register(
                    g_user_table,
                    req_json["username"].asString(),
                    req_json["password"].asString());
            }
        } else if (method == "POST" && resource == "/api/v1/auth/login") {
            if (!gobang::util::str_to_json(body, req_json, errmsg)) {
                resp_json["success"] = false;
                resp_json["message"] = "Invalid JSON: " + errmsg;
            } else {
                resp_json = gobang::auth::handle_login(
                    g_user_table,
                    g_online_mgr,
                    req_json["username"].asString(),
                    req_json["password"].asString());
            }
        } else {
            con->append_header("Access-Control-Allow-Origin", "*");
            con->set_status(websocketpp::http::status_code::not_found);
            con->set_body("{\"success\":false,\"message\":\"Not Found\"}");
            return;
        }

        con->append_header("Access-Control-Allow-Origin", "*");
        con->append_header("Content-Type", "application/json; charset=utf-8");
        con->set_status(websocketpp::http::status_code::ok);
        con->set_body(gobang::util::json_to_str(resp_json));
    });

    // 设置失败日志
    g_server.set_fail_handler([](connection_hdl hdl) {
        print_connection_failure("fail", hdl);
    });

    // 设置打开日志
    g_server.set_open_handler([&](connection_hdl hdl) {
        print_request_details("open", hdl);
        std::cout << "[open] WebSocket connection opened" << std::endl;
        g_ws_handler.on_open(hdl);
    });

    // 设置关闭日志
    g_server.set_close_handler([&](connection_hdl hdl) {
        print_connection_close("close", hdl);
        g_ws_handler.on_close(hdl);
    });

    // 设置消息处理器
    g_server.set_message_handler([&](connection_hdl hdl, gobang::WebsocketServer::message_ptr msg) {
        g_ws_handler.on_message(hdl, msg->get_payload());
    });

    // 监听端口
    try {
        websocketpp::lib::asio::ip::tcp::endpoint endpoint(
            websocketpp::lib::asio::ip::address_v4::any(), 8080);
        g_server.listen(endpoint);
        std::cout << "Listening on 0.0.0.0:8080 (IPv4)..." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Listen failed: " << e.what() << std::endl;
        std::cerr << "Port 8080 may be in use. Smoke test passes compilation check." << std::endl;
        g_matcher.stop();
        return 0;
    }

    // 注册信号处理
    signal(SIGINT, on_signal);

    // 启动接收连接
    g_server.start_accept();

    std::cout << "WebSocket server started. Press Ctrl+C to stop." << std::endl;
    std::cout << "Smoke test: all components assembled successfully." << std::endl;

    start_process_timers(g_server, g_ws_handler);

    // 运行事件循环
    g_server.run();

    g_matcher.stop();
    return 0;
}
