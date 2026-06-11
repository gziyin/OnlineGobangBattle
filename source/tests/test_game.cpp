#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <sys/stat.h>

#include "config.h"
#include "game.hpp"
#include "room.hpp"
#include "online.hpp"
#include "asio_timer_types.hpp"
#include "logger.hpp"

#include <websocketpp/common/asio.hpp>

namespace {

static void ensure_source_logs_dir() {
    const std::string dir = std::string(GOBANG_SOURCE_DIR) + "/logs";
    mkdir(dir.c_str(), 0755);
}

// 轻量夹具：不启动 WebSocket，专注 GameController / RoomManager 逻辑
class GameControllerUnitTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool logger_ready = false;
        if (!logger_ready) {
            ensure_source_logs_dir();
            const std::string log_path =
                std::string(GOBANG_SOURCE_DIR) + "/logs/test_game.log";
            gobang::Logger::instance().init(log_path, gobang::LogLevel::WARN);
            logger_ready = true;
        }

        online_mgr_.user_online(1001);
        online_mgr_.user_online(1002);

        game_ctrl_ = gobang::GameController::create();
        game_ctrl_->init(&room_mgr_, &online_mgr_, nullptr, nullptr);
        game_ctrl_->set_timeout_seconds(2);
    }

    void TearDown() override {
        if (game_ctrl_) {
            game_ctrl_->stop_all_timers();
        }
    }

    gobang::RoomManager room_mgr_;
    gobang::OnlineManager online_mgr_;
    std::shared_ptr<gobang::GameController> game_ctrl_;
};

TEST_F(GameControllerUnitTest, HandleGameStart) {
    game_ctrl_->handle_game_start(1001, 1002);

    gobang::GameRoom* room = room_mgr_.get_room_by_user(1001);
    ASSERT_NE(room, nullptr);
    EXPECT_TRUE(room->has_player(1001));
    EXPECT_TRUE(room->has_player(1002));

    EXPECT_EQ(online_mgr_.get_status(1001), gobang::OnlineStatus::IN_ROOM);
    EXPECT_EQ(online_mgr_.get_status(1002), gobang::OnlineStatus::IN_ROOM);
}

TEST_F(GameControllerUnitTest, HandleMoveNotInRoom) {
    game_ctrl_->handle_move(9999, 7, 7);
    SUCCEED();
}

TEST_F(GameControllerUnitTest, HandleGiveupNotInRoom) {
    game_ctrl_->handle_giveup(9999);
    SUCCEED();
}

TEST_F(GameControllerUnitTest, HandleDisconnectNotInRoom) {
    game_ctrl_->handle_disconnect(9999);
    SUCCEED();
}

TEST_F(GameControllerUnitTest, HandleReconnectNotInRoom) {
    game_ctrl_->handle_reconnect(9999);
    SUCCEED();
}

TEST_F(GameControllerUnitTest, TimerCleanupOnDestroy) {
    {
        auto ctrl = gobang::GameController::create();
        ctrl->init(&room_mgr_, &online_mgr_, nullptr, nullptr);
        ctrl->set_timeout_seconds(1);
        online_mgr_.user_online(2001);
        online_mgr_.user_online(2002);
        ctrl->handle_game_start(2001, 2002);
        ctrl->handle_giveup(2001);
    }
    SUCCEED();
}

TEST_F(GameControllerUnitTest, MultipleGameStartStop) {
    for (int i = 0; i < 3; ++i) {
        const int64_t p1 = 3001 + static_cast<int64_t>(i) * 2;
        const int64_t p2 = 3002 + static_cast<int64_t>(i) * 2;
        online_mgr_.user_online(p1);
        online_mgr_.user_online(p2);
        game_ctrl_->handle_game_start(p1, p2);
        game_ctrl_->handle_giveup(p1);
        game_ctrl_->stop_all_timers();
    }
    SUCCEED();
}

TEST_F(GameControllerUnitTest, TurnTimeoutViaAsioTimer) {
    gobang::IoService io;
    typedef websocketpp::lib::asio::io_service::work IoWorkGuard;
    std::unique_ptr<IoWorkGuard> work_guard(new IoWorkGuard(io));
    std::thread io_thread([&io]() { io.run(); });

    auto ctrl = gobang::GameController::create();
    ctrl->init(&room_mgr_, &online_mgr_, nullptr, nullptr, &io);
    ctrl->set_timeout_seconds(2);

    online_mgr_.user_online(4001);
    online_mgr_.user_online(4002);
    ctrl->handle_game_start(4001, 4002);
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    EXPECT_EQ(room_mgr_.get_room_by_user(4001), nullptr);
    EXPECT_EQ(online_mgr_.get_status(4001), gobang::OnlineStatus::HALL_IDLE);
    EXPECT_EQ(online_mgr_.get_status(4002), gobang::OnlineStatus::HALL_IDLE);

    ctrl->stop_all_timers();
    work_guard.reset();
    io.stop();
    if (io_thread.joinable()) {
        io_thread.join();
    }
}

} // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
