# 前端质量规范

> 前端代码质量标准

---

## 概述

前端**无 lint、无测试框架、无构建步骤**。通过手动浏览器测试和一致的编码模式保证质量。

---

## 禁止的模式

1. **外部 CDN 依赖** —— 保持自包含
2. **npm/package.json/bundler** —— 前端无构建步骤
3. **jQuery 或其他工具库** —— 使用原生 DOM API
4. **内联事件处理器**（`onclick="..."`） —— 在 script 中用 `addEventListener()`
5. **`var` 声明** —— 只用 `const` 和 `let`
6. **`==` 比较** —— 始终用 `===` 和 `!==`

---

## 必须遵循的模式

1. **`GobangWebSocket` 类**管理所有 WebSocket 通信 —— 不直接用 `new WebSocket()`
2. **`localStorage`** 持久化认证状态 —— 使用 `gobang_token` 和 `gobang_user` 键
3. **页面加载时检查 token** —— token 缺失则跳转 `login.html`
4. **`updateUI()` 函数** —— 单一函数同步 DOM 状态与应用状态
5. **`<script src="js/websocket.js">`** 放在内联脚本之前 —— 确保类可用

---

## 测试

**无自动化前端测试**基础设施。手动测试流程：

1. 在浏览器中打开页面
2. 检查控制台有无错误
3. 测试正常流程（注册 -> 登录 -> 匹配 -> 跳转）
4. 测试边界情况（token 过期、WebSocket 断连、快速点击）

---

## WebSocket 协议

所有 WebSocket 消息使用以下 JSON 格式：

```javascript
// 客户端 -> 服务器
{"event": "事件名", "data": {...}}

// 服务器 -> 客户端
{"event": "事件名", "data": {...}}
```

已冻结的事件（M4 接口冻结草案）：
- 客户端：`auth`、`match.start`、`match.cancel`、`game.move`、`game.chat`、`game.giveup`、`ping`
- 服务器：`auth.success`、`match.waiting`、`match.success`、`game.start`、`game.move`、`game.chat`、`game.over`、`game.reconnect`、`error`、`pong`

参考：`documents/m4_interface_freeze_draft.md`

---

## 常见错误

- **不处理 WebSocket 重连** —— 使用 `GobangWebSocket`，内置重连（5 次尝试，3 秒间隔）
- **忘记 CORS 头** —— 服务器设置了 `Access-Control-Allow-Origin: *`，但新接口需检查
- **在 localStorage 中存储密码** —— 只存 JWT token 和非敏感用户信息
