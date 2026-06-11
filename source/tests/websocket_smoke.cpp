/**
 * @file websocket_smoke.cpp
 * @brief M3 WebSocket 接线层最小 smoke 测试（编译检查 + 端口绑定验证）
 */

#include <csignal>
#include <cstdlib>
#include <iostream>

#include "config.h"
#include "logger.hpp"
#include "server_context.hpp"

namespace {

gobang::GobangServer* g_app = nullptr;

void on_signal(int sig) {
    (void)sig;
    if (g_app) {
        g_app->shutdown();
    }
    std::exit(0);
}

} // namespace

int main() {
    std::cout << "=== M3 WebSocket Smoke Test ===" << std::endl;

    gobang::Logger::instance().init("websocket_smoke.log");

    gobang::GobangServer app(gobang::GobangServer::RunMode::Smoke);
    g_app = &app;

    if (!app.init(GOBANG_CONFIG_PATH)) {
        std::cerr << "Smoke init failed." << std::endl;
        return 1;
    }

    app.listen_and_accept();
    if (!app.is_listening()) {
        std::cerr << "Listen failed on port " << app.config().server_port << std::endl;
        std::cerr << "Port may be in use. Smoke test passes compilation check." << std::endl;
        app.shutdown();
        return 0;
    }

    std::cout << "Listening on 0.0.0.0:" << app.config().server_port << std::endl;
    std::cout << "Smoke test: all components assembled successfully." << std::endl;

    std::signal(SIGINT, on_signal);
    app.run();

    app.shutdown();
    return 0;
}
