# 前端目录结构

> 前端代码的组织方式

---

## 概述

前端使用**原生 HTML/CSS/JavaScript**，无框架（无 React、Vue、Angular）。每个页面是一个自包含的 `.html` 文件，内联 `<style>` 和 `<script>`。可复用的 JavaScript 工具放在 `client/js/` 下，以 class 形式封装。

---

## 目录布局

```
client/
├── hall.html               # 游戏大厅页面（内联 HTML + CSS + JS）
├── js/
│   └── websocket.js        # GobangWebSocket 类封装
├── login.html              # （计划中，尚未实现）
└── room.html               # （计划中，尚未实现）
```

---

## 模块组织

- **页面**：每个页面是 `client/` 根目录下的独立 `.html` 文件，HTML、CSS、JS 全部内联
- **共享 JS**：可复用的类/工具放在 `client/js/` 下，作为独立 `.js` 文件
- **无构建步骤**：无 bundler、无转译器、无 npm —— 文件直接提供服务
- **无组件框架**：DOM 操作使用原生 `document.getElementById()` / `document.querySelector()`

---

## 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| HTML 页面 | `snake_case.html` | `hall.html`、`login.html` |
| JS 类 | `PascalCase` 文件名 | `websocket.js` 中的 `GobangWebSocket` |
| CSS 类名 | `kebab-case` | `.user-info`、`.match-button` |
| JS 变量 | `camelCase` | `appState`、`wsClient` |

---

## 参考示例

- 单页模式：`client/hall.html` —— 完整的大厅页面，样式和逻辑内联
- 共享 JS 封装：`client/js/websocket.js` —— `GobangWebSocket` 类，含重连、心跳、事件分发
