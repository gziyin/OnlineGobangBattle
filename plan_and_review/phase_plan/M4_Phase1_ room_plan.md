# M4 Phase 1 实现计划：room.hpp 游戏房间管理模块

> **状态**: ✅ 已完成  
> **完成日期**: 2026-05-26  
> **提交**: `b52ce33` feat(M4): Phase 1 游戏房间管理模块 room.hpp

## Context

M3 阶段已完成在线状态管理、连接管理、匹配器和 WebSocket 事件框架。M4 目标是实现完整对战闭环。Phase 1 是 M4 的第一步，实现 `room.hpp` 游戏房间管理模块，为后续 Phase 2（游戏控制器）提供基础。

**核心约束**：`room.hpp` 编译时禁止引入 WebSocket++ 依赖，`connection_hdl` 不可出现。

---

## 文件变更清单

| 操作 | 文件 | 说明 |
|------|------|------|
| 新建 | `source/include/room.hpp` | 游戏房间管理模块 |
| 新建 | `source/tests/test_room.cpp` | GTest 单元测试 |
| 修改 | `source/CMakeLists.txt` | 添加 test_room 构建目标（第 210 行后） |

---

## 一、room.hpp 设计 ✅

### 头文件依赖

```
#include <cstdint>, <string>, <mutex>, <atomic>, <chrono>, <unordered_map>, <memory>, <sstream>
#include "logger.hpp"
#include "util.hpp"   // 复用 board_to_str
```

### 枚举定义

- `RoomStatus`: WAITING=0, PLAYING=1, FINISHED=2
- `PieceColor`: NONE=0, BLACK=1, WHITE=2
- `GameResult`: NONE=0, BLACK_WIN=1, WHITE_WIN=2, DRAW=3, TIMEOUT=4, GIVEUP=5

### PlayerInfo 结构体

- `user_id` (int64_t), `color` (PieceColor), `ready` (bool)
- 不含 `username`（展示层信息，Phase 3 通过 UserTable 查询）

### GameRoom 类

**查询接口**：
- `get_room_id()`, `get_status()`, `get_result()`, `get_current_turn()`
- `get_board_state()` — 复用 `util::board_to_str`
- `get_move_count()`, `get_board(row, col)`
- `has_player(user_id)`, `get_player_color(user_id, out)`, `get_opponent_id(user_id)`

**游戏操作**：
- `place_piece(user_id, row, col)` → GameResult
- `give_up(user_id)` → GameResult

**私有方法**：
- `check_win(row, col, color)` — 四方向连续棋子检测
- `is_valid_move(row, col)` — 越界 + 空位检查
- `find_player_index(user_id)` — user_id → 索引映射

**成员变量**：
- `room_id_`, `status_`, `result_`, `board_[15][15]`, `players_[2]`, `current_turn_index_`, `move_count_`
- `mutable std::mutex mtx_`

### RoomManager 类

**接口**：
- `create_room(player1_id, player2_id)` → room_id（失败返回空串）
- `get_room(room_id)` → GameRoom*
- `get_room_by_user(user_id)` → GameRoom*
- `destroy_room(room_id)`
- `room_count()` → size_t

**成员变量**：
- `rooms_` (unordered_map<string, unique_ptr<GameRoom>>)
- `user_room_map_` (unordered_map<int64_t, string>)
- `next_seq_` (atomic<int64_t>)
- `mutable std::mutex mtx_`

**房间 ID 格式**：`R` + 毫秒时间戳后10位 + 4位自增序号（如 `R67000000010001`）

---

## 二、关键算法 ✅

### 胜负判定 check_win

以落子位置为中心，检查 4 个方向（水平、垂直、主对角线、副对角线），每个方向正反各延伸最多 4 格，统计连续同色棋子数 ≥ 5 即获胜。复杂度 O(1)。

### place_piece 流程

1. 加锁
2. 状态检查（PLAYING）
3. 轮次检查（find_player_index + current_turn_index_）
4. 位置合法性检查（is_valid_move）
5. 落子（board_[row][col] = color）
6. check_win → 若胜则设 FINISHED
7. move_count_ >= 225 → DRAW（预留）
8. 切换轮次

---

## 三、test_room.cpp 测试用例 ✅

使用 GTest 风格。Fixture `GameRoomTest` 在 SetUp 中创建房间 (player1=1001 黑, player2=1002 白)。

### GameRoom 测试（23 个）

| 测试名 | 覆盖点 |
|--------|--------|
| InitialStateIsPlaying | 新建房间状态 PLAYING |
| BlackGoesFirst | 黑方先手 |
| PlacePieceValidMove | 正常落子 + 轮次切换 |
| PlacePieceOutOfBounds | 越界拒绝 |
| PlacePieceAlreadyOccupied | 重复位置拒绝 |
| PlacePieceNotYourTurn | 非轮次拒绝 |
| PlacePieceUnknownUser | 非房间用户拒绝 |
| PlacePieceAfterFinished | 结束后拒绝 |
| WinHorizontal | 横向五连 |
| WinVertical | 纵向五连 |
| WinDiagonalMain | 主对角线五连 |
| WinDiagonalAnti | 副对角线五连 |
| WinExactlyFive | 恰好五连 |
| WinMoreThanFive | 六连也胜 |
| NoWinFourInRow | 四连不胜 |
| GiveUpBlackConcedes | 黑认输 → WHITE_WIN |
| GiveUpWhiteConcedes | 白认输 → BLACK_WIN |
| GiveUpAfterFinished | 结束后认输拒绝 |
| GiveUpUnknownUser | 非房间用户认输拒绝 |
| GetBoardState | board_state 225 字符串正确 |
| HasPlayer | 存在/不存在判断 |
| GetOpponentId | 对手 ID 正确 |
| GetPlayerColor | 颜色正确 |

### RoomManager 测试（8 个）

| 测试名 | 覆盖点 |
|--------|--------|
| CreateRoomReturnsValidId | ID 以 "R" 开头 |
| CreateRoomDuplicatePlayerFails | 重复玩家创建失败 |
| GetRoomById | room_id 查找 |
| GetRoomNotFound | 不存在返回 nullptr |
| GetRoomByUser | user_id 查找 |
| GetRoomByUserNotFound | 不在房间返回 nullptr |
| DestroyRoom | 销毁后清理 |
| RoomCount | 计数正确 |

### 并发测试（3 个）

| 测试名 | 覆盖点 |
|--------|--------|
| ConcurrentCreateAndDestroy | 50 线程并发创建/销毁 |
| ConcurrentPlacePiece | 多线程并发落子不崩溃 |
| ConcurrentGetRoomByUser | 多线程并发查找不崩溃 |

---

## 四、CMakeLists.txt 修改 ✅

在第 210 行（`register_gobang_ctest(test_websocket_m3_flow integration)`）之后插入：

```cmake
add_gobang_test(test_room tests/test_room.cpp
    ${JSONCPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_room unit)
```

---

## 五、注意事项 ✅

1. **C++11**：不能用 `std::make_unique`，用 `std::unique_ptr<T>(new T(...))`
2. **header-only**：所有实现 inline 写在头文件中
3. **命名空间**：所有代码在 `gobang` 命名空间下
4. **线程安全**：`GameRoom` 内部 mutex，`RoomManager` 全局 mutex
5. **PlayerInfo 不含 username**：保持 room.hpp 无外部依赖

---

## 六、实现顺序 ✅

1. room.hpp 枚举 + PlayerInfo 结构体
2. GameRoom 构造函数 + 查询接口
3. is_valid_move + check_win 核心算法
4. place_piece + give_up 游戏操作
5. RoomManager 房间管理
6. test_room.cpp 全部测试用例
7. CMakeLists.txt 添加构建目标
8. 编译运行 `ctest -L unit` 验证

---

## 七、验证方法 ✅

```bash
cd source/build
cmake .. && cmake --build . --target test_room
./bin/test_room
```

预期：34 个测试用例全部通过。
