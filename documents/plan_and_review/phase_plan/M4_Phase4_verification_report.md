# M4 Phase 4 验收记录

> **项目**: C++ 在线五子棋对战系统  
> **阶段**: M4 Phase 4 集成联调与端到端验证  
> **记录日期**: 2026-06-08（初版）；**2026-06-11 更新**（架构重构 + CTest 全量通过）  
> **记录人**: AI 助手  

---

## 一、验证环境说明

| 项 | 说明 |
|----|------|
| 初版验证主机 | Windows 10 开发机（无 cmake/g++，静态审查） |
| **2026-06-11 复验** | Ubuntu VM（`/opt/projects/OnlineGobangBattle`），`cmake --build` + `ctest` 全量通过 |
| 目标运行环境 | Rocky Linux 9 / Ubuntu 22.04（见 `documents/build-and-run-guide.md`） |

---

## 二、静态验收（代码与测试覆盖）

### 2.1 编译目标完整性

[`source/CMakeLists.txt`](../../../source/CMakeLists.txt) 含 M4 相关目标及重构新增项：

| 目标 | 标签 | 说明 |
|------|------|------|
| `test_room` | unit | Phase 1 |
| `test_game` | integration | Phase 2 |
| `test_websocket_game` | integration | Phase 3 端到端（11 用例） |
| `gobang_server` | — | **生产入口**（`GobangServer`） |
| `websocket_smoke` | smoke | 编译/绑定 smoke 验证 |
| `gobang_core` | — | 静态库（game / websocket_handler / server_context） |

### 2.2 Phase 1–3 模块接线

| 检查项 | 结果 | 依据 |
|--------|------|------|
| `room.hpp` 无 WebSocket++ 依赖 | ✅ | 标准库 + JsonCpp |
| `game.hpp` + `game.cpp` | ✅ | `GameController`；Asio 回合/断线 timer |
| `websocket_handler` 注入 `GameController` | ✅ | `src/websocket_handler.cpp` |
| `GobangServer` 统一组装 | ✅ | `server_context.cpp`；`init_asio` 后再绑定 io_service |
| 前端 `room.html` + `game.js` | ✅ | 文件存在 |
| `test_websocket_game.cpp` | ✅ | 11 个 GTest 用例（含 `GameTimeout`） |

### 2.3 架构约束

| 约束 | 结果 |
|------|------|
| `GameController` 依赖 `IMessageSender`，不直接 include WebSocket++ | ✅ |
| `connection_hdl` 仅出现在连接管理与 WS 接线层 | ✅ |
| 匹配回调经 `io_service.post` 投递 IO 线程 | ✅ |

---

## 三、Phase 4 验收清单状态

### 3.1 编译与测试

- [x] Linux `ctest --output-on-failure`：**2026-06-11 全量通过**（含 `GameTimeout`）
- [x] 历史问题 `ProcessPendingTimeouts` / 双通道 timer worker：已随 C1 架构重构移除
- [x] `GameTimeout` 失败（init_asio 顺序）：已修复 commit `e42e4e0`

### 3.2 浏览器联调（可选复验）

- [ ] `./bin/gobang_server` 监听 8080
- [ ] 双用户：注册/登录 → 匹配 → `room.html` → 落子 → 胜负/认输 → 回大厅
- [ ] 断线 60 秒内 `game.reconnect` 恢复
- [ ] 超时判负（`move_timeout` / 测试夹具 2s）

### 3.3 已通过

- [x] M4 Phase 1–3 代码与 CMake 目标齐全
- [x] 生产入口 `gobang_server` + `GET /health`
- [x] WebSocket 游戏事件接线完成
- [x] 文档同步（README、build-guide、project_plan、refactor 记录、本报告）

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

# 生产 / 联调
./bin/gobang_server
# GET http://localhost:8080/health

# Smoke（可选）
./bin/websocket_smoke

# 浏览器：client/login.html → hall.html → 匹配后进 room.html
```

---

## 五、已知风险与后续

1. **浏览器 E2E**：静态页需配置正确 WebSocket 地址（默认 `ws://localhost:8080/ws`）；前端 `config.js` 外置待 M6。
2. **MySQL 依赖**：integration 测试需 `server.conf` / `server.conf.test`。
3. **M5**：`game.chat` 尚未实现。
4. **M6 剩余**：CORS Origin 白名单、OpenAPI 文档。

---

*文档版本：v1.1（2026-06-11）*
