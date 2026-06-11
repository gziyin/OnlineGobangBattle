#pragma once

#include <cstdint>
#include <string>

namespace gobang {

class IMessageSender {
public:
    virtual ~IMessageSender() = default;

    virtual bool send(int64_t user_id, const std::string& msg) = 0;

    virtual void broadcast(int64_t uid1, int64_t uid2, const std::string& msg) {
        send(uid1, msg);
        send(uid2, msg);
    }
};

} // namespace gobang
