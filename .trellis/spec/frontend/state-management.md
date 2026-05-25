# 前端状态管理

> 状态管理方式

---

## 概述

前端使用简单的 **JavaScript 对象**管理页面状态，用 **`localStorage`** 持久化认证状态。无状态管理库（无 Redux、Vuex、Zustand 等）。

---

## 状态分类

### 1. 页面状态（内存中）

普通 JavaScript 对象跟踪当前页面状态。状态切换通过 `updateUI()` 函数更新 UI。

```javascript
const AppState = {
    IDLE: 'idle',
    MATCHING: 'matching',
    MATCHED: 'matched'
};

let currentState = AppState.IDLE;

function updateUI() {
    switch (currentState) {
        case AppState.IDLE:
            matchButton.disabled = false;
            matchButton.textContent = '开始匹配';
            break;
        case AppState.MATCHING:
            matchButton.disabled = true;
            matchButton.textContent = '取消匹配';
            break;
        case AppState.MATCHED:
            // 跳转到房间
            break;
    }
}
```

参考：`client/hall.html`

### 2. 认证状态（localStorage）

跨页面持久化：

```javascript
// 登录后保存
localStorage.setItem('gobang_token', token);
localStorage.setItem('gobang_user', JSON.stringify(userInfo));

// 页面加载时读取
const token = localStorage.getItem('gobang_token');
const user = JSON.parse(localStorage.getItem('gobang_user') || '{}');

// 登出时清除
localStorage.removeItem('gobang_token');
localStorage.removeItem('gobang_user');
```

键名：
| 键 | 值 | 格式 |
|----|-----|------|
| `gobang_token` | JWT token 字符串 | 纯字符串 |
| `gobang_user` | 用户信息对象 | JSON 字符串 |

---

## 页面加载流程

1. 检查 `localStorage` 中是否有 `gobang_token`
2. 如果没有，跳转到 `login.html`
3. 如果有，用 token 初始化 WebSocket 连接
4. 设置初始状态为 `IDLE`，调用 `updateUI()`

---

## WebSocket 事件处理

服务器事件更新页面状态并触发 `updateUI()`：

```javascript
const ws = new GobangWebSocket({
    url: `ws://${hostname}:8080/ws`,
    token: token,
    onEvent: (event, data) => {
        if (event === 'match.success') {
            currentState = AppState.MATCHED;
            updateUI();
            window.location.href = `room.html?room_id=${data.room_id}&color=${data.color}`;
        }
        // ...
    }
});
```

---

## 常见错误

- **在 localStorage 中存储敏感数据** —— 只存 token 和非敏感用户信息（id、username、score）
- **页面加载时不检查 token** —— 每个页面必须验证 token 存在
- **创建复杂的状态管理抽象** —— 保持简单，普通对象 + `updateUI()` switch 即可
