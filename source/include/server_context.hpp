#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "connection_manager.hpp"
#include "db.hpp"
#include "game.hpp"
#include "http_router.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "room.hpp"
#include "user_table.hpp"
#include "util.hpp"
#include "websocket_handler.hpp"

namespace gobang {

class GobangServer {
public:
    enum class RunMode {
        Production,
        Smoke
    };

    explicit GobangServer(RunMode mode = RunMode::Production);

    bool init(const char* config_path);
    bool listen_and_accept();
    bool is_listening() const { return listening_; }
    void run();
    void shutdown();

    WebsocketServer& server() { return server_; }
    WebSocketHandler& ws_handler() { return ws_handler_; }
    ConnectionManager& conn_mgr() { return conn_mgr_; }
    OnlineManager& online_mgr() { return online_mgr_; }
    Matcher& matcher() { return matcher_; }
    RoomManager& room_mgr() { return room_mgr_; }
    GameController* game_ctrl() { return game_ctrl_.get(); }
    const util::Config& config() const { return config_; }

private:
    void wire_handlers();
    void start_disconnect_grace_driver();
    static std::string trim_ws_resource(const std::string& resource);
    static bool is_allowed_websocket_resource(const std::string& resource);

    RunMode mode_;
    bool listening_ = false;
    util::Config config_;

    WebsocketServer server_;
    ConnectionManager conn_mgr_;
    OnlineManager online_mgr_;
    Matcher matcher_;
    WebSocketHandler ws_handler_;
    DBPool db_pool_;
    UserTable user_table_;
    RoomManager room_mgr_;
    HttpRouter http_router_;
    std::shared_ptr<GameController> game_ctrl_;
};

} // namespace gobang
