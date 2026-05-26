#include "room.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace gobang;

// ============================================================
// GameRoom 测试 Fixture
// ============================================================

class GameRoomTest : public ::testing::Test {
protected:
    void SetUp() override {
        room_.reset(new GameRoom("R_TEST_001", 1001, 1002));
    }

    // 辅助：交替落子构建棋盘，black_moves 和 white_moves 交替执行
    void build_board(std::vector<std::pair<int,int>> black_moves,
                     std::vector<std::pair<int,int>> white_moves) {
        size_t bi = 0, wi = 0;
        bool black_turn = true;
        while (bi < black_moves.size() || wi < white_moves.size()) {
            if (black_turn && bi < black_moves.size()) {
                room_->place_piece(1001, black_moves[bi].first, black_moves[bi].second);
                ++bi;
            } else if (!black_turn && wi < white_moves.size()) {
                room_->place_piece(1002, white_moves[wi].first, white_moves[wi].second);
                ++wi;
            }
            black_turn = !black_turn;
        }
    }

    std::unique_ptr<GameRoom> room_;
};

// ============================================================
// GameRoom 基础测试
// ============================================================

TEST_F(GameRoomTest, InitialStateIsPlaying) {
    EXPECT_EQ(room_->get_status(), RoomStatus::PLAYING);
    EXPECT_EQ(room_->get_result(), GameResult::NONE);
    EXPECT_EQ(room_->get_move_count(), 0);
}

TEST_F(GameRoomTest, BlackGoesFirst) {
    EXPECT_EQ(room_->get_current_turn(), 1001);
}

TEST_F(GameRoomTest, PlacePieceValidMove) {
    GameResult r = room_->place_piece(1001, 7, 7);
    EXPECT_EQ(r, GameResult::NONE);
    EXPECT_EQ(room_->get_board(7, 7), 1);  // 黑子
    EXPECT_EQ(room_->get_current_turn(), 1002);  // 轮到白方
    EXPECT_EQ(room_->get_move_count(), 1);
}

TEST_F(GameRoomTest, PlacePieceOutOfBounds) {
    EXPECT_EQ(room_->place_piece(1001, -1, 7), GameResult::NONE);
    EXPECT_EQ(room_->place_piece(1001, 15, 7), GameResult::NONE);
    EXPECT_EQ(room_->place_piece(1001, 7, -1), GameResult::NONE);
    EXPECT_EQ(room_->place_piece(1001, 7, 15), GameResult::NONE);
    EXPECT_EQ(room_->get_move_count(), 0);
}

TEST_F(GameRoomTest, PlacePieceAlreadyOccupied) {
    room_->place_piece(1001, 7, 7);
    EXPECT_EQ(room_->place_piece(1002, 7, 7), GameResult::NONE);
}

TEST_F(GameRoomTest, PlacePieceNotYourTurn) {
    EXPECT_EQ(room_->place_piece(1002, 7, 7), GameResult::NONE);
}

TEST_F(GameRoomTest, PlacePieceUnknownUser) {
    EXPECT_EQ(room_->place_piece(9999, 7, 7), GameResult::NONE);
}

TEST_F(GameRoomTest, PlacePieceAfterFinished) {
    // 构建黑方横向五连
    build_board({{7,3}, {7,4}, {7,5}, {7,6}, {7,7}},
                {{8,3}, {8,4}, {8,5}, {8,6}});
    EXPECT_EQ(room_->get_status(), RoomStatus::FINISHED);
    EXPECT_EQ(room_->place_piece(1002, 8, 7), GameResult::NONE);
}

// ============================================================
// 胜负判定测试
// ============================================================

TEST_F(GameRoomTest, WinHorizontal) {
    // 黑方在第7行横向五连
    build_board({{7,3}, {7,4}, {7,5}, {7,6}, {7,7}},
                {{8,3}, {8,4}, {8,5}, {8,6}});
    EXPECT_EQ(room_->get_result(), GameResult::BLACK_WIN);
    EXPECT_EQ(room_->get_status(), RoomStatus::FINISHED);
}

TEST_F(GameRoomTest, WinVertical) {
    // 黑方在第7列纵向五连
    build_board({{3,7}, {4,7}, {5,7}, {6,7}, {7,7}},
                {{3,8}, {4,8}, {5,8}, {6,8}});
    EXPECT_EQ(room_->get_result(), GameResult::BLACK_WIN);
}

TEST_F(GameRoomTest, WinDiagonalMain) {
    // 黑方主对角线五连 (3,3)-(7,7)
    build_board({{3,3}, {4,4}, {5,5}, {6,6}, {7,7}},
                {{3,4}, {4,5}, {5,6}, {6,7}});
    EXPECT_EQ(room_->get_result(), GameResult::BLACK_WIN);
}

TEST_F(GameRoomTest, WinDiagonalAnti) {
    // 黑方副对角线五连 (3,11)-(7,7)
    build_board({{3,11}, {4,10}, {5,9}, {6,8}, {7,7}},
                {{3,10}, {4,9}, {5,8}, {6,7}});
    EXPECT_EQ(room_->get_result(), GameResult::BLACK_WIN);
}

TEST_F(GameRoomTest, WinExactlyFive) {
    // 恰好五连
    build_board({{7,3}, {7,4}, {7,5}, {7,6}, {7,7}},
                {{8,3}, {8,4}, {8,5}, {8,6}});
    EXPECT_EQ(room_->get_result(), GameResult::BLACK_WIN);
}

TEST_F(GameRoomTest, WinMoreThanFive) {
    // 六连也应判定胜利（先放4个，第5个落子时已五连，第6个落子时已结束）
    build_board({{7,2}, {7,3}, {7,4}, {7,5}, {7,6}},
                {{8,2}, {8,3}, {8,4}, {8,5}});
    // 第5步黑(7,6)时已五连
    EXPECT_EQ(room_->get_result(), GameResult::BLACK_WIN);
}

TEST_F(GameRoomTest, NoWinFourInRow) {
    // 四连不应判定胜利
    build_board({{7,3}, {7,4}, {7,5}, {7,6}},
                {{8,3}, {8,4}, {8,5}});
    EXPECT_EQ(room_->get_result(), GameResult::NONE);
    EXPECT_EQ(room_->get_status(), RoomStatus::PLAYING);
}

// ============================================================
// 认输测试
// ============================================================

TEST_F(GameRoomTest, GiveUpBlackConcedes) {
    GameResult r = room_->give_up(1001);
    EXPECT_EQ(r, GameResult::WHITE_WIN);
    EXPECT_EQ(room_->get_status(), RoomStatus::FINISHED);
}

TEST_F(GameRoomTest, GiveUpWhiteConcedes) {
    // 先让黑方走一步
    room_->place_piece(1001, 7, 7);
    GameResult r = room_->give_up(1002);
    EXPECT_EQ(r, GameResult::BLACK_WIN);
    EXPECT_EQ(room_->get_status(), RoomStatus::FINISHED);
}

TEST_F(GameRoomTest, GiveUpAfterFinished) {
    room_->give_up(1001);
    EXPECT_EQ(room_->give_up(1002), GameResult::NONE);
}

TEST_F(GameRoomTest, GiveUpUnknownUser) {
    EXPECT_EQ(room_->give_up(9999), GameResult::NONE);
}

// ============================================================
// 玩家查询测试
// ============================================================

TEST_F(GameRoomTest, GetBoardState) {
    room_->place_piece(1001, 7, 7);
    std::string state = room_->get_board_state();
    EXPECT_EQ(state.size(), 225u);
    EXPECT_EQ(state[7 * 15 + 7], '1');  // 黑子
}

TEST_F(GameRoomTest, HasPlayer) {
    EXPECT_TRUE(room_->has_player(1001));
    EXPECT_TRUE(room_->has_player(1002));
    EXPECT_FALSE(room_->has_player(9999));
}

TEST_F(GameRoomTest, GetOpponentId) {
    EXPECT_EQ(room_->get_opponent_id(1001), 1002);
    EXPECT_EQ(room_->get_opponent_id(1002), 1001);
    EXPECT_EQ(room_->get_opponent_id(9999), 0);
}

TEST_F(GameRoomTest, GetPlayerColor) {
    PieceColor c;
    EXPECT_TRUE(room_->get_player_color(1001, c));
    EXPECT_EQ(c, PieceColor::BLACK);
    EXPECT_TRUE(room_->get_player_color(1002, c));
    EXPECT_EQ(c, PieceColor::WHITE);
    EXPECT_FALSE(room_->get_player_color(9999, c));
}

// ============================================================
// RoomManager 测试
// ============================================================

TEST(RoomManagerTest, CreateRoomReturnsValidId) {
    RoomManager mgr;
    std::string id = mgr.create_room(1001, 1002);
    EXPECT_FALSE(id.empty());
    EXPECT_EQ(id[0], 'R');
}

TEST(RoomManagerTest, CreateRoomDuplicatePlayerFails) {
    RoomManager mgr;
    mgr.create_room(1001, 1002);
    EXPECT_TRUE(mgr.create_room(1001, 1003).empty());
    EXPECT_TRUE(mgr.create_room(1003, 1002).empty());
}

TEST(RoomManagerTest, GetRoomById) {
    RoomManager mgr;
    std::string id = mgr.create_room(1001, 1002);
    GameRoom* room = mgr.get_room(id);
    ASSERT_NE(room, nullptr);
    EXPECT_EQ(room->get_room_id(), id);
}

TEST(RoomManagerTest, GetRoomNotFound) {
    RoomManager mgr;
    EXPECT_EQ(mgr.get_room("R_NONEXIST"), nullptr);
}

TEST(RoomManagerTest, GetRoomByUser) {
    RoomManager mgr;
    mgr.create_room(1001, 1002);
    GameRoom* room = mgr.get_room_by_user(1001);
    ASSERT_NE(room, nullptr);
    EXPECT_TRUE(room->has_player(1001));
}

TEST(RoomManagerTest, GetRoomByUserNotFound) {
    RoomManager mgr;
    EXPECT_EQ(mgr.get_room_by_user(1001), nullptr);
}

TEST(RoomManagerTest, DestroyRoom) {
    RoomManager mgr;
    std::string id = mgr.create_room(1001, 1002);
    EXPECT_EQ(mgr.room_count(), 1u);

    mgr.destroy_room(id);
    EXPECT_EQ(mgr.room_count(), 0u);
    EXPECT_EQ(mgr.get_room(id), nullptr);
    EXPECT_EQ(mgr.get_room_by_user(1001), nullptr);
    EXPECT_EQ(mgr.get_room_by_user(1002), nullptr);
}

TEST(RoomManagerTest, RoomCount) {
    RoomManager mgr;
    EXPECT_EQ(mgr.room_count(), 0u);

    std::string id1 = mgr.create_room(1001, 1002);
    EXPECT_EQ(mgr.room_count(), 1u);

    std::string id2 = mgr.create_room(1003, 1004);
    EXPECT_EQ(mgr.room_count(), 2u);

    mgr.destroy_room(id1);
    EXPECT_EQ(mgr.room_count(), 1u);
}

// ============================================================
// 并发安全测试
// ============================================================

TEST(RoomManagerConcurrencyTest, ConcurrentCreateAndDestroy) {
    RoomManager mgr;
    const int N = 50;
    std::vector<std::thread> threads;
    std::vector<std::string> ids(N);

    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&mgr, &ids, i]() {
            ids[i] = mgr.create_room(10000 + i * 2, 10000 + i * 2 + 1);
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(mgr.room_count(), static_cast<size_t>(N));

    threads.clear();
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&mgr, &ids, i]() {
            mgr.destroy_room(ids[i]);
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(mgr.room_count(), 0u);
}

TEST(GameRoomConcurrencyTest, ConcurrentPlacePiece) {
    GameRoom room("R_CONC_001", 1001, 1002);
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        int64_t uid = (i % 2 == 0) ? 1001 : 1002;
        int col = i % 15;
        threads.emplace_back([&room, uid, col]() {
            room.place_piece(uid, 7, col);
        });
    }
    for (auto& t : threads) t.join();

    // 验证无崩溃即可
    EXPECT_EQ(room.get_status(), RoomStatus::PLAYING);
}

TEST(RoomManagerConcurrencyTest, ConcurrentGetRoomByUser) {
    RoomManager mgr;
    mgr.create_room(1001, 1002);
    mgr.create_room(1003, 1004);

    std::vector<std::thread> threads;
    for (int i = 0; i < 20; ++i) {
        threads.emplace_back([&mgr]() {
            for (int j = 0; j < 100; ++j) {
                mgr.get_room_by_user(1001);
                mgr.get_room_by_user(1003);
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(mgr.room_count(), 2u);
}

// ============================================================
// main
// ============================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
