# 视觉设计系统

> 执行契约：将 [PRODUCT.md](../../../PRODUCT.md) 与 [DESIGN.md](../../../DESIGN.md) 落地为可引用的 CSS 与组件规则。

---

## 1. Scope / Trigger

- **Register**：`product`（工具型对局 UI）；登录/入口页允许更强 **brand** 表达（水墨、道家意象）
- **触发**：新建或改版 `client/` 任意页面、组件、token

---

## 2. 品牌与页面职责

| 页面 | 视觉强度 | 职责 |
|------|----------|------|
| `login.html` | 高（玄墨底 + 书卷 + 太极/印章/经文） | 品牌印象 + 认证表单 |
| `hall.html` | 中 | 匹配、在线状态、用户信息；玄墨底 + 纸面卡片 + 五境壁纸 |
| `room.html` | 低–中 | **对局优先**：棋盘、回合、计时、连接；token 化、无全屏壁纸 |

原则（来自 PRODUCT.md）：

1. **对局优先** — 装饰不得削弱核心状态可读性  
2. **水墨为骨，数字为肉** — 墨韵/留白为识别，交互保持现代 Web 习惯  
3. **一页一职责** — 大气意象集中在 login；hall/room 收敛为工具界面  
4. **活泼但不幼稚** — 可有性格与微交互，禁止低龄卡通  
5. **中文可读性第一** — 标签 ≥14px，移动输入 ≥16px  

---

## 3. Contracts — CSS Token（`tokens.css`）

### 颜色变量

| CSS 变量 | DESIGN.md | 用途 |
|----------|-----------|------|
| `--color-ink-deep` | `ink-deep` | 玄墨底、标题、深色面板 |
| `--color-ink-mid` | `ink-mid` | 次级标签、边框 |
| `--color-ink-wash` | `ink-wash` | 占位符、弱化文案 |
| `--color-paper` | `paper` | 纸白书写区 |
| `--color-paper-muted` | `paper-muted` | 输入框边框、分隔 |
| `--color-cinnabar` | `cinnabar` | 主 CTA（入弈、匹配） |
| `--color-cinnabar-deep` | `cinnabar-deep` | Primary hover |
| `--color-jade` / `--color-jade-soft` | `jade` / `jade-soft` | 成功、己方回合、连接正常 |
| `--color-gold-mist` | `gold-mist` | 分数、段位点缀 |
| `--color-text-primary` | `text-primary` | 正文墨字 |
| `--color-text-secondary` | `text-secondary` | 副标题 |
| `--color-text-on-ink` | `text-on-ink` | 墨底上的纸白字 |
| `--color-error` | （语义） | 表单错误，等同朱砂 |

**待 hall/room 迁移时补充**：~~`--color-board-wood`、`--color-board-line`~~ 已加入 `tokens.css`。

### 字体

```css
--font-display: "Noto Serif SC", "Source Han Serif SC", "Songti SC", serif;
--font-body: "Noto Sans SC", "Source Han Sans SC", "Microsoft YaHei", sans-serif;
```

| 用途 | 字体 | 规则 |
|------|------|------|
| 品牌标题、经文、竖排装饰 | `var(--font-display)` | 禁止用于密集表格/小号错误文案 |
| 表单、按钮、状态条 | `var(--font-body)` | 正文行高 1.6 |

Google Fonts 加载（login 已用）：

```css
@import url('https://fonts.googleapis.com/css2?family=Noto+Sans+SC:wght@400;500;600&family=Noto+Serif+SC:wght@500;600;700&display=swap');
```

### 语义色映射

| 状态 | Token |
|------|-------|
| 成功 / 已连接 | `jade` |
| 警告 / 匹配中 | `gold-mist` |
| 错误 / 断开 / 认输 | `cinnabar` / `cinnabar-deep` |

### 动效

| Token | 值 | 用途 |
|-------|-----|------|
| `--ease-out` | `cubic-bezier(0.22, 1, 0.36, 1)` | 按钮、输入、toast |
| `--duration-fast` | `150ms` | 微交互 |
| `--duration-normal` | `220ms` | toast 入场 |

禁止：bounce、elastic、对局页长时间入场动画。

### z-index

`dropdown(100) → sticky(200) → modal-backdrop(300) → modal(400) → toast(500)`  
当前 token：`--z-toast: 500`。

---

## 4. 氛围层（Login 已实现）

类名契约（可复用于 hall 弱化版）：

| 类名 | 作用 |
|------|------|
| `.ink-scene` | 固定全屏装饰容器，`pointer-events: none` |
| `.ink-scene__taiji` | 背景大太极水印 + 呼吸动画 |
| `.ink-scene__mountains` | 底部水墨山峦 SVG |
| `.ink-scene__verse` | 道德经经文（低对比） |
| `.login-page__vertical` | 竖排「玄」字装饰 |

**body 背景**：玄墨渐变 `#0a0908 → #1a1814`，禁止 `#1a1a2e` 旧深蓝套路。

---

## 5. 组件语义（按钮 / 表单）

### Primary（朱砂）

```css
background: var(--color-cinnabar);
color: var(--color-text-on-ink);
min-height: 44px;
```

文案示例：登录页主按钮 **「入 弈」**（letter-spacing 0.2em）。

### Ghost（墨线框）

```css
background: transparent;
color: var(--color-ink-deep);
border: 1.5px solid var(--color-ink-mid);
```

### 输入框（书卷内）

```css
background: rgba(250, 247, 242, 0.55);
border: 1px solid rgba(61, 56, 48, 0.22);
border-radius: 2px;  /* 书卷内偏直角，区别于圆角卡片风 */
```

Focus：`border-color: var(--color-ink-mid)` + 浅墨阴影，**不用** jade 环（login 已改为墨系 focus，与纸面一致）。

---

## 6. Anti-patterns（禁止）

来自 PRODUCT.md + 实现踩坑：

| 禁止 | 原因 |
|------|------|
| SaaS 奶油暖白全身底、紫色渐变 | 品牌反例 |
| `backdrop-filter` 毛玻璃默认 | login 旧版已弃用 |
| `#1a1a2e / #16213e` 深蓝渐变 | 旧默认，与水墨体系冲突 |
| 直铺带透明棋盘格的 PNG 作书卷底 | 与玄墨背景割裂 |
| `background-clip: text` 渐变标题 | AI 俗套 |
| 对局页全屏背景动画 | 干扰读棋 |
| 表单 `#authForm { justify-content: center }` | 会把按钮挤离输入框（已修复） |
| Toast `border-left` 粗色条 | impeccable 反模式；改用 `border-top` |

---

## 7. Validation & 视觉验收

| 检查项 | 通过标准 |
|--------|----------|
| 对比度 | 正文 ≥ 4.5:1 |
| 触控 | 可点击元素 ≥ 44×44px |
| 减少动效 | `prefers-reduced-motion: reduce` 关闭动画 |
| 书卷 | 无矩形白底/灰格外露；天杆地杆可见 |
| 品牌 | 登录页含太极 + 朱砂印「弈」之一致意象 |

---

## 8. Wrong vs Correct

### Wrong — 旧版内联卡片

```html
<style>
  body { background: linear-gradient(135deg, #1a1a2e, #16213e); }
  .login-container { backdrop-filter: blur(10px); }
</style>
```

### Correct — token + 外链 + 水墨体系

```html
<link rel="stylesheet" href="css/login.css">
<!-- body 玄墨底 + .scroll 书卷 + tokens.css 变量 -->
```

### Wrong — 书卷用 PNG 铺满

```css
.scroll__frame {
  background: url('../assets/scroll-paper.png') 100% 100%;
}
```

### Correct — 纯 CSS 宣纸 + 墨晕

见 [书卷组件](./scroll-component.md) 中 `.scroll__body` 多层 `radial-gradient` + `inset box-shadow` + `clip-path`。

---

## 9. 五境壁纸（Hall 专用）

大厅在基础层（玄墨底 + 纸面卡片）之上可选叠加五境全屏封面。

| 类名 | 职责 |
|------|------|
| `.story-wallpaper` | 固定全屏背景图；竖版素材用 `background-size: contain` + `center center`，`background-color: #0a0908` 填充 letterbox |
| `.story-wallpaper__veil` | 玄墨渐变遮罩，保证纸面卡片对比度 ≥ 4.5:1 |
| `.realm-picker` | 壁纸选择器容器 |
| `.realm-picker__trigger` | 「五境 · 换壁纸」按钮 |
| `.realm-picker__panel` | 展开面板（5 项横滑） |
| `.realm-card` / `.realm-card.is-active` | 缩略图项；选中边框 `--color-cinnabar` |

**数据**：manifest 见 [`story-covers.md`](./story-covers.md)；持久化键 `gobang_hall_wallpaper`（localStorage）。

**禁止**：

- room 页全屏铺五境封面（对局优先）
- 用 `scroll-paper.png` 作书卷底图（透明格问题，见 scroll-component 禁令）
- 竖版五境封面使用 `cover` 裁切（会丢失下半构图；应使用 `contain`）
- 壁纸模式下与 `.ink-scene--hall` 山峦同时高亮（启用壁纸时隐藏山峦）

**减少动效**：`prefers-reduced-motion: reduce` 时 `.story-wallpaper` 跳过 opacity transition。
