#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "connection_manager.hpp"
#include "logger.hpp"
#include "matcher.hpp"
#include "online.hpp"
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

gobang::WebsocketServer g_server;
gobang::ConnectionManager g_conn_mgr;
gobang::OnlineManager g_online_mgr;
gobang::Matcher g_matcher;
gobang::WebSocketHandler g_ws_handler;

class LocalWebSocketServer {
public:
    static const uint16_t kPort = 18080;

    void start() {
        gobang::Logger::instance().init("test_websocket_m3_flow.log");

        g_conn_mgr.init(&g_server);
        g_matcher.init(&g_online_mgr);
        g_matcher.start();
        g_ws_handler.init(&g_conn_mgr, &g_online_mgr, &g_matcher, &g_server, nullptr);

        g_server.clear_access_channels(websocketpp::log::alevel::all);
        g_server.clear_error_channels(websocketpp::log::elevel::all);
        g_server.init_asio();
        g_server.set_reuse_addr(true);

        g_server.set_validate_handler([](connection_hdl hdl) {
            auto con = g_server.get_con_from_hdl(hdl);
            return is_allowed_websocket_resource(con->get_resource());
        });

        g_server.set_http_handler([](connection_hdl hdl) {
            auto con = g_server.get_con_from_hdl(hdl);
            con->set_body("WebSocket test server is running. Use /ws endpoint.");
            con->set_status(websocketpp::http::status_code::ok);
        });

        g_server.set_open_handler([](connection_hdl hdl) {
            g_ws_handler.on_open(hdl);
        });

        g_server.set_close_handler([](connection_hdl hdl) {
            g_ws_handler.on_close(hdl);
        });

        g_server.set_message_handler(
            [](connection_hdl hdl, gobang::WebsocketServer::message_ptr msg) {
                g_ws_handler.on_message(hdl, msg->get_payload());
            });

        websocketpp::lib::asio::ip::tcp::endpoint endpoint(
            websocketpp::lib::asio::ip::address_v4::loopback(), kPort);
        g_server.listen(endpoint);
        g_server.start_accept();

        _thread = std::thread([]() {
            g_server.run();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void stop() {
        websocketpp::lib::error_code ec;
        g_server.stop_listening(ec);
        g_server.stop();
        if (_thread.joinable()) {
            _thread.join();
        }
        g_matcher.stop();
    }

    std::string uri() const {
        std::ostringstream oss;
        oss << "ws://127.0.0.1:" << kPort << "/ws";
        return oss.str();
    }

private:
    std::thread _thread;
};

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
            std::ostringstream oss;
            try {
                auto con = _client.get_con_from_hdl(hdl);
                oss << "close code=" << con->get_remote_close_code()
                    << ", reason=" << con->get_remote_close_reason();
            } catch (...) {
                oss << "close";
            }
            {
                std::lock_guard<std::mutex> lock(_mtx);
                _closed = true;
                _last_error = oss.str();
            }
            _cv.notify_all();
        });

        _client.set_message_handler(
            [this](connection_hdl, TestClientImpl::message_ptr msg) {
                Json::Value root;
                std::string errmsg;
                if (!gobang::util::str_to_json(msg->get_payload(), root, errmsg)) {
                    std::lock_guard<std::mutex> lock(_mtx);
                    _last_error = "invalid json from server: " + errmsg;
                    _cv.notify_all();
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
        _thread = std::thread([this]() {
            _client.run();
        });

        std::unique_lock<std::mutex> lock(_mtx);
        bool ready = _cv.wait_for(
            lock, std::chrono::milliseconds(timeout_ms), [this]() {
                return _opened || _failed;
            });
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

    Json::Value wait_for_event(const std::string& event, int timeout_ms = 2000) {
        const std::chrono::steady_clock::time_point deadline =
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

class WebSocketM3FlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        _server.start();
        ASSERT_TRUE(_client.connect(_server.uri())) << _client.last_error();
    }

    void TearDown() override {
        _client.close();
        _server.stop();
    }

    LocalWebSocketServer _server;
    TestWebSocketClient _client;
};

TEST_F(WebSocketM3FlowTest, AuthPingMatchStartAndCancelSucceed) {
    const int64_t user_id = 1001;
    std::string token = gobang::security::jwt_generate(user_id, 3600);
    ASSERT_FALSE(token.empty());

    Json::Value auth_data;
    auth_data["token"] = token;
    ASSERT_TRUE(_client.send_event("auth", auth_data)) << _client.last_error();

    Json::Value auth_resp = _client.wait_for_event("auth.success");
    ASSERT_FALSE(auth_resp.isNull()) << _client.last_error();
    ASSERT_EQ(auth_resp["data"]["user_id"].asInt64(), user_id);

    Json::Value ping_data;
    ping_data["timestamp"] = static_cast<Json::Int64>(gobang::util::now_ms());
    ASSERT_TRUE(_client.send_event("ping", ping_data)) << _client.last_error();

    Json::Value pong_resp = _client.wait_for_event("pong");
    ASSERT_FALSE(pong_resp.isNull()) << _client.last_error();
    ASSERT_TRUE(pong_resp["data"].isMember("timestamp"));

    Json::Value start_data;
    start_data["token"] = token;
    start_data["score"] = 1200;
    ASSERT_TRUE(_client.send_event("match.start", start_data)) << _client.last_error();

    Json::Value waiting_resp = _client.wait_for_event("match.waiting");
    ASSERT_FALSE(waiting_resp.isNull()) << _client.last_error();
    ASSERT_EQ(waiting_resp["data"]["queue_position"].asInt(), 1);

    Json::Value cancel_data;
    cancel_data["token"] = token;
    ASSERT_TRUE(_client.send_event("match.cancel", cancel_data)) << _client.last_error();

    Json::Value cancel_resp = _client.wait_for_event("match.cancelled");
    ASSERT_FALSE(cancel_resp.isNull()) << _client.last_error();
    ASSERT_EQ(cancel_resp["data"]["message"].asString(), "cancelled");
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
