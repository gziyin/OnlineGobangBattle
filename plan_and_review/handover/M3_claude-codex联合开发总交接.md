# M3 Claude-Codex 联合开发总交接

> **项目**: C++ 在线五子棋对战系统  
> **阶段**: M3 在线管理与匹配模块  
> **交接日期**: 2026-04-16  
> **目的**: 汇总 Claude 与 Codex 在 M3 的联合交付结果，明确当前状态、接口约定、待收口问题与下一步动作，方便下一位接手者直接继续推进。

---

## 一、当前状态

| 模块/文件 | 当前状态 | 说明 |
|----------|----------|------|
| `source/include/online.hpp` | 已实现 | 纯业务在线状态管理，无 WebSocket++ 依赖 |
| `source/include/block_queue.hpp` | 已实现 | 阻塞队列已落地，支持 `timed_pop / shutdown` |
| `source/include/matcher.hpp` | 已实现待联调 | 匹配主链路已完成，但还需与 WebSocket 层最终收口 |
| `source/include/connection_manager.hpp` | 已实现 | 连接映射与发送逻辑已落地，是唯一暴露 `connection_hdl` 的业务头文件 |
| `source/include/websocket_handler.hpp` | 已实现待收口 | WebSocket 事件骨架已写好，但与真实 `Matcher` 的接口尚未完全统一 |
| `client/hall.html` | 已实现待联调 | 大厅页面状态机与基础交互已完成 |
| `client/js/websocket.js` | 已实现待联调 | WebSocket 客户端封装已完成 |
| `source/tests/test_online.cpp` | 已实现待验证 | 测试文件已写，待本机构建运行 |
| `source/tests/test_block_queue.cpp` | 已实现待验证 | 测试文件已写，待本机构建运行 |
| `source/tests/test_matcher.cpp` | 已实现待验证 | 测试文件已写，待本机构建运行 |
| `source/tests/test_connection_manager.cpp` | 已实现待收口 | 当前为模拟式测试，需确认与真实头文件的最终绑定方式 |
| `source/CMakeLists.txt` | 已实现待验证 | 已统一整理测试目标，并已并入 WebSocket++ 查找与 `test_connection_manager` 目标 |

---

## 二、联合开发成果汇总

### 2.1 在线状态与匹配核心

- `OnlineManager` 已完成，提供 `OFFLINE / HALL_IDLE / MATCHING / IN_ROOM` 四态管理。
- `BlockQueue` 已完成，队列不可拷贝，`timed_pop(false)` 语义已固定为“超时或 shutdown”。
- `Matcher` 已完成基础匹配能力：
  - 非 `HALL_IDLE` 用户不可入队
  - 入队成功后切换到 `MATCHING`
  - 同段优先匹配
  - 懒删除处理取消与断线
  - 匹配成功后通过回调返回 `room_id / 双方 user_id / color`
  - `room_id` 当前采用内存自增

### 2.2 连接管理与 WebSocket 接线

- `ConnectionManager` 已实现 `user_id <-> connection_hdl` 映射、获取、移除与发送。
- `connection_hdl` 当前被限制在 `connection_manager.hpp` 和 WebSocket 接线层内，未向匹配业务层渗透。
- `WebSocketHandler` 已实现事件骨架：
  - `match.start`
  - `match.cancel`
  - `ping`
  - `match.waiting`
  - `match.success`
  - `error`
  - `pong`
- token 校验与断线清理链路已经在接线层设计完成，但需要和真实 `Matcher` 最终对齐。

### 2.3 大厅前端

- `client/hall.html` 已具备三态 UI：
  - `idle`
  - `matching`
  - `matched`
- `client/js/websocket.js` 已封装：
  - 建连/断连
  - 消息发送
  - 事件分发
  - 心跳
  - 自动重连
- 前端已具备与 M3 协议对接的基础空壳，但仍需服务端联调验证。

### 2.4 测试与构建

- M3 已新增测试文件：
  - `test_online.cpp`
  - `test_block_queue.cpp`
  - `test_matcher.cpp`
  - `test_connection_manager.cpp`
- `source/CMakeLists.txt` 已统一为 `add_gobang_test(...)` 风格。
- 当前文档口径为：
  - 已完成部分语法级/结构级验证
  - 尚未在本轮完成完整本机构建、链接和运行验证

---

## 三、关键接口与对接约定

以下约定视为当前 M3 联合开发的权威口径，后续收口应以此为准。

### 3.1 在线状态

```cpp
enum class OnlineStatus {
    OFFLINE   = 0,
    HALL_IDLE = 1,
    MATCHING  = 2,
    IN_ROOM   = 3
};
```

- `user_online(user_id)` 默认将用户置为 `HALL_IDLE`
- `user_offline(user_id)` 清理在线态
- `OnlineManager` 只负责业务状态，不持有连接句柄

### 3.2 Matcher 职责边界

- `Matcher` 只依赖 `online.hpp` 与 `block_queue.hpp`
- `Matcher` 不直接接触 `connection_hdl`
- 匹配成功后只通过回调抛出 `MatchResult`
- 消息发送由 WebSocket 层完成，不在 `Matcher` 内部执行

### 3.3 断线处理顺序

统一约定为：

```cpp
conn_mgr.remove(user_id);
online_mgr.user_offline(user_id);
matcher.on_disconnect(user_id);
```

### 3.4 room_id 来源

- M3 阶段固定先使用进程内自增临时 `room_id`
- M4 若引入持久化房间，可再切换到数据库主键或房间管理模块

---

## 四、当前待收口问题

### 4.1 `MatchResult` 重复定义

**现状**:
- `source/include/matcher.hpp` 中定义了一份 `MatchResult`
- `source/include/websocket_handler.hpp` 中也定义了一份同名结构

**风险**:
- 后续字段一旦演进，极易产生双份定义不一致

**建议收口方式**:
- 以 `matcher.hpp` 中的 `MatchResult` 为唯一来源
- `websocket_handler.hpp` 直接复用该定义，不再重复声明

### 4.2 `MatcherInterface` 与真实 `Matcher` 尚未统一

**现状**:
- `websocket_handler.hpp` 目前依赖 `MatcherInterface` 占位接口
- 实际实现是 `matcher.hpp` 中的具体类 `Matcher`

**风险**:
- WebSocket 层已可编写骨架，但与真实匹配器尚未最终打通

**建议收口方式**:
- 推荐让 `Matcher` 继承 `MatcherInterface`
- 这样可保留 WebSocket 层与匹配器之间的抽象边界，同时减少改动面

### 4.3 `test_connection_manager.cpp` 仍是模拟式测试

**现状**:
- 当前测试文件为了脱离真实 WebSocket++ 运行环境，内置了一套模拟类型和简化版 `ConnectionManager`

**风险**:
- 测试覆盖的是“设计意图”，不完全等价于真实头文件行为

**建议收口方式**:
- 最终应将测试逐步过渡为直接针对真实 `connection_manager.hpp`
- 如保留模拟方式，也应明确标注为“隔离式单元测试”，并补一层真实集成验证

### 4.4 `CMakeLists.txt` 已合并，但仍待本机验证

**现状**:
- `source/CMakeLists.txt` 当前已经同时包含：
  - `add_gobang_test(...)`
  - WebSocket++ 查找
  - `test_connection_manager`
  - `test_online / test_block_queue / test_matcher`

**风险**:
- 逻辑已并回，但尚未在本机执行完整构建与运行验证

**建议收口方式**:
- 以当前 `CMakeLists.txt` 为最终基线
- 下一步只做运行验证与必要修正，不再并行改多套构建脚本

---

## 五、建议下一步执行顺序

1. 统一 `MatchResult` 的唯一定义来源  
   以 `matcher.hpp` 为准，删除 `websocket_handler.hpp` 中重复定义。

2. 统一 `MatcherInterface` 与 `Matcher` 的依赖关系  
   推荐让 `Matcher` 继承 `MatcherInterface`，减少 WebSocket 层改动。

3. 把 `websocket_handler.hpp` 接到真实 `Matcher`  
   完成 `init / enqueue / cancel / on_disconnect / set_match_callback` 的真实对接。

4. 以当前 `source/CMakeLists.txt` 为基线做构建验证  
   包括 WebSocket++ 依赖、`test_connection_manager` 和 M3 三个新测试。

5. 本机运行测试  
   先跑单测，再看是否需要修正接口或构建问题。

6. 做大厅联调  
   至少覆盖：
   - `match.start`
   - `match.cancel`
   - `ping/pong`
   - `match.success`
   - 断线恢复到可重新匹配状态

---

## 六、验收与验证

### 6.1 现有测试清单

- `test_online.cpp`
- `test_block_queue.cpp`
- `test_matcher.cpp`
- `test_connection_manager.cpp`

### 6.2 验证口径

**已完成**:
- `online.hpp`、`block_queue.hpp`、`matcher.hpp` 做过语法级/结构级检查
- M3 相关测试文件已落地
- 构建脚本已合并到统一版本

**尚未完成**:
- 完整 `cmake` 构建
- 真实链接运行
- WebSocket 服务端与大厅前端联调

### 6.3 建议本机构建与运行命令

```powershell
git config --global --add safe.directory "D:/code/C++code/C++ - Online Gobang Battle/OnlineGobangBattle"

cd source
New-Item -ItemType Directory -Force build
cd build
cmake ..
cmake --build . --config Release

.\bin\test_online.exe
.\bin\test_block_queue.exe
.\bin\test_matcher.exe
.\bin\test_connection_manager.exe
```

### 6.4 最终联调验收表

- [ ] 匹配发起成功，非 `HALL_IDLE` 用户不能入队
- [ ] 匹配取消成功，取消后不会再收到成功匹配结果
- [ ] 同段匹配成功，返回 `room_id`、对手信息和执子颜色
- [ ] 断线后连接映射、在线状态、匹配状态都被正确清理
- [ ] 所有匹配事件经过 token 校验
- [ ] 前端能正确展示等待状态、取消状态和成功跳转
- [ ] `connection_hdl` 未渗透到业务匹配层
- [ ] `online.hpp` 编译时不依赖 WebSocket++

---

## 七、结论

M3 联合开发目前已经完成了“后端核心业务层 + WebSocket 接线骨架 + 大厅前端空壳 + 测试与构建框架”的主体搭建，当前阶段的重点已经不是重新设计，而是**联合收口与验证**。

下一位接手者应优先处理接口统一和本机构建验证，再进入 WebSocket 与大厅联调。只要按本交接文档中的顺序推进，M3 可以较平滑地收口，并为 M4 房间与对战逻辑提供稳定入口。

---

## 八、文档口径说明

为避免后续阅读时出现状态混乱，当前以下文档已统一使用同一口径：

- `plan_and_review/milestone_plan/M3_开发计划.md`
- `plan_and_review/handover/M3_claude与codex联合开发_claude开发报告.md`
- `plan_and_review/handover/M3_claude与codex联合开发_codex开发报告.md`
- `plan_and_review/handover/M3_claude-codex联合开发总交接.md`

统一口径为：

- M3 各核心模块主体代码已落地
- 当前阶段属于“待联合收口、待本机构建验证、待大厅联调”
- 主要剩余问题集中在接口统一与验证，不是重新开发模块
