#include "websocket_handler.hpp"

#include "security.hpp"
#include "util.hpp"
#include "logger.hpp"

namespace gobang {

void WebSocketHandler::init(ConnectionManager* conn_mgr,
                            OnlineManager* online_mgr,
                            MatcherInterface* matcher,
                            WebsocketServer* server,
                            GameController* game_ctrl) {
    _conn_mgr = conn_mgr;
    _online_mgr = online_mgr;
    _matcher = matcher;
    _server = server;
    _game_ctrl = game_ctrl;

    if (_matcher) {
        _matcher->set_match_callback([this](const MatchResult& result) {
            if (_server) {
                _server->get_io_service().post([this, result]() {
                    this->on_match_success(result);
                });
            } else {
                this->on_match_success(result);
            }
        });
    }

    LOG_INFO("WebSocketHandler: initialized");
}

void WebSocketHandler::on_open(WebsocketConnectionHdl hdl) {
    (void)hdl;
    LOG_DEBUG("WebSocketHandler: connection opened");
}

void WebSocketHandler::execute_user_disconnect(int64_t user_id) {
    LOG_INFO("WebSocketHandler: user disconnected - user_id=" << user_id);

    if (_game_ctrl) {
        _game_ctrl->handle_disconnect(user_id);
    }

    _conn_mgr->remove(user_id);
    _online_mgr->user_offline(user_id);
    _matcher->on_disconnect(user_id);

    if (_game_ctrl) {
        _game_ctrl->cleanup_finished_room(user_id);
    }
}

bool WebSocketHandler::has_pending_disconnect(int64_t user_id) const {
    std::lock_guard<std::mutex> lock(_pending_disconnect_mtx);
    return _pending_disconnects.find(user_id) != _pending_disconnects.end();
}

void WebSocketHandler::commit_pending_game_disconnect(int64_t user_id) {
    if (!has_pending_disconnect(user_id)) {
        return;
    }
    cancel_pending_disconnect(user_id);
    if (_game_ctrl) {
        _game_ctrl->handle_disconnect(user_id);
    }
    LOG_INFO("WebSocketHandler: committed pending game disconnect - user_id=" << user_id);
}

bool WebSocketHandler::schedule_pending_disconnect(int64_t user_id) {
    std::lock_guard<std::mutex> lock(_pending_disconnect_mtx);
    PendingDisconnect entry;
    entry.deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds(_disconnect_grace_seconds);
    _pending_disconnects[user_id] = entry;
    LOG_INFO("WebSocketHandler: scheduled pending disconnect - user_id=" << user_id
             << ", grace=" << _disconnect_grace_seconds << "s");
    return true;
}

bool WebSocketHandler::cancel_pending_disconnect(int64_t user_id) {
    std::lock_guard<std::mutex> lock(_pending_disconnect_mtx);
    auto it = _pending_disconnects.find(user_id);
    if (it == _pending_disconnects.end()) {
        return false;
    }
    _pending_disconnects.erase(it);
    LOG_INFO("WebSocketHandler: cancelled pending disconnect - user_id=" << user_id);
    return true;
}

void WebSocketHandler::flush_pending_disconnects() {
    std::vector<int64_t> expired;
    const auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(_pending_disconnect_mtx);
        for (const auto& pair : _pending_disconnects) {
            if (pair.second.deadline <= now) {
                expired.push_back(pair.first);
            }
        }
        for (int64_t user_id : expired) {
            _pending_disconnects.erase(user_id);
        }
    }

    for (int64_t user_id : expired) {
        if (_conn_mgr->is_connected(user_id)) {
            LOG_INFO("WebSocketHandler: skip expired pending disconnect (reconnected) - user_id="
                     << user_id);
            continue;
        }
        execute_user_disconnect(user_id);
    }
}

void WebSocketHandler::on_close(WebsocketConnectionHdl hdl) {
    int64_t user_id = 0;
    {
        std::lock_guard<std::mutex> lock(_hdl_user_mtx);
        auto conn = hdl.lock();
        if (!conn) return;
        auto it = _hdl_to_user_id.find(conn.get());
        if (it == _hdl_to_user_id.end()) return;
        user_id = it->second;
        _hdl_to_user_id.erase(it);
    }

    if (user_id > 0) {
        WebsocketConnectionHdl current_hdl;
        if (_conn_mgr->get(user_id, current_hdl)) {
            auto current_conn = current_hdl.lock();
            auto closed_conn = hdl.lock();
            if (current_conn && closed_conn && current_conn.get() != closed_conn.get()) {
                return;
            }
        }

        _conn_mgr->remove(user_id);

        const bool in_active_game =
            _game_ctrl && _game_ctrl->has_active_room(user_id);

        if (in_active_game) {
            schedule_pending_disconnect(user_id);
            return;
        }

        execute_user_disconnect(user_id);
    } else {
        LOG_DEBUG("WebSocketHandler: unauthenticated connection closed");
    }
}

void WebSocketHandler::on_message(WebsocketConnectionHdl hdl, const std::string& msg) {
    Json::Value root;
    std::string errmsg;
    if (!util::str_to_json(msg, root, errmsg)) {
        LOG_WARN("WebSocketHandler: invalid JSON - " << errmsg);
        _server->send(hdl, make_error(4000, "invalid JSON"), websocketpp::frame::opcode::text);
        return;
    }

    std::string event = root["event"].asString();
    Json::Value data = root["data"];

    if (event == "auth" || event == "match.start") {
        int64_t existing_user_id = get_user_id_from_hdl(hdl);
        if (existing_user_id > 0 && event == "match.start") {
            std::string resp = handle_match_start(existing_user_id, data);
            _server->send(hdl, resp, websocketpp::frame::opcode::text);
            return;
        }

        int64_t user_id = verify_token_from_data(data);
        if (user_id == 0) {
            LOG_WARN("WebSocketHandler: invalid or missing token");
            _server->send(hdl, make_error(4001, "invalid token"), websocketpp::frame::opcode::text);
            return;
        }

        if (_online_mgr->is_online(user_id)) {
            LOG_INFO("WebSocketHandler: replacing connection for online user - user_id=" << user_id);
        }

        set_user_connection(user_id, hdl);

        if (event == "match.start") {
            std::string resp = handle_match_start(user_id, data);
            _server->send(hdl, resp, websocketpp::frame::opcode::text);
        } else {
            Json::Value resp_data;
            resp_data["user_id"] = (Json::Int64)user_id;
            _server->send(hdl, make_response("auth.success", resp_data), websocketpp::frame::opcode::text);

            if (_game_ctrl) {
                GameRoom* room = _game_ctrl->get_room_by_user(user_id);
                if (room && room->get_status() == RoomStatus::PLAYING) {
                    const bool hall_auth =
                        data.isMember("source") && data["source"].asString() == "hall";
                    if (has_pending_disconnect(user_id) && hall_auth) {
                        commit_pending_game_disconnect(user_id);
                        room = _game_ctrl->get_room_by_user(user_id);
                    }
                    if (room && room->is_disconnected()
                        && room->get_disconnected_user_id() == user_id) {
                        send_reconnect_available(user_id, hdl);
                    }
                } else if (room && room->get_status() == RoomStatus::FINISHED) {
                    GameResult result = room->get_result();
                    std::string result_str;
                    if (result == GameResult::BLACK_WIN) {
                        result_str = "black_win";
                    } else if (result == GameResult::WHITE_WIN) {
                        result_str = "white_win";
                    } else if (result == GameResult::TIMEOUT) {
                        result_str = "timeout";
                    } else if (result == GameResult::GIVEUP) {
                        result_str = "giveup";
                    } else {
                        result_str = "draw";
                    }

                    Json::Value game_over_msg;
                    game_over_msg["event"] = "game.over";
                    game_over_msg["data"]["result"] = result_str;
                    game_over_msg["data"]["reason"] = "timeout";
                    _server->send(hdl, game_over_msg.toStyledString(), websocketpp::frame::opcode::text);
                }
            }
        }
        return;
    }

    int64_t user_id = get_user_id_from_hdl(hdl);
    if (user_id == 0) {
        LOG_WARN("WebSocketHandler: unauthenticated user trying event: " << event);
        _server->send(hdl, make_error(4001, "unauthenticated"), websocketpp::frame::opcode::text);
        return;
    }

    std::string response;
    if (event == "match.cancel") {
        response = handle_match_cancel(user_id, data);
    } else if (event == "ping") {
        response = handle_ping(user_id, data);
    } else if (event == "game.move") {
        handle_game_move(user_id, data);
        return;
    } else if (event == "game.giveup") {
        handle_game_giveup(user_id);
        return;
    } else if (event == "game.reconnect") {
        handle_game_reconnect(user_id, data);
        return;
    } else if (event == "reconnect.accept") {
        handle_reconnect_accept(user_id);
        return;
    } else if (event == "reconnect.reject") {
        handle_reconnect_reject(user_id);
        return;
    } else {
        LOG_WARN("WebSocketHandler: unknown event: " << event);
        response = make_error(4002, "unknown event");
    }

    _server->send(hdl, response, websocketpp::frame::opcode::text);
}

void WebSocketHandler::set_user_connection(int64_t user_id, WebsocketConnectionHdl hdl) {
    {
        std::lock_guard<std::mutex> lock(_hdl_user_mtx);
        for (auto it = _hdl_to_user_id.begin(); it != _hdl_to_user_id.end(); ) {
            if (it->second == user_id) {
                it = _hdl_to_user_id.erase(it);
            } else {
                ++it;
            }
        }
    }

    _conn_mgr->add(user_id, hdl);

    if (!_online_mgr->is_online(user_id)) {
        _online_mgr->user_online(user_id);
    }

    {
        std::lock_guard<std::mutex> lock(_hdl_user_mtx);
        auto conn = hdl.lock();
        if (conn) {
            _hdl_to_user_id[conn.get()] = user_id;
        }
    }

    LOG_INFO("WebSocketHandler: user authenticated and connected - user_id=" << user_id);
}

int64_t WebSocketHandler::get_user_id_from_hdl(WebsocketConnectionHdl hdl) const {
    std::lock_guard<std::mutex> lock(_hdl_user_mtx);
    auto conn = hdl.lock();
    if (!conn) return 0;
    auto it = _hdl_to_user_id.find(conn.get());
    if (it == _hdl_to_user_id.end()) return 0;
    return it->second;
}

std::string WebSocketHandler::handle_match_start(int64_t user_id, const Json::Value& data) {
    OnlineStatus status = _online_mgr->get_status(user_id);
    if (status != OnlineStatus::HALL_IDLE) {
        LOG_WARN("WebSocketHandler: match.start rejected - user not in HALL_IDLE, user_id="
                 << user_id << ", status=" << (int)status);
        return make_error(4003, "not in hall");
    }

    int score = data.isMember("score") ? data["score"].asInt() : 1000;
    bool success = _matcher->enqueue(user_id, score);
    if (!success) {
        LOG_WARN("WebSocketHandler: match.start enqueue failed - user_id=" << user_id);
        return make_error(4004, "enqueue failed");
    }

    LOG_INFO("WebSocketHandler: match.start success - user_id=" << user_id);

    Json::Value resp_data;
    resp_data["queue_position"] = 1;
    resp_data["wait_time"] = 0;
    return make_response("match.waiting", resp_data);
}

std::string WebSocketHandler::handle_match_cancel(int64_t user_id, const Json::Value& data) {
    (void)data;
    bool success = _matcher->cancel(user_id);
    if (!success) {
        LOG_WARN("WebSocketHandler: match.cancel failed - user_id=" << user_id);
        return make_error(4005, "cancel failed");
    }

    LOG_INFO("WebSocketHandler: match.cancel success - user_id=" << user_id);

    Json::Value resp_data;
    resp_data["message"] = "cancelled";
    return make_response("match.cancelled", resp_data);
}

std::string WebSocketHandler::handle_ping(int64_t user_id, const Json::Value& data) {
    (void)user_id;
    (void)data;
    Json::Value resp_data;
    resp_data["timestamp"] = util::now_sec();
    return make_response("pong", resp_data);
}

void WebSocketHandler::handle_game_move(int64_t user_id, const Json::Value& data) {
    LOG_INFO("handle_game_move: user_id=" << user_id
             << ", data=" << data.toStyledString());
    if (!_game_ctrl) {
        send_error_to_user(user_id, 5000, "game not available");
        return;
    }
    if (!data.isMember("row") || !data.isMember("col")) {
        LOG_WARN("handle_game_move: 缺少 row 或 col 字段");
        send_error_to_user(user_id, 4002, "missing row or col");
        return;
    }
    _game_ctrl->handle_move(user_id, data["row"].asInt(), data["col"].asInt());
}

void WebSocketHandler::handle_game_giveup(int64_t user_id) {
    if (!_game_ctrl) {
        send_error_to_user(user_id, 5000, "game not available");
        return;
    }
    _game_ctrl->handle_giveup(user_id);
}

void WebSocketHandler::handle_game_reconnect(int64_t user_id, const Json::Value& data) {
    if (!_game_ctrl) {
        send_error_to_user(user_id, 5000, "game not available");
        return;
    }
    if (data.isMember("token")) {
        int64_t verified_id = verify_token_from_data(data);
        if (verified_id == 0 || verified_id != user_id) {
            send_error_to_user(user_id, 4001, "invalid token");
            return;
        }
    }
    cancel_pending_disconnect(user_id);
    _game_ctrl->handle_reconnect(user_id);
}

void WebSocketHandler::process_timers() {
    flush_pending_disconnects();
}

void WebSocketHandler::send_error_to_user(int64_t user_id, int code,
                                          const std::string& message) {
    if (_conn_mgr) {
        _conn_mgr->send(user_id, make_error(code, message));
    }
}

void WebSocketHandler::on_match_success(const MatchResult& result) {
    LOG_INFO("WebSocketHandler: match success - room_id=" << result.room_id
             << ", player1=" << result.player1_id
             << ", player2=" << result.player2_id);

    std::string game_room_id;
    if (_game_ctrl) {
        game_room_id = _game_ctrl->handle_game_start(result.player1_id, result.player2_id);
    }

    Json::Value player1_data;
    if (!game_room_id.empty()) {
        player1_data["room_id"] = game_room_id;
    } else {
        player1_data["room_id"] = (Json::Int64)result.room_id;
    }
    player1_data["color"] = (result.player1_color == 1) ? "black" : "white";

    Json::Value player1_opponent;
    player1_opponent["user_id"] = (Json::Int64)result.player2_id;
    player1_opponent["username"] = "opponent";
    player1_data["opponent"] = player1_opponent;

    std::string msg1 = make_response("match.success", player1_data);
    _conn_mgr->send(result.player1_id, msg1);

    Json::Value player2_data;
    if (!game_room_id.empty()) {
        player2_data["room_id"] = game_room_id;
    } else {
        player2_data["room_id"] = (Json::Int64)result.room_id;
    }
    player2_data["color"] = (result.player2_color == 1) ? "black" : "white";

    Json::Value player2_opponent;
    player2_opponent["user_id"] = (Json::Int64)result.player1_id;
    player2_opponent["username"] = "opponent";
    player2_data["opponent"] = player2_opponent;

    std::string msg2 = make_response("match.success", player2_data);
    _conn_mgr->send(result.player2_id, msg2);
}

std::string WebSocketHandler::make_response(const std::string& event, const Json::Value& data) {
    Json::Value resp;
    resp["event"] = event;
    resp["data"] = data;
    return util::json_to_str(resp);
}

std::string WebSocketHandler::make_error(int code, const std::string& message) {
    Json::Value resp;
    resp["event"] = "error";
    resp["data"]["code"] = code;
    resp["data"]["message"] = message;
    return util::json_to_str(resp);
}

int64_t WebSocketHandler::verify_token_from_data(const Json::Value& data) {
    if (!data.isMember("token")) {
        return 0;
    }
    std::string token = data["token"].asString();
    return security::jwt_verify(token);
}

void WebSocketHandler::handle_reconnect_accept(int64_t user_id) {
    if (!_game_ctrl) {
        send_error_to_user(user_id, 5000, "game not available");
        return;
    }
    _game_ctrl->handle_reconnect_accept(user_id);
}

void WebSocketHandler::handle_reconnect_reject(int64_t user_id) {
    if (!_game_ctrl) {
        send_error_to_user(user_id, 5000, "game not available");
        return;
    }
    _game_ctrl->handle_reconnect_reject(user_id);
}

void WebSocketHandler::send_reconnect_available(int64_t user_id, WebsocketConnectionHdl hdl) {
    if (!_game_ctrl) return;

    GameRoom* room = _game_ctrl->get_room_by_user(user_id);
    if (!room) return;

    Json::Value msg;
    msg["event"] = "reconnect.available";
    msg["data"]["room_id"] = room->get_room_id();
    msg["data"]["board"] = room->get_board_state();

    int64_t opponent_id = room->get_opponent_id(user_id);
    Json::Value opponent;
    opponent["user_id"] = opponent_id;
    msg["data"]["opponent"] = opponent;

    PieceColor color;
    room->get_player_color(user_id, color);
    msg["data"]["color"] = (color == PieceColor::BLACK) ? "black" : "white";

    msg["data"]["current_turn"] = room->get_current_turn_color_str();

    int elapsed = room->get_disconnect_elapsed();
    int remaining = 60 - elapsed;
    if (remaining < 1) remaining = 1;
    msg["data"]["timeout_remaining"] = remaining;

    _server->send(hdl, msg.toStyledString(), websocketpp::frame::opcode::text);

    LOG_INFO("WebSocketHandler: sent reconnect.available to user_id=" << user_id
             << ", room=" << room->get_room_id() << ", remaining=" << remaining << "s");
}

} // namespace gobang
