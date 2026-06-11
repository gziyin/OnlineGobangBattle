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

} // namespace gobang
