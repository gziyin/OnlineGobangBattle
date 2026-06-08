# M4 Phase 4 验收记录

> **项目**: C++ 在线五子棋对战系统  
> **阶段**: M4 Phase 4 集成联调与端到端验证  
> **记录日期**: 2026-06-08  
> **记录人**: AI 助手  

---

## 一、验证环境说明

| 项 | 说明 |
|----|------|
| 验证主机 | Windows 10 开发机 |
| 构建工具 | PATH 中无 `cmake` / `g++`；`source/build/bin/` 仅含历史 `test_base` 产物 |
| 目标运行环境 | Rocky Linux 9 / Ubuntu 22.04（见 `documents/build-and-run-guide.md`） |

**结论**：本机无法执行完整 `cmake --build` 与 `ctest`。以下验收基于 **代码审查、测试用例覆盖核对、模块接线检查**；完整编译与浏览器联调须在 Linux 环境按 §四 命令复验。

---

## 二、静态验收（代码与测试覆盖）

### 2.1 编译目标完整性

[`source/CMakeLists.txt`](../../../source/CMakeLists.txt) 已注册 14 个可执行目标，含 M4 新增：

| 目标 | 标签 | M4 相关 |
|------|------|---------|
| `test_room` | unit | Phase 1 |
| `test_game` | integration | Phase 2 |
| `test_websocket_game` | integration | Phase 3 |
| `websocket_smoke` | smoke | 联调入口 |

### 2.2 Phase 1–3 模块接线

| 检查项 | 结果 | 依据 |
|--------|------|------|
| `room.hpp` 存在且无 WebSocket++ 依赖 | ✅ | 头文件仅标准库 + JsonCpp |
| `game.hpp` 集成 RoomManager / OnlineManager | ✅ | `GameController` 声明完整 |
| `websocket_handler.hpp` 注入 `GameController` | ✅ | `init(..., GameController*)`；分发 `game.move/giveup/reconnect` |
| `websocket_smoke.cpp` 组装完整对象链 | ✅ | 含 `game.hpp`、`room.hpp` 与 GameController 初始化 |
| 前端 `client/room.html` + `game.js` + `game.css` | ✅ | 文件存在 |
| 端到端测试 `test_websocket_game.cpp` | ✅ | 11 个 GTest 用例 |

### 2.3 单元/集成测试用例规模（grep 统计）

| 测试文件 | TEST 用例约数 |
|----------|---------------|
| `test_room.cpp` | 34 |
| `test_game.cpp` | 8 |
| `test_websocket_game.cpp` | 11 |

### 2.4 架构约束

| 约束 | 结果 |
|------|------|
| `connection_hdl` 仅出现在 `connection_manager.hpp`、`websocket_handler.hpp` | ✅ 业务头文件无 websocketpp 引用 |
| 业务模块通过 `user_id` + `ConnectionManager::send` 通信 | ✅ |

---

## 三、Phase 4 验收清单状态

### 3.1 编译与测试

- [x] Linux `ctest --output-on-failure`：2026-06-08 实测 **13/14** 通过
- [x] 唯一失败 `test_game::ProcessPendingTimeouts` 已定位：`on_game_over` 未 `destroy_room`
- [x] 已修复：[`game.hpp`](../../../source/include/game.hpp) `on_game_over` 末尾调用 `destroy_room`
- [ ] 修复后复验 `ctest` 14/14 通过（需在 Linux 重新 `cmake --build` 后执行）

### 3.2 浏览器联调（待 Linux + 双窗口复验）

- [ ] `./bin/websocket_smoke` 监听 8080
- [ ] 双用户：注册/登录 → 匹配 → 进 `room.html` → 落子 → 胜负/认输 → 回大厅
- [ ] 断线 60 秒内 `game.reconnect` 恢复
- [ ] 超时判负（配置 `move_timeout`）

### 3.3 已通过（静态）

- [x] M4 Phase 1–3 代码与 CMake 目标齐全
- [x] WebSocket 游戏事件接线完成
- [x] 前端房间页与脚本就位
- [x] 文档同步（README、build-guide、project_plan、M4 计划、本报告）

---

## 四、Linux 复验命令

```bash
cd source
mkdir -p build && cd build
cmake ..
cmake --build .
ctest --output-on-failure

# 仅 M4 相关
ctest -R 'test_room|test_game|test_websocket_game' --output-on-failure

# 联调服务
./bin/websocket_smoke
# 浏览器：client/login.html → hall.html → 匹配后进 room.html
```

---

## 五、已知风险与后续

1. **生产入口**：尚无独立 `main.cpp`，联调依赖 `websocket_smoke`（M6 待统一）。
2. **MySQL 依赖**：integration 测试需本地 MySQL 与 `source/config/server.conf`。
3. **浏览器 E2E**：静态页需配置正确 WebSocket 地址（默认 `ws://localhost:8080/ws`）。

---

*文档版本：v1.0*
