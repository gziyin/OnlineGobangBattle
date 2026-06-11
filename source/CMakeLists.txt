cmake_minimum_required(VERSION 3.10)
project(online_gobang CXX)
include(CheckCXXSourceCompiles)

enable_testing()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

get_filename_component(SOURCE_DIR "${CMAKE_SOURCE_DIR}" ABSOLUTE)
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
include_directories(${JSONCPP_INCLUDE_DIRS})

find_package(Threads REQUIRED)

find_package(GTest)
if(GTEST_FOUND)
    include_directories(${GTEST_INCLUDE_DIRS})
    message(STATUS "GTest: found")
else()
    message(FATAL_ERROR "GTest: not found. Please install gtest-devel")
endif()

pkg_check_modules(OPENSSL openssl)
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "OpenSSL: found at ${OPENSSL_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR "OpenSSL: not found. Please install openssl-devel")
endif()

set(JWT_CPP_INCLUDE_DIR "/usr/local/include")
if(EXISTS ${JWT_CPP_INCLUDE_DIR}/jwt-cpp/jwt.h)
    include_directories(${JWT_CPP_INCLUDE_DIR})
    message(STATUS "jwt-cpp: found at ${JWT_CPP_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "jwt-cpp: not found. Please install jwt-cpp")
endif()

find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/version.hpp
    PATHS /usr/include /usr/local/include
)
if(WEBSOCKETPP_INCLUDE_DIR)
    include_directories(${WEBSOCKETPP_INCLUDE_DIR})
    message(STATUS "WebSocket++: found at ${WEBSOCKETPP_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "WebSocket++: not found. Please install websocketpp-devel")
endif()

set(_gobang_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
set(_gobang_saved_try_compile_target_type "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
set(CMAKE_REQUIRED_INCLUDES "${WEBSOCKETPP_INCLUDE_DIR}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
message(STATUS "WebSocket++ config probe mode: compile-only (STATIC_LIBRARY)")
check_cxx_source_compiles("
    #include <websocketpp/server.hpp>
    #include <websocketpp/config/asio_no_tls.hpp>
    int main() {
        websocketpp::server<websocketpp::config::asio_no_tls> s;
        (void)s;
        return 0;
    }"
    GOBANG_HAS_WEBSOCKETPP_ASIO_NO_TLS
)
check_cxx_source_compiles("
    #include <websocketpp/server.hpp>
    #include <websocketpp/config/asio.hpp>
    int main() {
        websocketpp::server<websocketpp::config::asio> s;
        (void)s;
        return 0;
    }"
    GOBANG_HAS_WEBSOCKETPP_ASIO
)
set(CMAKE_REQUIRED_INCLUDES "${_gobang_saved_required_includes}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE "${_gobang_saved_try_compile_target_type}")

if(GOBANG_HAS_WEBSOCKETPP_ASIO_NO_TLS)
    message(STATUS "WebSocket++ config: using asio_no_tls")
elseif(GOBANG_HAS_WEBSOCKETPP_ASIO)
    message(STATUS "WebSocket++ config: using asio")
else()
    message(FATAL_ERROR "WebSocket++ config probe failed: neither asio_no_tls nor asio is usable")
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/config.h.in"
    "${CMAKE_BINARY_DIR}/config.h"
    @ONLY
)

pkg_check_modules(MYSQL mysqlclient)
if(NOT MYSQL_FOUND)
    find_path(MYSQL_INCLUDE_DIR mysql.h PATHS /usr/include/mysql /usr/local/include/mysql)
    find_library(MYSQL_LIBRARY mysqlclient PATHS /usr/lib64/mysql /usr/lib/mysql /usr/local/lib64)
    if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
        set(MYSQL_FOUND TRUE)
        set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
        set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
        message(STATUS "MySQL: found via manual search at ${MYSQL_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "MySQL: not found. Please install mysql-devel package.")
    endif()
else()
    set(MYSQL_LIBRARY_DIRS ${MYSQL_LIBRARY_DIRS} /usr/lib64/mysql)
    message(STATUS "MySQL: found via pkg-config at ${MYSQL_INCLUDE_DIRS}")
endif()
include_directories(${MYSQL_INCLUDE_DIRS})
link_directories(${MYSQL_LIBRARY_DIRS})

add_library(gobang_core STATIC
    src/game.cpp
    src/websocket_handler.cpp
    src/server_context.cpp
    src/matcher.cpp
    src/user_table.cpp
)
target_link_libraries(gobang_core PUBLIC
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    Threads::Threads
)

function(add_gobang_test target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} gobang_core ${ARGN})
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    )
endfunction()

function(register_gobang_ctest target_name test_label)
    add_test(NAME ${target_name} COMMAND $<TARGET_FILE:${target_name}>)
    set_tests_properties(${target_name} PROPERTIES LABELS ${test_label})
endfunction()

add_gobang_test(test_base tests/test_base.cpp
    ${JSONCPP_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_base smoke)

add_gobang_test(test_db_pool tests/test_db_pool.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_db_pool integration)

add_gobang_test(test_user_table tests/test_user_table.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_user_table integration)

add_gobang_test(test_security tests/test_security.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_security unit)

add_gobang_test(test_auth_api tests/test_auth_api.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_auth_api integration)

add_gobang_test(test_online tests/test_online.cpp
    ${JSONCPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_online unit)

add_gobang_test(test_block_queue tests/test_block_queue.cpp
    ${JSONCPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_block_queue unit)

add_gobang_test(test_matcher tests/test_matcher.cpp
    ${JSONCPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_matcher unit)

add_gobang_test(test_connection_manager tests/test_connection_manager.cpp
    ${JSONCPP_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_connection_manager unit)

add_gobang_test(websocket_smoke tests/websocket_smoke.cpp
    ${JSONCPP_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${MYSQL_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(websocket_smoke smoke)

add_gobang_test(gobang_server app/server_main.cpp
    ${JSONCPP_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${MYSQL_LIBRARIES}
    Threads::Threads
)

add_gobang_test(test_websocket_m3_flow tests/test_websocket_m3_flow.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_websocket_m3_flow integration)

add_gobang_test(test_room tests/test_room.cpp
    ${JSONCPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_room unit)

add_gobang_test(test_game tests/test_game.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_game integration)

add_gobang_test(test_websocket_game tests/test_websocket_game.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_websocket_game integration)

message(STATUS "JSONCPP_INCLUDE_DIRS: ${JSONCPP_INCLUDE_DIRS}")
message(STATUS "JSONCPP_LIBRARIES: ${JSONCPP_LIBRARIES}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
