# Journal - gzy (Part 1)

> AI development session journal
> Started: 2026-05-25

---



## Session 1: 防止重复登录

**Date**: 2026-06-02
**Task**: 防止重复登录
**Branch**: `main`

### Summary

在 WebSocket 认证阶段添加 is_online 检查，已在线账号拒绝新连接并返回错误码 4009；前端处理 4009 显示'账号已登录'并跳转登录页

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `f7ab4e7` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete

---

## Session 2: 断线重连机制

**Date**: 2026-06-03
**Task**: 断线重连机制
**Branch**: `main`

### Summary

实现五子棋游戏的断线重连机制。玩家退出页面后重新登录时，向其发送重连请求（含棋盘预览）。同意则恢复游戏，拒绝/超时则判负加分。解决了 4009 防重复登录拦截、房间过早销毁、重连竞态条件等问题。

### Problems Encountered

1. **4009 防重复登录**：`is_online` 检查太严格，WebSocket 断线后重连时 `on_close` 可能还未执行导致被拒绝。解决：`is_online` 为 true 时直接替换旧连接。
2. **房间过早销毁**：`on_game_over` 在断线超时时立即销毁房间，导致对手重连时房间不存在。解决：`on_game_over` 不销毁房间，由 `on_close` 自然清理。
3. **服务器二进制未更新**：代码已推送到服务器但未重新编译。解决：用 `strings` 命令验证二进制内容。

### Main Changes

- `room.hpp`：新增 `DisconnectState` 结构体和断线管理方法
- `game.hpp`：断线超时逻辑、`handle_reconnect_accept/reject`、`cleanup_finished_room`
- `websocket_handler.hpp`：Auth 路径改造（替换旧连接而非拒绝）、新增事件处理、`send_game_over_to_reconnector`
- `hall.html`：重连模态框（棋盘预览 canvas + 倒计时 + 接受/拒绝按钮）
- `game.js`：对手断线覆盖层（半透明遮罩 + 倒计时）

### New WebSocket Events

| 事件 | 方向 | 说明 |
|---|---|---|
| `reconnect.available` | S→C | 有活跃游戏，含 board、opponent、timeout_remaining |
| `reconnect.accept` | C→S | 同意重连 |
| `reconnect.reject` | C→S | 拒绝重连，判负 |
| `reconnect.accepted` | S→C | 跳转房间页 |
| `opponent.disconnected` | S→C | 对手断线通知 |
| `opponent.reconnected` | S→C | 对手已重连 |

### Git Commits

| Hash | Message |
|------|---------|
| `9bd7fcb` | feat(reconnect)：断线重连机制，支持退出页面后返回恢复游戏 |
| `be4b4d2` | fix(websocket_handler)：已在线用户重连时直接替换旧连接，不再拒绝 4009 |
| `5020449` | fix(reconnect)：断线超时后保留房间，对手重连可查看结果 |

### Status

[~] **大部分完成** — 基本重连流程可用

### Remaining Issues

1. 对手断线倒计时结束后卡在 0 秒，未跳转回大厅结算胜负
2. 等待重连时，另一个页面登录同一账号会出现冲突
3. 需要在远程服务器编译验证完整流程


## Session 2: 问题7页面跳转竞态与重连修复

**Date**: 2026-06-03
**Task**: 问题7页面跳转竞态与重连修复
**Branch**: `main`

### Summary

grace延迟断线解决大厅跳转误断线；接入process_timers并修复pending/auth分流恢复真断线重连；修复timer bad_function_call；归档06-02-reconnect并写入断线grace spec

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `d0a0361` | (see git log) |
| `547ec99` | (see git log) |
| `8091ee7` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete
