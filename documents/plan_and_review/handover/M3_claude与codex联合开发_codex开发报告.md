# M3 联合开发 - Codex 开发报告

> **项目**: C++ 在线五子棋对战系统  
> **阶段**: M3 在线管理与匹配模块  
> **开发角色**: Codex (Phase 1 + Phase 2-B + Phase 3)  
> **对接伙伴**: Claude (Phase 2-A + Phase 4)  
> **报告日期**: 2026-04-16

---

## 一、负责范围

根据联合开发分工，我负责以下模块：

| Phase | 模块 | 文件路径 |
|-------|------|----------|
| Phase 1 | 在线状态管理 | `source/include/online.hpp` |
| Phase 2-B | 阻塞队列 | `source/include/block_queue.hpp` |
| Phase 3 | 匹配器骨架与匹配算法 | `source/include/matcher.hpp` |
| 测试 | 在线状态测试 | `source/tests/test_online.cpp` |
| 测试 | 阻塞队列测试 | `source/tests/test_block_queue.cpp` |
| 测试 | 匹配器测试 | `source/tests/test_matcher.cpp` |
| 构建 | 测试目标统一整理 | `source/CMakeLists.txt` |

---

## 二、交付物详情

### 2.1 online.hpp

**文件路径**: `source/include/online.hpp`

**职责**: 维护纯业务语义的在线状态，不依赖 WebSocket++，不包含任何连接句柄。

**核心接口**:
```cpp
enum class OnlineStatus {
    OFFLINE   = 0,
    HALL_IDLE = 1,
    MATCHING  = 2,
    IN_ROOM   = 3
};

class OnlineManager {
public:
    void user_online(int64_t user_id);
    void user_offline(int64_t user_id);
    bool is_online(int64_t user_id) const;
    bool set_status(int64_t user_id, OnlineStatus status);
    OnlineStatus get_status(int64_t user_id) const;
    size_t online_count() const;
};
```

**实现要点**:
- `user_online()` 默认把用户状态置为 `HALL_IDLE`
- `user_offline()` 直接清理内存在线态
- 所有读写统一由 `std::mutex` 保护
- 离线用户 `get_status()` 返回 `OFFLINE`

**架构约束**:
- ✅ 无 WebSocket++ include
- ✅ 无数据库依赖
- ✅ 可被 Matcher、WebSocketHandler 直接复用

---

### 2.2 block_queue.hpp

**文件路径**: `source/include/block_queue.hpp`

**职责**: 提供线程安全阻塞队列，支持匹配请求的生产消费。

**核心接口**:
```cpp
template <class T>
class BlockQueue {
public:
    bool timed_pop(T& out, int timeout_ms = 100);
    void push(const T& value);
    void shutdown();
    bool is_shutdown() const;
    size_t size() const;
    bool empty() const;

    BlockQueue(const BlockQueue&) = delete;
    BlockQueue& operator=(const BlockQueue&) = delete;
};
```

**语义约定**:
- `timed_pop()` 返回 `true`: 成功取到元素
- `timed_pop()` 返回 `false`: 超时或已 shutdown
- 调用方必须结合 `is_shutdown()` 决定是否退出消费循环

**实现要点**:
- 内部使用 `std::queue + std::mutex + std::condition_variable`
- `shutdown()` 唤醒全部等待线程
- 队列显式不可拷贝，避免后续误放入 `std::vector<BlockQueue<T>>`

---

### 2.3 matcher.hpp

**文件路径**: `source/include/matcher.hpp`

**职责**: 实现 M3 匹配主逻辑，包含分桶、入队、取消、断线处理、匹配成功回调。

**核心接口**:
```cpp
struct MatchResult {
    int64_t room_id;
    int64_t player1_id;
    int64_t player2_id;
    int player1_color;
    int player2_color;
};

typedef std::function<void(const MatchResult&)> MatchCallback;

class Matcher {
public:
    void init(OnlineManager* online_mgr);
    void set_match_callback(MatchCallback cb);
    bool enqueue(int64_t user_id, int score);
    bool cancel(int64_t user_id);
    void on_disconnect(int64_t user_id);
    void start();
    void stop();
};
```

**关键设计落实**:
- `Matcher` 只依赖 `online.hpp` 和 `block_queue.hpp`
- **未引入** `connection_manager.hpp`，保持与 WebSocket 层解耦
- `enqueue()` 前置条件固定为 `HALL_IDLE`
- 入队成功后自动切换状态为 `MATCHING`
- 匹配成功后通过 `MatchCallback` 返回结果，不直接发送消息
- `room_id` 在 M3 先用内存自增生成

**当前实现策略**:
- 分段规则：
  - `BRONZE`: `< 1300`
  - `SILVER`: `1300 - 1699`
  - `GOLD`: `>= 1700`
- 同段优先匹配
- 取消与断线通过移除 `_user_request_map` 实现懒删除
- 出队前二次校验：
  - 用户仍在线
  - 状态仍为 `MATCHING`
  - 请求 `request_id` 与当前映射一致
- 匹配成功后把双方状态切为 `IN_ROOM`

**实现细节说明**:
- 为避免 `BlockQueue` 不可拷贝问题，内部没有使用 `std::vector<BlockQueue<...>>`
- 改用 `std::array<std::unique_ptr<BlockQueue<MatchRequest>>, 3>`
- `matcher.hpp` 已去除对 `util.hpp` 的依赖，降低外部 JSON/配置头带来的编译耦合

---

## 三、测试交付

### 3.1 test_online.cpp

**覆盖点**:
- 用户上线默认进入 `HALL_IDLE`
- 重复上线会覆盖为 `HALL_IDLE`
- 下线后状态清理
- 状态流转：`HALL_IDLE -> MATCHING -> IN_ROOM -> HALL_IDLE`
- 并发读写安全
- 离线用户不能直接 `set_status`

### 3.2 test_block_queue.cpp

**覆盖点**:
- `push / timed_pop` 基本功能
- 超时返回 `false` 且 `is_shutdown() == false`
- shutdown 后返回 `false` 且 `is_shutdown() == true`
- 多线程生产消费
- shutdown 唤醒阻塞线程
- 编译期验证不可拷贝

### 3.3 test_matcher.cpp

**覆盖点**:
- 非 `HALL_IDLE` 用户不能入队
- 入队后状态切换为 `MATCHING`
- 同段用户匹配成功
- 取消后不会再匹配成功
- 断线用户不会残留为有效匹配请求
- FIFO 公平性
- 并发入队无重复配对

---

## 四、对 Claude 的接口约定

### 4.1 我提供给 Claude 使用的接口

**OnlineManager**
```cpp
void user_online(int64_t user_id);
void user_offline(int64_t user_id);
bool is_online(int64_t user_id) const;
bool set_status(int64_t user_id, OnlineStatus status);
OnlineStatus get_status(int64_t user_id) const;
```

**Matcher**
```cpp
void init(OnlineManager* online_mgr);
void set_match_callback(MatchCallback cb);
bool enqueue(int64_t user_id, int score);
bool cancel(int64_t user_id);
void on_disconnect(int64_t user_id);
void start();
void stop();
```

### 4.2 对接时的固定约束

- 在线状态固定为：
  - `OFFLINE=0`
  - `HALL_IDLE=1`
  - `MATCHING=2`
  - `IN_ROOM=3`
- 断线处理顺序固定为：
  - `conn_mgr.remove(user_id)`
  - `online_mgr.user_offline(user_id)`
  - `matcher.on_disconnect(user_id)`
- 匹配成功消息必须由 WebSocket 层通过回调发送，`Matcher` 本身不直接发送

---

## 五、构建配置修改

**文件路径**: `source/CMakeLists.txt`

**本次处理**:
- 新增 `add_gobang_test(...)` 统一测试目标创建方式
- 纳入以下新测试：
  - `test_online`
  - `test_block_queue`
  - `test_matcher`
  - `test_connection_manager`
- 并入 WebSocket++ 头文件查找

**说明**:
- 当前 `CMakeLists.txt` 已包含双方联合开发所需的测试目标与 WebSocket++ 查找
- 剩余工作主要是本机构建验证，而不是继续拆分维护多份构建口径

---

## 六、当前验证情况

### 已完成验证
- `online.hpp` 语法级检查通过
- `block_queue.hpp` 语法级检查通过
- `matcher.hpp` 在去除 `util.hpp` 依赖后，语法级检查通过
- 联合开发相关文件已并入统一 `CMakeLists.txt`

### 未在本轮完成的验证
- 未执行完整 `cmake` / 链接 / 运行测试
- 原因：当前会话环境中未提供可直接调用的 `cmake`
- 后续由人工本机构建验证

---

## 七、已知对接注意事项

### 7.1 MatchResult 定义需要统一

Claude 在 `websocket_handler.hpp` 中独立定义了一份 `MatchResult`，而我在 `matcher.hpp` 中也定义了同名结构。

**建议收口方式**:
- 最终以 `matcher.hpp` 中的 `MatchResult` 为唯一来源
- `websocket_handler.hpp` 不再重复定义同名结构，改为直接包含或前置声明对接

### 7.2 MatcherInterface 与 Matcher 实现需要对齐

Claude 当前 `websocket_handler.hpp` 中引入的是 `MatcherInterface` 占位接口，而我的实现是具体类 `Matcher`。当前状态是“双方代码都已落地，但接口尚未最终统一”。

**建议收口方式**:
- 二选一：
  1. 让 `Matcher` 继承 `MatcherInterface`
  2. 删除占位接口，`WebSocketHandler` 直接依赖 `Matcher`

**推荐**:
- 如果后续希望持续解耦 WebSocket 层和匹配器实现，建议采用方案 1

### 7.3 跨段超时匹配仍需联调验证

当前 `matcher.hpp` 已保留跨段匹配逻辑，但其验证仍主要依赖完整运行测试与联调场景。

**联调重点**:
- 超时阈值是否符合大厅体验预期
- 前端等待态展示与后端匹配节奏是否一致

---

## 八、建议联调顺序

1. 先统一 `MatchResult` 与 `MatcherInterface` 的最终归属
2. 让 `websocket_handler.hpp` 接上真实 `Matcher`
3. 完成 `test_connection_manager`、`test_matcher`、`test_online` 的本机构建运行
4. 再做大厅页面联调：
   - `match.start`
   - `match.cancel`
   - `ping/pong`
   - `match.success`
5. 最后确认断线清理链路是否完整

---

## 九、文件清单

```text
source/
├── include/
│   ├── online.hpp
│   ├── block_queue.hpp
│   └── matcher.hpp
├── tests/
│   ├── test_online.cpp
│   ├── test_block_queue.cpp
│   └── test_matcher.cpp
└── CMakeLists.txt
```

---

## 十、结论

我负责的 M3 核心业务层已经落地完成，当前状态可以概括为：

- 在线状态管理已就位
- 阻塞队列已就位
- 匹配器主链路已就位
- 单元测试已补齐
- 与 WebSocket 层的边界已经明确

剩余工作主要集中在**联合收口**，不是重新设计：
- 统一 `MatchResult` / `MatcherInterface`
- 跑完整构建和测试
- 完成 WebSocket 与大厅前端联调

*报告完成。当前状态已同步为“模块主体已落地，待联合收口与联调验证”。*
