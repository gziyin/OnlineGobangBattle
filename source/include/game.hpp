#pragma once

#include <cstdint>
#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <memory>

#include "room.hpp"
#include "online.hpp"
#include "connection_manager.hpp"
#include "user_table.hpp"
#include "logger.hpp"

#include <json/json.h>

namespace gobang {

class GameController {
public:
    GameController()
        : room_mgr_(nullptr), online_mgr_(nullptr),
          conn_mgr_(nullptr), user_table_(nullptr),
          timeout_seconds_(60) {}

    ~GameController() {
        std::cerr << "[DEBUG] ~GameController: start" << std::endl;
        // 停止所有定时器
        std::unordered_map<std::string, std::shared_ptr<TimeoutInfo>> timers_copy;
        {
            std::lock_guard<std::mutex> lock(timers_mtx_);
            timers_copy.swap(timers_);
        }
        for (auto& pair : timers_copy) {
            auto& info = pair.second;
            std::lock_guard<std::mutex> ilock(info->mtx);
            info->cancelled = true;
            info->cv.notify_all();
        }
        for (auto& pair : timers_copy) {
            if (pair.second->thread.joinable()) {
                std::cerr << "[DEBUG] ~GameController: joining thread for room " << pair.first << std::endl;
                pair.second->thread.join();
                std::cerr << "[DEBUG] ~GameController: thread joined" << std::endl;
            }
        }
        std::cerr << "[DEBUG] ~GameController: done" << std::endl;
    }

    void init(RoomManager* room_mgr, OnlineManager* online_mgr,
              ConnectionManager* conn_mgr, UserTable* user_table) {
        std::cerr << "[DEBUG] init: start" << std::endl;
        // 停止之前的定时器
        std::unordered_map<std::string, std::shared_ptr<TimeoutInfo>> timers_copy;
        {
            std::lock_guard<std::mutex> lock(timers_mtx_);
            timers_copy.swap(timers_);
        }
        for (auto& pair : timers_copy) {
            auto& info = pair.second;
            std::lock_guard<std::mutex> ilock(info->mtx);
            info->cancelled = true;
            info->cv.notify_all();
        }
        for (auto& pair : timers_copy) {
            if (pair.second->thread.joinable()) {
                std::cerr << "[DEBUG] init: joining thread for room " << pair.first << std::endl;
                pair.second->thread.join();
                std::cerr << "[DEBUG] init: thread joined" << std::endl;
            }
        }

        room_mgr_ = room_mgr;
        online_mgr_ = online_mgr;
        conn_mgr_ = conn_mgr;
        user_table_ = user_table;
        std::cerr << "[DEBUG] init: done" << std::endl;
    }

    void set_timeout_seconds(int seconds) {
        timeout_seconds_ = seconds;
    }

    // 停止所有定时器
    void stop_all_timers() {
        std::cerr << "[DEBUG] stop_all_timers: start" << std::endl;
        std::unordered_map<std::string, std::shared_ptr<TimeoutInfo>> timers_copy;
        {
            std::lock_guard<std::mutex> lock(timers_mtx_);
            timers_copy.swap(timers_);
        }
        std::cerr << "[DEBUG] stop_all_timers: timers_copy size=" << timers_copy.size() << std::endl;
        for (auto& pair : timers_copy) {
            auto& info = pair.second;
            std::lock_guard<std::mutex> ilock(info->mtx);
            info->cancelled = true;
            info->cv.notify_all();
        }
        for (auto& pair : timers_copy) {
            if (pair.second->thread.joinable()) {
                std::cerr << "[DEBUG] stop_all_timers: joining thread for room " << pair.first << std::endl;
                pair.second->thread.join();
                std::cerr << "[DEBUG] stop_all_timers: thread joined" << std::endl;
            }
        }
        std::cerr << "[DEBUG] stop_all_timers: done" << std::endl;
    }

    // 处理游戏开始（匹配成功后调用）
    void handle_game_start(int64_t player1_id, int64_t player2_id) {
        // 创建房间
        std::string room_id = room_mgr_->create_room(player1_id, player2_id);
        if (room_id.empty()) {
            LOG_WARN("handle_game_start: 创建房间失败");
            return;
        }

        // 设置双方状态为 IN_ROOM
        online_mgr_->set_status(player1_id, OnlineStatus::IN_ROOM);
        online_mgr_->set_status(player2_id, OnlineStatus::IN_ROOM);

        // 获取对手信息
        Json::Value opponent1;
        opponent1["user_id"] = player2_id;
        Json::Value opponent2;
        opponent2["user_id"] = player1_id;

        // 发送给 player1（黑棋先手）
        Json::Value msg1;
        msg1["event"] = "game.start";
        msg1["data"]["room_id"] = room_id;
        msg1["data"]["opponent"] = opponent2;
        msg1["data"]["color"] = "black";
        msg1["data"]["your_turn"] = true;
        conn_mgr_->send(player1_id, msg1.toStyledString());

        // 发送给 player2（白棋后手）
        Json::Value msg2;
        msg2["event"] = "game.start";
        msg2["data"]["room_id"] = room_id;
        msg2["data"]["opponent"] = opponent1;
        msg2["data"]["color"] = "white";
        msg2["data"]["your_turn"] = false;
        conn_mgr_->send(player2_id, msg2.toStyledString());

        // 启动超时定时器
        start_timeout_timer(room_id);

        LOG_INFO("游戏开始: " << room_id << ", 玩家: " << player1_id << " vs " << player2_id);
    }

    // 处理落子事件
    void handle_move(int64_t user_id, int row, int col) {
        std::cerr << "[DEBUG] handle_move: user_id=" << user_id << std::endl;
        GameRoom* room = room_mgr_->get_room_by_user(user_id);
        if (!room) {
            std::cerr << "[DEBUG] handle_move: not in room, sending error" << std::endl;
            send_error(user_id, 4001, "not in room");
            std::cerr << "[DEBUG] handle_move: error sent, returning" << std::endl;
            return;
        }

        GameResult result = room->place_piece(user_id, row, col);
        if (result == GameResult::NONE) {
            // 检查是否真的落子成功（通过检查棋盘状态）
            if (room->get_board(row, col) == 0) {
                send_error(user_id, 4002, "invalid move");
                return;
            }

            // 落子成功，广播给双方
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

            // 重启超时定时器
            start_timeout_timer(room->get_room_id());
        } else if (result == GameResult::BLACK_WIN || result == GameResult::WHITE_WIN) {
            // 游戏结束
            int64_t winner_id, loser_id;
            get_winner_loser(room, result, winner_id, loser_id);
            on_game_over(room->get_room_id(), result, winner_id, loser_id);
        } else if (result == GameResult::DRAW) {
            // 平局（M4 暂不处理积分）
            int64_t winner_id = 0, loser_id = 0;
            on_game_over(room->get_room_id(), result, winner_id, loser_id);
        }
    }

    // 处理认输事件
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
        int64_t loser_id = user_id;
        on_game_over(room->get_room_id(), GameResult::GIVEUP, winner_id, loser_id);
    }

    // 处理断线
    void handle_disconnect(int64_t user_id) {
        GameRoom* room = room_mgr_->get_room_by_user(user_id);
        if (!room) return;

        // 断线不立即判负，等待重连
        LOG_INFO("玩家 " << user_id << " 断线，等待重连");
    }

    // 处理重连
    void handle_reconnect(int64_t user_id) {
        GameRoom* room = room_mgr_->get_room_by_user(user_id);
        if (!room) {
            send_error(user_id, 4004, "no room to reconnect");
            return;
        }

        // 发送完整棋盘状态
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

        conn_mgr_->send(user_id, msg.toStyledString());
        LOG_INFO("玩家 " << user_id << " 重连成功");
    }

private:
    // 游戏结束处理
    void on_game_over(const std::string& room_id, GameResult result,
                      int64_t winner_id, int64_t loser_id) {
        // 停止超时定时器
        stop_timeout_timer(room_id);

        GameRoom* room = room_mgr_->get_room(room_id);
        if (!room) return;

        // 更新积分
        if (winner_id != 0 && loser_id != 0 && user_table_) {
            user_table_->update_score_match(winner_id, loser_id);
        }

        // 确定结果字符串
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

        // 构造结束消息
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

        // 广播给双方
        broadcast_to_room(room_id, msg.toStyledString());

        // 设置双方状态为 HALL_IDLE
        int64_t p1, p2;
        room->get_player_ids(p1, p2);
        online_mgr_->set_status(p1, OnlineStatus::HALL_IDLE);
        online_mgr_->set_status(p2, OnlineStatus::HALL_IDLE);

        // 销毁房间
        room_mgr_->destroy_room(room_id);

        LOG_INFO("游戏结束: " << room_id << ", 结果: " << result_str);
    }

    // 发送消息给房间内所有玩家
    void broadcast_to_room(const std::string& room_id, const std::string& msg) {
        GameRoom* room = room_mgr_->get_room(room_id);
        if (!room) return;

        int64_t p1, p2;
        room->get_player_ids(p1, p2);
        conn_mgr_->send(p1, msg);
        conn_mgr_->send(p2, msg);
    }

    // 启动超时检测定时器
    void start_timeout_timer(const std::string& room_id) {
        stop_timeout_timer(room_id);

        auto info = std::make_shared<TimeoutInfo>();
        info->cancelled = false;

        info->thread = std::thread([this, room_id, info]() {
            std::unique_lock<std::mutex> lock(info->mtx);
            info->cv.wait_for(lock, std::chrono::seconds(timeout_seconds_),
                              [&info]() { return info->cancelled; });
            if (!info->cancelled) {
                // 超时处理
                GameRoom* room = room_mgr_->get_room(room_id);
                if (room && room->get_status() == RoomStatus::PLAYING) {
                    int64_t current = room->get_current_turn();
                    int64_t opponent = room->get_opponent_id(current);
                    on_game_over(room_id, GameResult::TIMEOUT, opponent, current);
                }
            }
        });

        std::lock_guard<std::mutex> lock(timers_mtx_);
        timers_[room_id] = info;
    }

    // 停止超时定时器
    void stop_timeout_timer(const std::string& room_id) {
        std::shared_ptr<TimeoutInfo> info;
        {
            std::lock_guard<std::mutex> lock(timers_mtx_);
            auto it = timers_.find(room_id);
            if (it == timers_.end()) return;
            info = it->second;
            timers_.erase(it);
        }

        {
            std::lock_guard<std::mutex> lock(info->mtx);
            info->cancelled = true;
            info->cv.notify_all();
        }
        if (info->thread.joinable()) {
            info->thread.join();
        }
    }

    // 获取 GameResult 对应的获胜者和失败者
    void get_winner_loser(GameRoom* room, GameResult result,
                          int64_t& winner_id, int64_t& loser_id) {
        int64_t p1, p2;
        room->get_player_ids(p1, p2);

        // p1 是黑棋，p2 是白棋
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

    // 发送错误消息
    void send_error(int64_t user_id, int code, const std::string& message) {
        std::cerr << "[DEBUG] send_error: user_id=" << user_id << ", code=" << code << std::endl;
        Json::Value msg;
        msg["event"] = "error";
        msg["data"]["code"] = code;
        msg["data"]["message"] = message;
        std::cerr << "[DEBUG] send_error: calling conn_mgr_->send" << std::endl;
        conn_mgr_->send(user_id, msg.toStyledString());
        std::cerr << "[DEBUG] send_error: done" << std::endl;
    }

    RoomManager*       room_mgr_;
    OnlineManager*     online_mgr_;
    ConnectionManager* conn_mgr_;
    UserTable*         user_table_;
    int                timeout_seconds_;

    // 超时定时器
    struct TimeoutInfo {
        std::thread              thread;
        std::condition_variable  cv;
        std::mutex               mtx;
        bool                     cancelled;
    };
    std::unordered_map<std::string, std::shared_ptr<TimeoutInfo>> timers_;
    std::mutex timers_mtx_;
};

} // namespace gobang
