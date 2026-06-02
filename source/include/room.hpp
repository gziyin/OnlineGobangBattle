#pragma once

#include <cstdint>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <sstream>

#include "logger.hpp"
#include "util.hpp"

namespace gobang {

// ============================================================
// 枚举定义
// ============================================================

enum class RoomStatus {
    WAITING  = 0,
    PLAYING  = 1,
    FINISHED = 2
};

enum class PieceColor {
    NONE  = 0,
    BLACK = 1,
    WHITE = 2
};

enum class GameResult {
    NONE         = 0,
    BLACK_WIN    = 1,
    WHITE_WIN    = 2,
    DRAW         = 3,
    TIMEOUT      = 4,
    GIVEUP       = 5,
    MOVE_SUCCESS = 6  // 落子成功，游戏继续
};

// ============================================================
// PlayerInfo
// ============================================================

struct PlayerInfo {
    int64_t     user_id;
    PieceColor  color;
    bool        ready;

    PlayerInfo() : user_id(0), color(PieceColor::NONE), ready(false) {}
    PlayerInfo(int64_t uid, PieceColor c) : user_id(uid), color(c), ready(true) {}
};

// ============================================================
// GameRoom
// ============================================================

class GameRoom {
public:
    GameRoom(const std::string& room_id, int64_t player1_id, int64_t player2_id)
        : room_id_(room_id),
          status_(RoomStatus::PLAYING),
          result_(GameResult::NONE),
          board_{},
          current_turn_index_(0),
          move_count_(0)
    {
        players_[0] = PlayerInfo(player1_id, PieceColor::BLACK);
        players_[1] = PlayerInfo(player2_id, PieceColor::WHITE);
    }

    GameRoom(const GameRoom&) = delete;
    GameRoom& operator=(const GameRoom&) = delete;

    // ===== 查询接口 =====

    std::string get_room_id() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return room_id_;
    }

    RoomStatus get_status() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return status_;
    }

    GameResult get_result() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return result_;
    }

    int64_t get_current_turn() const {
        std::lock_guard<std::mutex> lock(mtx_);
        if (status_ != RoomStatus::PLAYING) return 0;
        return players_[current_turn_index_].user_id;
    }

    std::string get_board_state() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return util::board_to_str(board_);
    }

    int get_move_count() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return move_count_;
    }

    int get_board(int row, int col) const {
        std::lock_guard<std::mutex> lock(mtx_);
        if (row < 0 || row >= 15 || col < 0 || col >= 15) return -1;
        return board_[row][col];
    }

    // ===== 玩家查询 =====

    bool has_player(int64_t user_id) const {
        std::lock_guard<std::mutex> lock(mtx_);
        return find_player_index(user_id) >= 0;
    }

    bool get_player_color(int64_t user_id, PieceColor& out) const {
        std::lock_guard<std::mutex> lock(mtx_);
        int idx = find_player_index(user_id);
        if (idx < 0) return false;
        out = players_[idx].color;
        return true;
    }

    int64_t get_opponent_id(int64_t user_id) const {
        std::lock_guard<std::mutex> lock(mtx_);
        int idx = find_player_index(user_id);
        if (idx < 0) return 0;
        return players_[1 - idx].user_id;
    }

    // 获取两个玩家的 ID
    void get_player_ids(int64_t& player1_id, int64_t& player2_id) const {
        std::lock_guard<std::mutex> lock(mtx_);
        player1_id = players_[0].user_id;
        player2_id = players_[1].user_id;
    }

    // ===== 游戏操作 =====

    GameResult place_piece(int64_t user_id, int row, int col) {
        std::lock_guard<std::mutex> lock(mtx_);

        if (status_ != RoomStatus::PLAYING) return GameResult::NONE;

        int idx = find_player_index(user_id);
        if (idx < 0 || idx != current_turn_index_) return GameResult::NONE;

        if (!is_valid_move(row, col)) return GameResult::NONE;

        PieceColor color = players_[idx].color;
        board_[row][col] = static_cast<int>(color);
        ++move_count_;

        if (check_win(row, col, color)) {
            status_ = RoomStatus::FINISHED;
            result_ = (color == PieceColor::BLACK) ? GameResult::BLACK_WIN : GameResult::WHITE_WIN;
            LOG_INFO("房间 " << room_id_ << " 游戏结束，玩家 " << user_id << " 获胜");
            return result_;
        }

        if (move_count_ >= 225) {
            status_ = RoomStatus::FINISHED;
            result_ = GameResult::DRAW;
            return result_;
        }

        current_turn_index_ = 1 - current_turn_index_;
        return GameResult::MOVE_SUCCESS;
    }

    GameResult give_up(int64_t user_id) {
        std::lock_guard<std::mutex> lock(mtx_);

        if (status_ != RoomStatus::PLAYING) return GameResult::NONE;

        int idx = find_player_index(user_id);
        if (idx < 0) return GameResult::NONE;

        status_ = RoomStatus::FINISHED;
        result_ = (players_[idx].color == PieceColor::BLACK)
                  ? GameResult::WHITE_WIN : GameResult::BLACK_WIN;
        LOG_INFO("房间 " << room_id_ << " 玩家 " << user_id << " 认输");
        return result_;
    }

private:
    bool check_win(int row, int col, PieceColor color) const {
        static const int dirs[4][2] = {
            {0, 1}, {1, 0}, {1, 1}, {1, -1}
        };
        int c = static_cast<int>(color);

        for (int d = 0; d < 4; ++d) {
            int count = 1;
            for (int step = 1; step <= 4; ++step) {
                int r2 = row + dirs[d][0] * step;
                int c2 = col + dirs[d][1] * step;
                if (r2 < 0 || r2 >= 15 || c2 < 0 || c2 >= 15) break;
                if (board_[r2][c2] != c) break;
                ++count;
            }
            for (int step = 1; step <= 4; ++step) {
                int r2 = row - dirs[d][0] * step;
                int c2 = col - dirs[d][1] * step;
                if (r2 < 0 || r2 >= 15 || c2 < 0 || c2 >= 15) break;
                if (board_[r2][c2] != c) break;
                ++count;
            }
            if (count >= 5) return true;
        }
        return false;
    }

    bool is_valid_move(int row, int col) const {
        if (row < 0 || row >= 15 || col < 0 || col >= 15) return false;
        return board_[row][col] == 0;
    }

    int find_player_index(int64_t user_id) const {
        if (players_[0].user_id == user_id) return 0;
        if (players_[1].user_id == user_id) return 1;
        return -1;
    }

    std::string      room_id_;
    RoomStatus       status_;
    GameResult       result_;
    int              board_[15][15];
    PlayerInfo       players_[2];
    int              current_turn_index_;
    int              move_count_;
    mutable std::mutex mtx_;
};

// ============================================================
// RoomManager
// ============================================================

class RoomManager {
public:
    RoomManager() : next_seq_(0) {}

    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;

    std::string create_room(int64_t player1_id, int64_t player2_id) {
        std::lock_guard<std::mutex> lock(mtx_);

        if (user_room_map_.count(player1_id) || user_room_map_.count(player2_id)) {
            LOG_WARN("创建房间失败：玩家已在房间中");
            return "";
        }

        std::string room_id = generate_room_id();
        auto room = std::unique_ptr<GameRoom>(new GameRoom(room_id, player1_id, player2_id));
        user_room_map_[player1_id] = room_id;
        user_room_map_[player2_id] = room_id;
        rooms_[room_id] = std::move(room);

        LOG_INFO("房间创建成功: " << room_id << ", 玩家: " << player1_id << " vs " << player2_id);
        return room_id;
    }

    GameRoom* get_room(const std::string& room_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = rooms_.find(room_id);
        if (it == rooms_.end()) return nullptr;
        return it->second.get();
    }

    GameRoom* get_room_by_user(int64_t user_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = user_room_map_.find(user_id);
        if (it == user_room_map_.end()) return nullptr;
        auto rit = rooms_.find(it->second);
        if (rit == rooms_.end()) return nullptr;
        return rit->second.get();
    }

    void destroy_room(const std::string& room_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = rooms_.find(room_id);
        if (it == rooms_.end()) return;

        for (auto uit = user_room_map_.begin(); uit != user_room_map_.end(); ) {
            if (uit->second == room_id) {
                uit = user_room_map_.erase(uit);
            } else {
                ++uit;
            }
        }
        rooms_.erase(it);
        LOG_INFO("房间销毁: " << room_id);
    }

    size_t room_count() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return rooms_.size();
    }

private:
    std::string generate_room_id() {
        int64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t seq = next_seq_.fetch_add(1);

        std::ostringstream oss;
        oss << "R" << (ts % 10000000000LL);
        oss.width(4);
        oss.fill('0');
        oss << (seq % 10000);
        return oss.str();
    }

    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::unique_ptr<GameRoom>> rooms_;
    std::unordered_map<int64_t, std::string> user_room_map_;
    std::atomic<int64_t> next_seq_;
};

} // namespace gobang
