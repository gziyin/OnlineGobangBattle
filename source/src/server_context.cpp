#include "server_context.hpp"

#include "logger.hpp"

namespace gobang {

GobangServer::GobangServer(RunMode mode)
    : mode_(mode) {}

bool GobangServer::init(const char* config_path) {
    try {
        config_ = util::load_config(config_path);
        util::validate_config(config_);
        db_pool_.init(config_);
        user_table_.init(&db_pool_);
        LOG_INFO("GobangServer: database initialized");
    } catch (const std::exception& e) {
        LOG_ERROR("GobangServer: init failed - " << e.what());
        return false;
    }

    conn_mgr_.init(&server_);
    matcher_.init(&online_mgr_);
    matcher_.start();
    http_router_.init(&user_table_, &online_mgr_, &conn_mgr_);

    game_ctrl_ = GameController::create();
    game_ctrl_->init(&room_mgr_, &online_mgr_, &conn_mgr_, &user_table_,
                     &server_.get_io_service());
    game_ctrl_->set_disconnect_timeout_seconds(config_.reconnect_timeout);

    ws_handler_.init(&conn_mgr_, &online_mgr_, &matcher_, &server_, game_ctrl_.get());

    server_.init_asio();
    server_.set_reuse_addr(true);

    if (mode_ == RunMode::Production) {
        server_.set_access_channels(websocketpp::log::alevel::all);
        server_.set_error_channels(websocketpp::log::elevel::all);
    } else {
        server_.clear_access_channels(websocketpp::log::alevel::all);
        server_.clear_error_channels(websocketpp::log::elevel::all);
    }

    wire_handlers();
    return true;
}

std::string GobangServer::trim_ws_resource(const std::string& resource) {
    size_t end = resource.find_first_of("?#");
    return resource.substr(0, end);
}

bool GobangServer::is_allowed_websocket_resource(const std::string& resource) {
    std::string path = trim_ws_resource(resource);
    return path == "/ws" || path == "/ws/";
}

void GobangServer::wire_handlers() {
    server_.set_validate_handler([this](WebsocketConnectionHdl hdl) {
        auto con = server_.get_con_from_hdl(hdl);
        return is_allowed_websocket_resource(con->get_resource());
    });

    server_.set_http_handler([this](WebsocketConnectionHdl hdl) {
        http_router_.handle_request(server_, hdl);
    });

    server_.set_open_handler([this](WebsocketConnectionHdl hdl) {
        ws_handler_.on_open(hdl);
    });

    server_.set_close_handler([this](WebsocketConnectionHdl hdl) {
        ws_handler_.on_close(hdl);
    });

    server_.set_message_handler(
        [this](WebsocketConnectionHdl hdl, WebsocketServer::message_ptr msg) {
            ws_handler_.on_message(hdl, msg->get_payload());
        });
}

void GobangServer::start_disconnect_grace_driver() {
    typedef websocketpp::lib::asio::steady_timer timer;
    struct TimerDriver : std::enable_shared_from_this<TimerDriver> {
        WebSocketHandler* handler = nullptr;
        std::shared_ptr<timer> timer_ptr;

        void schedule() {
            std::shared_ptr<TimerDriver> self = shared_from_this();
            timer_ptr->expires_from_now(websocketpp::lib::asio::milliseconds(500));
            timer_ptr->async_wait([self](const websocketpp::lib::error_code& ec) {
                if (ec) {
                    return;
                }
                self->handler->process_timers();
                self->schedule();
            });
        }
    };

    std::shared_ptr<TimerDriver> driver = std::make_shared<TimerDriver>();
    driver->handler = &ws_handler_;
    driver->timer_ptr = std::make_shared<timer>(server_.get_io_service());
    driver->schedule();
}

bool GobangServer::listen_and_accept() {
    try {
        websocketpp::lib::asio::ip::tcp::endpoint endpoint(
            websocketpp::lib::asio::ip::address_v4::any(),
            static_cast<unsigned short>(config_.server_port));
        server_.listen(endpoint);
        server_.start_accept();
        listening_ = true;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("GobangServer: listen failed - " << e.what());
        listening_ = false;
        if (mode_ == RunMode::Smoke) {
            return true;
        }
        return false;
    }
}

void GobangServer::run() {
    start_disconnect_grace_driver();
    server_.run();
}

void GobangServer::shutdown() {
    if (game_ctrl_) {
        game_ctrl_->stop_all_timers();
    }
    matcher_.stop();
    websocketpp::lib::error_code ec;
    server_.stop_listening(ec);
    server_.stop();
}

} // namespace gobang
