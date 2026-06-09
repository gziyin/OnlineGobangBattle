#pragma once

#include "../include/util.hpp"
#include "config.h"

#include <stdexcept>
#include <string>

namespace gobang {
namespace testutil {

inline util::Config load_test_config() {
    try {
        return util::load_config(GOBANG_TEST_CONFIG_PATH);
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Failed to load test config at ") + GOBANG_TEST_CONFIG_PATH +
            ": " + e.what() +
            ". Copy source/config/server.conf.test.example to source/config/server.conf.test "
            "and point db_name to gobang_db_test.");
    }
}

}  // namespace testutil
}  // namespace gobang
