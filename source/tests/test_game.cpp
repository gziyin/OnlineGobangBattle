#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "game.hpp"
#include "room.hpp"
#include "online.hpp"
#include "connection_manager.hpp"
#include "logger.hpp"
#include "util.hpp"

#include <json/json.h>

using websocketpp::connection_hdl;

namespace {

typedef websocketpp::client<websocketpp::config::asio_client> TestClientImpl;

std::string json_compact(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    builder["emitUTF8"] = true;
    return Json::writeString(builder, value);
}

// 简单的消息收集器
class MessageCollector {
public:
    void push(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx_);
        messages_.push_back(msg);
        cv_.notify_all();
    }

    bool wait_for(size_t expected_count, int timeout_ms) {
        std::unique_lock<std::mutex> lock(mtx_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                           [this, expected_count]() {
                               return messages_.size() >= expected_count;
                           });
    }

    std::vector<std::string> get_messages() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return messages_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        messages_.clear();
    }

private:
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<std::string> messages_;
};

// 测试用的 WebSocket 服务器
class TestGameServer {
public:
    static const uint16_t kPort = 18081;

    ~TestGameServer() {
        stop();  // 确保线程被 join
    }

    void start() {
        // 创建 logs 目录（如果不存在）
        system("mkdir -p ../logs");
        gobang::Logger::instance().init("../logs/test.log");

        conn_mgr_.init(&server_);
        online_mgr_.user_online(1001);
        online_mgr_.user_online(1002);

        game_ctrl_ = gobang::GameController::create();
        game_ctrl_->init(&room_mgr_, &online_mgr_, &conn_mgr_, nullptr);
        game_ctrl_->set_timeout_seconds(2);  // 短超时用于测试

        server_.clear_access_channels(websocketpp::log::alevel::all);
        server_.clear_error_channels(websocketpp::log::elevel::all);
        server_.init_asio();
        server_.set_reuse_addr(true);

        server_.set_open_handler([this](connection_hdl hdl) {
            auto con = server_.get_con_from_hdl(hdl);
            std::string resource = con->get_resource();
            // 从 URL 提取 user_id
            if (resource.find("/ws?user_id=") == 0) {
                int64_t user_id = std::stoll(resource.substr(12));
                conn_mgr_.add(user_id, hdl);
                user_connections_[user_id] = hdl;
            }
        });

        server_.set_close_handler([this](connection_hdl hdl) {
            // 清理连接
            for (auto it = user_connections_.begin(); it != user_connections_.end(); ++it) {
                // 不能直接比较 hdl，需要通过其他方式
            }
        });

        server_.set_message_handler([this](connection_hdl hdl, TestClientImpl::message_ptr msg) {
            // 收集消息
            collector_.push(msg->get_payload());
        });

        websocketpp::lib::asio::ip::tcp::endpoint endpoint(
            websocketpp::lib::asio::ip::address_v4::loopback(), kPort);
        server_.listen(endpoint);
        server_.start_accept();

        thread_ = std::thread([this]() {
            server_.run();
        });
    }

    void stop() {
        server_.stop_listening();
        server_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    gobang::GameController& game_ctrl() { return *game_ctrl_; }
    gobang::RoomManager& room_mgr() { return room_mgr_; }
    gobang::OnlineManager& online_mgr() { return online_mgr_; }
    MessageCollector& collector() { return collector_; }

private:
    // 声明顺序决定析构顺序（逆序析构）
    // 先声明的后析构，后声明的先析构
    std::thread thread_;  // 最后析构，确保在 game_ctrl_ 之后
    std::unordered_map<int64_t, connection_hdl> user_connections_;
    MessageCollector collector_;
    std::shared_ptr<gobang::GameController> game_ctrl_;  // 在 server_ 之前析构
    gobang::RoomManager room_mgr_;
    gobang::OnlineManager online_mgr_;
    gobang::ConnectionManager conn_mgr_;
    gobang::WebsocketServer server_;
};

class GameControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        server_.game_ctrl().stop_all_timers();
        server_.stop();
    }

    TestGameServer server_;
};

// 测试用例
TEST_F(GameControllerTest, HandleGameStart) {
    server_.game_ctrl().handle_game_start(1001, 1002);

    // 验证房间已创建
    gobang::GameRoom* room = server_.room_mgr().get_room_by_user(1001);
    ASSERT_NE(room, nullptr);
    EXPECT_TRUE(room->has_player(1001));
    EXPECT_TRUE(room->has_player(1002));

    // 验证状态变为 IN_ROOM
    EXPECT_EQ(server_.online_mgr().get_status(1001), gobang::OnlineStatus::IN_ROOM);
    EXPECT_EQ(server_.online_mgr().get_status(1002), gobang::OnlineStatus::IN_ROOM);
}

TEST_F(GameControllerTest, HandleMoveNotInRoom) {
    // 玩家不在房间中，应该收到错误消息
    // 由于没有实际连接，send 会失败，但不应该崩溃
    server_.game_ctrl().handle_move(9999, 7, 7);
}

TEST_F(GameControllerTest, HandleGiveupNotInRoom) {
    server_.game_ctrl().handle_giveup(9999);
}

TEST_F(GameControllerTest, HandleDisconnectNotInRoom) {
    server_.game_ctrl().handle_disconnect(9999);
}

TEST_F(GameControllerTest, HandleReconnectNotInRoom) {
    server_.game_ctrl().handle_reconnect(9999);
}

TEST_F(GameControllerTest, TimerCleanupOnDestroy) {
    // 测试析构时定时器清理是否安全
    {
        auto ctrl = gobang::GameController::create();
        ctrl->init(&server_.room_mgr(), &server_.online_mgr(), nullptr, nullptr);
        ctrl->set_timeout_seconds(1);
        ctrl->handle_game_start(2001, 2002);
        // ctrl 在这里析构，应该不会崩溃
    }
    SUCCEED();
}

TEST_F(GameControllerTest, MultipleGameStartStop) {
    // 测试多次创建和销毁游戏
    for (int i = 0; i < 3; ++i) {
        int64_t p1 = 3001 + i * 2;
        int64_t p2 = 3002 + i * 2;
        server_.online_mgr().user_online(p1);
        server_.online_mgr().user_online(p2);
        server_.game_ctrl().handle_game_start(p1, p2);
    }
    // 停止所有定时器
    server_.game_ctrl().stop_all_timers();
    SUCCEED();
}

TEST_F(GameControllerTest, ProcessPendingTimeouts) {
    // 测试超时队列处理
    server_.game_ctrl().handle_game_start(4001, 4002);
    // process_pending_timeouts 应该可以安全调用
    server_.game_ctrl().process_pending_timeouts();
    SUCCEED();
}

} // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
