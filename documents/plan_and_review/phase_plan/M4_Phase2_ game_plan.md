# M4 Phase 2 实现计划：game.hpp 游戏逻辑与定时器

> **状态**: ✅ 已完成
> **完成日期**: 2026-05-26
> **提交**: 已提交，未测试

## Context

Phase 1 已完成 `room.hpp`（GameRoom + RoomManager），测试通过。Phase 2 实现 `game.hpp` 游戏控制器，协调房间管理、在线状态、连接管理和积分更新，提供完整的对局生命周期管理。

**核心职责**：处理落子/认输/游戏开始/断线重连/超时判负，游戏结束时更新积分并清理资源。

---

## 文件变更清单

| 操作 | 文件 | 说明 |
|------|------|------|
| 新建 | `source/include/game.hpp` | 游戏控制器模块 |
| 新建 | `source/tests/test_game.cpp` | GTest 单元测试 |
| 修改 | `source/CMakeLists.txt` | 添加 test_game 构建目标 |

---

## 一、game.hpp 设计

### 头文件依赖

```
#include <cstdint>, <string>, <mutex>, <thread>, <condition_variable>, <atomic>, <functional>
#include "room.hpp"
#include "online.hpp"
#include "connection_manager.hpp"
#include "user_table.hpp"
#include "logger.hpp"
```

### GameController 类

```cpp
class GameController {
public:
    GameController();
    ~GameController();

    void init(RoomManager* room_mgr, OnlineManager* online_mgr,
              ConnectionManager* conn_mgr, UserTable* user_table);

    // 设置超时时间（秒）
    void set_timeout_seconds(int seconds);

    // 处理游戏开始（匹配成功后调用）
    void handle_game_start(int64_t player1_id, int64_t player2_id);

    // 处理落子事件
    void handle_move(int64_t user_id, int row, int col);

    // 处理认输事件
    void handle_giveup(int64_t user_id);

    // 处理断线
    void handle_disconnect(int64_t user_id);

    // 处理重连
    void handle_reconnect(int64_t user_id);

private:
    // 游戏结束处理
    void on_game_over(const std::string& room_id, GameResult result,
                      int64_t winner_id, int64_t loser_id);

    // 发送消息给房间内所有玩家
    void broadcast_to_room(const std::string& room_id, const std::string& msg);

    // 启动超时检测定时器
    void start_timeout_timer(const std::string& room_id);

    // 停止超时定时器
    void stop_timeout_timer(const std::string& room_id);

    // 获取 GameResult 对应的获胜者和失败者
    void get_winner_loser(GameRoom* room, GameResult result,
                          int64_t& winner_id, int64_t& loser_id);

    RoomManager*       room_mgr_;
    OnlineManager*     online_mgr_;
    ConnectionManager* conn_mgr_;
    UserTable*         user_table_;
    int                timeout_seconds_;

    // 超时定时器
    struct TimeoutInfo {
        std::thread              thread;
        std::condition_variable  cv;
        std::mutex               mtx;
        bool                     cancelled;
    };
    std::unordered_map<std::string, std::shared_ptr<TimeoutInfo>> timers_;
    std::mutex timers_mtx_;
};

---

## 二、关键流程

### handle_game_start 流程

1. 调用 `RoomManager::create_room(p1, p2)` 创建房间
2. 设置双方状态为 `OnlineStatus::IN_ROOM`
3. 构造 `game.start` JSON 消息（包含 room_id、opponent、color、your_turn）
4. 发送给双方
5. 启动超时定时器

### handle_move 流程

1. 通过 `RoomManager::get_room_by_user(user_id)` 获取房间
2. 调用 `GameRoom::place_piece(user_id, row, col)`
3. 若返回 NONE：构造 `game.move` 消息广播，重启超时定时器
4. 若返回 BLACK_WIN/WHITE_WIN：调用 `on_game_over` 处理结束
5. 若返回 NONE 且落子失败（轮次/合法性）：发送 error 消息

### handle_giveup 流程

1. 获取房间，调用 `GameRoom::give_up(user_id)`
2. 若返回 GIVEUP：调用 `on_game_over` 处理结束

### on_game_over 流程

1. 停止超时定时器
2. 调用 `UserTable::update_score_match(winner_id, loser_id)` 更新积分
3. 构造 `game.over` 消息（result、reason、winner、score_change）
4. 发送给双方
5. 设置双方状态为 `OnlineStatus::HALL_IDLE`
6. 调用 `RoomManager::destroy_room(room_id)`

### handle_disconnect 流程

1. 获取房间，若不在房间则忽略
2. 启动重连等待定时器（60秒）
3. 若超时未重连：调用 `on_game_over` 判负

### handle_reconnect 流程

1. 获取房间
2. 构造 `game.reconnect` 消息（room_id、board、current_turn、opponent）
3. 发送给重连玩家

### 超时定时器实现

```cpp
void start_timeout_timer(const std::string& room_id) {
    stop_timeout_timer(room_id);  // 先停止旧的

    auto info = std::make_shared<TimeoutInfo>();
    info->cancelled = false;

    info->thread = std::thread([this, room_id, info]() {
        std::unique_lock<std::mutex> lock(info->mtx);
        info->cv.wait_for(lock, std::chrono::seconds(timeout_seconds_),
                          [&info]() { return info->cancelled; });
        if (!info->cancelled) {
            // 超时处理：获取房间，判负
            GameRoom* room = room_mgr_->get_room(room_id);
            if (room && room->get_status() == RoomStatus::PLAYING) {
                int64_t current = room->get_current_turn();
                int64_t opponent = room->get_opponent_id(current);
                on_game_over(room_id, GameResult::TIMEOUT, opponent, current);
            }
        }
    });

    std::lock_guard<std::mutex> lock(timers_mtx_);
    timers_[room_id] = info;
}
```

---

## 三、test_game.cpp 测试设计

### 测试策略

由于 GameController 依赖 ConnectionManager（需要 WebSocket++）和 UserTable（需要 MySQL），测试采用**真实依赖 + 本地服务器**模式，与 `test_websocket_m3_flow.cpp` 类似。

### 测试 Fixture

```cpp
class GameControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 启动本地 WebSocket 服务器
        // 初始化 ConnectionManager、OnlineManager、RoomManager、UserTable
        // 初始化 GameController
        // 创建两个测试客户端连接
    }

    void TearDown() override {
        // 关闭连接、停止服务器
    }

    // 辅助方法：等待消息
    bool wait_for_message(TestWebSocketClient& client, Json::Value& msg, int timeout_ms);
};
```

### 测试用例（8 个）

| 测试名 | 覆盖点 |
|--------|--------|
| HandleGameStart | 匹配成功后双方收到 game.start，状态变为 IN_ROOM |
| HandleMoveValid | 正常落子后双方收到 game.move，棋盘更新 |
| HandleMoveNotYourTurn | 非轮次落子收到 error 消息 |
| HandleMoveWin | 五连触发 game.over，积分更新 |
| HandleGiveup | 认输触发 game.over，积分更新 |
| HandleDisconnectAndReconnect | 断线后重连收到完整棋盘状态 |
| HandleTimeout | 超时后自动判负（设置短超时时间测试） |
| ScoreUpdateVerification | 游戏结束后数据库积分正确更新 |

---

## 四、CMakeLists.txt 修改

在 `test_websocket_m3_flow` 之后插入：

```cmake
add_gobang_test(test_game tests/test_game.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_game integration)
```

注意：需要链接 `MYSQL_LIBRARIES`（UserTable 依赖）和 `OPENSSL_LIBRARIES`（ConnectionManager 依赖）。

---

## 五、注意事项

1. **C++11**：不能用 `std::make_unique`，用 `std::unique_ptr<T>(new T(...))`
2. **header-only**：所有实现 inline 写在头文件中
3. **命名空间**：所有代码在 `gobang` 命名空间下
4. **ConnectionManager 依赖 WebSocket++**：game.hpp 包含 connection_manager.hpp，因此引入 WebSocket++ 依赖
5. **超时定时器生命周期**：房间销毁时必须停止定时器，避免悬空指针
6. **JSON 消息格式**：使用 jsoncpp 构造消息，格式参考 M4 开发计划附录 C

---

## 六、实现顺序

1. game.hpp 头文件结构 + GameController 类声明
2. init() + handle_game_start() 实现
3. handle_move() + handle_giveup() 实现
4. on_game_over() + broadcast_to_room() 实现
5. 超时定时器实现（start/stop_timeout_timer）
6. handle_disconnect() + handle_reconnect() 实现
7. test_game.cpp 测试用例
8. CMakeLists.txt 添加构建目标
9. 编译运行 `ctest -L integration` 验证

---

## 七、验证方法

```bash
cd source/build
cmake .. && cmake --build . --target test_game
./bin/test_game
```

预期：8 个测试用例全部通过。
