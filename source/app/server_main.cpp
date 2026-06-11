/**
 * @file server_main.cpp
 * @brief Online Gobang Battle 生产入口
 */

#include <iostream>
#include <memory>
#include <sstream>
#include <signal.h>

#include "config.h"
#include "connection_manager.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "websocket_handler.hpp"
#include "logger.hpp"
#include "db.hpp"
#include "user_table.hpp"
#include "util.hpp"
#include "game.hpp"
#include "room.hpp"
#include "http_router.hpp"

using websocketpp::connection_hdl;

namespace {

gobang::WebsocketServer g_server;
gobang::ConnectionManager g_conn_mgr;
gobang::OnlineManager g_online_mgr;
gobang::Matcher g_matcher;
gobang::WebSocketHandler g_ws_handler;
gobang::DBPool g_db_pool;
gobang::UserTable g_user_table;
gobang::RoomManager g_room_mgr;
gobang::HttpRouter g_http_router;
std::shared_ptr<gobang::GameController> g_game_ctrl;

std::string trim_query_and_fragment(const std::string& resource) {
    size_t end = resource.find_first_of("?#");
    return resource.substr(0, end);
}

bool is_allowed_websocket_resource(const std::string& resource) {
    std::string path = trim_query_and_fragment(resource);
    return path == "/ws" || path == "/ws/";
}

void on_signal(int sig) {
    (void)sig;
    std::cout << "\nShutting down..." << std::endl;
    g_matcher.stop();
    g_server.stop();
    exit(0);
}

void start_disconnect_grace_driver(gobang::WebsocketServer& server,
                                   gobang::WebSocketHandler& handler) {
    typedef websocketpp::lib::asio::steady_timer timer;
    struct TimerDriver : std::enable_shared_from_this<TimerDriver> {
        gobang::WebSocketHandler* handler = nullptr;
        std::shared_ptr<timer> timer_ptr;

        void schedule() {
            std::shared_ptr<TimerDriver> self = shared_from_this();
            timer_ptr->expires_from_now(websocketpp::lib::asio::milliseconds(500));
            timer_ptr->async_wait([self](const websocketpp::lib::error_code& ec) {
                if (ec) {
                    return;
                }
                self->handler->process_timers();
                self->schedule();
            });
        }
    };

    std::shared_ptr<TimerDriver> driver = std::make_shared<TimerDriver>();
    driver->handler = &handler;
    driver->timer_ptr = std::make_shared<timer>(server.get_io_service());
    driver->schedule();
}

} // namespace

int main() {
    std::cout << "=== Online Gobang Battle Server ===" << std::endl;

    gobang::Logger::instance().init("gobang_server.log");

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

    g_conn_mgr.init(&g_server);
    g_matcher.init(&g_online_mgr);
    g_matcher.start();
    g_http_router.init(&g_user_table, &g_online_mgr);

    g_game_ctrl = gobang::GameController::create();
    g_game_ctrl->init(&g_room_mgr, &g_online_mgr, &g_conn_mgr, &g_user_table,
                      &g_server.get_io_service());

    g_ws_handler.init(&g_conn_mgr, &g_online_mgr, &g_matcher, &g_server,
                      g_game_ctrl.get());

    g_server.init_asio();
    g_server.set_reuse_addr(true);
    g_server.set_access_channels(websocketpp::log::alevel::all);
    g_server.set_error_channels(websocketpp::log::elevel::all);

    g_server.set_validate_handler([](connection_hdl hdl) {
        auto con = g_server.get_con_from_hdl(hdl);
        return is_allowed_websocket_resource(con->get_resource());
    });

    g_server.set_http_handler([](connection_hdl hdl) {
        g_http_router.handle_request(g_server, hdl);
    });

    g_server.set_open_handler([&](connection_hdl hdl) {
        g_ws_handler.on_open(hdl);
    });

    g_server.set_close_handler([&](connection_hdl hdl) {
        g_ws_handler.on_close(hdl);
    });

    g_server.set_message_handler(
        [&](connection_hdl hdl, gobang::WebsocketServer::message_ptr msg) {
            g_ws_handler.on_message(hdl, msg->get_payload());
        });

    try {
        websocketpp::lib::asio::ip::tcp::endpoint endpoint(
            websocketpp::lib::asio::ip::address_v4::any(), 8080);
        g_server.listen(endpoint);
        std::cout << "Listening on 0.0.0.0:8080 (IPv4)..." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Listen failed: " << e.what() << std::endl;
        g_matcher.stop();
        return 1;
    }

    signal(SIGINT, on_signal);
    g_server.start_accept();

    std::cout << "Server started. Press Ctrl+C to stop." << std::endl;

    start_disconnect_grace_driver(g_server, g_ws_handler);
    g_server.run();

    g_matcher.stop();
    return 0;
}
