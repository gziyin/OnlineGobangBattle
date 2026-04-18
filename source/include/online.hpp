#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace gobang {

enum class OnlineStatus {
    OFFLINE   = 0,
    HALL_IDLE = 1,
    MATCHING  = 2,
    IN_ROOM   = 3
};

class OnlineManager {
public:
    void user_online(int64_t user_id);
    void user_offline(int64_t user_id);
    bool is_online(int64_t user_id) const;

    bool set_status(int64_t user_id, OnlineStatus status);
    OnlineStatus get_status(int64_t user_id) const;

    size_t online_count() const;

private:
    mutable std::mutex _mtx;
    std::unordered_map<int64_t, OnlineStatus> _users;
};

inline void OnlineManager::user_online(int64_t user_id) {
    std::lock_guard<std::mutex> lock(_mtx);
    _users[user_id] = OnlineStatus::HALL_IDLE;
}

inline void OnlineManager::user_offline(int64_t user_id) {
    std::lock_guard<std::mutex> lock(_mtx);
    _users.erase(user_id);
}

inline bool OnlineManager::is_online(int64_t user_id) const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _users.find(user_id) != _users.end();
}

inline bool OnlineManager::set_status(int64_t user_id, OnlineStatus status) {
    std::lock_guard<std::mutex> lock(_mtx);
    std::unordered_map<int64_t, OnlineStatus>::iterator it = _users.find(user_id);
    if (it == _users.end()) {
        return false;
    }
    it->second = status;
    return true;
}

inline OnlineStatus OnlineManager::get_status(int64_t user_id) const {
    std::lock_guard<std::mutex> lock(_mtx);
    std::unordered_map<int64_t, OnlineStatus>::const_iterator it = _users.find(user_id);
    if (it == _users.end()) {
        return OnlineStatus::OFFLINE;
    }
    return it->second;
}

inline size_t OnlineManager::online_count() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _users.size();
}

} // namespace gobang
