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

# ─── 测试目标：test_base ─────────────────────────────────────
add_executable(test_base tests/test_base.cpp)

# 链接库
target_link_libraries(test_base
    ${JSONCPP_LIBRARIES}
    Threads::Threads
)

# 输出目录
set_target_properties(test_base PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)

# ─── 测试目标：test_db_pool ──────────────────────────────────
add_executable(test_db_pool tests/test_db_pool.cpp)

target_link_libraries(test_db_pool
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    Threads::Threads
)

set_target_properties(test_db_pool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)

# ─── 测试目标：test_user_table ──────────────────────────────────
add_executable(test_user_table tests/test_user_table.cpp)

target_link_libraries(test_user_table
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    Threads::Threads
)

set_target_properties(test_user_table PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)

# ─── 打印配置信息 ───────────────────────────────────────────
message(STATUS "JSONCPP_INCLUDE_DIRS: ${JSONCPP_INCLUDE_DIRS}")
message(STATUS "JSONCPP_LIBRARIES: ${JSONCPP_LIBRARIES}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
