# Online Gobang Battle

基于 C++ 的网页版在线五子棋对战系统：用户注册登录、天梯分桶匹配、WebSocket 实时对战。采用前后端分离架构，后端以模块化头文件组织，前端为原生 HTML/JavaScript。

## 功能概览

| 能力 | 状态 | 说明 |
|------|------|------|
| 用户注册 / 登录 | 已实现 | HTTP JSON API，JWT 鉴权，PBKDF2 密码存储 |
| 天梯匹配 | 已实现 | 三段位分桶，支持超时跨段放宽 |
| 在线状态 | 已实现 | 大厅空闲 / 匹配中 / 对局中 四态管理 |
| 五子棋对战 | 开发中 (M4) | 房间、落子、胜负、超时判负；WebSocket 与前端房间页联调进行中 |
| 实时聊天 | 规划中 (M5) | 敏感词过滤等 |
| 生产级统一服务入口 | 待补齐 | 当前通过测试二进制与 `websocket_smoke` 验证 |

## 技术栈

- **后端**：C++11、WebSocket++、MySQL、JsonCpp、OpenSSL、jwt-cpp
- **前端**：HTML5 / CSS3 / JavaScript（ES6+）
- **构建**：CMake 3.10+、Google Test、CTest
- **运行环境**：Rocky Linux 9（开发）/ Ubuntu 22.04 LTS（部署）

## 仓库结构

```
OnlineGobangBattle/
├── source/
│   ├── include/          # 服务端核心模块（header-only 为主）
│   ├── tests/            # 单元 / 集成 / smoke 测试
│   └── config/           # server.conf.example
├── client/               # 静态前端（大厅等）
├── scripts/              # 数据库初始化
├── documents/            # 构建与工程文档
├── ops/                  # 部署与回滚脚本
└── plan_and_review/      # 里程碑计划与复盘
```

### 核心模块（`source/include/`）

| 模块 | 文件 | 职责 |
|------|------|------|
| 数据访问 | `db.hpp`, `user_table.hpp` | MySQL 连接池、用户表 |
| 安全认证 | `security.hpp`, `auth_handler.hpp`, `auth_middleware.hpp` | 密码哈希、JWT、限流 |
| 在线与匹配 | `online.hpp`, `matcher.hpp`, `block_queue.hpp` | 在线状态、分桶匹配 |
| 连接与事件 | `connection_manager.hpp`, `websocket_handler.hpp` | WebSocket 映射与消息分发 |
| 对战 | `room.hpp`, `game.hpp` | 房间、棋盘、对局生命周期 |

**架构约束**：`websocketpp::connection_hdl` 仅出现在连接管理与 WebSocket 接线层，业务模块通过 `user_id` 与 `ConnectionManager::send` 通信。

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

### 5. 本地联调（WebSocket 冒烟服务）

> 当前尚无统一的 `main` 生产入口；联调可使用内置冒烟服务：

```bash
cd source/build
./bin/websocket_smoke
```

默认监听 `0.0.0.0:8080`：

| 端点 | 类型 | 说明 |
|------|------|------|
| `http://host:8080` | HTTP | 健康提示 |
| `ws://host:8080/ws` | WebSocket | 大厅 / 匹配 / 对战事件 |

前端：在浏览器打开 `client/hall.html`（需根据实际部署调整 WebSocket 地址）。

## WebSocket 事件（摘要）

上行：`match.start`、`match.cancel`、`game.move`、`game.chat`、`game.giveup`、`ping`  
下行：`match.waiting`、`match.success`、`game.start`、`game.move`、`game.chat`、`game.over`、`game.reconnect`、`error`、`pong`

完整契约见 [plan_and_review/milestone_plan/M4 接口冻结草案（基于 M3）.md](plan_and_review/milestone_plan/M4%20接口冻结草案（基于%20M3）.md)。

## 里程碑进度

| 里程碑 | 状态 |
|--------|------|
| M1 环境搭建与基础框架 | 已完成 |
| M2 数据库与用户模块 | 已完成 |
| M3 在线管理与匹配 | 已完成 |
| M4 游戏房间与对战逻辑 | 进行中 |
| M5 聊天与优化 | 未开始 |
| M6 部署与文档 | 部分完成 |

详细计划与复盘见 [plan_and_review/](plan_and_review/)。

## 文档索引

| 文档 | 说明 |
|------|------|
| [documents/build-and-run-guide.md](documents/build-and-run-guide.md) | 依赖、构建、测试、常见问题 |
| [documents/engineering_hygiene.md](documents/engineering_hygiene.md) | 工程规范 |
| [plan_and_review/project_plan/current/project_plan_v2.2.md](plan_and_review/project_plan/current/project_plan_v2.2.md) | 项目总规划 |

## 部署

生产发布可参考 `ops/deploy.sh`（拉取代码 → CMake 构建 → CTest → 切换发布目录）。

## 开发说明

- 服务端实现以 `source/include/*.hpp` 为主，测试驱动分里程碑交付。
- AI 协作流程使用 [Trellis](.trellis/workflow.md)（`.trellis/` 目录）。
- 修改架构或测试入口时，请同步更新 `plan_and_review/` 中对应文档（见 [plan_and_review/README.md](plan_and_review/README.md)）。

## 许可证

尚未在仓库中声明开源许可证；使用前请与项目维护者确认。
