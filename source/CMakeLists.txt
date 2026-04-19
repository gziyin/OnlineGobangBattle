cmake_minimum_required(VERSION 3.10)
project(online_gobang CXX)

# C++11 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# 获取项目根目录绝对路径
get_filename_component(SOURCE_DIR "${CMAKE_SOURCE_DIR}" ABSOLUTE)

# 生成 config.h 文件（包含路径配置）
configure_file(
    "${CMAKE_SOURCE_DIR}/config.h.in"
    "${CMAKE_BINARY_DIR}/config.h"
    @ONLY
)

# 包含目录（含生成的 config.h）
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR})

# JsonCpp 库查找
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
include_directories(${JSONCPP_INCLUDE_DIRS})

# pthread 库
find_package(Threads REQUIRED)

# GTest 库（Phase 3 新增）
find_package(GTest)
if(GTEST_FOUND)
    include_directories(${GTEST_INCLUDE_DIRS})
    message(STATUS "GTest: found")
else()
    message(FATAL_ERROR "GTest: not found. Please install gtest-devel")
endif()

# OpenSSL 库（Phase 3 新增）
pkg_check_modules(OPENSSL openssl)
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "OpenSSL: found at ${OPENSSL_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR "OpenSSL: not found. Please install openssl-devel")
endif()

# jwt-cpp（header-only 库，Phase 3 新增）
set(JWT_CPP_INCLUDE_DIR "/usr/local/include")
if(EXISTS ${JWT_CPP_INCLUDE_DIR}/jwt-cpp/jwt.h)
    include_directories(${JWT_CPP_INCLUDE_DIR})
    message(STATUS "jwt-cpp: found at ${JWT_CPP_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "jwt-cpp: not found. Please install jwt-cpp")
endif()

# WebSocket++（header-only 库，M3 Phase 2-A 新增）
find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/version.hpp
    PATHS /usr/include /usr/local/include
)
if(WEBSOCKETPP_INCLUDE_DIR)
    include_directories(${WEBSOCKETPP_INCLUDE_DIR})
    message(STATUS "WebSocket++: found at ${WEBSOCKETPP_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "WebSocket++: not found. Please install websocketpp-devel")
endif()

# MySQL 客户端库
# 优先尝试 pkg-config，如果失败则手动指定路径
pkg_check_modules(MYSQL mysqlclient)
if(NOT MYSQL_FOUND)
    # 手动指定 MySQL 路径（Rocky 9 默认安装路径）
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
    # pkg-config 找到了，但需要补充库路径（Rocky 9 上常见）
    set(MYSQL_LIBRARY_DIRS ${MYSQL_LIBRARY_DIRS} /usr/lib64/mysql)
    message(STATUS "MySQL: found via pkg-config at ${MYSQL_INCLUDE_DIRS}")
endif()
include_directories(${MYSQL_INCLUDE_DIRS})
link_directories(${MYSQL_LIBRARY_DIRS})

function(add_gobang_test target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} ${ARGN})
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    )
endfunction()

add_gobang_test(test_base tests/test_base.cpp
    ${JSONCPP_LIBRARIES}
    Threads::Threads
)

add_gobang_test(test_db_pool tests/test_db_pool.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    Threads::Threads
)

add_gobang_test(test_user_table tests/test_user_table.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    Threads::Threads
)

add_gobang_test(test_security tests/test_security.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)

add_gobang_test(test_auth_api tests/test_auth_api.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)

add_gobang_test(test_online tests/test_online.cpp
    ${JSONCPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)

add_gobang_test(test_block_queue tests/test_block_queue.cpp
    ${JSONCPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)

add_gobang_test(test_matcher tests/test_matcher.cpp
    ${JSONCPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)

# M3 Phase 2-A: ConnectionManager 测试
add_gobang_test(test_connection_manager tests/test_connection_manager.cpp
    ${JSONCPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)

# M3 Phase 2-B: WebSocket 接线层 smoke 测试（最小服务端入口）
add_gobang_test(websocket_smoke tests/websocket_smoke.cpp
    ${JSONCPP_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
)

# M3 Phase 2-C: WebSocket M3 事件流联调回归
add_gobang_test(test_websocket_m3_flow tests/test_websocket_m3_flow.cpp
    ${JSONCPP_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)

# ─── 打印配置信息 ───────────────────────────────────────────
message(STATUS "JSONCPP_INCLUDE_DIRS: ${JSONCPP_INCLUDE_DIRS}")
message(STATUS "JSONCPP_LIBRARIES: ${JSONCPP_LIBRARIES}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
