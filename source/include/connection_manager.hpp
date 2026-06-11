#pragma once
#include <cstdint>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <memory>

#include "config.h"
#include "message_sender.hpp"

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio.hpp>
#if GOBANG_HAS_WEBSOCKETPP_ASIO_NO_TLS
#include <websocketpp/config/asio_no_tls.hpp>
#endif

namespace gobang {

#if GOBANG_HAS_WEBSOCKETPP_ASIO_NO_TLS
typedef websocketpp::server<websocketpp::config::asio_no_tls> WebsocketServer;
#else
typedef websocketpp::server<websocketpp::config::asio> WebsocketServer;
#endif
typedef websocketpp::connection_hdl WebsocketConnectionHdl;

class ConnectionManager : public IMessageSender {
public:
    void init(WebsocketServer* server);
    void add(int64_t user_id, WebsocketConnectionHdl hdl);
    void remove(int64_t user_id);
    bool get(int64_t user_id, WebsocketConnectionHdl& out) const;
    bool send(int64_t user_id, const std::string& msg) override;
    bool is_connected(int64_t user_id) const;
    std::vector<int64_t> get_all_user_ids() const;
    size_t connection_count() const;

private:
    mutable std::mutex _mtx;
    std::unordered_map<int64_t, WebsocketConnectionHdl> _connections;
    WebsocketServer* _server = nullptr;
};

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

    if (hdl.expired()) {
        return false;
    }

    try {
        auto conn = _server->get_con_from_hdl(hdl);
        if (!conn) {
            return false;
        }
        _server->send(hdl, msg, websocketpp::frame::opcode::text);
        return true;
    } catch (const std::exception&) {
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
