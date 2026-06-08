# M4 Phase 4 部署检查清单与操作指令

> **目标**: 编译、测试、运行完整对战流程
> **环境**: Ubuntu (推荐 20.04/22.04) 或 WSL2
> **预计耗时**: 30-60 分钟

---

## 一、环境准备检查

### 1.1 系统依赖

```bash
# 检查是否已安装所有依赖
dpkg -l | grep -E "(libgtest|libjsoncpp|libmysqlclient|libssl|libwebsocketpp|libboost)"

# 如果缺失，执行以下安装命令
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libgtest-dev \
    libjsoncpp-dev \
    libmysqlclient-dev \
    libssl-dev \
    libwebsocketpp-dev \
    libboost-system-dev \
    libboost-thread-dev
```

**检查清单**:
- [ ] CMake >= 3.10
- [ ] G++ 支持 C++11
- [ ] libgtest-dev 已安装
- [ ] libjsoncpp-dev 已安装
- [ ] libmysqlclient-dev 已安装
- [ ] libssl-dev 已安装
- [ ] libwebsocketpp-dev 已安装
- [ ] libboost-system-dev 已安装
- [ ] libboost-thread-dev 已安装

### 1.2 MySQL 数据库

```bash
# 检查 MySQL 服务状态
sudo systemctl status mysql

# 如果未启动
sudo systemctl start mysql

# 如果未安装
sudo apt install -y mysql-server
sudo systemctl start mysql
sudo systemctl enable mysql
```

**检查清单**:
- [ ] MySQL 服务正在运行
- [ ] 可以使用 mysql 客户端连接
- [ ] 数据库 `gobang` 已创建（或使用默认数据库）

### 1.3 jwt-cpp 库

```bash
# 检查 jwt-cpp 是否已安装
ls /usr/local/include/jwt-cpp/jwt.h

# 如果未安装，从源码安装
cd /tmp
git clone https://github.com/Thalhammer/jwt-cpp.git
cd jwt-cpp
sudo cp -r include/jwt-cpp /usr/local/include/
```

**检查清单**:
- [ ] `/usr/local/include/jwt-cpp/jwt.h` 存在

---

## 二、代码拉取与更新

```bash
# 进入项目目录
cd /path/to/OnlineGobangBattle

# 拉取最新代码
git checkout main
git pull origin main

# 确认关键文件存在
ls -la source/include/room.hpp
ls -la source/include/game.hpp
ls -la source/include/websocket_handler.hpp
ls -la client/room.html
ls -la client/js/game.js
ls -la client/css/game.css
```

**检查清单**:
- [ ] 代码已更新到最新版本
- [ ] `room.hpp` 存在
- [ ] `game.hpp` 存在
- [ ] `websocket_handler.hpp` 存在
- [ ] `client/room.html` 存在
- [ ] `client/js/game.js` 存在
- [ ] `client/css/game.css` 存在

---

## 三、编译构建

### 3.1 清理旧构建（可选）

```bash
# 如果之前有构建缓存，建议清理
rm -rf source/build
mkdir -p source/build
```

### 3.2 CMake 配置

```bash
cd /path/to/OnlineGobangBattle

# 生成构建文件
cmake -S source -B source/build

# 预期输出：
# -- GTest: found
# -- OpenSSL: found at ...
# -- jwt-cpp: found at /usr/local/include
# -- WebSocket++: found at ...
# -- WebSocket++ config: using asio_no_tls
# -- MySQL: found via ...
```

**检查清单**:
- [ ] CMake 配置成功，无 FATAL_ERROR
- [ ] 所有依赖都显示 "found"

### 3.3 编译

```bash
# 编译所有目标（-j 表示并行编译）
cmake --build source/build -j

# 预期输出：
# [100%] Built target test_base
# [100%] Built target test_room
# [100%] Built target test_game
# [100%] Built target test_websocket_game
# ...
```

**检查清单**:
- [ ] 编译成功，无错误
- [ ] 所有测试目标都已生成

### 3.4 验证编译产物

```bash
# 查看生成的可执行文件
ls -la source/build/bin/

# 预期看到：
# test_base
# test_room
# test_game
# test_websocket_game
# ...
```

**检查清单**:
- [ ] `test_room` 可执行文件存在
- [ ] `test_game` 可执行文件存在
- [ ] `test_websocket_game` 可执行文件存在

---

## 四、运行测试

### 4.1 运行所有测试

```bash
cd /path/to/OnlineGobangBattle

# 运行所有测试并显示详细输出
ctest --test-dir source/build --output-on-failure

# 预期输出：
# Test project .../source/build
#     Start 1: test_base
# 1/8 Test #1: test_base ....................   Passed    0.01 sec
#     Start 2: test_room
# 2/8 Test #2: test_room ....................   Passed    0.02 sec
# ...
# 100% tests passed, 0 tests failed out of 8
```

**检查清单**:
- [ ] 所有测试通过（100% passed）
- [ ] 无测试失败

### 4.2 单独运行游戏相关测试（可选）

```bash
# 运行房间管理测试
cd source/build
./bin/test_room

# 运行游戏逻辑测试
./bin/test_game

# 运行 WebSocket 集成测试
./bin/test_websocket_game
```

**检查清单**:
- [ ] `test_room` 通过
- [ ] `test_game` 通过
- [ ] `test_websocket_game` 通过（8 个测试用例）

---

## 五、服务器配置（如果需要手动启动）

### 5.1 检查服务器入口

当前 CMakeLists.txt 中没有定义服务器可执行文件。如果需要手动启动服务器，需要：

1. **确认联调入口**（当前无独立 `main.cpp`，使用 smoke 服务）:
```bash
ls source/tests/websocket_smoke.cpp
ls source/build/bin/websocket_smoke
```

2. **如果没有，需要创建或从测试代码中启动**

### 5.2 数据库配置

```bash
# 登录 MySQL
mysql -u root -p

# 创建数据库（如果不存在）
CREATE DATABASE IF NOT EXISTS gobang CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

# 创建用户（可选）
CREATE USER 'gobang'@'localhost' IDENTIFIED BY 'your_password';
GRANT ALL PRIVILEGES ON gobang.* TO 'gobang'@'localhost';
FLUSH PRIVILEGES;

# 退出
exit
```

**检查清单**:
- [ ] 数据库 `gobang` 存在
- [ ] 有可用的数据库用户和密码

---

## 六、浏览器联调测试

### 6.1 启动服务器

**方式一：如果有独立的服务器程序**
```bash
cd source/build
./bin/gobang_server  # 或实际的服务器可执行文件
```

**方式二：使用测试服务器（如果可用）**
```bash
# 某些测试可能包含服务器启动逻辑
./bin/test_websocket_game --server
```

**方式三：使用 Python/Node 简易服务器（仅静态文件）**
```bash
# 在 client 目录启动简易 HTTP 服务器
cd client
python3 -m http.server 8080

# 或使用 Node.js
npx http-server -p 8080
```

### 6.2 访问页面

1. **打开浏览器**（推荐 Chrome/Firefox）

2. **访问登录页面**:
```
http://localhost:8080/login.html
```

3. **检查清单**:
- [ ] 页面正常加载，无 404 错误
- [ ] 样式正确显示
- [ ] 浏览器控制台无 JavaScript 错误

### 6.3 完整对战流程测试

**准备**：打开两个浏览器窗口（或一个正常窗口 + 一个无痕窗口）

**步骤**：

| 步骤 | 操作 | 预期结果 |
|------|------|----------|
| 1 | 窗口A：登录用户A | 登录成功，跳转到大厅 |
| 2 | 窗口B：登录用户B | 登录成功，跳转到大厅 |
| 3 | 窗口A：点击"开始匹配" | 状态变为"正在匹配..." |
| 4 | 窗口B：点击"开始匹配" | 双方都显示"匹配成功！" |
| 5 | 等待 2 秒 | 自动跳转到 `room.html` |
| 6 | 验证棋盘 | 15×15 网格正确显示 |
| 7 | 验证玩家信息 | 显示双方用户名和分数 |
| 8 | 黑方点击棋盘 | 落子成功，显示黑色棋子 |
| 9 | 验证白方窗口 | 同步显示黑方落子 |
| 10 | 白方点击棋盘 | 落子成功，显示白色棋子 |
| 11 | 验证黑方窗口 | 同步显示白方落子 |
| 12 | 点击"认输" | 弹出确认对话框 |
| 13 | 确认认输 | 双方都显示游戏结果 |
| 14 | 点击"返回大厅" | 跳转回大厅页面 |

**检查清单**:
- [ ] 匹配成功后自动跳转
- [ ] 棋盘正确显示
- [ ] 落子后双方实时同步
- [ ] 轮次正确切换
- [ ] 认输功能正常
- [ ] 游戏结果正确显示
- [ ] 返回大厅功能正常

### 6.4 五子连珠胜利测试

1. 重复上述步骤 1-7
2. 黑方连续落子形成五连（例如：横排 5 颗）
3. **预期**：第 5 颗落下后，双方都显示胜利/失败结果

**检查清单**:
- [ ] 五连触发胜利判定
- [ ] 胜方显示"你赢了！"
- [ ] 负方显示"你输了"
- [ ] 积分变化正确显示

### 6.5 断线重连测试

1. 游戏进行中，关闭一个窗口（或断开网络）
2. 重新打开页面并登录
3. **预期**：自动发送重连请求，恢复棋盘状态

**检查清单**:
- [ ] 断线后可以重连
- [ ] 棋盘状态正确恢复
- [ ] 可以继续落子

---

## 七、常见问题排查

### 7.1 编译错误

| 错误信息 | 原因 | 解决方案 |
|----------|------|----------|
| `fatal error: gtest/gtest.h: No such file` | GTest 未安装 | `sudo apt install libgtest-dev` |
| `fatal error: json/json.h: No such file` | JsonCpp 未安装 | `sudo apt install libjsoncpp-dev` |
| `fatal error: mysql.h: No such file` | MySQL 开发库未安装 | `sudo apt install libmysqlclient-dev` |
| `fatal error: jwt-cpp/jwt.h: No such file` | jwt-cpp 未安装 | 参考 1.3 节安装 |
| `undefined reference to 'boost::...'` | Boost 库未链接 | `sudo apt install libboost-system-dev libboost-thread-dev` |

### 7.2 测试失败

| 测试 | 失败原因 | 解决方案 |
|------|----------|----------|
| test_db_pool | MySQL 连接失败 | 检查 MySQL 服务状态和配置 |
| test_user_table | 数据库表不存在 | 确保数据库已初始化 |
| test_websocket_game | 端口占用 | 更改测试端口或关闭占用进程 |

### 7.3 运行时错误

| 错误 | 原因 | 解决方案 |
|------|------|----------|
| `Address already in use` | 端口 8080 被占用 | `lsof -i :8080` 查看并关闭占用进程 |
| `Connection refused` | 服务器未启动 | 先启动服务器 |
| `Segmentation fault` | 程序崩溃 | 检查 core dump 或用 gdb 调试 |

### 7.4 前端问题

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 页面空白 | JavaScript 错误 | 打开浏览器控制台（F12）查看错误 |
| 样式异常 | CSS 文件未加载 | 检查文件路径是否正确 |
| WebSocket 连接失败 | 地址/端口错误 | 检查 `websocket.js` 中的 URL |
| 落子无反应 | 事件未发送 | 检查控制台网络请求 |

---

## 八、快速检查脚本

创建一个脚本 `check_m4.sh`，一键检查所有前置条件：

```bash
#!/bin/bash

echo "=== M4 Phase 4 部署检查 ==="
echo ""

# 检查依赖
echo "[1/6] 检查系统依赖..."
deps=("cmake" "g++" "mysql")
for dep in "${deps[@]}"; do
    if command -v $dep &> /dev/null; then
        echo "  ✓ $dep 已安装"
    else
        echo "  ✗ $dep 未安装"
    fi
done

# 检查开发库
echo ""
echo "[2/6] 检查开发库..."
libs=("libgtest-dev" "libjsoncpp-dev" "libmysqlclient-dev" "libssl-dev" "libwebsocketpp-dev")
for lib in "${libs[@]}"; do
    if dpkg -l | grep -q "$lib"; then
        echo "  ✓ $lib 已安装"
    else
        echo "  ✗ $lib 未安装"
    fi
done

# 检查 jwt-cpp
echo ""
echo "[3/6] 检查 jwt-cpp..."
if [ -f "/usr/local/include/jwt-cpp/jwt.h" ]; then
    echo "  ✓ jwt-cpp 已安装"
else
    echo "  ✗ jwt-cpp 未安装"
fi

# 检查 MySQL 服务
echo ""
echo "[4/6] 检查 MySQL 服务..."
if systemctl is-active --quiet mysql; then
    echo "  ✓ MySQL 服务正在运行"
else
    echo "  ✗ MySQL 服务未运行"
fi

# 检查项目文件
echo ""
echo "[5/6] 检查项目文件..."
files=("source/include/room.hpp" "source/include/game.hpp" "source/include/websocket_handler.hpp" "client/room.html" "client/js/game.js")
for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        echo "  ✓ $file 存在"
    else
        echo "  ✗ $file 不存在"
    fi
done

# 检查构建目录
echo ""
echo "[6/6] 检查构建目录..."
if [ -d "source/build" ]; then
    echo "  ✓ source/build 存在"
    if [ -f "source/build/bin/test_websocket_game" ]; then
        echo "  ✓ test_websocket_game 已编译"
    else
        echo "  ✗ test_websocket_game 未编译"
    fi
else
    echo "  ✗ source/build 不存在"
fi

echo ""
echo "=== 检查完成 ==="
```

**使用方法**：
```bash
chmod +x check_m4.sh
./check_m4.sh
```

---

## 九、完成标志

当以下条件全部满足时，Phase 4 完成：

**编译测试**:
- [ ] `cmake -S source -B source/build` 成功
- [ ] `cmake --build source/build -j` 成功
- [ ] `ctest --test-dir source/build --output-on-failure` 全部通过

**浏览器联调**:
- [ ] 两个用户可以成功匹配
- [ ] 匹配成功后自动跳转到游戏房间
- [ ] 棋盘正确显示
- [ ] 落子后双方实时同步
- [ ] 五子连珠触发胜利判定
- [ ] 认输功能正常
- [ ] 游戏结束后可以返回大厅

**稳定性**:
- [ ] 连续进行 3 场对战无崩溃
- [ ] 断线后可以重连恢复

---

## 十、后续步骤

Phase 4 完成后：

1. **标记 M4 完成**
```bash
git tag v0.4.0
git push origin v0.4.0
```

2. **进入 M5 阶段**
- 聊天功能
- 和棋协议
- 观战模式

---

*文档版本：v1.0*
*制定日期：2026-06-01*
