---
name: Online Gobang Battle
description: 水墨写意 × 现代对局工具——活泼但不幼稚的在线五子棋界面
colors:
  ink-deep: "#1a1814"
  ink-mid: "#3d3830"
  ink-wash: "#6b6358"
  paper: "#f5f0e8"
  paper-muted: "#e8e0d4"
  cinnabar: "#c23b22"
  cinnabar-deep: "#8f2a18"
  jade: "#2d6a4f"
  jade-soft: "#40916c"
  gold-mist: "#c9a227"
  board-wood: "#c4a35a"
  board-line: "#5c4a32"
  cloud-fog: "#d9d3c7"
  rain-blue: "#4a6670"
  surface-elevated: "#faf7f2"
  text-primary: "#1a1814"
  text-secondary: "#5c5348"
  text-on-ink: "#f5f0e8"
typography:
  display:
    fontFamily: "\"Noto Serif SC\", \"Source Han Serif SC\", \"Songti SC\", serif"
    fontSize: "clamp(1.75rem, 4vw, 2.75rem)"
    fontWeight: 600
    lineHeight: 1.2
    letterSpacing: "0.02em"
  body:
    fontFamily: "\"Noto Sans SC\", \"Source Han Sans SC\", \"Microsoft YaHei\", sans-serif"
    fontSize: "1rem"
    fontWeight: 400
    lineHeight: 1.6
    letterSpacing: "normal"
  label:
    fontFamily: "\"Noto Sans SC\", \"Source Han Sans SC\", sans-serif"
    fontSize: "0.875rem"
    fontWeight: 500
    lineHeight: 1.4
    letterSpacing: "0.01em"
rounded:
  sm: "6px"
  md: "12px"
  lg: "20px"
  full: "9999px"
spacing:
  xs: "4px"
  sm: "8px"
  md: "16px"
  lg: "24px"
  xl: "40px"
components:
  button-primary:
    backgroundColor: "{colors.cinnabar}"
    textColor: "{colors.text-on-ink}"
    rounded: "{rounded.md}"
    padding: "12px 28px"
  button-primary-hover:
    backgroundColor: "{colors.cinnabar-deep}"
    textColor: "{colors.text-on-ink}"
    rounded: "{rounded.md}"
    padding: "12px 28px"
  button-ghost:
    backgroundColor: "transparent"
    textColor: "{colors.text-primary}"
    rounded: "{rounded.md}"
    padding: "10px 20px"
  card-surface:
    backgroundColor: "{colors.surface-elevated}"
    textColor: "{colors.text-primary}"
    rounded: "{rounded.lg}"
    padding: "20px 24px"
  board-frame:
    backgroundColor: "{colors.board-wood}"
    textColor: "{colors.board-line}"
    rounded: "{rounded.sm}"
    padding: "10px"
---

<!-- SEED: 基于 init 访谈生成。login 已落地；执行契约见 .trellis/spec/frontend/。hall/room 迁移后请用 `/impeccable document` 扫描回写 token。 -->

## Overview

视觉系统以 **水墨留白 + 朱砂点睛** 为品牌识别，以 **纸白/墨黑层次** 替代现有深蓝渐变默认。登录页可呈现写意氛围（云雾、墨迹、星辰意象）；大厅与房间页收敛为清晰的产品界面，棋盘与状态信息始终置顶。

技术栈约束：原生 `client/` 多页 HTML + CSS + ES6，无 React/Vue。优先抽取共享 `tokens.css`，消除三页内联样式重复。

## Colors

| 角色 | Token | 用途 |
|------|-------|------|
| 墨深 | `ink-deep` | 页眉、深色面板、夜间对局氛围底 |
| 纸白 | `paper` | 主内容区、表单、卡片底 |
| 朱砂 | `cinnabar` | 主 CTA、重要强调（匹配、确认） |
| 玉色 | `jade` | 己方回合、连接正常、正向状态 |
| 金雾 | `gold-mist` | 分数、段位、成就点缀 |
| 木纹 | `board-wood` | 棋盘外框与落子区域 |

- 背景避免 SaaS 奶油暖白 band（见 PRODUCT.md Anti-references）。
- 彩色背景上的文字用更深同色相或 `text-on-ink`，不用浅灰 `#aaa`。
- 语义色：成功 `jade`、警告 `gold-mist`、危险 `cinnabar`、断开 `cinnabar-deep`。

## Typography

- **Display（标题/品牌）**：Noto Serif SC / 思源宋体——呼应水墨书卷气；登录页与大标题使用。
- **Body/UI**：Noto Sans SC / 思源黑体——中文 UI 扫读；表单、按钮、状态条。
- 正文行长控制在 65–75ch；对局页标签用 `label` scale。
- 禁止 Display 字体用于密集数据表格或小号错误提示。

## Elevation

- **纸面卡片**：`surface-elevated` + 轻阴影 `0 4px 24px rgba(26, 24, 20, 0.08)`，无玻璃拟态默认。
- **墨底浮层**：modal / 通知用 `ink-deep` 85% 遮罩 + `paper` 内容面板。
- **棋盘**：`board-frame` 实体木纹感；阴影略强以突出对局焦点。
- z-index 语义刻度：`dropdown(100) → sticky(200) → modal-backdrop(300) → modal(400) → toast(500)`。

## Components

### 按钮

- Primary：朱砂底 + 纸白字；hover 加深至 `cinnabar-deep`。
- Ghost：透明底 + 墨字边框 1px `ink-wash`。
- Danger（认输）：`cinnabar`，禁用态降饱和而非纯灰。

### 表单（login）

- 输入框：纸白底、`paper-muted` 边框，focus 环 `jade-soft` 2px。
- 避免 login 页全屏毛玻璃容器；可用墨迹装饰边框或局部水墨背景图。

### 大厅（hall）

- 用户信息区：横向卡片，头像用墨色圆形 + 金字段位。
- 匹配按钮：Primary + 轻微墨晕 hover（CSS `box-shadow` 扩散，非 bounce）。

### 房间（room）

- 对手/己方信息条：对称布局，回合高亮用 `jade` / `gold-mist` 左边线 3px（对局状态允许细强调边，非装饰卡片）。
- 棋盘：Canvas 保持 15×15；网格线 `board-line`，落子黑白对比清晰。
- 计时器：等宽数字，`label` 字号，避免闪烁动画。

### 连接状态

- 固定左下角胶囊：`connected` 玉色 / `connecting` 金雾 / `disconnected` 朱砂。

## Do's and Don'ts

**Do**

- 用留白划分信息层级；大气意象集中在登录/空状态。
- 共享 CSS 变量（`tokens.css`）统一三页。
- 落子、匹配成功用 150–250ms ease-out 微动效。
- 提供 `prefers-reduced-motion` 降级。

**Don't**

- 不要 SaaS 紫渐变、奶油暖白全身底、儿童卡通角色。
- 不要三页各写一套内联 `<style>` 重复渐变。
- 不要对局页全屏背景动画干扰读棋。
- 不要使用 `background-clip: text` 渐变标题。
- 不要无意义卡片套卡片。
