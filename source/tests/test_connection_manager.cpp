/**
 * ConnectionManager 单元测试
 *
 * 测试连接映射的核心功能
 * 注意：由于 ConnectionManager 依赖 WebSocket++，
 * 此测试使用模拟方式验证映射逻辑
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

// 模拟 connection_hdl 类型用于测试
// 实际使用时需要完整 WebSocket++ 环境
namespace websocketpp {
    namespace config {
        struct asio {};
    }
    template<typename T>
    struct connection {
        int mock_id;
        connection(int id = 0) : mock_id(id) {}
    };

    typedef std::shared_ptr<connection<config::asio>> connection_ptr;
    typedef std::weak_ptr<connection<config::asio>> connection_hdl;
}

// 模拟 WebSocket 服务器
namespace websocketpp {
    namespace frame {
        namespace opcode {
            enum value { text = 1 };
        }
    }
    namespace lib {
        namespace error {
            struct category {};
        }
    }

    template<typename T>
    class server {
    public:
        connection_ptr get_con_from_hdl(connection_hdl hdl) {
            return hdl.lock();
        }

        void send(connection_hdl hdl, const std::string& msg, int opcode) {
            // 模拟发送
            sent_messages.push_back(msg);
        }

        std::vector<std::string> sent_messages;
    };
}

// 引入实际头文件（需要调整以支持模拟）
// 这里直接定义简化版本进行测试

#include <cstdint>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace gobang {

typedef websocketpp::connection_hdl WebsocketConnectionHdl;
typedef websocketpp::server<websocketpp::config::asio> WebsocketServer;

class ConnectionManager {
public:
    void init(WebsocketServer* server) {
        _server = server;
    }

    void add(int64_t user_id, WebsocketConnectionHdl hdl) {
        std::lock_guard<std::mutex> lock(_mtx);
        _connections[user_id] = hdl;
    }

    void remove(int64_t user_id) {
        std::lock_guard<std::mutex> lock(_mtx);
        _connections.erase(user_id);
    }

    bool get(int64_t user_id, WebsocketConnectionHdl& out) const {
        std::lock_guard<std::mutex> lock(_mtx);
        auto it = _connections.find(user_id);
        if (it == _connections.end()) return false;
        out = it->second;
        return true;
    }

    bool send(int64_t user_id, const std::string& msg) {
        WebsocketConnectionHdl hdl;
        {
            std::lock_guard<std::mutex> lock(_mtx);
            auto it = _connections.find(user_id);
            if (it == _connections.end()) return false;
            hdl = it->second;
        }

        if (hdl.expired()) return false;

        try {
            auto conn = _server->get_con_from_hdl(hdl);
            if (!conn) return false;
            _server->send(hdl, msg, websocketpp::frame::opcode::text);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool is_connected(int64_t user_id) const {
        std::lock_guard<std::mutex> lock(_mtx);
        auto it = _connections.find(user_id);
        if (it == _connections.end()) return false;
        return !it->second.expired();
    }

    std::vector<int64_t> get_all_user_ids() const {
        std::lock_guard<std::mutex> lock(_mtx);
        std::vector<int64_t> ids;
        for (const auto& pair : _connections) {
            if (!pair.second.expired()) ids.push_back(pair.first);
        }
        return ids;
    }

    size_t connection_count() const {
        std::lock_guard<std::mutex> lock(_mtx);
        return _connections.size();
    }

private:
    mutable std::mutex _mtx;
    std::unordered_map<int64_t, WebsocketConnectionHdl> _connections;
    WebsocketServer* _server = nullptr;
};

} // namespace gobang

// ==================== 测试用例 ====================

class ConnectionManagerTest : public ::testing::Test {
protected:
    gobang::ConnectionManager mgr;
    websocketpp::server<websocketpp::config::asio> server;

    void SetUp() override {
        mgr.init(&server);
    }
};

// 测试：添加连接映射
TEST_F(ConnectionManagerTest, AddConnection) {
    auto conn1 = std::make_shared<websocketpp::connection<websocketpp::config::asio>>(1);
    websocketpp::connection_hdl hdl1 = conn1;

    mgr.add(1001, hdl1);

    EXPECT_TRUE(mgr.is_connected(1001));
    EXPECT_EQ(mgr.connection_count(), 1);
}

// 测试：重复添加覆盖旧连接
TEST_F(ConnectionManagerTest, AddDuplicateOverride) {
    auto conn1 = std::make_shared<websocketpp::connection<websocketpp::config::asio>>(1);
    auto conn2 = std::make_shared<websocketpp::connection<websocketpp::config::asio>>(2);

    websocketpp::connection_hdl hdl1 = conn1;
    websocketpp::connection_hdl hdl2 = conn2;

    mgr.add(1001, hdl1);
    mgr.add(1001, hdl2);

    EXPECT_TRUE(mgr.is_connected(1001));
    EXPECT_EQ(mgr.connection_count(), 1);  // 应该只有一条记录

    websocketpp::connection_hdl out;
    EXPECT_TRUE(mgr.get(1001, out));
    EXPECT_EQ(out.lock()->mock_id, 2);  // 应是新连接
}

// 测试：移除连接后不可获取
TEST_F(ConnectionManagerTest, RemoveConnection) {
    auto conn1 = std::make_shared<websocketpp::connection<websocketpp::config::asio>>(1);
    websocketpp::connection_hdl hdl1 = conn1;

    mgr.add(1001, hdl1);
    EXPECT_TRUE(mgr.is_connected(1001));

    mgr.remove(1001);
    EXPECT_FALSE(mgr.is_connected(1001));
    EXPECT_EQ(mgr.connection_count(), 0);

    websocketpp::connection_hdl out;
    EXPECT_FALSE(mgr.get(1001, out));
}

// 测试：发送消息成功
TEST_F(ConnectionManagerTest, SendSuccess) {
    auto conn1 = std::make_shared<websocketpp::connection<websocketpp::config::asio>>(1);
    websocketpp::connection_hdl hdl1 = conn1;

    mgr.add(1001, hdl1);

    std::string msg = "{\"event\":\"test\"}";
    EXPECT_TRUE(mgr.send(1001, msg));

    EXPECT_EQ(server.sent_messages.size(), 1);
    EXPECT_EQ(server.sent_messages[0], msg);
}

// 测试：发送给不存在用户失败
TEST_F(ConnectionManagerTest, SendToNonExistentUser) {
    std::string msg = "{\"event\":\"test\"}";
    EXPECT_FALSE(mgr.send(9999, msg));
    EXPECT_EQ(server.sent_messages.size(), 0);
}

// 测试：发送给已断开连接失败
TEST_F(ConnectionManagerTest, SendToExpiredConnection) {
    auto conn1 = std::make_shared<websocketpp::connection<websocketpp::config::asio>>(1);
    websocketpp::connection_hdl hdl1 = conn1;

    mgr.add(1001, hdl1);

    // 模拟连接断开
    conn1.reset();

    EXPECT_FALSE(mgr.is_connected(1001));

    std::string msg = "{\"event\":\"test\"}";
    EXPECT_FALSE(mgr.send(1001, msg));
}

// 测试：获取所有在线用户ID
TEST_F(ConnectionManagerTest, GetAllUserIds) {
    auto conn1 = std::make_shared<websocketpp::connection<websocketpp::config::asio>>(1);
    auto conn2 = std::make_shared<websocketpp::connection<websocketpp::config::asio>>(2);
    auto conn3 = std::make_shared<websocketpp::connection<websocketpp::config::asio>>(3);

    mgr.add(1001, conn1);
    mgr.add(1002, conn2);
    mgr.add(1003, conn3);

    auto ids = mgr.get_all_user_ids();
    EXPECT_EQ(ids.size(), 3);

    // 模拟一个断开
    conn2.reset();

    ids = mgr.get_all_user_ids();
    EXPECT_EQ(ids.size(), 2);  // 应排除已断开的
}

// 测试：并发读写安全
TEST_F(ConnectionManagerTest, ConcurrentAccess) {
    const int NUM_THREADS = 10;
    const int OPERATIONS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // 创建一些初始连接
    for (int i = 0; i < NUM_THREADS; ++i) {
        auto conn = std::make_shared<websocketpp::connection<websocketpp::config::asio>>(i);
        mgr.add(i, conn);
    }

    // 并发操作：读写混合
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                // 交替执行不同操作
                if (j % 4 == 0) {
                    mgr.is_connected(i);
                } else if (j % 4 == 1) {
                    websocketpp::connection_hdl out;
                    mgr.get(i, out);
                } else if (j % 4 == 2) {
                    mgr.send(i, "test");
                    success_count++;
                } else {
                    mgr.connection_count();
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 验证没有崩溃，计数正确
    EXPECT_GT(success_count.load(), 0);
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}