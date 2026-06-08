# C++ 在线五子棋对战项目规划 v2.2

> **文档同步日期**: 2026-06-08  
> **说明**: 本节反映仓库当前实现事实；历史规划差异见 `project_plan/history/`。

## 一、项目概述

### 1.1 项目背景
基于 C++ 开发的网页版五子棋对战游戏，支持玩家在线匹配对战和实时聊天功能。采用前后端分离架构，后端使用 C++11 实现，前端使用 HTML/CSS/JavaScript。

### 1.2 核心功能
| 功能模块 | 描述 |
|---------|------|
| 用户管理 | 用户注册、登录、信息查询、天梯分数记录、比赛场次记录 |
| 匹配对战 | 基于天梯分数分桶匹配，支持跨分段超时匹配 |
| 聊天功能 | 玩家在对战时实时聊天，支持敏感词过滤 |
| 断线重连 | 断线后 60 秒内可重连恢复游戏 |
| 在线管理 | 游戏大厅和房间用户在线状态管理 |

### 1.3 技术栈
| 类别 | 技术 |
|------|------|
| 后端语言 | C++11 |
| 网络框架 | WebSocket++ (HTTP/WebSocket，Boost.Asio) |
| 数据库 | MySQL 8.0 |
| JSON处理 | JsonCpp |
| 密码加密 | OpenSSL PBKDF2 |
| JWT 认证 | jwt-cpp (header-only) |
| 前端 | HTML5/CSS3/JavaScript (ES6+) |
| 开发环境 | Rocky Linux 9 (开发) / Ubuntu 22.04 LTS (部署) |
| 构建工具 | CMake 3.10+、Google Test、CTest |
| 服务入口 | `source/tests/websocket_smoke.cpp`（联调）；生产级 `main` 待 M6 |
| API文档 | OpenAPI 3.0（规划，M6 待编写） |

---

## 二、系统架构

### 2.1 整体架构图
```
┌─────────────────────────────────────────────────────────────┐
│                        客户端层                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │  登录页面   │  │  游戏大厅   │  │  游戏房间   │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
│        │                │                │                   │
│        └────────────────┴────────────────┘                   │
│                         │ HTTPS/WSS                        │
└─────────────────────────┼───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                      服务端层                                │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              HTTP/WebSocket 服务器                    │   │
│  │              (基于 WebSocket++)                       │   │
│  └─────────────────────────────────────────────────────┘   │
│        │              │              │                      │
│        ▼              ▼              ▼                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐      │
│  │ 用户管理 │  │ 在线管理 │  │ 房间/匹配管理        │      │
│  │  模块    │  │   模块   │  │       模块           │      │
│  └──────────┘  └──────────┘  └──────────────────────┘      │
│        │              │              │                      │
│        └──────────────┴──────────────┘                      │
│                       │                                     │
│              ┌────────┴────────┐                           │
│              │  日志/监控模块   │                           │
│              └─────────────────┘                           │
└────────────────────────┬────────────────────────────────────┘
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                      数据层                                  │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                    MySQL 8.0 数据库                   │   │
│  │    用户表 (user)  │  房间表 (room)                    │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 模块划分（当前仓库结构）

```
OnlineGobangBattle/
├── source/
│   ├── CMakeLists.txt          # 唯一构建入口
│   ├── config/                 # server.conf.example
│   ├── include/                # 核心模块（header-only 为主）
│   │   ├── db.hpp              # MySQL 连接池
│   │   ├── user_table.hpp      # 用户表访问
│   │   ├── security.hpp        # PBKDF2、JWT、限流
│   │   ├── auth_handler.hpp    # HTTP 注册/登录
│   │   ├── auth_middleware.hpp # JWT 中间件
│   │   ├── online.hpp          # 四态在线管理
│   │   ├── block_queue.hpp     # 匹配阻塞队列
│   │   ├── matcher.hpp         # 分桶匹配器
│   │   ├── matcher_interface.hpp
│   │   ├── connection_manager.hpp  # WebSocket 连接映射（唯一暴露 connection_hdl 之一）
│   │   ├── websocket_handler.hpp   # 事件分发（唯一暴露 connection_hdl 之一）
│   │   ├── room.hpp            # GameRoom + RoomManager
│   │   ├── game.hpp            # GameController
│   │   ├── logger.hpp
│   │   └── util.hpp
│   └── tests/                  # 单元/集成/smoke（含 websocket_smoke 联调入口）
├── client/
│   ├── login.html
│   ├── hall.html
│   ├── room.html
│   ├── css/game.css
│   └── js/websocket.js, game.js
├── scripts/                    # init_db.sh / init_db.sql
├── documents/                  # 构建指南、工程规范
│   └── plan_and_review/        # 里程碑计划与复盘
└── ops/                        # deploy.sh, rollback.sh
```

**架构约束**：`websocketpp::connection_hdl` 仅出现在 `connection_manager.hpp` 与 `websocket_handler.hpp`；业务模块通过 `user_id` 与 `ConnectionManager::send` 通信。

---

## 三、数据库设计

### 3.1 用户表
```sql
CREATE TABLE user (
    id INT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(32) UNIQUE NOT NULL COMMENT '用户名',
    password_hash VARCHAR(128) NOT NULL COMMENT 'bcrypt密码哈希',
    score INT UNSIGNED DEFAULT 1500 COMMENT '天梯分数',
    total_count INT UNSIGNED DEFAULT 0 COMMENT '总场次',
    win_count INT UNSIGNED DEFAULT 0 COMMENT '胜场',
    status TINYINT DEFAULT 0 COMMENT '0-离线 1-大厅 2-房间',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_score (score),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 3.2 房间表（持久化，用于复盘）
```sql
CREATE TABLE room (
    id INT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    white_id INT UNSIGNED NOT NULL COMMENT '白方玩家ID',
    black_id INT UNSIGNED NOT NULL COMMENT '黑方玩家ID',
    winner_id INT UNSIGNED DEFAULT 0 COMMENT '获胜者ID，0表示未结束',
    board_state VARCHAR(225) DEFAULT '' COMMENT '15x15棋盘序列化(225字符，0空1黑2白)',
    current_turn TINYINT DEFAULT 1 COMMENT '1-黑方 2-白方',
    game_status TINYINT DEFAULT 0 COMMENT '0-等待中 1-进行中 2-已结束 3-已放弃',
    chat_history JSON DEFAULT NULL COMMENT '聊天记录',
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    end_time TIMESTAMP NULL DEFAULT NULL,
    FOREIGN KEY (white_id) REFERENCES user(id) ON DELETE CASCADE,
    FOREIGN KEY (black_id) REFERENCES user(id) ON DELETE CASCADE,
    INDEX idx_status (game_status),
    INDEX idx_create_time (create_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 3.3 比赛记录表
```sql
CREATE TABLE game_history (
    id INT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    room_id INT UNSIGNED NOT NULL,
    white_id INT UNSIGNED NOT NULL,
    black_id INT UNSIGNED NOT NULL,
    winner_id INT UNSIGNED DEFAULT 0,
    start_time TIMESTAMP NOT NULL,
    end_time TIMESTAMP NULL DEFAULT NULL,
    moves TEXT COMMENT '所有落子记录 JSON',
    white_score_change INT DEFAULT 0 COMMENT '白方分数变化',
    black_score_change INT DEFAULT 0 COMMENT '黑方分数变化',
    FOREIGN KEY (room_id) REFERENCES room(id),
    FOREIGN KEY (white_id) REFERENCES user(id),
    FOREIGN KEY (black_id) REFERENCES user(id),
    INDEX idx_player (white_id, black_id),
    INDEX idx_end_time (end_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

---

## 四、API 设计 (OpenAPI 规范)

### 4.1 HTTP REST API

#### 用户认证
| 接口 | 方法 | 描述 | 请求体 | 响应 |
|------|------|------|--------|------|
| `/api/v1/auth/register` | POST | 用户注册 | `{username, password}` | `{code, message, data:{user_id}}` |
| `/api/v1/auth/login` | POST | 用户登录 | `{username, password}` | `{code, message, data:{token, user_info}}` |
| `/api/v1/auth/logout` | POST | 用户登出 | `{token}` | `{code, message}` |
| `/api/v1/user/info` | GET | 获取用户信息 | Header: Authorization | `{code, message, data:{user}}` |
| `/api/v1/user/rank` | GET | 获取天梯排行 | Query: page, limit | `{code, message, data:{list, total}}` |

#### 限流策略
- 注册/登录：单 IP 每分钟 10 次
- 其他接口：单 IP 每分钟 100 次

### 4.2 WebSocket 事件

#### 客户端 -> 服务端
| 事件名 | 描述 |  payload |
|--------|------|----------|
| `match.start` | 开始匹配 | `{token, tier?}` |
| `match.cancel` | 取消匹配 | `{token}` |
| `game.move` | 落子 | `{room_id, row, col}` |
| `game.chat` | 发送聊天 | `{room_id, message}` |
| `game.giveup` | 认输 | `{room_id}` |
| `ping` | 心跳 | `{timestamp}` |

#### 服务端 -> 客户端
| 事件名 | 描述 | payload |
|--------|------|---------|
| `match.success` | 匹配成功 | `{room_id, opponent, color}` |
| `match.waiting` | 等待中 | `{queue_position, wait_time}` |
| `game.start` | 游戏开始 | `{room_id, white_id, black_id}` |
| `game.move` | 对手落子 | `{row, col, color, is_win}` |
| `game.chat` | 收到聊天 | `{from, message, timestamp}` |
| `game.over` | 游戏结束 | `{winner_id, reason}` |
| `game.reconnect` | 重连信息 | `{room_id, board_state, current_turn}` |
| `error` | 错误通知 | `{code, message}` |
| `pong` | 心跳响应 | `{timestamp}` |

---

## 五、核心算法设计

### 5.1 匹配算法（分桶 + 超时放宽）

```cpp
// 匹配分桶策略
enum class MatchTier {
    BRONZE  = 0,  // < 2000 分
    SILVER  = 1,  // 2000 - 3000 分
    GOLD    = 2,  // > 3000 分
    TIER_COUNT = 3
};

// 匹配等待超时配置
const int WAIT_TIMEOUT_TIERS[] = {30, 60, 90}; // 各分段等待秒数

class Matcher {
private:
    // 三个匹配队列
    // ⚠️ 注意：block_queue 含 mutex/condition_variable，不可拷贝
    // 不能用 std::vector<block_queue<MatchRequest>>，应改用指针数组
    std::array<std::unique_ptr<block_queue<MatchRequest>>, TIER_COUNT> _queues;

    // 匹配逻辑
    void match_handler() {
        while (true) {
            for (int tier = 0; tier < TIER_COUNT; ++tier) {
                // 优先同分段匹配
                try_match_same_tier(tier);

                // 超时后尝试跨分段匹配
                try_match_cross_tier(tier);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void try_match_same_tier(int tier) {
        auto& queue = _queues[tier];
        if (queue.size() >= 2) {
            auto p1 = queue.pop();
            auto p2 = queue.pop();
            create_room(p1.user_id, p2.user_id);
        }
    }

    void try_match_cross_tier(int tier) {
        // 检查是否有等待超时的玩家
        // 尝试相邻分段匹配
    }
};
```

### 5.2 五子棋胜负判定

```cpp
// 四个方向: 横、竖、正斜、反斜
const int DIRS[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

bool check_win(const Board& board, int row, int col, int color) {
    for (int d = 0; d < 4; ++d) {
        int count = 1; // 当前位置
        int dr = DIRS[d][0], dc = DIRS[d][1];

        // 正向计数
        for (int i = 1; i < 5; ++i) {
            int nr = row + dr * i, nc = col + dc * i;
            if (valid(nr, nc) && board[nr][nc] == color) count++;
            else break;
        }

        // 反向计数
        for (int i = 1; i < 5; ++i) {
            int nr = row - dr * i, nc = col - dc * i;
            if (valid(nr, nc) && board[nr][nc] == color) count++;
            else break;
        }

        if (count >= 5) return true;
    }
    return false;
}
```

### 5.3 断线重连机制

```cpp
class SessionManager {
private:
    std::unordered_map<uint64_t, Session> _sessions;
    const int RECONNECT_TIMEOUT = 60; // 60秒重连窗口

public:
    void on_disconnect(uint64_t user_id) {
        auto& session = _sessions[user_id];
        session.disconnect_time = now();
        session.status = SessionStatus::DISCONNECTED;

        // 启动定时器，超时后判定为逃跑
        start_reconnect_timer(user_id, RECONNECT_TIMEOUT);
    }

    bool on_reconnect(uint64_t user_id, ConnectionPtr conn) {
        auto it = _sessions.find(user_id);
        if (it == _sessions.end()) return false;

        auto& session = it->second;
        if (session.status != SessionStatus::DISCONNECTED) return false;

        // 检查是否在重连窗口内
        if (now() - session.disconnect_time > RECONNECT_TIMEOUT) {
            handle_escape(user_id);
            return false;
        }

        // 恢复连接
        session.conn = conn;
        session.status = SessionStatus::CONNECTED;

        // 发送房间状态
        if (session.room_id != 0) {
            send_room_state(conn, session.room_id);
        }
        return true;
    }
};
```

---

## 六、里程碑规划

### 里程碑总览

| 阶段 | 名称 | 计划周期 | 实际周期 | 主要目标 | 完成状态 |
|------|------|----------|----------|----------|----------|
| M1 | 环境搭建与基础框架 | 第 1-2 周 | 2026-03-31 | 完成开发环境，搭建 HTTP/WebSocket 服务器框架 | ✅ 已完成 |
| M2 | 数据库与用户模块 | 第 3-4 周 | 2026-04-15 | 完成数据库设计、用户注册/登录/安全功能 | ✅ 已完成 |
| M3 | 在线管理与匹配模块 | 第 5-6 周 | 2026-05-25 | 完成在线用户管理、匹配对战功能 | ✅ 已完成 |
| M4 | 游戏房间与对战逻辑 | 第 7-8 周 | 2026-05-26 起 | 完成房间管理、五子棋对战核心逻辑、断线重连 | 🟡 进行中（Phase 1–3 完成，Phase 4 联调验收中） |
| M5 | 聊天功能与优化 | 第 9 周 | 待定 | 完成实时聊天、敏感词过滤、性能优化 | ⏳ 待开始 |
| M6 | 测试与部署 | 第 10 周 | 待定 | 完成测试、部署、文档编写 | 🟡 部分完成（ops 脚本已有） |

> **缓冲时间**: 预留 2 周应对延期风险
>
> **当前进度说明（2026-06-08）**: M4 Phase 1 `room.hpp`、Phase 2 `game.hpp`、Phase 3 WebSocket 集成与 `room.html` 前端已完成；Phase 4 编译与浏览器端到端联调验收进行中。详见 `documents/plan_and_review/phase_plan/M4_Phase4_verification_report.md`。

---

## 七、详细实施计划

### M1: 环境搭建与基础框架 (第1-2周)

#### Week 1: 开发环境搭建

| 天数 | 任务 | 交付物 | 完成状态 |
|------|------|--------|----------|
| Day 1-2 | Ubuntu 22.04 环境配置<br>- 安装 g++ 11, CMake, gdb<br>- 配置 VSCode 开发环境 | 开发环境就绪 | T |
| Day 3-4 | 依赖库安装<br>- Boost 1.74+<br>- JsonCpp 1.9+<br>- WebSocket++ 0.8+<br>- libbcrypt | 依赖库安装完成 | T |
| Day 5-6 | MySQL 8.0 安装配置<br>- 字符集 utf8mb4<br>- 创建数据库和用户 | 数据库服务就绪 | T |
| Day 7 | 项目脚手架搭建<br>- CMake 构建系统<br>- 目录结构设计<br>- 代码仓库初始化 | 可编译的空项目 | T |

#### Week 2: 基础框架开发

| 天数 | 任务 | 交付物 | 完成状态 |
|------|------|--------|----------|
| Day 1-3 | WebSocket++ 服务器框架<br>- HTTP 请求路由<br>- WebSocket 连接管理<br>- 回调函数注册机制 | 基础服务器框架 | T |
| Day 4-5 | 日志模块<br>- 分级日志(INFO/WARN/ERROR)<br>- 异步写入<br>- 日志轮转 | logger.hpp/cpp | T |
| Day 6-7 | 工具模块<br>- JSON 序列化/反序列化<br>- 时间工具<br>- 字符串处理 | util.hpp/cpp | T |

### M2: 数据库与用户模块 (第3-4周)

#### Week 3: 数据库与安全模块

| 天数 | 任务 | 交付物 | 完成状态 |
|------|------|--------|----------|
| Day 1-2 | 数据库设计实现<br>- 用户表、房间表、比赛记录表<br>- 创建表脚本<br>- **RAII 连接池（`std::queue<MYSQL*> + condition_variable`）** | db.hpp (✅ Phase 1 完成) | ✅ |
| Day 3-5 | 安全模块<br>- OpenSSL PBKDF2 密码加密<br>- JWT Token 生成/验证 (jwt-cpp)<br>- 限流器实现 | security.hpp (Phase 3) | ⏳ |
| Day 6 | 用户数据访问层<br>- UserTable 类<br>- 注册/登录/查询接口<br>- 分数更新逻辑 | user_table.hpp (Phase 2) | 🟡 进行中 |
| Day 7 | 单元测试<br>- 数据库操作测试<br>- 加密/验证测试 | 测试报告 | ⏳ |

#### Week 4: 用户业务与前端

| 天数 | 任务 | 交付物 | 完成状态 |
|------|------|--------|----------|
| Day 1-3 | HTTP API 实现<br>- /api/v1/auth/register<br>- /api/v1/auth/login<br>- /api/v1/user/info | 认证 API (Phase 4) | ⏳ |
| Day 4-5 | 会话管理<br>- **纯 JWT 方案，无需 session**<br>- Token 生命周期管理 | ~~session.hpp~~ (已移除) | ❌ 已取消 |
| Day 6-7 | 前端登录页面<br>- 注册/登录表单<br>- AJAX API 调用<br>- Token 本地存储 | login.html | ⏳ |
### M3: 在线管理与匹配模块 (第5-6周)

#### Week 5: 在线管理

| 天数 | 任务 | 交付物 | 完成状态 |
|------|------|--------|----------|
| Day 1-2 | 在线用户管理模块<br>- OnlineManager 类<br>- 大厅/房间状态管理<br>- 连接映射表 | online.hpp/cpp | ⏳ |
| Day 3-4 | 阻塞队列实现<br>- BlockQueue 模板类<br>- 线程安全保证<br>- 条件变量通知 | block_queue.hpp | ⏳ |
| Day 5-6 | 匹配器设计<br>- Matcher 类架构<br>- 分桶队列设计<br>- 匹配线程池 | matcher.hpp 框架 | ⏳ |
| Day 7 | 代码审查与测试 | 审查报告 | ⏳ |

#### Week 6: 匹配算法

| 天数 | 任务 | 交付物 | 完成状态 |
|------|------|--------|----------|
| Day 1-2 | 匹配算法实现<br>- 同分段匹配<br>- 等待超时检测 | match_handler 实现 | ⏳ |
| Day 3-4 | 跨分段匹配<br>- 超时放宽策略<br>- 相邻分段匹配<br>- 匹配优先级处理 | 完整匹配逻辑 | ⏳ |
| Day 5 | 匹配 WebSocket 事件<br>- match.start<br>- match.cancel<br>- match.success | 匹配 API | ⏳ |
| Day 6-7 | 游戏大厅前端<br>- 匹配按钮交互<br>- 等待状态显示<br>- 匹配成功跳转 | hall.html | ⏳ |

### M4: 游戏房间与对战逻辑 (第7-8周)

#### Week 7: 房间管理

| 天数 | 任务 | 交付物 | 完成状态 |
|------|------|--------|----------|
| Day 1-2 | 房间类实现<br>- Room 类<br>- 棋盘状态管理<br>- 玩家信息管理 | room.hpp/cpp | ⏳ |
| Day 3 | 胜负判定算法<br>- 四方向检查<br>- 禁手规则(可选) | check_win 实现 | ⏳ |
| Day 4-5 | 房间管理类<br>- RoomManager 类<br>- 房间创建/销毁<br>- 用户房间映射 | room_manager.hpp/cpp | ⏳ |
| Day 6-7 | 游戏事件处理<br>- game.move<br>- game.giveup<br>- game.over | 游戏事件 API | ⏳ |

#### Week 8: 断线重连

| 天数 | 任务 | 交付物 | 完成状态 |
|------|------|--------|----------|
| Day 1-2 | 断线检测<br>- 心跳机制(ping/pong)<br>- 异常连接处理<br>- 超时判定 | 心跳系统 | ⏳ |
| Day 3-4 | 重连机制<br>- 会话状态保存<br>- 60秒重连窗口<br>- 房间状态恢复 | reconnect 实现 | ⏳ |
| Day 5 | 逃跑处理<br>- 逃跑判定逻辑<br>- 分数惩罚<br>- 对手判胜 | escape_handler | ⏳ |
| Day 6-7 | 游戏房间前端<br>- 15x15 棋盘绘制<br>- 落子交互<br>- 实时更新 | room.html | ⏳ |

### M5: 聊天功能与优化 (第9周)

| 天数 | 任务 | 交付物 | 完成状态 |
|------|------|--------|----------|
| Day 1-2 | 聊天功能<br>- game.chat 事件<br>- 消息广播<br>- 聊天记录 | 聊天功能 | ⏳ |
| Day 3 | 敏感词过滤<br>- DFA 算法<br>- 敏感词库<br>- 消息替换 | 过滤系统 | ⏳ |
| Day 4 | 性能优化<br>- WebSocket 批量发送<br>- 内存优化<br>- 连接池参数调优（连接数上限 10-20） | 优化实现 | ⏳ |
| Day 5-6 | 集成测试<br>- 功能测试<br>- 并发测试<br>- 压力测试 | 测试报告 | ⏳ |
| Day 7 | Bug 修复与稳定化 | 稳定版本 | ⏳ |

### M6: 测试与部署 (第10周)

#### Week 10: 部署与文档

| 天数 | 任务 | 交付物 | 完成状态 |
|------|------|--------|----------|
| Day 1-2 | 服务器部署<br>- 云服务器选购(2核4G起步)<br>- 环境配置<br>- 服务部署 | 线上环境 | ⏳ |
| Day 3 | 安全加固<br>- Nginx HTTPS/WSS 反向代理<br>- 防火墙配置<br>- 数据库安全 | 安全配置 | ⏳ |
| Day 4-5 | 文档编写<br>- API 文档(OpenAPI)<br>- 部署文档<br>- 使用手册 | 完整文档 | ⏳ |
| Day 6 | 监控配置<br>- 日志收集<br>- 性能监控(可选) | 监控系统 | ⏳ |
| Day 7 | 项目总结与复盘 | 总结报告 | ⏳ |

---

## 八、风险评估与应对策略

| 风险 | 概率 | 影响 | 应对策略 |
|------|------|------|----------|
| WebSocket 连接不稳定 | 中 | 高 | 心跳检测(30秒间隔)、自动重连、断线保护 |
| 匹配算法效率低 | 低 | 中 | 分桶策略、多线程匹配、超时放宽机制 |
| 数据库性能瓶颈 | 中 | 高 | 连接池(10-20连接)、查询优化、索引设计 |
| 并发安全问题 | 中 | 高 | 互斥锁、原子操作、线程安全容器 |
| 内存泄漏 | 中 | 高 | RAII管理、智能指针、Valgrind检测 |
| 安全漏洞 | 低 | 高 | 密码bcrypt加密、SQL注入防护、XSS过滤、限流 |
| 前端兼容性问题 | 低 | 低 | 多浏览器测试、Polyfill、降级方案 |

---

## 九、验收标准

### 功能验收
- [ ] 用户能够正常注册、登录（bcrypt加密）
- [ ] 用户能够在游戏大厅进行匹配（三段位分桶）
- [ ] 匹配成功后能够进入房间对战
- [ ] 五子棋对战逻辑正确，胜负判定准确
- [ ] 玩家能够在游戏中实时聊天（敏感词过滤）
- [ ] 断线后60秒内可重连恢复游戏
- [ ] 异常退出处理正确（逃跑判定）

### 性能验收
- [ ] 支持 500+ 并发连接
- [ ] 匹配响应时间 < 60秒（超时跨分段）
- [ ] 消息延迟 < 50ms（局域网）
- [ ] 心跳检测间隔 30秒，超时 90秒

### 安全验收
- [ ] 用户密码 bcrypt 加密存储
- [ ] HTTPS/WSS 加密传输
- [ ] SQL 注入防护（预处理语句）
- [ ] XSS 攻击防护（输入过滤）
- [ ] API 限流防护

---

## 十、参考资源

- 项目代码仓库: https://gitee.com/qigezi/online_gobang.git
- WebSocket++ 文档: http://docs.websocketpp.org/
- JsonCpp 文档: https://github.com/open-source-parsers/jsoncpp
- bcrypt: https://github.com/trusch/libbcrypt
- jwt-cpp (header-only JWT): https://github.com/Thalhammer/jwt-cpp
- MySQL C API: https://dev.mysql.com/doc/c-api/8.0/en/
- OpenAPI 规范: https://swagger.io/specification/

---

*文档版本: v2.2*
*更新日期: 2026-03-30*
