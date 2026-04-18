# M3 联合开发 - Claude 开发报告

> **项目**: C++ 在线五子棋对战系统  
> **阶段**: M3 在线管理与匹配模块  
> **开发角色**: Claude (Phase 2-A + Phase 4)  
> **对接伙伴**: Codex (Phase 1 + Phase 2-B + Phase 3)  
> **报告日期**: 2026-04-16

---

## 一、负责范围

根据分工方案，我负责以下模块的开发：

| Phase | 模块 | 文件路径 |
|-------|------|----------|
| Phase 2-A | ConnectionManager | `source/include/connection_manager.hpp` |
| Phase 2-A | 连接管理测试 | `source/tests/test_connection_manager.cpp` |
| Phase 4 | WebSocket 事件处理器 | `source/include/websocket_handler.hpp` |
| Phase 4 | 游戏大厅页面 | `client/hall.html` |
| Phase 4 | WebSocket 客户端 | `client/js/websocket.js` |
| 构建配置 | CMakeLists.txt 更新 | `source/CMakeLists.txt` |

---

## 二、交付物详情

### 2.1 connection_manager.hpp

**文件路径**: `source/include/connection_manager.hpp`

**职责**: 维护 user_id 与 WebSocket 连接的映射关系，是**唯一暴露 connection_hdl 的业务头文件**。

**核心接口**:
```cpp
class ConnectionManager {
public:
    void init(WebsocketServer* server);
    void add(int64_t user_id, WebsocketConnectionHdl hdl);
    void remove(int64_t user_id);
    bool get(int64_t user_id, WebsocketConnectionHdl& out) const;
    bool send(int64_t user_id, const std::string& msg);
    bool is_connected(int64_t user_id) const;
    std::vector<int64_t> get_all_user_ids() const;
    size_t connection_count() const;
};
```

**实现要点**:
- 所有操作加 `std::mutex` 保护
- `add()` 时如 user_id 已存在，覆盖旧连接
- `send()` 失败语义：用户不存在返回 `false`，连接失效（`expired()`）返回 `false`
- 连接失效时**不自动 remove**，由上层统一决定是否清理

**架构约束验证**:
- ✅ `connection_hdl` 仅出现在本文件和 WebSocket 接线层
- ✅ 业务模块（Matcher）不直接依赖本文件

---

### 2.2 websocket_handler.hpp

**文件路径**: `source/include/websocket_handler.hpp`

**职责**: WebSocket 事件处理器，负责 token 校验、消息分发、调用 Matcher、匹配成功回调。

**核心接口**:
```cpp
class WebSocketHandler {
public:
    void init(ConnectionManager* conn_mgr,
              OnlineManager* online_mgr,
              MatcherInterface* matcher);
    void on_open(WebsocketConnectionHdl hdl);
    void on_close(WebsocketConnectionHdl hdl);
    void on_message(WebsocketConnectionHdl hdl, const std::string& msg);
};
```

**事件协议**:

| 方向 | 事件 | 说明 |
|------|------|------|
| Client→Server | `match.start` | 携带 token 和 score，发起匹配 |
| Client→Server | `match.cancel` | 携带 token，取消匹配 |
| Client→Server | `ping` | 心跳请求 |
| Server→Client | `match.waiting` | 已进入匹配队列 |
| Server→Client | `match.success` | 匹配成功，返回 room_id 和对手信息 |
| Server→Client | `match.cancelled` | 取消成功确认 |
| Server→Client | `error` | 错误响应（code + message） |
| Server→Client | `pong` | 心跳响应 |

**错误码定义**:
- `4000`: invalid JSON
- `4001`: invalid token / unauthenticated
- `4002`: unknown event
- `4003`: not in hall
- `4004`: enqueue failed
- `4005`: cancel failed

**断线处理流程**:
```cpp
void on_close(hdl) {
    user_id = get_user_id_from_hdl(hdl);
    conn_mgr->remove(user_id);
    online_mgr->user_offline(user_id);
    matcher->on_disconnect(user_id);
}
```

**Matcher 解耦设计**:
- 定义 `MatcherInterface` 纯虚接口，避免直接依赖 Matcher 实现
- 通过 `MatchCallback` 回调接收匹配结果，再调用 `ConnectionManager::send()` 通知双方

---

### 2.3 hall.html

**文件路径**: `client/hall.html`

**功能**: 游戏大厅页面，支持匹配状态展示和交互。

**UI 状态机**:
- `idle`: 空闲中，显示"开始匹配"按钮
- `matching`: 匹配中，显示等待时间和"取消匹配"按钮
- `matched`: 匹配成功，显示房间号并自动跳转

**关键交互**:
- 点击"开始匹配" → 发送 `match.start` 事件 → 进入 matching 状态
- 点击"取消匹配" → 发送 `match.cancel` 事件 → 返回 idle 状态
- 收到 `match.success` → 显示成功提示 → 2秒后跳转房间页

**Token 处理**:
- 从 `localStorage` 读取 `gobang_token`
- 无 token 时自动跳转登录页
- token 失效时清除存储并跳转登录页

---

### 2.4 websocket.js

**文件路径**: `client/js/websocket.js`

**功能**: WebSocket 客户端封装类 `GobangWebSocket`。

**核心能力**:
```javascript
class GobangWebSocket {
    connect()           // 建立连接
    reconnect()         // 自动重连（最多5次）
    close()             // 关闭连接
    send(event, data)   // 发送事件
    isConnected()       // 检查连接状态
}
```

**心跳机制**:
- 间隔: 30秒
- 消息: `{ "event": "ping", "data": { "timestamp": Date.now() } }`
- 自动启停

**事件分发**:
- 通过 `onEvent` 回调将服务端事件分发到业务处理函数
- 支持自定义事件处理器注册

---

### 2.5 test_connection_manager.cpp

**文件路径**: `source/tests/test_connection_manager.cpp`

**测试用例**:
1. `AddConnection`: 添加连接映射
2. `AddDuplicateOverride`: 重复添加覆盖旧连接
3. `RemoveConnection`: 移除后不可获取
4. `SendSuccess`: 发送消息成功
5. `SendToNonExistentUser`: 发送给不存在用户失败
6. `SendToExpiredConnection`: 发送给已断开连接失败
7. `GetAllUserIds`: 获取所有在线用户（自动过滤已失效连接）
8. `ConcurrentAccess`: 并发读写安全测试

**测试策略**:
- 使用模拟的 WebSocket++ 类型进行单元测试
- 实际集成测试需完整 WebSocket++ 环境

---

## 三、与 Codex 的接口约定

### 3.1 依赖 Codex 提供的接口

**OnlineManager** (`source/include/online.hpp`):
```cpp
enum class OnlineStatus { OFFLINE=0, HALL_IDLE=1, MATCHING=2, IN_ROOM=3 };

void user_online(int64_t user_id);      // 默认置为 HALL_IDLE
void user_offline(int64_t user_id);
bool set_status(int64_t user_id, OnlineStatus status);
OnlineStatus get_status(int64_t user_id) const;
```

**MatcherInterface / Matcher 对接现状**:
```cpp
bool enqueue(int64_t user_id, int score);
bool cancel(int64_t user_id);
void on_disconnect(int64_t user_id);
void set_match_callback(MatchCallback cb);
```

说明：
- Codex 侧 `Matcher` 实现已经落地
- 当前待收口点不是“是否实现”，而是 `MatcherInterface` 与真实 `Matcher` 的最终统一方式

### 3.2 我提供的接口供 Codex 使用

**ConnectionManager**:
- `send(user_id, msg)` - Matcher 匹配成功后通过回调调用

**WebSocketHandler**:
- 提供 `MatcherInterface` 定义，供 Matcher 继承实现

---

## 四、CMakeLists.txt 状态

当前 `source/CMakeLists.txt` 已合并以下内容：

**已并入**:
```cmake
# WebSocket++（header-only 库）
find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/version.hpp ...)
include_directories(${WEBSOCKETPP_INCLUDE_DIR})
```

**新增测试目标**:
```cmake
add_gobang_test(test_connection_manager tests/test_connection_manager.cpp
    ${JSONCPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    Threads::Threads
)
```

当前口径：
- 构建脚本逻辑已并回统一版本
- 剩余工作为本机构建验证，不再是等待 CMake 配置合并

---

## 五、待对接项

以下工作为当前联合收口阶段的剩余事项：

| 序号 | 待对接项 | 阻塞条件 |
|------|----------|----------|
| 1 | `MatchResult` 唯一定义来源统一 | `matcher.hpp` 与 `websocket_handler.hpp` 当前重复定义 |
| 2 | `MatcherInterface` 与真实 `Matcher` 对齐 | 需要确定继承或直接依赖方案 |
| 3 | WebSocket 服务端主程序 | 需要完整的服务端入口 |
| 4 | 本机构建与联调测试 | 依赖统一接口后执行 |

---

## 六、风险与建议

### 已规避的风险
- ✅ `connection_hdl` 依赖未渗透到 Matcher 层
- ✅ Matcher 与 ConnectionManager 通过回调解耦
- ✅ 断线流程统一封装在 `on_close()` 中

### 待关注的风险
1. **WebSocket++ 编译依赖**: 需确保服务端环境已安装 `websocketpp-devel`
2. **回调线程安全**: `on_match_success` 回调可能在 Matcher 线程调用，需确保 `ConnectionManager::send()` 线程安全
3. **Token 刷新**: 当前实现 token 过期后跳转登录页，后续可优化为静默刷新

---

## 七、文件清单

```
source/
├── include/
│   ├── connection_manager.hpp      # 连接管理
│   └── websocket_handler.hpp       # WebSocket 事件处理
├── tests/
│   └── test_connection_manager.cpp # 连接管理测试
└── CMakeLists.txt                  # 构建配置（已更新）

client/
├── hall.html                       # 游戏大厅页面
└── js/
    └── websocket.js                # WebSocket 客户端封装
```

---

## 八、验收 checklist（供联调使用）

### 基础功能
- [ ] WebSocket 连接建立成功
- [ ] `match.start` 事件携带 token 认证通过
- [ ] 用户状态从 `HALL_IDLE` 切换到 `MATCHING`
- [ ] `match.cancel` 成功取消匹配
- [ ] 收到 `match.success` 并正确跳转

### 断线处理
- [ ] 断线后 `ConnectionManager` 清理映射
- [ ] 断线后 `OnlineManager` 标记离线
- [ ] 断线后 `Matcher` 取消匹配

### 错误处理
- [ ] 无效 token 返回 4001 错误
- [ ] 非空闲状态发起匹配返回 4003 错误
- [ ] 异常断线前端自动重连

---

*报告完成。当前状态已同步为“模块主体已落地，待联合收口与联调验证”。*
