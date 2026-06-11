#pragma once

#include <cstdint>
#include <string>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <memory>

#include "room.hpp"
#include "online.hpp"
#include "message_sender.hpp"
#include "user_table.hpp"
#include "asio_timer_types.hpp"
#include "logger.hpp"

#include <json/json.h>

namespace gobang {

class GameController : public std::enable_shared_from_this<GameController> {
public:
    static std::shared_ptr<GameController> create();

    ~GameController();

    void init(RoomManager* room_mgr, OnlineManager* online_mgr,
              IMessageSender* sender, UserTable* user_table,
              IoService* io_service = nullptr);

    void set_timeout_seconds(int seconds);
    void set_disconnect_timeout_seconds(int seconds);

    bool has_active_room(int64_t user_id);
    GameRoom* get_room_by_user(int64_t user_id);
    void cleanup_finished_room(int64_t user_id);
    void stop_all_timers();

    std::string handle_game_start(int64_t player1_id, int64_t player2_id);
    void handle_move(int64_t user_id, int row, int col);
    void handle_giveup(int64_t user_id);
    void handle_disconnect(int64_t user_id);
    void handle_reconnect(int64_t user_id);
    void handle_reconnect_accept(int64_t user_id);
    void handle_reconnect_reject(int64_t user_id);

private:
    GameController();

    void on_game_over(const std::string& room_id, GameResult result,
                      int64_t winner_id, int64_t loser_id);
    void broadcast_to_room(const std::string& room_id, const std::string& msg);

    void start_timeout_timer(const std::string& room_id);
    void stop_timeout_timer(const std::string& room_id);
    void start_disconnect_timer(const std::string& room_id);
    void stop_disconnect_timer(const std::string& room_id);
    void cleanup_all_timers();

    void cancel_timer_in_map(
        std::unordered_map<std::string, std::shared_ptr<SteadyTimer>>& timers,
        const std::string& room_id);

    void on_turn_timeout(const std::string& room_id);
    void on_disconnect_timeout(const std::string& room_id);

    void get_winner_loser(GameRoom* room, GameResult result,
                          int64_t& winner_id, int64_t& loser_id);
    void send_error(int64_t user_id, int code, const std::string& message);
    void send_reconnect_state(int64_t user_id, GameRoom* room);

    RoomManager* room_mgr_;
    OnlineManager* online_mgr_;
    IMessageSender* msg_sender_;
    UserTable* user_table_;
    IoService* io_service_;
    int timeout_seconds_;
    int disconnect_timeout_seconds_;

    std::unordered_map<std::string, std::shared_ptr<SteadyTimer>> turn_timers_;
    std::unordered_map<std::string, std::shared_ptr<SteadyTimer>> disconnect_timers_;
    std::mutex timers_mtx_;
};

} // namespace gobang
