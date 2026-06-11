# 前端目录结构

## 布局

```text
client/
├── login.html          # 登录/注册（已迁移水墨书卷）
├── hall.html           # 游戏大厅（待迁移）
├── room.html           # 对战房间
├── css/
│   ├── tokens.css      # 全局设计 token（:root 变量）— 所有页面必须引用
│   ├── login.css       # login 页样式（含 .scroll 书卷、.ink-scene 氛围）
│   ├── hall.css        # hall 页样式（待建）
│   └── game.css        # room 页棋盘与对局 UI（已有，迁移时接入 token）
├── js/
│   ├── websocket.js    # WebSocket 客户端封装
│   └── game.js         # 棋盘绘制与对局逻辑
└── assets/             # 仅放必要 SVG；禁止用书卷整图 PNG 作卡片底
```

## 文件职责

| 文件 | 职责 | 禁止 |
|------|------|------|
| `tokens.css` | 颜色、字体、间距、圆角、动效、z-index | 页面布局、组件完整样式 |
| `<page>.css` | 单页布局、氛围层、组件组合 | 重复定义已在 token 中的色值 |
| `*.html` | 结构、文案、`css/` 与 `js/` 引用 | 大段内联 `<style>` |
| `game.js` | Canvas 棋盘、落子、WS 事件处理 | 修改 DOM 全局样式 |

## 引用顺序（HTML `<head>`）

```html
<link rel="stylesheet" href="css/tokens.css">   <!-- 若 page.css 已 @import 可省略重复 -->
<link rel="stylesheet" href="css/login.css">
```

`login.css` 当前通过 `@import url('tokens.css')` 引入 token；其他页面任选 **一种** 方式，避免重复加载。

## 与 DESIGN.md 的关系

- **DESIGN.md**：视觉真源（YAML frontmatter + 六段说明）
- **tokens.css**：运行时 CSS 变量，须与 DESIGN.md 颜色/字体命名对齐
- 新增 token 时：先更新 DESIGN.md frontmatter，再同步 `tokens.css`
