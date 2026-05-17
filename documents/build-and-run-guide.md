# 在线五子棋对战 — 构建与运行指南

> 适用环境：Rocky Linux 9 / Ubuntu 22.04 LTS

## 1. 依赖安装

### Rocky Linux 9

```bash
sudo dnf install -y gcc-c++ cmake make git pkg-config
sudo dnf install -y jsoncpp-devel websocketpp-devel gtest-devel
sudo dnf install -y openssl-devel mysql-devel picojson-devel

# jwt-cpp (header-only)
git clone https://github.com/Thalhammer/jwt-cpp.git
cd jwt-cpp && sudo cp -r include/jwt-cpp /usr/local/include/
```

### Ubuntu 22.04

```bash
sudo apt update
sudo apt install -y g++ cmake make git pkg-config
sudo apt install -y libjsoncpp-dev libwebsocketpp-dev libgtest-dev
sudo apt install -y libssl-dev libmysqlclient-dev nlohmann-json3-dev

# jwt-cpp (header-only)
git clone https://github.com/Thalhammer/jwt-cpp.git
cd jwt-cpp && sudo cp -r include/jwt-cpp /usr/local/include/
```

## 2. MySQL 配置

```bash
sudo systemctl start mysqld
sudo mysql -u root
```

```sql
CREATE DATABASE gobang_db CHARACTER SET utf8mb4;
CREATE USER 'gobang'@'localhost' IDENTIFIED BY 'your_password';
GRANT ALL ON gobang_db.* TO 'gobang'@'localhost';
FLUSH PRIVILEGES;

USE gobang_db;

CREATE TABLE user (
    id INT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(32) UNIQUE NOT NULL,
    password_hash VARCHAR(128) NOT NULL,
    score INT UNSIGNED DEFAULT 1500,
    total_count INT UNSIGNED DEFAULT 0,
    win_count INT UNSIGNED DEFAULT 0,
    status TINYINT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_score (score),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

## 3. 配置文件

```bash
cd source/config
cp server.conf.example server.conf
vim server.conf
```

需要修改的关键字段：

| 字段 | 说明 |
|------|------|
| `db_user` | MySQL 用户名 |
| `db_password` | MySQL 密码 |
| `db_name` | 数据库名（默认 `gobang_db`） |
| `jwt_secret` | 替换为随机字符串（≥ 32 字符） |

## 4. 构建

```bash
cd source
mkdir -p build && cd build
cmake ..
cmake --build .
```

构建产物在 `source/build/bin/` 下。

## 5. 运行测试

```bash
cd source/build

# M2 测试（需要 MySQL 运行中）
./bin/test_db_pool
./bin/test_user_table
./bin/test_security
./bin/test_auth_api

# M3 测试（无外部依赖）
./bin/test_online
./bin/test_block_queue
./bin/test_matcher
./bin/test_connection_manager
```

## 6. 启动服务

```bash
cd source/build
./bin/websocket_smoke
```

服务监听 `0.0.0.0:8080`：

| 端点 | 类型 | 说明 |
|------|------|------|
| `http://host:8080` | HTTP | 提示 "WebSocket server is running" |
| `ws://host:8080/ws` | WebSocket | 大厅匹配事件入口 |

按 `Ctrl+C` 优雅停止。

## 7. 当前功能状态

| 里程碑 | 状态 | 说明 |
|--------|------|------|
| M1 环境搭建 | ✅ 已完成 | |
| M2 数据库与用户模块 | ✅ 已完成 | 注册/登录/鉴权 |
| M3 在线管理与匹配 | 🟡 代码已落地 | 待联调验证 |
| M4 房间对战 | ⏳ 未开始 | |
| M5 聊天 | ⏳ 未开始 | |
| M6 部署 | ⏳ 未开始 | |

## 8. 常见问题

### cmake 找不到 jsoncpp

```bash
# Ubuntu
sudo apt install libjsoncpp-dev

# Rocky
sudo dnf install jsoncpp-devel
```

### cmake 找不到 mysqlclient

```bash
# 确认 MySQL 开发库已安装
sudo dnf install mysql-devel          # Rocky
sudo apt install libmysqlclient-dev   # Ubuntu

# 如仍失败，检查库文件是否存在
ls /usr/lib64/mysql/
```

### websocketpp 编译报错 asio_no_tls

CMakeLists.txt 已自动探测并回退到 `asio` 配置，正常不会阻塞。如失败：

```bash
sudo dnf install websocketpp-devel    # 确保版本 ≥ 0.8
```
