#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "connection_manager.hpp"
#include "game.hpp"
#include "logger.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "room.hpp"
#include "security.hpp"
#include "util.hpp"
#include "websocket_handler.hpp"

using websocketpp::connection_hdl;

namespace {

typedef websocketpp::client<websocketpp::config::asio_client> TestClientImpl;

std::string trim_query_and_fragment(const std::string& resource) {
    size_t end = resource.find_first_of("?#");
    return resource.substr(0, end);
}

bool is_allowed_websocket_resource(const std::string& resource) {
    std::string path = trim_query_and_fragment(resource);
    return path == "/ws" || path == "/ws/";
}

std::string json_compact(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    builder["emitUTF8"] = true;
    return Json::writeString(builder, value);
}

class TestWebSocketClient {
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

        _client.set_fail_handler([this](connection_hdl hdl) {
            std::string reason = "connection failed";
            try {
                reason = _client.get_con_from_hdl(hdl)->get_ec().message();
            } catch (...) {
            }
            {
                std::lock_guard<std::mutex> lock(_mtx);
                _failed = true;
                _last_error = reason;
            }
            _cv.notify_all();
        });

        _client.set_close_handler([this](connection_hdl hdl) {
            (void)hdl;
            {
                std::lock_guard<std::mutex> lock(_mtx);
                _closed = true;
            }
            _cv.notify_all();
        });

        _client.set_message_handler(
            [this](connection_hdl, TestClientImpl::message_ptr msg) {
                Json::Value root;
                std::string errmsg;
                if (!gobang::util::str_to_json(msg->get_payload(), root, errmsg)) {
                    return;
                }
                {
                    std::lock_guard<std::mutex> lock(_mtx);
                    _messages.push_back(root);
                }
                _cv.notify_all();
            });

        websocketpp::lib::error_code ec;
        TestClientImpl::connection_ptr con = _client.get_connection(uri, ec);
        if (ec) {
            _last_error = ec.message();
            return false;
        }

        _client.connect(con);
        _thread = std::thread([this]() { _client.run(); });

        std::unique_lock<std::mutex> lock(_mtx);
        const bool ready = _cv.wait_for(
            lock, std::chrono::milliseconds(timeout_ms),
            [this]() { return _opened || _failed; });
        return ready && _opened && !_failed;
    }

    void close() {
        websocketpp::lib::error_code ec;
        if (_opened && !_closed) {
            _client.close(_hdl, websocketpp::close::status::normal, "test done", ec);
        }
        _client.stop_perpetual();
        if (_thread.joinable()) {
            _thread.join();
        }
    }

    bool send_event(const std::string& event, const Json::Value& data) {
        Json::Value root;
        root["event"] = event;
        root["data"] = data;

        websocketpp::lib::error_code ec;
        _client.send(_hdl, json_compact(root), websocketpp::frame::opcode::text, ec);
        if (ec) {
            std::lock_guard<std::mutex> lock(_mtx);
            _last_error = ec.message();
            return false;
        }
        return true;
    }

    Json::Value wait_for_event(const std::string& event, int timeout_ms = 3000) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

        std::unique_lock<std::mutex> lock(_mtx);
        while (std::chrono::steady_clock::now() < deadline) {
            for (size_t i = 0; i < _messages.size(); ++i) {
                if (_messages[i]["event"].asString() == event) {
                    Json::Value message = _messages[i];
                    _messages.erase(_messages.begin() + static_cast<long long>(i));
                    return message;
                }
            }
            if (_failed || _closed) {
                break;
            }
            _cv.wait_until(lock, deadline);
        }
        return Json::Value();
    }

    void drain_events() {
        std::lock_guard<std::mutex> lock(_mtx);
        _messages.clear();
    }

    std::string last_error() const {
        std::lock_guard<std::mutex> lock(_mtx);
        return _last_error;
    }

private:
    TestClientImpl _client;
    std::thread _thread;
    connection_hdl _hdl;

    mutable std::mutex _mtx;
    std::condition_variable _cv;
    std::vector<Json::Value> _messages;
    std::string _last_error;
    bool _opened = false;
    bool _failed = false;
    bool _closed = false;
};

class LocalWebSocketGameServer {
public:
    static const uint16_t kPort = 18081;

    void start() {
        // Logger::init 内部使用线程，重复 init 会触发 std::terminate（thread::operator=）
        static std::once_flag logger_once;
        std::call_once(logger_once, []() {
            gobang::Logger::instance().init("test_websocket_game.log");
        });

        // 每次 start 都创建全新 server，避免 websocketpp server 复用导致 terminate
        server_.reset(new gobang::WebsocketServer());

        conn_mgr_.init(server_.get());
        matcher_.init(&online_mgr_);
        matcher_.start();

        game_ctrl_ = gobang::GameController::create();
        game_ctrl_->init(&room_mgr_, &online_mgr_, &conn_mgr_, nullptr);
        game_ctrl_->set_timeout_seconds(2);

        ws_handler_.init(&conn_mgr_, &online_mgr_, &matcher_, server_.get(),
                         game_ctrl_.get());

        server_->clear_access_channels(websocketpp::log::alevel::all);
        server_->clear_error_channels(websocketpp::log::elevel::all);
        server_->init_asio();
        server_->set_reuse_addr(true);

        server_->set_validate_handler([this](connection_hdl hdl) {
            auto con = server_->get_con_from_hdl(hdl);
            return is_allowed_websocket_resource(con->get_resource());
        });

        server_->set_http_handler([this](connection_hdl hdl) {
            auto con = server_->get_con_from_hdl(hdl);
            con->set_body("WebSocket game test server");
            con->set_status(websocketpp::http::status_code::ok);
        });

        server_->set_open_handler([this](connection_hdl hdl) {
            ws_handler_.on_open(hdl);
        });

        server_->set_close_handler([this](connection_hdl hdl) {
            ws_handler_.on_close(hdl);
        });

        server_->set_message_handler(
            [this](connection_hdl hdl, gobang::WebsocketServer::message_ptr msg) {
                ws_handler_.on_message(hdl, msg->get_payload());
            });

        websocketpp::lib::asio::ip::tcp::endpoint endpoint(
            websocketpp::lib::asio::ip::address_v4::loopback(), kPort);
        server_->listen(endpoint);
        server_->start_accept();

        _thread = std::thread([this]() { server_->run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void stop() {
        if (game_ctrl_) {
            game_ctrl_->stop_all_timers();
        }
        websocketpp::lib::error_code ec;
        if (server_) {
            server_->stop_listening(ec);
            server_->stop();
        }
        if (_thread.joinable()) {
            _thread.join();
        }
        matcher_.stop();

        // 清理各组件，确保下次 start 是全新状态
        game_ctrl_.reset();
        server_.reset();
    }

    std::string uri() const {
        std::ostringstream oss;
        oss << "ws://127.0.0.1:" << kPort << "/ws";
        return oss.str();
    }

    gobang::RoomManager& room_mgr() { return room_mgr_; }
    gobang::WebSocketHandler& ws_handler() { return ws_handler_; }

private:
    std::thread _thread;
    std::unique_ptr<gobang::WebsocketServer> server_;
    gobang::ConnectionManager conn_mgr_;
    gobang::OnlineManager online_mgr_;
    gobang::Matcher matcher_;
    gobang::RoomManager room_mgr_;
    std::shared_ptr<gobang::GameController> game_ctrl_;
    gobang::WebSocketHandler ws_handler_;
};

std::string make_token(int64_t user_id) {
    return gobang::security::jwt_generate(user_id, 3600);
}

void auth_client(TestWebSocketClient& client, int64_t user_id) {
    const std::string token = make_token(user_id);
    Json::Value data;
    data["token"] = token;
    ASSERT_TRUE(client.send_event("auth", data)) << client.last_error();
    Json::Value resp = client.wait_for_event("auth.success");
    ASSERT_FALSE(resp.isNull()) << client.last_error();
    ASSERT_EQ(resp["data"]["user_id"].asInt64(), user_id);
}

struct MatchedClients {
    TestWebSocketClient* black_client;
    TestWebSocketClient* white_client;
    int64_t black_id;
    int64_t white_id;
};

void match_two_players(TestWebSocketClient& c1, TestWebSocketClient& c2,
                       int64_t id1, int64_t id2,
                       MatchedClients* out) {
    ASSERT_NE(out, nullptr);
    auth_client(c1, id1);
    auth_client(c2, id2);

    Json::Value start1;
    start1["token"] = make_token(id1);
    start1["score"] = 1200;
    Json::Value start2;
    start2["token"] = make_token(id2);
    start2["score"] = 1250;

    ASSERT_TRUE(c1.send_event("match.start", start1)) << c1.last_error();
    ASSERT_TRUE(c2.send_event("match.start", start2)) << c2.last_error();

    Json::Value m1 = c1.wait_for_event("match.success", 5000);
    Json::Value m2 = c2.wait_for_event("match.success", 5000);
    ASSERT_FALSE(m1.isNull()) << c1.last_error();
    ASSERT_FALSE(m2.isNull()) << c2.last_error();

    Json::Value g1 = c1.wait_for_event("game.start", 5000);
    Json::Value g2 = c2.wait_for_event("game.start", 5000);
    ASSERT_FALSE(g1.isNull()) << c1.last_error();
    ASSERT_FALSE(g2.isNull()) << c2.last_error();

    if (g1["data"]["color"].asString() == "black") {
        out->black_client = &c1;
        out->white_client = &c2;
        out->black_id = id1;
        out->white_id = id2;
    } else {
        out->black_client = &c2;
        out->white_client = &c1;
        out->black_id = id2;
        out->white_id = id1;
    }
}

bool send_move(TestWebSocketClient& client, int64_t user_id, int row, int col) {
    Json::Value data;
    data["token"] = make_token(user_id);
    data["row"] = row;
    data["col"] = col;
    return client.send_event("game.move", data);
}

class WebSocketGameTest : public ::testing::Test {
protected:
    void SetUp() override {
        _server.start();
    }

    void TearDown() override {
        _client1.close();
        _client2.close();
        _server.stop();
    }

    LocalWebSocketGameServer _server;
    TestWebSocketClient _client1;
    TestWebSocketClient _client2;

    void connect_clients() {
        ASSERT_TRUE(_client1.connect(_server.uri())) << _client1.last_error();
        ASSERT_TRUE(_client2.connect(_server.uri())) << _client2.last_error();
    }
};

TEST_F(WebSocketGameTest, MatchSuccessCreatesRoom) {
    connect_clients();
    MatchedClients matched{};
    match_two_players(_client1, _client2, 5001, 5002, &matched);

    EXPECT_GT(_server.room_mgr().room_count(), 0u);

    gobang::GameRoom* room = _server.room_mgr().get_room_by_user(matched.black_id);
    ASSERT_NE(room, nullptr);
    EXPECT_TRUE(room->has_player(matched.black_id));
    EXPECT_TRUE(room->has_player(matched.white_id));
}

TEST_F(WebSocketGameTest, GameMoveValid) {
    connect_clients();
    MatchedClients m{};
    match_two_players(_client1, _client2, 5101, 5102, &m);
    m.black_client->drain_events();
    m.white_client->drain_events();

    ASSERT_TRUE(send_move(*m.black_client, m.black_id, 7, 7));

    Json::Value move_black = m.black_client->wait_for_event("game.move");
    Json::Value move_white = m.white_client->wait_for_event("game.move");
    ASSERT_FALSE(move_black.isNull());
    ASSERT_FALSE(move_white.isNull());
    EXPECT_EQ(move_black["data"]["row"].asInt(), 7);
    EXPECT_EQ(move_black["data"]["col"].asInt(), 7);
    EXPECT_EQ(move_black["data"]["color"].asString(), "black");
}

TEST_F(WebSocketGameTest, GameMoveNotYourTurn) {
    connect_clients();
    MatchedClients m{};
    match_two_players(_client1, _client2, 5201, 5202, &m);
    m.black_client->drain_events();
    m.white_client->drain_events();

    ASSERT_TRUE(send_move(*m.white_client, m.white_id, 3, 3));

    Json::Value err = m.white_client->wait_for_event("error");
    ASSERT_FALSE(err.isNull());
    EXPECT_EQ(err["data"]["code"].asInt(), 4002);
}

TEST_F(WebSocketGameTest, GameMoveWin) {
    connect_clients();
    MatchedClients m{};
    match_two_players(_client1, _client2, 5301, 5302, &m);
    m.black_client->drain_events();
    m.white_client->drain_events();

    const int black_row = 7;
    const int black_cols[] = {3, 4, 5, 6, 7};
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(send_move(*m.black_client, m.black_id, black_row, black_cols[i]));
        Json::Value move_ev = m.black_client->wait_for_event("game.move");
        ASSERT_FALSE(move_ev.isNull()) << "move " << i;

        if (i < 4) {
            ASSERT_TRUE(send_move(*m.white_client, m.white_id, i, i));
            Json::Value white_move = m.white_client->wait_for_event("game.move");
            ASSERT_FALSE(white_move.isNull());
            m.black_client->wait_for_event("game.move");
            m.white_client->drain_events();
        }
    }

    Json::Value over1 = m.black_client->wait_for_event("game.over", 5000);
    Json::Value over2 = m.white_client->wait_for_event("game.over", 5000);
    ASSERT_FALSE(over1.isNull());
    ASSERT_FALSE(over2.isNull());
    EXPECT_TRUE(over1["data"]["result"].asString().find("win") != std::string::npos);
}

TEST_F(WebSocketGameTest, GameGiveup) {
    connect_clients();
    MatchedClients m{};
    match_two_players(_client1, _client2, 5401, 5402, &m);
    m.black_client->drain_events();
    m.white_client->drain_events();

    Json::Value giveup_data;
    giveup_data["token"] = make_token(m.black_id);
    ASSERT_TRUE(m.black_client->send_event("game.giveup", giveup_data));

    Json::Value over1 = m.black_client->wait_for_event("game.over", 5000);
    Json::Value over2 = m.white_client->wait_for_event("game.over", 5000);
    ASSERT_FALSE(over1.isNull());
    ASSERT_FALSE(over2.isNull());
    EXPECT_EQ(over1["data"]["reason"].asString(), "giveup");
}

TEST_F(WebSocketGameTest, GameReconnect) {
    connect_clients();
    MatchedClients m{};
    match_two_players(_client1, _client2, 5501, 5502, &m);
    ASSERT_TRUE(send_move(*m.black_client, m.black_id, 8, 8));
    m.black_client->wait_for_event("game.move");
    m.white_client->wait_for_event("game.move");

    const int64_t reconnect_id = m.white_id;
    TestWebSocketClient* old_client = m.white_client;
    old_client->close();

    TestWebSocketClient new_client;
    ASSERT_TRUE(new_client.connect(_server.uri())) << new_client.last_error();

    auth_client(new_client, reconnect_id);
    Json::Value reconnect_data;
    reconnect_data["token"] = make_token(reconnect_id);
    ASSERT_TRUE(new_client.send_event("game.reconnect", reconnect_data));

    Json::Value re = new_client.wait_for_event("game.reconnect", 5000);
    ASSERT_FALSE(re.isNull()) << new_client.last_error();
    EXPECT_FALSE(re["data"]["room_id"].asString().empty());
    EXPECT_TRUE(re["data"].isMember("board"));

    new_client.close();
}

TEST_F(WebSocketGameTest, GameTimeout) {
    connect_clients();
    MatchedClients m{};
    match_two_players(_client1, _client2, 5601, 5602, &m);
    m.black_client->drain_events();
    m.white_client->drain_events();

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    _server.ws_handler().process_timers();

    Json::Value over1 = m.black_client->wait_for_event("game.over", 5000);
    Json::Value over2 = m.white_client->wait_for_event("game.over", 5000);
    ASSERT_FALSE(over1.isNull()) << m.black_client->last_error();
    ASSERT_FALSE(over2.isNull()) << m.white_client->last_error();
    EXPECT_EQ(over1["data"]["reason"].asString(), "timeout");
}

TEST_F(WebSocketGameTest, ConcurrentMoves) {
    connect_clients();
    MatchedClients m{};
    match_two_players(_client1, _client2, 5701, 5702, &m);
    m.black_client->drain_events();
    m.white_client->drain_events();

    ASSERT_TRUE(send_move(*m.black_client, m.black_id, 4, 4));
    ASSERT_TRUE(send_move(*m.black_client, m.black_id, 5, 5));

    Json::Value first = m.black_client->wait_for_event("game.move", 3000);
    Json::Value err = m.black_client->wait_for_event("error", 3000);

    EXPECT_FALSE(first.isNull());
    EXPECT_FALSE(err.isNull());
    EXPECT_EQ(err["data"]["code"].asInt(), 4002);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
