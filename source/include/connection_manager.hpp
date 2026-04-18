#pragma once
#include <cstdint>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <memory>

// WebSocket++ 连接句柄类型定义
// 此文件是唯一暴露 connection_hdl 的业务头文件
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

namespace gobang {

// WebSocket 服务器类型定义
typedef websocketpp::server<websocketpp::config::asio_no_tls> WebsocketServer;
typedef websocketpp::connection_hdl WebsocketConnectionHdl;

/**
 * @brief 连接管理器
 *
 * 负责维护 user_id 与 WebSocket 连接的映射关系
 * 此文件是唯一暴露 connection_hdl 的业务头文件
 *
 * 职责边界：
 * - 只做 user_id <-> connection_hdl 映射和消息发送
 * - 不承载匹配业务状态（匹配状态由 OnlineManager 和 Matcher 管理）
 */
class ConnectionManager {
public:
    /**
     * @brief 初始化，注入 WebSocket 服务器实例
     * @param server WebSocket 服务器指针
     */
    void init(WebsocketServer* server);

    /**
     * @brief 添加用户连接映射
     * @param user_id 用户 ID
     * @param hdl WebSocket 连接句柄
     *
     * 如果 user_id 已存在，覆盖旧连接
     */
    void add(int64_t user_id, WebsocketConnectionHdl hdl);

    /**
     * @brief 移除用户连接映射
     * @param user_id 用户 ID
     */
    void remove(int64_t user_id);

    /**
     * @brief 获取用户连接句柄
     * @param user_id 用户 ID
     * @param out 输出参数，连接句柄
     * @return 成功返回 true，用户不存在返回 false
     */
    bool get(int64_t user_id, WebsocketConnectionHdl& out) const;

    /**
     * @brief 向指定用户发送消息
     * @param user_id 用户 ID
     * @param msg 消息内容（JSON 字符串）
     * @return 成功发送返回 true；用户不存在或连接失效返回 false
     *
     * 连接失效时返回 false，允许由上层统一决定是否调用 remove()
     */
    bool send(int64_t user_id, const std::string& msg);

    /**
     * @brief 检查用户是否已连接
     * @param user_id 用户 ID
     * @return 已连接返回 true
     */
    bool is_connected(int64_t user_id) const;

    /**
     * @brief 获取所有在线用户ID
     * @return 用户ID列表
     */
    std::vector<int64_t> get_all_user_ids() const;

    /**
     * @brief 获取当前连接数
     */
    size_t connection_count() const;

private:
    mutable std::mutex _mtx;
    std::unordered_map<int64_t, WebsocketConnectionHdl> _connections;
    WebsocketServer* _server = nullptr;
};

// ==================== 实现 ====================

inline void ConnectionManager::init(WebsocketServer* server) {
    _server = server;
}

inline void ConnectionManager::add(int64_t user_id, WebsocketConnectionHdl hdl) {
    std::lock_guard<std::mutex> lock(_mtx);
    _connections[user_id] = hdl;
}

inline void ConnectionManager::remove(int64_t user_id) {
    std::lock_guard<std::mutex> lock(_mtx);
    _connections.erase(user_id);
}

inline bool ConnectionManager::get(int64_t user_id, WebsocketConnectionHdl& out) const {
    std::lock_guard<std::mutex> lock(_mtx);
    auto it = _connections.find(user_id);
    if (it == _connections.end()) {
        return false;
    }
    out = it->second;
    return true;
}

inline bool ConnectionManager::send(int64_t user_id, const std::string& msg) {
    WebsocketConnectionHdl hdl;
    {
        std::lock_guard<std::mutex> lock(_mtx);
        auto it = _connections.find(user_id);
        if (it == _connections.end()) {
            return false;
        }
        hdl = it->second;
    }

    // 检查连接是否有效
    if (hdl.expired()) {
        return false;
    }

    // 获取连接指针并发送
    try {
        auto conn = _server->get_con_from_hdl(hdl);
        if (!conn) {
            return false;
        }
        _server->send(hdl, msg, websocketpp::frame::opcode::text);
        return true;
    } catch (const std::exception& e) {
        // 连接可能已断开
        return false;
    }
}

inline bool ConnectionManager::is_connected(int64_t user_id) const {
    std::lock_guard<std::mutex> lock(_mtx);
    auto it = _connections.find(user_id);
    if (it == _connections.end()) {
        return false;
    }
    return !it->second.expired();
}

inline std::vector<int64_t> ConnectionManager::get_all_user_ids() const {
    std::lock_guard<std::mutex> lock(_mtx);
    std::vector<int64_t> ids;
    ids.reserve(_connections.size());
    for (const auto& pair : _connections) {
        if (!pair.second.expired()) {
            ids.push_back(pair.first);
        }
    }
    return ids;
}

inline size_t ConnectionManager::connection_count() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _connections.size();
}

} // namespace gobang
