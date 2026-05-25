# 前端组件规范

> 组件构建方式

---

## 概述

本项目**不使用前端框架**。UI 通过原生 HTML 页面构建，内联 `<style>` 和 `<script>` 块。可复用的 JavaScript 功能封装为 **ES6 class**（不是 Web Components，不是 React 组件）。

---

## 页面结构

每个页面是自包含的 `.html` 文件：

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>在线五子棋 - 页面标题</title>
    <style>
        /* 页面所有样式内联 */
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: 'Microsoft YaHei', sans-serif; }
        /* ... */
    </style>
</head>
<body>
    <div class="container">
        <!-- 页面 HTML -->
    </div>
    <script src="js/websocket.js"></script>
    <script>
        // 页面所有逻辑内联
        // 状态管理、DOM 操作、WebSocket 事件
    </script>
</body>
</html>
```

参考：`client/hall.html`

---

## JavaScript 类模式

可复用功能封装为 ES6 class，放在 `client/js/` 下：

```javascript
class GobangWebSocket {
    constructor(options) {
        this.url = options.url;
        this.token = options.token;
        this.onEvent = options.onEvent || (() => {});
        // ...
    }
    connect() { /* ... */ }
    send(event, data = {}) { /* ... */ }
    // ...
}

// 同时支持模块和浏览器全局变量
if (typeof module !== 'undefined' && module.exports) {
    module.exports = GobangWebSocket;
} else {
    window.GobangWebSocket = GobangWebSocket;
}
```

参考：`client/js/websocket.js`

---

## DOM 操作

只用原生 DOM API —— 不用 jQuery 等库：

```javascript
// 获取元素
const btn = document.getElementById('matchButton');
const status = document.querySelector('.status-text');

// 更新 UI
btn.disabled = true;
btn.textContent = '匹配中...';
status.innerText = '正在匹配对手';
```

---

## 样式模式

- **所有样式内联**在每个 HTML 页面的 `<style>` 块中
- **无 CSS 框架**（无 Tailwind、Bootstrap 等）
- **暗色主题**：背景 `linear-gradient(135deg, #1a1a2e 0%, #16213e 100%)`
- **毛玻璃卡片**：`background: rgba(255, 255, 255, 0.1)` + `border-radius: 10px`
- **字体**：`'Microsoft YaHei', sans-serif`
- **配色**：深色背景白色文字，主色调 `#4a90d9`

参考：`client/hall.html` 的 `<style>` 块

---

## 常见错误

- **使用外部 CDN 依赖** —— 保持自包含
- **创建构建系统** —— 前端故意没有构建步骤
- **不必要地拆分页面逻辑到多个文件** —— 保持简单，内联即可
