# 前端编码约定

---

## 技术栈

- HTML5、CSS3、ES6+ 原生实现
- **禁止** 引入 React/Vue 等框架（除非项目计划变更）
- 构建：无前端打包；静态文件由 `gobang_server` 或 `npx serve client` 提供

---

## HTML

- `lang="zh-CN"`，`viewport` meta 必填
- 语义标签：`<main>`、`<header>`、`<article class="scroll">`、`<form>`
- 表单：`autocomplete`、`aria-live`（错误区）、`role="alert"`（`#errorMsg`）
- 装饰层：`aria-hidden="true"`

## CSS

1. 页面样式放 `css/<page>.css`，**禁止** 新增大段内联 `<style>`
2. 颜色/间距 **必须** 优先使用 `tokens.css` 变量
3. 书卷/氛围详见 [scroll-component.md](./scroll-component.md)
4. 响应式断点：主断点 `520px`（login 已用）
5. 所有非必要 `@keyframes` 须在 `prefers-reduced-motion: reduce` 中禁用

## JavaScript

### 认证 API

```javascript
const API_BASE = window.location.origin + '/api/v1/auth';
// POST /login | /register  body: { username, password }
```

### sessionStorage 键（契约）

| 键 | 类型 | 内容 |
|----|------|------|
| `gobang_token` | string | JWT |
| `gobang_user` | JSON string | `{ id, username, score }` |

登录成功写入后跳转 `hall.html`；**不得** 改名除非同步后端文档与 hall/room 读取逻辑。

### WebSocket

- 封装：`js/websocket.js`（`GobangWebSocket` 类）
- 地址：当前多硬编码；M6 计划 `config.js` — 新增配置时更新本 spec 与 cross-layer guide

---

## 页面跳转链

```text
login.html → hall.html → room.html（匹配成功）
```

改版时保持文件名稳定，避免破坏文档与部署路径。

---

## Design Decision: 视觉规范双源

**Context**：impeccable 引入 `PRODUCT.md` / `DESIGN.md`，同时需要 Trellis 可执行 spec。

**Decision**：

- `DESIGN.md` — 设计师/AI 视觉真源（Stitch 兼容 YAML）
- `.trellis/spec/frontend/` — 工程师/AI 实现契约（路径、类名、禁止项）
- `client/css/tokens.css` — 运行时变量，与 DESIGN.md 同步

**扩展**：hall/room 迁移完成后，运行 `/impeccable document` 扫描模式回写 DESIGN.md 中未落地的 token（如 `board-wood`）。

---

## Common Mistake: 书卷 PNG 直铺

**Symptom**：玄墨背景上出现白块或灰白棋盘格，视觉割裂。

**Cause**：参考图 PNG 带透明/留白，被 `background-size: 100% 100%` 拉伸铺满。

**Fix**：改用 [scroll-component.md](./scroll-component.md) 纯 CSS 结构。

**Prevention**：`assets/` 仅放必要 SVG；书卷禁止整图 raster 底。

---

## Common Mistake: 按钮与输入框间距过大

**Symptom**：密码框与「入弈/注册」之间大片空白。

**Cause**：`#authForm { flex: 1; justify-content: center; }`。

**Fix**：移除垂直居中；`.btn-group { margin-top: var(--space-sm); }`。
