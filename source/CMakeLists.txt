cmake_minimum_required(VERSION 3.10)
project(online_gobang CXX)

# C++11 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# 包含目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# JsonCpp 库查找
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
include_directories(${JSONCPP_INCLUDE_DIRS})

# pthread 库
find_package(Threads REQUIRED)

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

# ─── 打印配置信息 ───────────────────────────────────────────
message(STATUS "JSONCPP_INCLUDE_DIRS: ${JSONCPP_INCLUDE_DIRS}")
message(STATUS "JSONCPP_LIBRARIES: ${JSONCPP_LIBRARIES}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
