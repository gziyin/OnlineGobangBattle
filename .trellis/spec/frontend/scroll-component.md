# 书卷组件（Scroll）

> 古代悬挂书卷 UI：**纯 CSS/SVG 实现**，参照传统宣纸+天杆视觉，禁止直铺 PNG 底图。

---

## 1. Scope / Trigger

- 用于 **login** 及未来品牌入口/空状态
- 修改 `.scroll*` 类名或结构前必读本文

---

## 2. HTML 结构（契约）

```html
<article class="scroll">
  <div class="scroll__roller scroll__roller--top" aria-hidden="true">
    <span class="scroll__knob"></span>
    <span class="scroll__bar"></span>
    <span class="scroll__knob"></span>
  </div>
  <div class="scroll__body">
    <div class="scroll__content">
      <!-- 品牌区 .login-brand -->
      <!-- 表单 #authForm -->
      <!-- 底饰 .scroll__footer（八卦点） -->
    </div>
  </div>
  <div class="scroll__roller scroll__roller--bottom" aria-hidden="true">
    <span class="scroll__knob"></span>
    <span class="scroll__bar"></span>
    <span class="scroll__knob"></span>
  </div>
</article>
```

### 品牌区装饰（login 已实现）

| 元素 | 类名 / 标签 | 说明 |
|------|-------------|------|
| 太极图标 | `.login-brand__taiji`（内联 SVG） | 与 `.dao-seal` 并列 |
| 朱砂印 | `.dao-seal` > `弈` | 2.5px 朱砂边框，微旋转 -4deg |
| 主标题 | `h1` | Noto Serif SC |
| 标语 | `.login-brand__tagline` | 例：知白守黑 · 道法自然 |
| 副标 | `.login-brand__sub` | 例：水墨对弈，即时匹配 |

---

## 3. CSS 实现要点

### 悬挂阴影

```css
.scroll {
  filter: drop-shadow(0 28px 48px rgba(0, 0, 0, 0.5))
          drop-shadow(0 6px 16px rgba(0, 0, 0, 0.3));
}
```

### 天杆 / 地杆（`.scroll__roller`）

- `.scroll__knob`：轴头，圆柱渐变 + `border-radius: 45% 45% 50% 50%`
- `.scroll__bar`：杆身，高度 16px，金属灰渐变
- `.scroll__roller--bottom`：`scale(0.96)` + 略透明，模拟远景地杆

### 宣纸幅面（`.scroll__body`）

多层叠加：

1. 中心亮斑 `radial-gradient`（书写区）
2. 四边墨晕 `radial-gradient` + `inset box-shadow`
3. 纤维细纹 `repeating-linear-gradient`（93° / 177°）
4. `::before` 四角水墨晕染
5. `clip-path: polygon(...)` 轻微毛边

**禁止** 使用带透明区域的整图 PNG 作为 `background-image`。

### 内容区（`.scroll__content`）

```css
.scroll__content {
  display: flex;
  flex-direction: column;
  padding: 28px 32px 24px;
  min-height: 500px;
}
```

### 表单布局（重要）

```css
#authForm {
  display: flex;
  flex-direction: column;
  /* 禁止 flex: 1 + justify-content: center — 会把按钮推离输入框 */
}

.btn-group {
  margin-top: var(--space-sm);  /* 8px，紧贴密码框 */
}

.scroll__footer {
  margin-top: auto;  /* 八卦点贴书卷底部 */
}
```

---

## 4. 道家意象使用规范

| 意象 | 允许场景 | 限制 |
|------|----------|------|
| 太极图 | 品牌区图标 + 背景大水印 | 背景 opacity 低，不抢表单 |
| 朱砂印「弈」 | 品牌区 | 单处，不作重复纹理 |
| 道德经经文 | `.ink-scene__verse` | 低对比 `rgba(245,240,232,0.22)` |
| 竖排「玄」 | `.login-page__vertical` | 移动端 `display: none` |
| 八卦八点 | `.scroll__footer .bagua-dot` | 纯装饰，非导航 |

---

## 5. Good / Base / Bad Cases

| 级别 | 描述 |
|------|------|
| **Good** | 纯 CSS 书卷 + 玄墨页底，表单在宣纸亮区，按钮距密码框 8px |
| **Base** | 纸白卡片 + token 色，无书卷结构（仅临时页） |
| **Bad** | PNG 书卷图 + 透明格；毛玻璃 login 容器；表单垂直居中导致按钮下沉 |

---

## 6. Tests Required（人工 / 浏览器）

| 场景 | 断言 |
|------|------|
| 桌面 1280×800 | 书卷居中，无白块/灰格外框 |
| 移动 ≤520px | 竖排「玄」、经文隐藏；按钮纵向堆叠 |
| 键盘 Tab | 输入框、按钮可见 focus 环 |
| `prefers-reduced-motion` | 太极呼吸动画停止 |
| 对比度 | 标题、标签在纸面上可读 |

---

## 7. Wrong vs Correct

#### Wrong

```css
#authForm {
  flex: 1;
  justify-content: center;
}
```

#### Correct

```css
#authForm {
  display: flex;
  flex-direction: column;
}
.btn-group { margin-top: var(--space-sm); }
.scroll__footer { margin-top: auto; }
```

#### Wrong

```css
background: url('../assets/scroll-paper.png') center / 100% 100%;
```

#### Correct

`.scroll__body` 使用 `radial-gradient` + `repeating-linear-gradient` + `inset box-shadow`（见 `client/css/login.css`）。
