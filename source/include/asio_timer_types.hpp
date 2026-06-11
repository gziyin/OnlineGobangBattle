#pragma once

#include <websocketpp/common/asio.hpp>

namespace gobang {

using IoService = websocketpp::lib::asio::io_service;
using SteadyTimer = websocketpp::lib::asio::steady_timer;
using AsioErrorCode = websocketpp::lib::error_code;

} // namespace gobang
