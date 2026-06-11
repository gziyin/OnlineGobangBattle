#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <json/json.h>

#include "connection_manager.hpp"
#include "matcher_interface.hpp"
#include "online.hpp"
#include "security.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "game.hpp"

namespace gobang {

/**
 * @brief WebSocket 事件处理器
 *
 * 负责：
 * - token 校验
 * - 消息分发（match.start / match.cancel / game.* / ping）
 * - 调用 matcher.enqueue/cancel
 * - 调用 GameController 处理游戏事件
 * - 接收 MatchCallback 后向双方发送 match.success 并启动游戏
 * - 断线处理：game_ctrl.handle_disconnect -> conn_mgr.remove -> online_mgr.user_offline -> matcher.on_disconnect
 */
class WebSocketHandler {
public:
    /**
     * @brief 初始化，注入各管理器、游戏控制器和 WebSocket 服务器
     */
    void init(ConnectionManager* conn_mgr,
              OnlineManager* online_mgr,
              MatcherInterface* matcher,
              WebsocketServer* server,
              GameController* game_ctrl);

    /**
     * @brief 处理 WebSocket 连接建立
     * @param hdl WebSocket 连接句柄
     *
     * 此时还未进行身份验证，用户处于未认证状态
     */
    void on_open(WebsocketConnectionHdl hdl);

    /**
     * @brief 处理 WebSocket 连接断开
     * @param hdl WebSocket 连接句柄
     *
     * 执行统一断线流程：conn_mgr.remove -> online_mgr.user_offline -> matcher.on_disconnect
     */
    void on_close(WebsocketConnectionHdl hdl);

    /**
     * @brief 处理 WebSocket 消息
     * @param hdl WebSocket 连接句柄
     * @param msg 消息内容
     *
     * 解析 JSON，校验 token，分发到具体事件处理函数
     */
    void on_message(WebsocketConnectionHdl hdl, const std::string& msg);

    /**
     * @brief 设置用户连接（认证成功后调用）
     * @param user_id 用户 ID
     * @param hdl WebSocket 连接句柄
     *
     * 认证成功后，将 user_id 与连接绑定，并调用 online_mgr.user_online()
     */
    void set_user_connection(int64_t user_id, WebsocketConnectionHdl hdl);

    /**
     * @brief 从连接句柄获取 user_id
     * @param hdl WebSocket 连接句柄
     * @return user_id，未认证返回 0
     */
    int64_t get_user_id_from_hdl(WebsocketConnectionHdl hdl) const;

    /**
     * @brief 处理断线 grace 延迟队列（可在主循环中周期性调用）
     */
    void process_timers();

    /** @brief 设置页面跳转 grace 延迟（秒），主要用于测试 */
    void set_disconnect_grace_seconds(int seconds) {
        _disconnect_grace_seconds = seconds > 0 ? seconds : 1;
    }

private:
    // 事件处理函数
    std::string handle_match_start(int64_t user_id, const Json::Value& data);
    std::string handle_match_cancel(int64_t user_id, const Json::Value& data);
    std::string handle_ping(int64_t user_id, const Json::Value& data);
    void handle_game_move(int64_t user_id, const Json::Value& data);
    void handle_game_giveup(int64_t user_id);
    void handle_game_reconnect(int64_t user_id, const Json::Value& data);
    void handle_reconnect_accept(int64_t user_id);
    void handle_reconnect_reject(int64_t user_id);

    // 发送重连通知给大厅页面（断线后重新登录场景）
    void send_reconnect_available(int64_t user_id, WebsocketConnectionHdl hdl);

    // 工具函数
    std::string make_response(const std::string& event, const Json::Value& data);
    std::string make_error(int code, const std::string& message);
    int64_t verify_token_from_data(const Json::Value& data);

    // 匹配成功回调处理
    void on_match_success(const MatchResult& result);

    void send_error_to_user(int64_t user_id, int code, const std::string& message);

    void execute_user_disconnect(int64_t user_id);
    bool has_pending_disconnect(int64_t user_id) const;
    bool schedule_pending_disconnect(int64_t user_id);
    bool cancel_pending_disconnect(int64_t user_id);
    void commit_pending_game_disconnect(int64_t user_id);
    void flush_pending_disconnects();

    // 管理器引用
    ConnectionManager* _conn_mgr = nullptr;
    OnlineManager* _online_mgr = nullptr;
    MatcherInterface* _matcher = nullptr;
    GameController* _game_ctrl = nullptr;

    // 连接到 user_id 的临时映射（认证前）
    mutable std::mutex _hdl_user_mtx;
    std::unordered_map<void*, int64_t> _hdl_to_user_id;

    // 进行中游戏断线 grace：页面跳转时旧连接先关闭，新连接稍后 auth，可取消延迟断线
    mutable std::mutex _pending_disconnect_mtx;
    struct PendingDisconnect {
        std::chrono::steady_clock::time_point deadline;
    };
    std::unordered_map<int64_t, PendingDisconnect> _pending_disconnects;
    int _disconnect_grace_seconds = 3;

    // WebsocketServer 引用（用于获取连接指针）
    WebsocketServer* _server = nullptr;
};

// ==================== 实现 ====================

inline void WebSocketHandler::init(ConnectionManager* conn_mgr,
                                    OnlineManager* online_mgr,
                                    MatcherInterface* matcher,
                                    WebsocketServer* server,
                                    GameController* game_ctrl) {
    _conn_mgr = conn_mgr;
    _online_mgr = online_mgr;
    _matcher = matcher;
    _server = server;
    _game_ctrl = game_ctrl;

    // 设置匹配成功回调（投递到 IO 线程，避免跨线程 WebSocket 操作竞态）
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

inline void WebSocketHandler::on_open(WebsocketConnectionHdl hdl) {
    (void)hdl;
    LOG_DEBUG("WebSocketHandler: connection opened");
    // 连接建立时暂不做任何操作，等待认证事件
}

inline void WebSocketHandler::execute_user_disconnect(int64_t user_id) {
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

inline bool WebSocketHandler::has_pending_disconnect(int64_t user_id) const {
    std::lock_guard<std::mutex> lock(_pending_disconnect_mtx);
    return _pending_disconnects.find(user_id) != _pending_disconnects.end();
}

inline void WebSocketHandler::commit_pending_game_disconnect(int64_t user_id) {
    if (!has_pending_disconnect(user_id)) {
        return;
    }
    cancel_pending_disconnect(user_id);
    if (_game_ctrl) {
        _game_ctrl->handle_disconnect(user_id);
    }
    LOG_INFO("WebSocketHandler: committed pending game disconnect - user_id=" << user_id);
}

inline bool WebSocketHandler::schedule_pending_disconnect(int64_t user_id) {
    std::lock_guard<std::mutex> lock(_pending_disconnect_mtx);
    PendingDisconnect entry;
    entry.deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds(_disconnect_grace_seconds);
    _pending_disconnects[user_id] = entry;
    LOG_INFO("WebSocketHandler: scheduled pending disconnect - user_id=" << user_id
             << ", grace=" << _disconnect_grace_seconds << "s");
    return true;
}

inline bool WebSocketHandler::cancel_pending_disconnect(int64_t user_id) {
    std::lock_guard<std::mutex> lock(_pending_disconnect_mtx);
    auto it = _pending_disconnects.find(user_id);
    if (it == _pending_disconnects.end()) {
        return false;
    }
    _pending_disconnects.erase(it);
    LOG_INFO("WebSocketHandler: cancelled pending disconnect - user_id=" << user_id);
    return true;
}

inline void WebSocketHandler::flush_pending_disconnects() {
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

inline void WebSocketHandler::on_close(WebsocketConnectionHdl hdl) {
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

inline void WebSocketHandler::on_message(WebsocketConnectionHdl hdl, const std::string& msg) {
    // 解析 JSON
    Json::Value root;
    std::string errmsg;
    if (!util::str_to_json(msg, root, errmsg)) {
        LOG_WARN("WebSocketHandler: invalid JSON - " << errmsg);
        _server->send(hdl, make_error(4000, "invalid JSON"), websocketpp::frame::opcode::text);
        return;
    }

    // 提取事件类型
    std::string event = root["event"].asString();
    Json::Value data = root["data"];

    // 特殊处理：认证事件（不需要预先绑定 user_id）
    if (event == "auth" || event == "match.start") {
        // 先检查当前连接是否已认证（已认证的 match.start 直接处理）
        int64_t existing_user_id = get_user_id_from_hdl(hdl);
        if (existing_user_id > 0 && event == "match.start") {
            // 连接已认证，直接处理匹配逻辑
            std::string resp = handle_match_start(existing_user_id, data);
            _server->send(hdl, resp, websocketpp::frame::opcode::text);
            return;
        }

        // 未认证的连接，走认证流程
        int64_t user_id = verify_token_from_data(data);
        if (user_id == 0) {
            LOG_WARN("WebSocketHandler: invalid or missing token");
            _server->send(hdl, make_error(4001, "invalid token"), websocketpp::frame::opcode::text);
            return;
        }

        // 已在线：同用户替换连接（页面跳转 grace 期间新连接接入）
        if (_online_mgr->is_online(user_id)) {
            LOG_INFO("WebSocketHandler: replacing connection for online user - user_id=" << user_id);
        }

        set_user_connection(user_id, hdl);

        // 如果是 match.start，继续处理匹配逻辑
        if (event == "match.start") {
            std::string resp = handle_match_start(user_id, data);
            _server->send(hdl, resp, websocketpp::frame::opcode::text);
        } else {
            // 纯认证事件，返回成功
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
                    // 游戏已结束，发送结果给重连用户
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

    // 其他事件：需要已认证
    int64_t user_id = get_user_id_from_hdl(hdl);
    if (user_id == 0) {
        LOG_WARN("WebSocketHandler: unauthenticated user trying event: " << event);
        _server->send(hdl, make_error(4001, "unauthenticated"), websocketpp::frame::opcode::text);
        return;
    }

    // 分发事件
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

inline void WebSocketHandler::set_user_connection(int64_t user_id, WebsocketConnectionHdl hdl) {
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

inline int64_t WebSocketHandler::get_user_id_from_hdl(WebsocketConnectionHdl hdl) const {
    std::lock_guard<std::mutex> lock(_hdl_user_mtx);
    auto conn = hdl.lock();
    if (!conn) return 0;
    auto it = _hdl_to_user_id.find(conn.get());
    if (it == _hdl_to_user_id.end()) return 0;
    return it->second;
}

inline std::string WebSocketHandler::handle_match_start(int64_t user_id, const Json::Value& data) {
    // 检查用户状态
    OnlineStatus status = _online_mgr->get_status(user_id);
    if (status != OnlineStatus::HALL_IDLE) {
        LOG_WARN("WebSocketHandler: match.start rejected - user not in HALL_IDLE, user_id=" << user_id << ", status=" << (int)status);
        return make_error(4003, "not in hall");
    }

    // 获取用户分数（从 token 或需要查询数据库）
    // 暂时使用默认分数，后续可从 data 或数据库获取
    int score = data.isMember("score") ? data["score"].asInt() : 1000;

    // 调用 Matcher 入队
    bool success = _matcher->enqueue(user_id, score);
    if (!success) {
        LOG_WARN("WebSocketHandler: match.start enqueue failed - user_id=" << user_id);
        return make_error(4004, "enqueue failed");
    }

    LOG_INFO("WebSocketHandler: match.start success - user_id=" << user_id);

    // 返回等待状态
    Json::Value resp_data;
    resp_data["queue_position"] = 1;  // 近似值
    resp_data["wait_time"] = 0;
    return make_response("match.waiting", resp_data);
}

inline std::string WebSocketHandler::handle_match_cancel(int64_t user_id, const Json::Value& data) {
    (void)data;
    // 调用 Matcher 取消
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

inline std::string WebSocketHandler::handle_ping(int64_t user_id, const Json::Value& data) {
    (void)user_id;
    (void)data;
    Json::Value resp_data;
    resp_data["timestamp"] = util::now_sec();
    return make_response("pong", resp_data);
}

inline void WebSocketHandler::handle_game_move(int64_t user_id, const Json::Value& data) {
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

inline void WebSocketHandler::handle_game_giveup(int64_t user_id) {
    if (!_game_ctrl) {
        send_error_to_user(user_id, 5000, "game not available");
        return;
    }
    _game_ctrl->handle_giveup(user_id);
}

inline void WebSocketHandler::handle_game_reconnect(int64_t user_id, const Json::Value& data) {
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

inline void WebSocketHandler::process_timers() {
    flush_pending_disconnects();
}

inline void WebSocketHandler::send_error_to_user(int64_t user_id, int code,
                                                  const std::string& message) {
    if (_conn_mgr) {
        _conn_mgr->send(user_id, make_error(code, message));
    }
}

inline void WebSocketHandler::on_match_success(const MatchResult& result) {
    LOG_INFO("WebSocketHandler: match success - room_id=" << result.room_id
             << ", player1=" << result.player1_id
             << ", player2=" << result.player2_id);

    std::string game_room_id;
    if (_game_ctrl) {
        game_room_id = _game_ctrl->handle_game_start(result.player1_id, result.player2_id);
    }

    // 向双方发送 match.success（房间号与 game.start 一致）
    Json::Value player1_data;
    if (!game_room_id.empty()) {
        player1_data["room_id"] = game_room_id;
    } else {
        player1_data["room_id"] = (Json::Int64)result.room_id;
    }
    player1_data["color"] = (result.player1_color == 1) ? "black" : "white";

    Json::Value player1_opponent;
    player1_opponent["user_id"] = (Json::Int64)result.player2_id;
    // 后续可从数据库获取对手用户名
    player1_opponent["username"] = "opponent";
    player1_data["opponent"] = player1_opponent;

    std::string msg1 = make_response("match.success", player1_data);
    _conn_mgr->send(result.player1_id, msg1);

    // 向 player2 发送
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

inline std::string WebSocketHandler::make_response(const std::string& event, const Json::Value& data) {
    Json::Value resp;
    resp["event"] = event;
    resp["data"] = data;
    return util::json_to_str(resp);
}

inline std::string WebSocketHandler::make_error(int code, const std::string& message) {
    Json::Value resp;
    resp["event"] = "error";
    resp["data"]["code"] = code;
    resp["data"]["message"] = message;
    return util::json_to_str(resp);
}

inline int64_t WebSocketHandler::verify_token_from_data(const Json::Value& data) {
    if (!data.isMember("token")) {
        return 0;
    }
    std::string token = data["token"].asString();
    return security::jwt_verify(token);
}

inline void WebSocketHandler::handle_reconnect_accept(int64_t user_id) {
    if (!_game_ctrl) {
        send_error_to_user(user_id, 5000, "game not available");
        return;
    }
    _game_ctrl->handle_reconnect_accept(user_id);
}

inline void WebSocketHandler::handle_reconnect_reject(int64_t user_id) {
    if (!_game_ctrl) {
        send_error_to_user(user_id, 5000, "game not available");
        return;
    }
    _game_ctrl->handle_reconnect_reject(user_id);
}

inline void WebSocketHandler::send_reconnect_available(int64_t user_id, WebsocketConnectionHdl hdl) {
    if (!_game_ctrl) return;

    // 获取房间信息
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

    // 剩余断线超时秒数
    int elapsed = room->get_disconnect_elapsed();
    int remaining = 60 - elapsed;
    if (remaining < 1) remaining = 1;
    msg["data"]["timeout_remaining"] = remaining;

    _server->send(hdl, msg.toStyledString(), websocketpp::frame::opcode::text);

    LOG_INFO("WebSocketHandler: sent reconnect.available to user_id=" << user_id
             << ", room=" << room->get_room_id() << ", remaining=" << remaining << "s");
}

} // namespace gobang
