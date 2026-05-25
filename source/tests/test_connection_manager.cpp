/**
 * ConnectionManager 单元测试
 *
 * 使用生产头文件 connection_manager.hpp，通过真实 websocketpp 服务器
 * 验证连接映射的核心功能。
 */

#include "connection_manager.hpp"

#include <gtest/gtest.h>

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using websocketpp::connection_hdl;

namespace {

typedef websocketpp::client<websocketpp::config::asio_client> TestClient;

// ========== 辅助：最小 WebSocket 服务器 ==========

class MiniServer {
public:
    void start(gobang::ConnectionManager* mgr) {
        _mgr = mgr;
        _mgr->init(&_server);

        _server.clear_access_channels(websocketpp::log::alevel::all);
        _server.clear_error_channels(websocketpp::log::elevel::all);
        _server.init_asio();
        _server.set_reuse_addr(true);

        _server.set_open_handler([this](connection_hdl hdl) {
            std::lock_guard<std::mutex> lock(_mtx);
            _client_hdl = hdl;
            _connected = true;
            _cv.notify_all();
        });

        _server.set_message_handler(
            [this](connection_hdl, gobang::WebsocketServer::message_ptr msg) {
                std::lock_guard<std::mutex> lock(_msg_mtx);
                _received.push_back(msg->get_payload());
                _msg_cv.notify_all();
            });

        // port 0 = 由操作系统分配可用端口
        websocketpp::lib::asio::ip::tcp::endpoint endpoint(
            websocketpp::lib::asio::ip::address_v4::loopback(), 0);
        _server.listen(endpoint);
        _server.start_accept();

        websocketpp::lib::error_code ec;
        _port = _server.get_local_endpoint(ec).port();

        _thread = std::thread([this]() { _server.run(); });
    }

    void stop() {
        websocketpp::lib::error_code ec;
        _server.stop_listening(ec);
        _server.stop();
        if (_thread.joinable()) {
            _thread.join();
        }
    }

    uint16_t port() const { return _port; }

    // 阻塞等待服务器接收到客户端连接
    bool wait_for_connection(int timeout_ms = 2000) {
        std::unique_lock<std::mutex> lock(_mtx);
        return _cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                            [this]() { return _connected; });
    }

    // 返回服务器侧的客户端连接句柄
    connection_hdl client_hdl() {
        std::lock_guard<std::mutex> lock(_mtx);
        return _client_hdl;
    }

    // 阻塞等待服务器接收到至少 count 条消息
    bool wait_for_messages(size_t count, int timeout_ms = 2000) {
        std::unique_lock<std::mutex> lock(_msg_mtx);
        return _msg_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                [this, count]() {
                                    return _received.size() >= count;
                                });
    }

    std::vector<std::string> received_messages() {
        std::lock_guard<std::mutex> lock(_msg_mtx);
        return _received;
    }

private:
    gobang::WebsocketServer _server;
    gobang::ConnectionManager* _mgr = nullptr;
    std::thread _thread;
    uint16_t _port = 0;

    std::mutex _mtx;
    std::condition_variable _cv;
    connection_hdl _client_hdl;
    bool _connected = false;

    std::mutex _msg_mtx;
    std::condition_variable _msg_cv;
    std::vector<std::string> _received;
};

// ========== 辅助：最小 WebSocket 客户端 ==========

class MiniClient {
public:
    bool connect(const std::string& uri, int timeout_ms = 2000) {
        _client.clear_access_channels(websocketpp::log::alevel::all);
        _client.clear_error_channels(websocketpp::log::elevel::all);
        _client.init_asio();
        _client.start_perpetual();

        _client.set_open_handler([this](connection_hdl hdl) {
            {
                std::lock_guard<std::mutex> lock(_mtx);
                _hdl = hdl;
                _opened = true;
            }
            _cv.notify_all();
        });

        _client.set_fail_handler([this](connection_hdl) {
            {
                std::lock_guard<std::mutex> lock(_mtx);
                _failed = true;
            }
            _cv.notify_all();
        });

        websocketpp::lib::error_code ec;
        auto con = _client.get_connection(uri, ec);
        if (ec) return false;

        _client.connect(con);
        _thread = std::thread([this]() { _client.run(); });

        std::unique_lock<std::mutex> lock(_mtx);
        bool ready = _cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                  [this]() { return _opened || _failed; });
        return ready && _opened;
    }

    void close() {
        websocketpp::lib::error_code ec;
        if (_opened) {
            _client.close(_hdl, websocketpp::close::status::normal, "done", ec);
        }
        _client.stop_perpetual();
        if (_thread.joinable()) {
            _thread.join();
        }
    }

    connection_hdl hdl() {
        std::lock_guard<std::mutex> lock(_mtx);
        return _hdl;
    }

private:
    TestClient _client;
    std::thread _thread;
    connection_hdl _hdl;

    std::mutex _mtx;
    std::condition_variable _cv;
    bool _opened = false;
    bool _failed = false;
};

// ========== 测试 Fixture ==========

class ConnectionManagerTest : public ::testing::Test {
protected:
    gobang::ConnectionManager mgr;
    MiniServer server;

    void SetUp() override {
        server.start(&mgr);
    }

    void TearDown() override {
        server.stop();
    }

    std::string uri() const {
        std::ostringstream oss;
        oss << "ws://127.0.0.1:" << server.port() << "/";
        return oss.str();
    }
};

// ========== 测试用例 ==========

TEST_F(ConnectionManagerTest, AddConnection) {
    MiniClient client;
    ASSERT_TRUE(client.connect(uri()));
    ASSERT_TRUE(server.wait_for_connection());

    connection_hdl hdl = server.client_hdl();
    mgr.add(1001, hdl);

    EXPECT_TRUE(mgr.is_connected(1001));
    EXPECT_EQ(mgr.connection_count(), 1u);

    client.close();
}

TEST_F(ConnectionManagerTest, AddDuplicateOverride) {
    MiniClient client1;
    ASSERT_TRUE(client1.connect(uri()));
    ASSERT_TRUE(server.wait_for_connection());
    connection_hdl hdl1 = server.client_hdl();
    mgr.add(1001, hdl1);

    MiniClient client2;
    ASSERT_TRUE(client2.connect(uri()));
    ASSERT_TRUE(server.wait_for_connection());
    connection_hdl hdl2 = server.client_hdl();
    mgr.add(1001, hdl2);

    EXPECT_EQ(mgr.connection_count(), 1u);

    connection_hdl out;
    EXPECT_TRUE(mgr.get(1001, out));
    // out 应指向第二个连接（hdl2）
    EXPECT_EQ(out.lock(), hdl2.lock());

    client1.close();
    client2.close();
}

TEST_F(ConnectionManagerTest, RemoveConnection) {
    MiniClient client;
    ASSERT_TRUE(client.connect(uri()));
    ASSERT_TRUE(server.wait_for_connection());

    connection_hdl hdl = server.client_hdl();
    mgr.add(1001, hdl);
    EXPECT_TRUE(mgr.is_connected(1001));

    mgr.remove(1001);
    EXPECT_FALSE(mgr.is_connected(1001));
    EXPECT_EQ(mgr.connection_count(), 0u);

    connection_hdl out;
    EXPECT_FALSE(mgr.get(1001, out));

    client.close();
}

TEST_F(ConnectionManagerTest, SendSuccess) {
    MiniClient client;
    ASSERT_TRUE(client.connect(uri()));
    ASSERT_TRUE(server.wait_for_connection());

    connection_hdl hdl = server.client_hdl();
    mgr.add(1001, hdl);

    std::string msg = "{\"event\":\"test\"}";
    EXPECT_TRUE(mgr.send(1001, msg));

    // 服务器端应收到 ConnectionManager 发出的消息
    ASSERT_TRUE(server.wait_for_messages(1));
    auto msgs = server.received_messages();
    EXPECT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0], msg);

    client.close();
}

TEST_F(ConnectionManagerTest, SendToNonExistentUser) {
    std::string msg = "{\"event\":\"test\"}";
    EXPECT_FALSE(mgr.send(9999, msg));
}

TEST_F(ConnectionManagerTest, SendToExpiredConnection) {
    // 使用已过期的 weak_ptr 模拟已断开的客户端
    std::shared_ptr<gobang::WebsocketServer::connection_type> dead;
    connection_hdl hdl = dead; // 已过期

    mgr.add(1001, hdl);
    EXPECT_FALSE(mgr.is_connected(1001));

    std::string msg = "{\"event\":\"test\"}";
    EXPECT_FALSE(mgr.send(1001, msg));
}

TEST_F(ConnectionManagerTest, GetAllUserIds) {
    MiniClient c1, c2, c3;
    ASSERT_TRUE(c1.connect(uri()));
    ASSERT_TRUE(server.wait_for_connection());
    connection_hdl h1 = server.client_hdl();
    mgr.add(1001, h1);

    ASSERT_TRUE(c2.connect(uri()));
    ASSERT_TRUE(server.wait_for_connection());
    connection_hdl h2 = server.client_hdl();
    mgr.add(1002, h2);

    ASSERT_TRUE(c3.connect(uri()));
    ASSERT_TRUE(server.wait_for_connection());
    connection_hdl h3 = server.client_hdl();
    mgr.add(1003, h3);

    auto ids = mgr.get_all_user_ids();
    EXPECT_EQ(ids.size(), 3u);

    // 模拟断连：移除一个用户
    mgr.remove(1002);
    ids = mgr.get_all_user_ids();
    EXPECT_EQ(ids.size(), 2u);

    c1.close();
    c2.close();
    c3.close();
}

TEST_F(ConnectionManagerTest, ConcurrentAccess) {
    const int NUM_THREADS = 10;
    const int OPS_PER_THREAD = 100;

    // 预先创建连接用于并发测试
    std::vector<MiniClient*> clients;
    for (int i = 0; i < NUM_THREADS; ++i) {
        auto* c = new MiniClient();
        ASSERT_TRUE(c->connect(uri()));
        ASSERT_TRUE(server.wait_for_connection());
        mgr.add(i, server.client_hdl());
        clients.push_back(c);
    }

    std::vector<std::thread> threads;
    std::atomic<int> ops_done{0};

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                if (j % 4 == 0) {
                    mgr.is_connected(i);
                } else if (j % 4 == 1) {
                    connection_hdl out;
                    mgr.get(i, out);
                } else if (j % 4 == 2) {
                    mgr.send(i, "test");
                } else {
                    mgr.connection_count();
                }
                ops_done++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(ops_done.load(), NUM_THREADS * OPS_PER_THREAD);

    for (auto* c : clients) {
        c->close();
        delete c;
    }
}

} // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
