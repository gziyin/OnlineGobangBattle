# plan_and_review 目录说明

## 目录结构

| 子目录 | 用途 | 文档类型 |
|---|---|---|
| `project_plan/current/` | 当前有效的项目总计划 | 架构、里程碑、治理规则 |
| `project_plan/history/` | 历史版本归档 | 被替代的旧计划 |
| `milestone_plan/` | 里程碑实施计划 | M1/M2/M3 等阶段计划 |
| `phase_plan/` | 阶段任务拆分 | 每个阶段的执行/验收计划 |
| `review/` | 阶段复盘 | 结果、问题、后续改进 |
| `handover/` | 交接文档 | 跨阶段与协作交接 |

## 当前工程事实（必须与代码一致）

> **架构重构基准日**：2026-06-11（详见 `project_plan/current/refactor_26_6_11.md`）

- **生产入口**：`source/app/server_main.cpp` → 可执行文件 `gobang_server`；由 `GobangServer`（`server_context.hpp/cpp`）组装各组件。
- **核心库**：`gobang_core` 静态库，实现位于 `source/src/game.cpp`、`websocket_handler.cpp`、`server_context.cpp`。
- **头文件**：`source/include/` 保留声明与体量较小的 header-only 模块（如 `online.hpp`、`matcher.hpp`）。
- **消息边界**：`GameController` 依赖 `IMessageSender`（`message_sender.hpp`），不直接 include WebSocket++。
- **定时器**：回合/断线超时使用 Asio `steady_timer`；断线 grace 仍由 `WebSocketHandler::process_timers()` + 500ms `TimerDriver` 驱动。
- **初始化顺序（强制）**：`server.init_asio()` 之后才能 `game_ctrl->init(..., &server.get_io_service())`。
- **测试**：`source/tests/*.cpp` 经 CMake + CTest 注册；smoke 目标 `websocket_smoke` 用于绑定验证，非生产入口。
- **部署模板**：`ops/gobang_server.service`、`ops/nginx-gobang.conf.example`；HTTP `GET /health` 由 `http_router.hpp` 提供。

## 历史文档说明

`milestone_plan/`、`phase_plan/`（M3 及更早）、`review/`（M3 及更早）、`handover/` 中的部分描述（如「纯 header-only」「`websocket_smoke` 为唯一入口」「独立 timer worker」）反映**当时**实现，已被 2026-06-11 架构重构 supersede。阅读历史文档时以本文件与 `project_plan/current/` 为准。

## 文档同步规则

当 PR 涉及以下任一变更时，必须同步更新文档：

1. 架构边界或目录结构变更：同步 `project_plan/current/*` 与本文件。
2. 测试入口、运行命令或标签策略变更：同步 `project_plan/current/*` 与相关 `phase_plan/*`。
3. 里程碑状态变化：同步 `milestone_plan/*` 与 `review/*`。

未满足上述同步规则的 PR 不应合并。
