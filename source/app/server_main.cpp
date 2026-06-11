/**
 * @file server_main.cpp
 * @brief Online Gobang Battle 生产入口
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
    std::cout << "\nShutting down..." << std::endl;
    if (g_app) {
        g_app->shutdown();
    }
    std::exit(0);
}

} // namespace

int main() {
    std::cout << "=== Online Gobang Battle Server ===" << std::endl;

    gobang::Logger::instance().init("gobang_server.log");

    gobang::GobangServer app(gobang::GobangServer::RunMode::Production);
    g_app = &app;

    if (!app.init(GOBANG_CONFIG_PATH)) {
        std::cerr << "Server init failed." << std::endl;
        return 1;
    }

    if (!app.listen_and_accept()) {
        std::cerr << "Listen failed on port " << app.config().server_port << std::endl;
        return 1;
    }

    std::cout << "Listening on 0.0.0.0:" << app.config().server_port
              << " (IPv4)..." << std::endl;
    std::cout << "Health check: GET /health" << std::endl;
    std::cout << "Server started. Press Ctrl+C to stop." << std::endl;

    std::signal(SIGINT, on_signal);
    app.run();

    app.shutdown();
    return 0;
}
