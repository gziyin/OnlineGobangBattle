#pragma once

#include <cstdint>
#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <memory>
#include <vector>

#include "room.hpp"
#include "online.hpp"
#include "connection_manager.hpp"
#include "user_table.hpp"
#include "logger.hpp"

#include <json/json.h>

namespace gobang {

class GameController : public std::enable_shared_from_this<GameController> {
public:
    static std::shared_ptr<GameController> create() {
        return std::shared_ptr<GameController>(new GameController());
    }

    ~GameController() {
        cleanup_all_timers();
    }

private:
    GameController()
        : room_mgr_(nullptr), online_mgr_(nullptr),
          conn_mgr_(nullptr), user_table_(nullptr),
          timeout_seconds_(60), timer_shutdown_(false),
          timer_worker_running_(false) {}

public:

    void init(RoomManager* room_mgr, OnlineManager* online_mgr,
              ConnectionManager* conn_mgr, UserTable* user_table) {
        cleanup_all_timers();
        room_mgr_ = room_mgr;
        online_mgr_ = online_mgr;
        conn_mgr_ = conn_mgr;
        user_table_ = user_table;
    }

    void set_timeout_seconds(int seconds) {
        timeout_seconds_ = seconds > 0 ? seconds : 1;
    }

    void stop_all_timers() {
        cleanup_all_timers();
    }

    void process_pending_timeouts() {
        std::vector<std::string> pending;
        {
            std::lock_guard<std::mutex> lock(timeout_queue_mtx_);
            pending.swap(timeout_queue_);
        }
        for (const auto& room_id : pending) {
            GameRoom* room = room_mgr_->get_room(room_id);
            if (room && room->get_status() == RoomStatus::PLAYING) {
                int64_t current = room->get_current_turn();
                int64_t opponent = room->get_opponent_id(current);
                on_game_over(room_id, GameResult::TIMEOUT, opponent, current);
            }
        }
    }

    std::string handle_game_start(int64_t player1_id, int64_t player2_id) {
        std::string room_id = room_mgr_->create_room(player1_id, player2_id);
        if (room_id.empty()) {
            LOG_WARN("handle_game_start: 创建房间失败");
            return "";
        }

        online_mgr_->set_status(player1_id, OnlineStatus::IN_ROOM);
        online_mgr_->set_status(player2_id, OnlineStatus::IN_ROOM);

        Json::Value opponent1;
        opponent1["user_id"] = player2_id;
        Json::Value opponent2;
        opponent2["user_id"] = player1_id;

        Json::Value msg1;
        msg1["event"] = "game.start";
        msg1["data"]["room_id"] = room_id;
        msg1["data"]["opponent"] = opponent2;
        msg1["data"]["color"] = "black";
        msg1["data"]["your_turn"] = true;

        Json::Value msg2;
        msg2["event"] = "game.start";
        msg2["data"]["room_id"] = room_id;
        msg2["data"]["opponent"] = opponent1;
        msg2["data"]["color"] = "white";
        msg2["data"]["your_turn"] = false;

        if (conn_mgr_) {
            conn_mgr_->send(player1_id, msg1.toStyledString());
            conn_mgr_->send(player2_id, msg2.toStyledString());
        }

        start_timeout_timer(room_id);

        LOG_INFO("游戏开始: " << room_id << ", 玩家: " << player1_id << " vs " << player2_id);
        return room_id;
    }

    void handle_move(int64_t user_id, int row, int col) {
        GameRoom* room = room_mgr_->get_room_by_user(user_id);
        if (!room) {
            send_error(user_id, 4001, "not in room");
            return;
        }

        GameResult result = room->place_piece(user_id, row, col);
        if (result == GameResult::NONE) {
            // place_piece 返回 NONE 表示落子失败（非当前回合或位置无效）
            send_error(user_id, 4002, "invalid move");
            return;
        }

        if (result == GameResult::MOVE_SUCCESS) {
            // 落子成功，游戏继续
            PieceColor color;
            room->get_player_color(user_id, color);

            Json::Value msg;
            msg["event"] = "game.move";
            msg["data"]["row"] = row;
            msg["data"]["col"] = col;
            msg["data"]["color"] = (color == PieceColor::BLACK) ? "black" : "white";
            int64_t next_turn = room->get_current_turn();
            PieceColor next_color;
            room->get_player_color(next_turn, next_color);
            msg["data"]["next_turn"] = (next_color == PieceColor::BLACK) ? "black" : "white";
            broadcast_to_room(room->get_room_id(), msg.toStyledString());

            start_timeout_timer(room->get_room_id());
        } else if (result == GameResult::BLACK_WIN || result == GameResult::WHITE_WIN) {
            // 落子成功，有胜负
            // 先广播最后一步落子
            PieceColor color;
            room->get_player_color(user_id, color);
            Json::Value move_msg;
            move_msg["event"] = "game.move";
            move_msg["data"]["row"] = row;
            move_msg["data"]["col"] = col;
            move_msg["data"]["color"] = (color == PieceColor::BLACK) ? "black" : "white";
            move_msg["data"]["next_turn"] = "none";
            broadcast_to_room(room->get_room_id(), move_msg.toStyledString());

            int64_t winner_id, loser_id;
            get_winner_loser(room, result, winner_id, loser_id);
            on_game_over(room->get_room_id(), result, winner_id, loser_id);
        } else if (result == GameResult::DRAW) {
            on_game_over(room->get_room_id(), result, 0, 0);
        }
    }

    void handle_giveup(int64_t user_id) {
        GameRoom* room = room_mgr_->get_room_by_user(user_id);
        if (!room) {
            send_error(user_id, 4001, "not in room");
            return;
        }

        GameResult result = room->give_up(user_id);
        if (result == GameResult::NONE) {
            send_error(user_id, 4003, "cannot give up");
            return;
        }

        int64_t winner_id = room->get_opponent_id(user_id);
        on_game_over(room->get_room_id(), GameResult::GIVEUP, winner_id, user_id);
    }

    void handle_disconnect(int64_t user_id) {
        GameRoom* room = room_mgr_->get_room_by_user(user_id);
        if (!room) return;
        LOG_INFO("玩家 " << user_id << " 断线，等待重连");
    }

    void handle_reconnect(int64_t user_id) {
        GameRoom* room = room_mgr_->get_room_by_user(user_id);
        if (!room) {
            send_error(user_id, 4004, "no room to reconnect");
            return;
        }

        Json::Value msg;
        msg["event"] = "game.reconnect";
        msg["data"]["room_id"] = room->get_room_id();
        msg["data"]["board"] = room->get_board_state();

        int64_t current_turn = room->get_current_turn();
        PieceColor turn_color;
        room->get_player_color(current_turn, turn_color);
        msg["data"]["current_turn"] = (turn_color == PieceColor::BLACK) ? "black" : "white";

        int64_t opponent_id = room->get_opponent_id(user_id);
        Json::Value opponent;
        opponent["user_id"] = opponent_id;
        msg["data"]["opponent"] = opponent;

        if (conn_mgr_) {
            conn_mgr_->send(user_id, msg.toStyledString());
        }
        LOG_INFO("玩家 " << user_id << " 重连成功");
    }

private:
    void on_game_over(const std::string& room_id, GameResult result,
                      int64_t winner_id, int64_t loser_id) {
        stop_timeout_timer(room_id);

        GameRoom* room = room_mgr_->get_room(room_id);
        if (!room) return;

        if (winner_id != 0 && loser_id != 0 && user_table_) {
            user_table_->update_score_match(winner_id, loser_id);
        }

        std::string result_str;
        std::string reason;
        switch (result) {
            case GameResult::BLACK_WIN:
                result_str = "black_win";
                reason = "five_in_row";
                break;
            case GameResult::WHITE_WIN:
                result_str = "white_win";
                reason = "five_in_row";
                break;
            case GameResult::DRAW:
                result_str = "draw";
                reason = "board_full";
                break;
            case GameResult::TIMEOUT: {
                PieceColor winner_color;
                result_str = (winner_id == room->get_opponent_id(loser_id)) ?
                    (room->get_player_color(winner_id, winner_color) && winner_color == PieceColor::BLACK ? "black_win" : "white_win") : "timeout";
                reason = "timeout";
                break;
            }
            case GameResult::GIVEUP: {
                PieceColor winner_color;
                result_str = (winner_id == room->get_opponent_id(loser_id)) ?
                    (room->get_player_color(winner_id, winner_color) && winner_color == PieceColor::BLACK ? "black_win" : "white_win") : "giveup";
                reason = "giveup";
                break;
            }
            default:
                result_str = "unknown";
                reason = "unknown";
                break;
        }

        Json::Value msg;
        msg["event"] = "game.over";
        msg["data"]["result"] = result_str;
        msg["data"]["reason"] = reason;
        if (winner_id != 0) {
            Json::Value winner;
            winner["user_id"] = winner_id;
            msg["data"]["winner"] = winner;
        }
        msg["data"]["score_change"] = 25;

        broadcast_to_room(room_id, msg.toStyledString());

        int64_t p1, p2;
        room->get_player_ids(p1, p2);
        online_mgr_->set_status(p1, OnlineStatus::HALL_IDLE);
        online_mgr_->set_status(p2, OnlineStatus::HALL_IDLE);

        room_mgr_->destroy_room(room_id);

        LOG_INFO("游戏结束: " << room_id << ", 结果: " << result_str);
    }

    void broadcast_to_room(const std::string& room_id, const std::string& msg) {
        if (!conn_mgr_) return;
        GameRoom* room = room_mgr_->get_room(room_id);
        if (!room) return;

        int64_t p1, p2;
        room->get_player_ids(p1, p2);
        conn_mgr_->send(p1, msg);
        conn_mgr_->send(p2, msg);
    }

    void start_timeout_timer(const std::string& room_id) {
        {
            std::lock_guard<std::mutex> lock(timers_mtx_);
            RoomTimerEntry& entry = room_timers_[room_id];
            entry.deadline = std::chrono::steady_clock::now() +
                             std::chrono::seconds(timeout_seconds_);
            entry.active = true;
        }
        ensure_timer_worker();
        timer_cv_.notify_one();
    }

    void stop_timeout_timer(const std::string& room_id) {
        {
            std::lock_guard<std::mutex> lock(timers_mtx_);
            room_timers_.erase(room_id);
        }
        timer_cv_.notify_one();
    }

    void cleanup_all_timers() {
        {
            std::lock_guard<std::mutex> lock(timers_mtx_);
            room_timers_.clear();
        }
        timer_shutdown_ = true;
        timer_cv_.notify_one();
        if (timer_worker_.joinable()) {
            timer_worker_.join();
        }
        timer_shutdown_ = false;
        timer_worker_running_ = false;
    }

    void ensure_timer_worker() {
        if (timer_worker_running_) {
            return;
        }
        timer_worker_running_ = true;
        timer_shutdown_ = false;
        timer_worker_ = std::thread(&GameController::timer_worker_loop, this);
    }

    void timer_worker_loop() {
        while (!timer_shutdown_) {
            std::vector<std::string> expired;

            {
                std::unique_lock<std::mutex> lock(timers_mtx_);
                const auto now = std::chrono::steady_clock::now();

                for (auto it = room_timers_.begin(); it != room_timers_.end(); ) {
                    if (!it->second.active) {
                        it = room_timers_.erase(it);
                        continue;
                    }
                    if (it->second.deadline <= now) {
                        expired.push_back(it->first);
                        it = room_timers_.erase(it);
                    } else {
                        ++it;
                    }
                }

                if (!expired.empty()) {
                    lock.unlock();
                    for (const auto& room_id : expired) {
                        handle_timeout_async(room_id);
                    }
                    continue;
                }

                if (timer_shutdown_) {
                    break;
                }

                if (room_timers_.empty()) {
                    timer_cv_.wait(lock, [this]() {
                        return timer_shutdown_ || !room_timers_.empty();
                    });
                    continue;
                }

                auto wake_at = room_timers_.begin()->second.deadline;
                for (const auto& pair : room_timers_) {
                    if (pair.second.active && pair.second.deadline < wake_at) {
                        wake_at = pair.second.deadline;
                    }
                }

                timer_cv_.wait_until(lock, wake_at, [this]() {
                    if (timer_shutdown_) {
                        return true;
                    }
                    const auto now = std::chrono::steady_clock::now();
                    for (const auto& pair : room_timers_) {
                        if (pair.second.active && pair.second.deadline <= now) {
                            return true;
                        }
                    }
                    return false;
                });
            }
        }
    }

    void handle_timeout_async(const std::string& room_id) {
        std::lock_guard<std::mutex> lock(timeout_queue_mtx_);
        timeout_queue_.push_back(room_id);
    }

    void get_winner_loser(GameRoom* room, GameResult result,
                          int64_t& winner_id, int64_t& loser_id) {
        int64_t p1, p2;
        room->get_player_ids(p1, p2);

        if (result == GameResult::BLACK_WIN) {
            winner_id = p1;
            loser_id = p2;
        } else if (result == GameResult::WHITE_WIN) {
            winner_id = p2;
            loser_id = p1;
        } else {
            winner_id = 0;
            loser_id = 0;
        }
    }

    void send_error(int64_t user_id, int code, const std::string& message) {
        if (!conn_mgr_) return;
        Json::Value msg;
        msg["event"] = "error";
        msg["data"]["code"] = code;
        msg["data"]["message"] = message;
        conn_mgr_->send(user_id, msg.toStyledString());
    }

    struct RoomTimerEntry {
        std::chrono::steady_clock::time_point deadline;
        bool active;
        RoomTimerEntry()
            : deadline(std::chrono::steady_clock::now()), active(false) {}
    };

    RoomManager*       room_mgr_;
    OnlineManager*     online_mgr_;
    ConnectionManager* conn_mgr_;
    UserTable*         user_table_;
    int                timeout_seconds_;

    std::unordered_map<std::string, RoomTimerEntry> room_timers_;
    std::mutex timers_mtx_;
    std::thread timer_worker_;
    std::condition_variable timer_cv_;
    std::atomic<bool> timer_shutdown_;
    std::atomic<bool> timer_worker_running_;

    std::vector<std::string> timeout_queue_;
    std::mutex timeout_queue_mtx_;
};

} // namespace gobang
