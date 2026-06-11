#include "game.hpp"

namespace gobang {

std::shared_ptr<GameController> GameController::create() {
    return std::shared_ptr<GameController>(new GameController());
}

GameController::GameController()
    : room_mgr_(nullptr),
      online_mgr_(nullptr),
      msg_sender_(nullptr),
      user_table_(nullptr),
      io_service_(nullptr),
      timeout_seconds_(60),
      disconnect_timeout_seconds_(60) {}

GameController::~GameController() {
    cleanup_all_timers();
}

void GameController::init(RoomManager* room_mgr, OnlineManager* online_mgr,
                          IMessageSender* sender, UserTable* user_table,
                          IoService* io_service) {
    cleanup_all_timers();
    room_mgr_ = room_mgr;
    online_mgr_ = online_mgr;
    msg_sender_ = sender;
    user_table_ = user_table;
    io_service_ = io_service;
}

void GameController::set_timeout_seconds(int seconds) {
    timeout_seconds_ = seconds > 0 ? seconds : 1;
}

void GameController::set_disconnect_timeout_seconds(int seconds) {
    disconnect_timeout_seconds_ = seconds > 0 ? seconds : 1;
}

bool GameController::has_active_room(int64_t user_id) {
    GameRoom* room = room_mgr_->get_room_by_user(user_id);
    return room != nullptr && room->get_status() == RoomStatus::PLAYING;
}

GameRoom* GameController::get_room_by_user(int64_t user_id) {
    return room_mgr_->get_room_by_user(user_id);
}

void GameController::cleanup_finished_room(int64_t user_id) {
    GameRoom* room = room_mgr_->get_room_by_user(user_id);
    if (!room) return;
    if (room->get_status() != RoomStatus::FINISHED) return;

    int64_t p1, p2;
    room->get_player_ids(p1, p2);
    if (!online_mgr_->is_online(p1) && !online_mgr_->is_online(p2)) {
        room_mgr_->destroy_room(room->get_room_id());
    }
}

void GameController::stop_all_timers() {
    cleanup_all_timers();
}

std::string GameController::handle_game_start(int64_t player1_id, int64_t player2_id) {
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

    if (msg_sender_) {
        msg_sender_->send(player1_id, msg1.toStyledString());
        msg_sender_->send(player2_id, msg2.toStyledString());
    }

    start_timeout_timer(room_id);

    LOG_INFO("游戏开始: " << room_id << ", 玩家: " << player1_id << " vs " << player2_id);
    return room_id;
}

void GameController::handle_move(int64_t user_id, int row, int col) {
    LOG_INFO("handle_move: user_id=" << user_id << ", row=" << row << ", col=" << col);

    GameRoom* room = room_mgr_->get_room_by_user(user_id);
    if (!room) {
        LOG_WARN("handle_move: user_id=" << user_id << " 不在任何房间中");
        send_error(user_id, 4001, "not in room");
        return;
    }

    GameResult result = room->place_piece(user_id, row, col);
    if (result == GameResult::NONE) {
        send_error(user_id, 4002, "invalid move");
        return;
    }

    if (result == GameResult::MOVE_SUCCESS) {
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

void GameController::handle_giveup(int64_t user_id) {
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

void GameController::handle_disconnect(int64_t user_id) {
    GameRoom* room = room_mgr_->get_room_by_user(user_id);
    if (!room) return;
    if (room->is_disconnected()) return;

    std::string room_id = room->get_room_id();
    room->mark_disconnected(user_id);
    stop_timeout_timer(room_id);
    start_disconnect_timer(room_id);

    int64_t opponent_id = room->get_opponent_id(user_id);
    if (opponent_id > 0) {
        Json::Value msg;
        msg["event"] = "opponent.disconnected";
        msg["data"]["timeout_seconds"] = disconnect_timeout_seconds_;
        if (msg_sender_) {
            msg_sender_->send(opponent_id, msg.toStyledString());
        }
    }

    LOG_INFO("玩家 " << user_id << " 断线，等待重连（超时 "
             << disconnect_timeout_seconds_ << " 秒）");
}

void GameController::handle_reconnect(int64_t user_id) {
    GameRoom* room = room_mgr_->get_room_by_user(user_id);
    if (!room) {
        send_error(user_id, 4004, "no room to reconnect");
        return;
    }

    std::string room_id = room->get_room_id();

    if (room->is_disconnected() && room->get_disconnected_user_id() == user_id) {
        room->clear_disconnected();
        stop_disconnect_timer(room_id);

        int64_t opponent_id = room->get_opponent_id(user_id);
        if (opponent_id > 0 && msg_sender_) {
            Json::Value opp_msg;
            opp_msg["event"] = "opponent.reconnected";
            msg_sender_->send(opponent_id, opp_msg.toStyledString());
        }

        start_timeout_timer(room_id);
    }

    send_reconnect_state(user_id, room);
    LOG_INFO("玩家 " << user_id << " 重连成功");
}

void GameController::handle_reconnect_accept(int64_t user_id) {
    GameRoom* room = room_mgr_->get_room_by_user(user_id);
    if (!room) {
        send_error(user_id, 4004, "no room to reconnect");
        return;
    }

    std::string room_id = room->get_room_id();

    if (room->is_disconnected() && room->get_disconnected_user_id() == user_id) {
        room->clear_disconnected();
        stop_disconnect_timer(room_id);

        int64_t opponent_id = room->get_opponent_id(user_id);
        if (opponent_id > 0 && msg_sender_) {
            Json::Value opp_msg;
            opp_msg["event"] = "opponent.reconnected";
            msg_sender_->send(opponent_id, opp_msg.toStyledString());
        }

        start_timeout_timer(room_id);
    }

    online_mgr_->set_status(user_id, OnlineStatus::IN_ROOM);

    Json::Value resp;
    resp["event"] = "reconnect.accepted";
    resp["data"]["room_id"] = room_id;
    PieceColor color;
    room->get_player_color(user_id, color);
    resp["data"]["color"] = (color == PieceColor::BLACK) ? "black" : "white";
    if (msg_sender_) {
        msg_sender_->send(user_id, resp.toStyledString());
    }

    LOG_INFO("玩家 " << user_id << " 接受重连，房间 " << room_id);
}

void GameController::handle_reconnect_reject(int64_t user_id) {
    GameRoom* room = room_mgr_->get_room_by_user(user_id);
    if (!room) {
        send_error(user_id, 4004, "no room to reconnect");
        return;
    }

    std::string room_id = room->get_room_id();
    stop_disconnect_timer(room_id);
    room->clear_disconnected();

    int64_t winner_id = room->get_opponent_id(user_id);
    on_game_over(room_id, GameResult::GIVEUP, winner_id, user_id);

    LOG_INFO("玩家 " << user_id << " 拒绝重连，判负");
}

void GameController::on_game_over(const std::string& room_id, GameResult result,
                                  int64_t winner_id, int64_t loser_id) {
    stop_timeout_timer(room_id);
    stop_disconnect_timer(room_id);

    GameRoom* room = room_mgr_->get_room(room_id);
    if (!room) return;

    room->set_game_over(result);

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
                (room->get_player_color(winner_id, winner_color) &&
                 winner_color == PieceColor::BLACK ? "black_win" : "white_win") : "timeout";
            reason = "timeout";
            break;
        }
        case GameResult::GIVEUP: {
            PieceColor winner_color;
            result_str = (winner_id == room->get_opponent_id(loser_id)) ?
                (room->get_player_color(winner_id, winner_color) &&
                 winner_color == PieceColor::BLACK ? "black_win" : "white_win") : "giveup";
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

    if (winner_id != 0 && loser_id != 0) {
        Json::Value winner_msg = msg;
        winner_msg["data"]["score_change"] = 25;
        Json::Value loser_msg = msg;
        loser_msg["data"]["score_change"] = -15;

        if (msg_sender_) {
            msg_sender_->send(winner_id, winner_msg.toStyledString());
            msg_sender_->send(loser_id, loser_msg.toStyledString());
        }
    } else {
        msg["data"]["score_change"] = 0;
        broadcast_to_room(room_id, msg.toStyledString());
    }

    int64_t p1, p2;
    room->get_player_ids(p1, p2);
    online_mgr_->set_status(p1, OnlineStatus::HALL_IDLE);
    online_mgr_->set_status(p2, OnlineStatus::HALL_IDLE);

    LOG_INFO("游戏结束: " << room_id << ", 结果: " << result_str);
    room_mgr_->destroy_room(room_id);
}

void GameController::broadcast_to_room(const std::string& room_id, const std::string& msg) {
    if (!msg_sender_) return;
    GameRoom* room = room_mgr_->get_room(room_id);
    if (!room) return;

    int64_t p1, p2;
    room->get_player_ids(p1, p2);
    msg_sender_->broadcast(p1, p2, msg);
}

void GameController::cancel_timer_in_map(
    std::unordered_map<std::string, std::shared_ptr<SteadyTimer>>& timers,
    const std::string& room_id) {
    auto it = timers.find(room_id);
    if (it != timers.end()) {
        AsioErrorCode ec;
        it->second->cancel(ec);
        timers.erase(it);
    }
}

void GameController::start_timeout_timer(const std::string& room_id) {
    if (!io_service_) return;

    std::shared_ptr<GameController> self = shared_from_this();
    std::shared_ptr<SteadyTimer> timer;

    {
        std::lock_guard<std::mutex> lock(timers_mtx_);
        cancel_timer_in_map(turn_timers_, room_id);
        timer = std::make_shared<SteadyTimer>(*io_service_);
        turn_timers_[room_id] = timer;
    }

    timer->expires_from_now(std::chrono::seconds(timeout_seconds_));
    timer->async_wait([self, room_id, timer](const AsioErrorCode& ec) {
        if (ec) return;
        self->on_turn_timeout(room_id);
    });
}

void GameController::stop_timeout_timer(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(timers_mtx_);
    cancel_timer_in_map(turn_timers_, room_id);
}

void GameController::start_disconnect_timer(const std::string& room_id) {
    if (!io_service_) return;

    std::shared_ptr<GameController> self = shared_from_this();
    std::shared_ptr<SteadyTimer> timer;

    {
        std::lock_guard<std::mutex> lock(timers_mtx_);
        cancel_timer_in_map(disconnect_timers_, room_id);
        timer = std::make_shared<SteadyTimer>(*io_service_);
        disconnect_timers_[room_id] = timer;
    }

    timer->expires_from_now(std::chrono::seconds(disconnect_timeout_seconds_));
    timer->async_wait([self, room_id, timer](const AsioErrorCode& ec) {
        if (ec) return;
        self->on_disconnect_timeout(room_id);
    });
}

void GameController::stop_disconnect_timer(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(timers_mtx_);
    cancel_timer_in_map(disconnect_timers_, room_id);
}

void GameController::cleanup_all_timers() {
    std::lock_guard<std::mutex> lock(timers_mtx_);
    for (auto& pair : turn_timers_) {
        AsioErrorCode ec;
        pair.second->cancel(ec);
    }
    for (auto& pair : disconnect_timers_) {
        AsioErrorCode ec;
        pair.second->cancel(ec);
    }
    turn_timers_.clear();
    disconnect_timers_.clear();
}

void GameController::on_turn_timeout(const std::string& room_id) {
    {
        std::lock_guard<std::mutex> lock(timers_mtx_);
        turn_timers_.erase(room_id);
    }

    GameRoom* room = room_mgr_->get_room(room_id);
    if (room && room->get_status() == RoomStatus::PLAYING && !room->is_disconnected()) {
        int64_t current = room->get_current_turn();
        int64_t opponent = room->get_opponent_id(current);
        on_game_over(room_id, GameResult::TIMEOUT, opponent, current);
    }
}

void GameController::on_disconnect_timeout(const std::string& room_id) {
    {
        std::lock_guard<std::mutex> lock(timers_mtx_);
        disconnect_timers_.erase(room_id);
    }

    GameRoom* room = room_mgr_->get_room(room_id);
    if (room && room->get_status() == RoomStatus::PLAYING && room->is_disconnected()) {
        int64_t disconnected_uid = room->get_disconnected_user_id();
        int64_t winner_id = room->get_opponent_id(disconnected_uid);
        room->clear_disconnected();
        on_game_over(room_id, GameResult::TIMEOUT, winner_id, disconnected_uid);
        LOG_INFO("断线超时: 玩家 " << disconnected_uid << " 判负，房间 " << room_id);
    }
}

void GameController::get_winner_loser(GameRoom* room, GameResult result,
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

void GameController::send_error(int64_t user_id, int code, const std::string& message) {
    if (!msg_sender_) return;
    Json::Value msg;
    msg["event"] = "error";
    msg["data"]["code"] = code;
    msg["data"]["message"] = message;
    msg_sender_->send(user_id, msg.toStyledString());
}

void GameController::send_reconnect_state(int64_t user_id, GameRoom* room) {
    if (!msg_sender_ || !room) return;

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

    msg_sender_->send(user_id, msg.toStyledString());
}

} // namespace gobang
