# C++ 在线五子棋对战项目方案

## 一、项目概述

### 1.1 项目背景
本项目是一个基于 C++ 开发的网页版五子棋对战游戏，支持玩家在线匹配对战和实时聊天功能。项目采用前后端分离架构，后端使用 C++ 实现，前端使用 HTML/CSS/JavaScript。

### 1.2 核心功能
| 功能模块 | 描述 |
|---------|------|
| 用户管理 | 用户注册、登录、信息查询、天梯分数记录、比赛场次记录 |
| 匹配对战 | 基于天梯分数匹配对手，进行五子棋对战 |
| 聊天功能 | 玩家在下棋时进行实时聊天 |
| 在线管理 | 游戏大厅和房间用户在线状态管理 |

### 1.3 技术栈
| 类别 | 技术 |
|------|------|
| 后端语言 | C++11 |
| 网络框架 | WebSocket++ (HTTP/WebSocket) |
| 数据库 | MySQL 5.7 |
| JSON处理 | JsonCpp |
| 前端 | HTML/CSS/JS/AJAX |
| 开发环境 | Linux (CentOS-7.6/Ubuntu-22.04) |
| 构建工具 | Makefile/CMake |

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
│                         │ HTTP/WebSocket                     │
└─────────────────────────┼───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                      服务端层                                │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              HTTP/WebSocket 服务器                    │   │
│  │              (基于 WebSocket++ 实现)                  │   │
│  └─────────────────────────────────────────────────────┘   │
│        │              │              │                      │
│        ▼              ▼              ▼                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐      │
│  │ 用户管理 │  │ 在线管理 │  │ 房间/匹配管理        │      │
│  │  模块    │  │   模块   │  │       模块           │      │
│  └──────────┘  └──────────┘  └──────────────────────┘      │
│        │                                                   │
└────────┼───────────────────────────────────────────────────┘
         ▼
┌─────────────────────────────────────────────────────────────┐
│                      数据层                                  │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                    MySQL 数据库                       │   │
│  │    用户表 (user)  │  房间表 (room)                    │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 模块划分
```
online_gobang/
├── server/                 # 服务端代码
│   ├── db.hpp             # 数据库操作模块
│   ├── online.hpp         # 在线用户管理模块
│   ├── room.hpp           # 房间管理模块
│   ├── matcher.hpp        # 匹配器模块
│   ├── server.hpp         # 服务器主模块
│   ├── logger.hpp         # 日志模块
│   └── util.hpp           # 工具模块
├── client/                 # 客户端代码
│   ├── login.html         # 登录页面
│   ├── hall.html          # 游戏大厅页面
│   ├── room.html          # 游戏房间页面
│   └── static/            # 静态资源
└── makefile               # 构建脚本
```

---

## 三、里程碑规划

### 里程碑总览

| 阶段 | 名称 | 周期 | 主要目标 |
|------|------|------|----------|
| M1 | 环境搭建与基础框架 | 第1周 | 完成开发环境配置，搭建HTTP/WebSocket服务器框架 |
| M2 | 数据库与用户模块 | 第2周 | 完成数据库设计、用户注册/登录功能 |
| M3 | 在线管理与匹配模块 | 第3周 | 完成在线用户管理、匹配对战功能 |
| M4 | 游戏房间与对战逻辑 | 第4周 | 完成房间管理、五子棋对战核心逻辑 |
| M5 | 聊天功能与优化 | 第5周 | 完成实时聊天、性能优化、测试 |
| M6 | 部署与文档 | 第6周 | 完成部署上线、文档编写 |

---

## 四、详细实施计划



### M1: 环境搭建与基础框架 (第1周)

#### Week 1 任务分解

| 天数 | 任务 | 预估工时 | 交付物 |
|------|------|----------|--------|
| Day 1-2 | 开发环境搭建 | 8h | 完整的开发环境 |
| Day 3-4 | WebSocket++ 框架搭建 | 10h | HTTP/WebSocket服务器原型 |
| Day 5-6 | 基础工具模块开发 | 8h | 日志模块、JSON工具类 |
| Day 7 | 代码整合与测试 | 4h | 可运行的基础框架 |

#### 详细任务说明

**Day 1-2: 开发环境搭建**
- [ T ] 安装 Linux 系统 (CentOS-7.6 或 Ubuntu-22.04)
- [ T ] 安装编译工具链 (gcc/g++ 7+, gdb, make/cmake)
- [ T ] 安装依赖库:
  - [ T] Boost 库
  - [ T] JsonCpp 库
  - [ T] MySQL 开发包
  - [ T] WebSocket++ 库
- [T ] 配置 MySQL 数据库服务
- [ T] 克隆项目代码仓库

**Day 3-4: WebSocket++ 框架搭建**
- [T ] 实现 HTTP 请求处理框架
- [T ] 实现 WebSocket 连接管理
- [ T] 定义回调函数接口:
  - [T ] `on_open` - 连接建立
  - [ T] `on_close` - 连接关闭
  - [T ] `on_message` - 消息处理
  - [T ] `on_http` - HTTP请求处理

**Day 5-6: 基础工具模块**
- [ T] 实现日志模块 (`logger.hpp`)
- [ T] 实现 JSON 序列化/反序列化工具类 (`util.hpp`)
- [T ] 实现数据库连接工具类 (`db.hpp`)

---

### M2: 数据库与用户模块 (第2周)

#### Week 2 任务分解

| 天数 | 任务 | 预估工时 | 交付物 |
|------|------|----------|--------|
| Day 1-2 | 数据库表设计 | 6h | 数据库表结构 |
| Day 3-4 | 用户数据访问层 | 8h | user_table 类实现 |
| Day 5-6 | 用户业务逻辑层 | 10h | 注册/登录API实现 |
| Day 7 | 前端登录页面 | 6h | 登录/注册页面 |

#### 详细任务说明

**Day 1-2: 数据库设计**

```sql
-- 用户表
CREATE TABLE user (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(32) UNIQUE NOT NULL,
    password VARCHAR(128) NOT NULL,
    score INT DEFAULT 1500,           -- 天梯分数
    total_count INT DEFAULT 0,        -- 总比赛场次
    win_count INT DEFAULT 0           -- 胜利场次
);

-- 房间表 (可选，用于持久化)
CREATE TABLE room (
    id INT PRIMARY KEY AUTO_INCREMENT,
    white_id INT NOT NULL,
    black_id INT NOT NULL,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (white_id) REFERENCES user(id),
    FOREIGN KEY (black_id) REFERENCES user(id)
);
```

**Day 3-4: 用户数据访问层**

```cpp
class user_table {
public:
    bool insert(Json::Value &user);           // 用户注册
    bool login(Json::Value &user);            // 用户登录验证
    bool select_by_name(const std::string &name, Json::Value &user);
    bool select_by_id(uint64_t id, Json::Value &user);
    bool win(uint64_t id);                    // 胜利分数更新
    bool lose(uint64_t id);                   // 失败分数更新
};
```

**Day 5-6: 用户业务逻辑层**
- [ ] 实现用户注册接口 (`/reg`)
- [ ] 实现用户登录接口 (`/login`)
- [ ] 实现用户信息查询接口 (`/info`)
- [ ] 密码加密存储 (可选: MD5/SHA256)

**Day 7: 前端登录页面**
- [ ] 实现登录/注册页面 HTML
- [ ] 实现 AJAX 请求交互
- [ ] 实现页面跳转逻辑

---

### M3: 在线管理与匹配模块 (第3周)

#### Week 3 任务分解

| 天数 | 任务 | 预估工时 | 交付物 |
|------|------|----------|--------|
| Day 1-2 | 在线用户管理模块 | 8h | online_manager 类 |
| Day 3-4 | 匹配器模块设计 | 10h | matcher 类实现 |
| Day 5-6 | 匹配队列与线程池 | 8h | 多线程匹配实现 |
| Day 7 | 游戏大厅前端 | 6h | 大厅页面实现 |

#### 详细任务说明

**Day 1-2: 在线用户管理模块**

```cpp
class online_manager {
private:
    std::unordered_map<uint64_t, websocket_server::connection_ptr> _game_hall;
    std::unordered_map<uint64_t, websocket_server::connection_ptr> _game_room;
    std::mutex _mutex;

public:
    void enter_game_hall(uint64_t uid, const websocket_server::connection_ptr &conn);
    void exit_game_hall(uint64_t uid);
    void enter_game_room(uint64_t uid, const websocket_server::connection_ptr &conn);
    void exit_game_room(uint64_t uid);
    bool in_game_hall(uint64_t uid);
    bool in_game_room(uint64_t uid);
};
```

**Day 3-4: 匹配器模块设计**

匹配策略:
- 基于天梯分数进行匹配
- 分数相近的玩家优先匹配
- 等待时间过长时放宽匹配条件

```cpp
class matcher {
private:
    // 按分数范围分桶
    std::vector<block_queue<uint64_t>> _queues;
    room_manager *_rm;
    online_manager *_om;
    user_table *_ut;

public:
    void add_user(uint64_t uid);      // 添加玩家到匹配队列
    void del_user(uint64_t uid);      // 从队列移除玩家
    void match_handler();             // 匹配处理线程
};
```

**Day 5-6: 匹配队列与线程池**
- [ ] 实现阻塞队列 (`block_queue`)
- [ ] 实现多线程匹配处理
- [ ] 实现匹配成功后的房间创建

**Day 7: 游戏大厅前端**
- [ ] 实现大厅页面 HTML
- [ ] 实现开始匹配按钮交互
- [ ] 实现匹配状态显示

---

### M4: 游戏房间与对战逻辑 (第4周)

#### Week 4 任务分解

| 天数 | 任务 | 预估工时 | 交付物 |
|------|------|----------|--------|
| Day 1-2 | 房间类实现 | 10h | room 类 |
| Day 3-4 | 五子棋胜负判定 | 8h | check_win 算法 |
| Day 5-6 | 房间管理类 | 8h | room_manager 类 |
| Day 7 | 游戏房间前端 | 6h | 棋盘页面实现 |

#### 详细任务说明

**Day 1-2: 房间类实现**

```cpp
#define BOARD_ROW 15
#define BOARD_COL 15

class room {
private:
    uint64_t _room_id;
    uint64_t _white_id;
    uint64_t _black_id;
    std::vector<std::vector<int>> _board;  // 15x15 棋盘
    room_statu _statu;

public:
    Json::Value handle_chess(Json::Value &req);   // 处理下棋
    Json::Value handle_chat(Json::Value &req);    // 处理聊天
    void handle_exit(uint64_t uid);               // 处理退出
    void broadcast(Json::Value &rsp);             // 广播消息
};
```

**Day 3-4: 五子棋胜负判定**

```cpp
// 检查四个方向是否有五子连珠
bool five(int row, int col, int row_off, int col_off, int color) {
    // 横向: (0, 1)
    // 纵向: (1, 0)
    // 正斜: (-1, 1)
    // 反斜: (-1, -1)
}

uint64_t check_win(int row, int col, int color) {
    // 返回获胜玩家ID，无获胜返回0
}
```

**Day 5-6: 房间管理类**
- [ ] 房间创建与销毁
- [ ] 用户与房间映射管理
- [ ] 房间查找接口

**Day 7: 游戏房间前端**
- [ ] 15x15 棋盘绘制
- [ ] 落子交互
- [ ] 实时棋局更新

---

### M5: 聊天功能与优化 (第5周)

#### Week 5 任务分解

| 天数 | 任务 | 预估工时 | 交付物 |
|------|------|----------|--------|
| Day 1-2 | 聊天功能实现 | 6h | 实时聊天功能 |
| Day 3-4 | 敏感词过滤 | 4h | 消息过滤功能 |
| Day 5-6 | 性能优化 | 8h | 优化后的系统 |
| Day 7 | 集成测试 | 6h | 测试报告 |

#### 详细任务说明

**Day 1-2: 聊天功能实现**
- [ ] 房间内消息广播
- [ ] 前端聊天界面
- [ ] 消息历史记录 (可选)

**Day 3-4: 敏感词过滤**
- [ ] 实现敏感词检测
- [ ] 消息过滤逻辑

**Day 5-6: 性能优化**
- [ ] 数据库连接池优化
- [ ] WebSocket 连接管理优化
- [ ] 内存管理优化

**Day 7: 集成测试**
- [ ] 功能测试
- [ ] 压力测试
- [ ] Bug 修复

---

### M6: 部署与文档 (第6周)

#### Week 6 任务分解

| 天数 | 任务 | 预估工时 | 交付物 |
|------|------|----------|--------|
| Day 1-2 | 服务器部署 | 8h | 线上运行环境 |
| Day 3-4 | 安全配置 | 6h | 安全加固 |
| Day 5-6 | 文档编写 | 8h | 完整文档 |
| Day 7 | 项目总结 | 4h | 总结报告 |

#### 详细任务说明

**Day 1-2: 服务器部署**
- [ ] 云服务器选购
- [ ] 环境配置
- [ ] 服务部署
- [ ] 域名配置 (可选)

**Day 3-4: 安全配置**
- [ ] 防火墙配置
- [ ] 数据库安全加固
- [ ] 日志审计

**Day 5-6: 文档编写**
- [ ] 部署文档
- [ ] API 文档
- [ ] 使用手册

**Day 7: 项目总结**
- [ ] 项目复盘
- [ ] 经验总结
- [ ] 后续优化方向

---

## 五、风险评估与应对策略

| 风险 | 概率 | 影响 | 应对策略 |
|------|------|------|----------|
| WebSocket 连接不稳定 | 中 | 高 | 实现心跳检测、自动重连机制 |
| 匹配算法效率低 | 低 | 中 | 优化匹配算法、增加分桶策略 |
| 数据库性能瓶颈 | 中 | 高 | 使用连接池、优化SQL查询 |
| 并发安全问题 | 中 | 高 | 使用互斥锁、原子操作 |
| 前端兼容性问题 | 低 | 低 | 多浏览器测试、Polyfill |

---

## 六、验收标准

### 功能验收
- [ ] 用户能够正常注册、登录
- [ ] 用户能够在游戏大厅进行匹配
- [ ] 匹配成功后能够进入房间对战
- [ ] 五子棋对战逻辑正确，胜负判定准确
- [ ] 玩家能够在游戏中实时聊天
- [ ] 断线重连或异常退出处理正确

### 性能验收
- [ ] 支持 100+ 并发用户
- [ ] 匹配响应时间 < 30秒
- [ ] 消息延迟 < 100ms

### 安全验收
- [ ] 用户密码加密存储
- [ ] SQL 注入防护
- [ ] XSS 攻击防护
- [ ] 敏感词过滤

---

## 七、参考资源

- 项目代码仓库: https://gitee.com/qigezi/online_gobang.git
- WebSocket++ 文档: http://docs.websocketpp.org/
- JsonCpp 文档: https://github.com/open-source-parsers/jsoncpp
- MySQL C API: https://dev.mysql.com/doc/c-api/8.0/en/

---

*文档生成时间: 2026-03-23*