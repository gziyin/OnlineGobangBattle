# Online Gobang Battle

基于 C++ 的网页版在线五子棋对战系统：用户注册登录、天梯分桶匹配、WebSocket 实时对战。采用前后端分离架构，后端以模块化组织（头文件 + `gobang_core` 静态库），前端为原生 HTML/JavaScript。

## 功能概览

| 能力 | 状态 | 说明 |
|------|------|------|
| 用户注册 / 登录 | 已实现 | HTTP JSON API，JWT 鉴权，PBKDF2 密码存储 |
| 天梯匹配 | 已实现 | 三段位分桶，支持超时跨段放宽 |
| 在线状态 | 已实现 | 大厅空闲 / 匹配中 / 对局中 四态管理 |
| 五子棋对战 | 已实现 (M4) | room/game/WebSocket 集成 + `room.html`；Linux CTest 全量通过 |
| 实时聊天 | 规划中 (M5) | 敏感词过滤等 |
| 生产级统一服务入口 | 已实现 | `gobang_server`（`GobangServer`）；`websocket_smoke` 用于 smoke 验证 |

## 技术栈

- **后端**：C++11、WebSocket++、MySQL、JsonCpp、OpenSSL、jwt-cpp
- **前端**：HTML5 / CSS3 / JavaScript（ES6+）
- **构建**：CMake 3.10+、Google Test、CTest、`gobang_core` 静态库
- **运行环境**：Rocky Linux 9（开发）/ Ubuntu 22.04 LTS（部署）

## 仓库结构

```
OnlineGobangBattle/
├── source/
│   ├── app/              # 生产入口 server_main.cpp → gobang_server
│   ├── src/              # gobang_core 实现（game / websocket_handler / server_context）
│   ├── include/          # 服务端模块声明与 header-only 组件
│   ├── tests/            # 单元 / 集成 / smoke 测试
│   └── config/           # server.conf.example
├── client/               # 静态前端（大厅等）
├── scripts/              # 数据库初始化
├── documents/            # 构建与工程文档
│   └── plan_and_review/  # 里程碑计划与复盘
├── ops/                  # deploy.sh、systemd、nginx 模板
```

### 核心模块

| 模块 | 文件 | 职责 |
|------|------|------|
| 服务组装 | `server_context.hpp/cpp`, `app/server_main.cpp` | `GobangServer` 统一初始化与运行 |
| 数据访问 | `db.hpp`, `user_table.hpp` | MySQL 连接池、用户表 |
| 安全认证 | `security.hpp`, `auth_handler.hpp`, `http_router.hpp` | 密码哈希、JWT、HTTP 路由、`GET /health` |
| 在线与匹配 | `online.hpp`, `matcher.hpp`, `block_queue.hpp` | 在线状态、分桶匹配 |
| 连接与事件 | `connection_manager.hpp`, `message_sender.hpp`, `websocket_handler.hpp` | WebSocket 映射、消息发送抽象、事件分发 |
| 对战 | `room.hpp`, `game.hpp`, `asio_timer_types.hpp` | 房间、棋盘、Asio 回合/断线定时器 |

**架构约束**：`websocketpp::connection_hdl` 仅出现在连接管理与 WebSocket 接线层；`GameController` 通过 `IMessageSender` 发消息，不直接依赖 WebSocket++。

**初始化顺序（强制）**：`server.init_asio()` → `game_ctrl->init(..., &io_service)` → `ws_handler.init(...)`。详见 `documents/plan_and_review/project_plan/current/refactor_26_6_11.md`。

## 快速开始

### 1. 安装依赖

详见 [documents/build-and-run-guide.md](documents/build-and-run-guide.md)。简要示例（Ubuntu 22.04）：

```bash
sudo apt update
sudo apt install -y g++ cmake make git pkg-config \
  libjsoncpp-dev libwebsocketpp-dev libgtest-dev \
  libssl-dev libmysqlclient-dev

# jwt-cpp（header-only）
git clone https://github.com/Thalhammer/jwt-cpp.git
cd jwt-cpp && sudo cp -r include/jwt-cpp /usr/local/include/
```

### 2. 初始化数据库

```bash
./scripts/init_db.sh
# 或按 documents/build-and-run-guide.md 手动建库建表
```

### 3. 配置

```bash
cp source/config/server.conf.example source/config/server.conf
# 编辑 db_*、jwt_secret 等字段
```

### 4. 构建与测试

```bash
cd source
mkdir -p build && cd build
cmake ..
cmake --build .

# 运行全部 CTest
ctest --output-on-failure

# 或按标签运行，例如仅单元测试
ctest -L unit --output-on-failure
```

构建产物位于 `source/build/bin/`。

### 5. 启动服务

**生产 / 联调（推荐）**

```bash
cd source/build
./bin/gobang_server
```

默认监听 `0.0.0.0:8080`（以 `server.conf` 为准）：

| 端点 | 类型 | 说明 |
|------|------|------|
| `GET http://host:8080/health` | HTTP | 健康检查 |
| `http://host:8080/api/v1/auth/*` | HTTP | 注册 / 登录 |
| `ws://host:8080/ws` | WebSocket | 大厅 / 匹配 / 对战事件 |

**Smoke 验证（可选）**

```bash
./bin/websocket_smoke
```

前端：在浏览器打开 `client/login.html` → `hall.html` → 匹配成功后进入 `client/room.html`（需根据实际部署调整 WebSocket 地址）。

## WebSocket 事件（摘要）

上行：`match.start`、`match.cancel`、`game.move`、`game.chat`、`game.giveup`、`ping`  
下行：`match.waiting`、`match.success`、`game.start`、`game.move`、`game.chat`、`game.over`、`game.reconnect`、`error`、`pong`

完整契约见 [documents/plan_and_review/milestone_plan/M4 接口冻结草案（基于 M3）.md](documents/plan_and_review/milestone_plan/M4%20接口冻结草案（基于%20M3）.md)。

## 里程碑进度

| 里程碑 | 状态 |
|--------|------|
| M1 环境搭建与基础框架 | 已完成 |
| M2 数据库与用户模块 | 已完成 |
| M3 在线管理与匹配 | 已完成 |
| M4 游戏房间与对战逻辑 | 已完成（代码 + Linux CTest；浏览器 E2E 可按需复验） |
| M5 聊天与优化 | 未开始 |
| M6 部署与文档 | 部分完成（`deploy.sh`、`gobang_server.service`、nginx 模板；OpenAPI / CORS 白名单待补齐） |

详细计划与复盘见 [documents/plan_and_review/](documents/plan_and_review/)。

## 文档索引

| 文档 | 说明 |
|------|------|
| [documents/build-and-run-guide.md](documents/build-and-run-guide.md) | 依赖、构建、测试、启动服务 |
| [documents/engineering_hygiene.md](documents/engineering_hygiene.md) | 工程规范 |
| [documents/plan_and_review/project_plan/current/project_plan_v2.2.md](documents/plan_and_review/project_plan/current/project_plan_v2.2.md) | 项目总规划 |
| [documents/plan_and_review/project_plan/current/refactor_26_6_11.md](documents/plan_and_review/project_plan/current/refactor_26_6_11.md) | 2026-06-11 架构重构记录 |
| [documents/plan_and_review/phase_plan/M4_Phase4_verification_report.md](documents/plan_and_review/phase_plan/M4_Phase4_verification_report.md) | M4 Phase 4 验收记录 |

## 部署

生产发布可参考 `ops/deploy.sh`（拉取代码 → CMake 构建 → CTest → 切换发布目录 → `gobang_server`）。systemd 与 nginx 示例见 `ops/gobang_server.service`、`ops/nginx-gobang.conf.example`。

## 开发说明

- 核心实现：`source/include/`（声明）+ `source/src/`（`gobang_core`）+ `source/app/`（生产入口）。
- AI 协作流程使用 [Trellis](.trellis/workflow.md)（`.trellis/` 目录，本机 gitignore）。
- 修改架构或测试入口时，请同步更新 `documents/plan_and_review/` 中对应文档（见 [documents/plan_and_review/README.md](documents/plan_and_review/README.md)）。

## 许可证

尚未在仓库中声明开源许可证；使用前请与项目维护者确认。
