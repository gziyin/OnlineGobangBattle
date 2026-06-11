# 前端开发规范

> 原生 HTML/CSS/ES6 前端；水墨道家视觉体系；对局信息优先。

---

## 概述

本目录规范 `client/` 下静态前端的实现契约。战略层见项目根目录 [PRODUCT.md](../../../PRODUCT.md)（用户、品牌、反例）与 [DESIGN.md](../../../DESIGN.md)（视觉 token、组件语义）。**实现时以本 spec 的可执行规则为准；DESIGN.md 为视觉真源，变更 token 须双向同步。**

当前进度：**login 页**已落地水墨书卷风格；**hall / room** 仍为旧版内联样式，迁移中。

---

## 规范索引

| 规范 | 描述 | 状态 |
|------|------|------|
| [目录结构](./directory-structure.md) | `client/` 文件布局与命名 | 已填充 |
| [视觉设计系统](./visual-design-system.md) | Token、字体、页面职责、语义色、动效 | 已填充 |
| [书卷组件](./scroll-component.md) | 古代书卷 UI 结构与纯 CSS 实现契约 | 已填充 |
| [编码约定](./conventions.md) | HTML/CSS/JS 模式、存储键、禁止事项 | 已填充 |

---

## Pre-Development Checklist

开发前 **必须** 阅读：

1. [PRODUCT.md](../../../PRODUCT.md) — Register、品牌气质、Anti-references、对局优先原则
2. [DESIGN.md](../../../DESIGN.md) — 颜色/字体/组件语义
3. [目录结构](./directory-structure.md)
4. [视觉设计系统](./visual-design-system.md)
5. 若改登录或品牌入口页 → [书卷组件](./scroll-component.md)
6. [编码约定](./conventions.md)
7. 若页面涉及 WebSocket / API → [guides/cross-layer-thinking-guide.md](../guides/cross-layer-thinking-guide.md)

---

## Quality Check

提交前核对：

- [ ] 新样式使用 `tokens.css` 变量，无硬编码重复色值（除非书卷渐变等局部效果）
- [ ] 无整页内联 `<style>` 新增；页面样式在 `css/<page>.css`
- [ ] 符合 PRODUCT.md Anti-references（无 SaaS 奶油底、无玻璃拟态默认、无深蓝渐变套路）
- [ ] 对局相关页（hall/room）装饰不遮挡回合、连接、计时、胜负信息
- [ ] 正文对比度 ≥ 4.5:1；按钮热区 ≥ 44×44px
- [ ] `@media (prefers-reduced-motion: reduce)` 已覆盖非必要动画
- [ ] 书卷/氛围装饰使用 **纯 CSS/SVG**，未直铺带透明棋盘格的 PNG 底图
- [ ] `sessionStorage` 键名与 [编码约定](./conventions.md) 一致
- [ ] 未改动 WebSocket 事件名与后端契约（跨层变更须先读 cross-layer guide）
