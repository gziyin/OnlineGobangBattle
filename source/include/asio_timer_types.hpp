#pragma once

#include <websocketpp/common/asio.hpp>

namespace gobang {

using IoService = websocketpp::lib::asio::io_service;
using SteadyTimer = websocketpp::lib::asio::steady_timer;

#if defined(ASIO_STANDALONE)
using AsioErrorCode = websocketpp::lib::asio::error_code;
#else
#include <boost/system/error_code.hpp>
using AsioErrorCode = boost::system::error_code;
#endif

} // namespace gobang
