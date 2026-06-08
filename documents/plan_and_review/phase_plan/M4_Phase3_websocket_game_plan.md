# M4 Phase 3 实现计划：WebSocket 事件接入与游戏前端

> **状态**: ✅ 已完成（2026-06-08 文档同步；Linux 环境 `ctest -R test_websocket_game` 待复验）
> **前置条件**: Phase 2 (game.hpp) 已完成
> **预计工时**: 3-4 小时

## Context

Phase 1 已完成 `room.hpp`（GameRoom + RoomManager），Phase 2 已完成 `game.hpp`（GameController）。当前存在架构断层：WebSocketHandler 与 GameController 未集成，匹配成功后游戏无法启动，前端缺少游戏页面。

Phase 3 的核心任务是**打通 WebSocket 事件层与游戏控制器的连接**，实现完整的对战事件流，并创建游戏前端页面。

**核心约束**：
- `websocket_handler.hpp` 负责事件分发和鉴权，不包含游戏逻辑
- `game.hpp` 负责游戏逻辑，不直接处理 WebSocket 消息解析
- 前端页面通过 URL 参数获取房间信息，通过 WebSocket 事件同步状态

---

## 文件变更清单

| 操作 | 文件 | 说明 |
|------|------|------|
| 修改 | `source/include/websocket_handler.hpp` | 新增 game.* 事件分发，集成 GameController |
| 新建 | `client/room.html` | 游戏房间页面（棋盘 + 交互） |
| 新建 | `source/tests/test_websocket_game.cpp` | 端到端集成测试 |
| 修改 | `source/CMakeLists.txt` | 添加 test_websocket_game 构建目标 |

---

## 一、websocket_handler.hpp 修改设计

### 1.1 新增依赖注入

```cpp
// 新增 GameController 指针
class WebSocketHandler {
public:
    void init(ConnectionManager* conn_mgr, OnlineManager* online_mgr,
              MatcherInterface* matcher, WebsocketServer* server,
              GameController* game_ctrl);  // 新增参数

private:
    GameController* _game_ctrl;  // 新增成员
};
```

### 1.2 修改 on_match_success 回调

当前问题：匹配成功后只发送 `match.success` 消息，没有创建房间。

修改方案：

```cpp
void on_match_success(const MatchResult& result) {
    // 1. 通知前端匹配成功
    send_match_success(result.player1_id, result);
    send_match_success(result.player2_id, result);

    // 2. 调用 GameController 创建房间并启动游戏
    _game_ctrl->handle_game_start(result.player1_id, result.player2_id);
}
```

### 1.3 新增事件分发分支

在 `on_message` 的事件分发中新增：

```cpp
if (event == "game.move") {
    handle_game_move(user_id, data);
} else if (event == "game.giveup") {
    handle_game_giveup(user_id);
} else if (event == "game.reconnect") {
    handle_game_reconnect(user_id, data);
}
```

### 1.4 事件处理函数实现

#### handle_game_move

```cpp
void handle_game_move(int64_t user_id, const Json::Value& data) {
    // 1. 参数校验
    if (!data.isMember("row") || !data.isMember("col")) {
        send_error(user_id, 4002, "missing row or col");
        return;
    }

    int row = data["row"].asInt();
    int col = data["col"].asInt();

    // 2. 调用 GameController
    _game_ctrl->handle_move(user_id, row, col);
}
```

#### handle_game_giveup

```cpp
void handle_game_giveup(int64_t user_id) {
    _game_ctrl->handle_giveup(user_id);
}
```

#### handle_game_reconnect

```cpp
void handle_game_reconnect(int64_t user_id, const Json::Value& data) {
    // token 验证（重连可能需要重新认证）
    if (data.isMember("token")) {
        int64_t verified_id = verify_token(data["token"].asString());
        if (verified_id == 0 || verified_id != user_id) {
            send_error(user_id, 4001, "invalid token");
            return;
        }
    }

    _game_ctrl->handle_reconnect(user_id);
}
```

### 1.5 修改 on_close 断线处理

```cpp
void on_close(WebsocketConnectionHdl hdl) {
    int64_t user_id = get_user_id_from_hdl(hdl);
    if (user_id == 0) return;

    // 通知 GameController 用户断线
    _game_ctrl->handle_disconnect(user_id);

    // 原有清理逻辑
    _conn_mgr->remove(user_id);
    _online_mgr->user_offline(user_id);
    _matcher->on_disconnect(user_id);
}
```

### 1.6 定时器集成

需要在服务器主循环中定期调用 `GameController::process_pending_timeouts()`：

```cpp
// 在 WebSocketHandler 中添加定时器或在主循环中调用
void process_timers() {
    _game_ctrl->process_pending_timeouts();
}
```

---

## 二、room.html 游戏前端设计

### 2.1 页面结构

```html
<!DOCTYPE html>
<html>
<head>
    <title>五子棋对战</title>
    <link rel="stylesheet" href="css/game.css">
</head>
<body>
    <div class="game-container">
        <!-- 对手信息区 -->
        <div class="player-info opponent">
            <span class="username"></span>
            <span class="score"></span>
        </div>

        <!-- 棋盘区域 -->
        <div class="board-container">
            <canvas id="board" width="600" height="600"></canvas>
        </div>

        <!-- 本方信息区 -->
        <div class="player-info self">
            <span class="username"></span>
            <span class="score"></span>
        </div>

        <!-- 游戏状态区 -->
        <div class="game-status">
            <span class="turn-indicator"></span>
            <span class="timer"></span>
        </div>

        <!-- 操作按钮区 -->
        <div class="game-actions">
            <button id="btn-giveup" class="btn-danger">认输</button>
        </div>

        <!-- 游戏结束弹窗 -->
        <div id="game-over-modal" class="modal hidden">
            <div class="modal-content">
                <h2 class="result-text"></h2>
                <p class="score-change"></p>
                <button id="btn-back-hall">返回大厅</button>
            </div>
        </div>
    </div>

    <script src="js/websocket.js"></script>
    <script src="js/game.js"></script>
</body>
</html>
```

### 2.2 game.js 核心逻辑

#### 状态管理

```javascript
const gameState = {
    roomId: null,
    myColor: null,        // 'black' 或 'white'
    myUserId: null,
    isMyTurn: false,
    board: Array(15).fill(null).map(() => Array(15).fill(0)),
    opponent: null,
    gameActive: false
};
```

#### 初始化流程

```javascript
// 1. 从 URL 参数获取房间信息
const urlParams = new URLSearchParams(window.location.search);
gameState.roomId = urlParams.get('room_id');
gameState.myColor = urlParams.get('color');

// 2. 从 localStorage 获取用户信息
const token = localStorage.getItem('gobang_token');
const user = JSON.parse(localStorage.getItem('gobang_user'));
gameState.myUserId = user.id;

// 3. 建立 WebSocket 连接
const ws = new GobangWebSocket(
    `ws://${window.location.hostname}:8080/ws`,
    token,
    onOpen,
    onClose,
    onError,
    onEvent
);
ws.connect();
```

#### 事件处理器

```javascript
const eventHandlers = {
    // 游戏开始（可能是重连后的恢复）
    'game.start': (data) => {
        gameState.opponent = data.opponent;
        gameState.isMyTurn = data.your_turn;
        gameState.gameActive = true;
        updateUI();
    },

    // 对手落子
    'game.move': (data) => {
        const { row, col, color, next_turn } = data;
        gameState.board[row][col] = color === 'black' ? 1 : 2;
        gameState.isMyTurn = (next_turn === gameState.myColor);
        drawPiece(row, col, color);
        updateTurnIndicator();
    },

    // 游戏结束
    'game.over': (data) => {
        gameState.gameActive = false;
        showGameOverModal(data);
    },

    // 重连恢复
    'game.reconnect': (data) => {
        gameState.roomId = data.room_id;
        gameState.opponent = data.opponent;
        gameState.myColor = data.current_turn;  // 需要根据实际情况调整
        restoreBoard(data.board);
        gameState.gameActive = true;
        updateUI();
    },

    // 错误处理
    'error': (data) => {
        showErrorNotification(data.message);
    }
};
```

#### 棋盘绘制

```javascript
function drawBoard() {
    const canvas = document.getElementById('board');
    const ctx = canvas.getContext('2d');
    const cellSize = 40;
    const padding = 20;

    // 绘制网格线
    ctx.strokeStyle = '#333';
    ctx.lineWidth = 1;
    for (let i = 0; i < 15; i++) {
        // 横线
        ctx.beginPath();
        ctx.moveTo(padding, padding + i * cellSize);
        ctx.lineTo(padding + 14 * cellSize, padding + i * cellSize);
        ctx.stroke();

        // 竖线
        ctx.beginPath();
        ctx.moveTo(padding + i * cellSize, padding);
        ctx.lineTo(padding + i * cellSize, padding + 14 * cellSize);
        ctx.stroke();
    }

    // 绘制星位
    const starPoints = [[3,3], [3,11], [7,7], [11,3], [11,11]];
    starPoints.forEach(([r, c]) => {
        ctx.beginPath();
        ctx.arc(padding + c * cellSize, padding + r * cellSize, 4, 0, Math.PI * 2);
        ctx.fill();
    });
}

function drawPiece(row, col, color) {
    const canvas = document.getElementById('board');
    const ctx = canvas.getContext('2d');
    const cellSize = 40;
    const padding = 20;
    const x = padding + col * cellSize;
    const y = padding + row * cellSize;

    ctx.beginPath();
    ctx.arc(x, y, 18, 0, Math.PI * 2);
    ctx.fillStyle = color === 'black' ? '#000' : '#fff';
    ctx.fill();
    ctx.strokeStyle = '#333';
    ctx.stroke();

    // 标记最后落子
    drawLastMoveMarker(row, col);
}
```

#### 落子交互

```javascript
document.getElementById('board').addEventListener('click', (e) => {
    if (!gameState.gameActive || !gameState.isMyTurn) return;

    const canvas = e.target;
    const rect = canvas.getBoundingClientRect();
    const cellSize = 40;
    const padding = 20;

    const x = e.clientX - rect.left - padding;
    const y = e.clientY - rect.top - padding;

    const col = Math.round(x / cellSize);
    const row = Math.round(y / cellSize);

    if (row < 0 || row >= 15 || col < 0 || col >= 15) return;
    if (gameState.board[row][col] !== 0) return;

    // 发送落子事件
    ws.send('game.move', {
        token: localStorage.getItem('gobang_token'),
        row: row,
        col: col
    });
});
```

#### 认输功能

```javascript
document.getElementById('btn-giveup').addEventListener('click', () => {
    if (!gameState.gameActive) return;

    if (confirm('确定要认输吗？')) {
        ws.send('game.giveup', {
            token: localStorage.getItem('gobang_token')
        });
    }
});
```

### 2.3 断线重连处理

```javascript
function onClose() {
    if (gameState.gameActive) {
        // 尝试重连
        ws.reconnect();
    }
}

function onOpen() {
    // 重连后发送重连事件
    if (gameState.gameActive && gameState.roomId) {
        ws.send('game.reconnect', {
            token: localStorage.getItem('gobang_token'),
            room_id: gameState.roomId
        });
    }
}
```

---

## 三、测试设计

### 3.1 测试策略

采用**真实 WebSocket 服务器 + 双客户端**模式，验证端到端事件流。

### 3.2 测试 Fixture

```cpp
class WebSocketGameTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 启动本地 WebSocket 服务器
        // 初始化所有管理器和 GameController
        // 初始化 WebSocketHandler（注入 GameController）
        // 创建两个测试客户端
    }

    void TearDown() override {
        // 关闭连接、停止服务器
    }

    // 辅助方法
    void auth_client(TestClient& client, int64_t user_id);
    bool wait_for_event(TestClient& client, const std::string& event,
                        Json::Value& data, int timeout_ms = 2000);
};
```

### 3.3 测试用例

| 测试名 | 覆盖点 |
|--------|--------|
| MatchSuccessCreatesRoom | 匹配成功后双方收到 game.start，房间已创建 |
| GameMoveValid | 落子后双方收到 game.move，棋盘状态更新 |
| GameMoveNotYourTurn | 非轮次落子收到 error 消息 |
| GameMoveWin | 五连触发 game.over，积分更新 |
| GameGiveup | 认输触发 game.over |
| GameReconnect | 断线后重连收到 game.reconnect，棋盘状态恢复 |
| GameTimeout | 超时后自动判负（设置短超时） |
| ConcurrentMoves | 并发落子只有一方成功 |

---

## 四、CMakeLists.txt 修改

在 `test_game` 之后插入：

```cmake
add_gobang_test(test_websocket_game tests/test_websocket_game.cpp
    ${JSONCPP_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
register_gobang_ctest(test_websocket_game integration)
```

---

## 五、注意事项

1. **事件顺序**：匹配成功 -> `match.success` 通知前端 -> `handle_game_start` 创建房间 -> `game.start` 通知双方 -> 前端跳转 room.html
2. **token 传递**：所有 game.* 事件都需要携带 token 用于鉴权
3. **状态同步**：前端通过 URL 参数获取初始状态（room_id, color），通过 WebSocket 事件同步后续状态
4. **断线重连**：重连时需要重新认证，然后发送 `game.reconnect` 获取完整棋盘状态
5. **定时器集成**：需要在服务器主循环中定期调用 `process_pending_timeouts()`

---

## 六、实现顺序

1. **websocket_handler.hpp 修改**
   - 新增 GameController 指针成员
   - 修改 init() 方法签名
   - 实现 handle_game_move/handle_game_giveup/handle_game_reconnect
   - 修改 on_match_success 调用 handle_game_start
   - 修改 on_close 调用 handle_disconnect

2. **room.html 前端实现**
   - 页面结构和样式
   - 棋盘绘制逻辑
   - WebSocket 事件处理
   - 落子交互和认输功能
   - 断线重连处理

3. **test_websocket_game.cpp 测试**
   - 测试 Fixture 搭建
   - 编写 8 个测试用例
   - 验证端到端流程

4. **CMakeLists.txt 更新**
   - 添加 test_websocket_game 构建目标

5. **集成验证**
   - 编译运行测试
   - 手动测试完整对战流程

---

## 七、验证方法

```bash
cd source/build
cmake .. && cmake --build . --target test_websocket_game
./bin/test_websocket_game
```

预期：8 个测试用例全部通过。

手动验证流程：
1. 启动服务器
2. 打开两个浏览器窗口，分别登录不同用户
3. 两个用户都点击"开始匹配"
4. 匹配成功后自动跳转到 room.html
5. 黑方先手落子，白方收到同步
6. 交替落子直到五连或认输
7. 游戏结束后返回大厅

---

*文档版本：v1.0*
*制定日期：2026-05-27*
