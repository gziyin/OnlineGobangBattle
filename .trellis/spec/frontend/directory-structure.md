# 前端目录结构

> `client/` 多页 HTML 应用的文件职责。

```
client/
├── assets/
│   ├── 1雨天棋.png … 5天地棋.png   # 五境大厅壁纸
│   └── scroll-paper.png            # 书卷参考素材（login 不直铺）
├── css/
│   ├── tokens.css                  # 设计 token 单一来源
│   ├── login.css                   # 登录页（书卷 + 水墨氛围）
│   ├── hall.css                    # 大厅（基础层 + 五境壁纸层）
│   └── game.css                    # 对局页
├── js/
│   ├── websocket.js                # WebSocket 客户端
│   ├── realm-wallpapers.js         # 五境 manifest + picker
│   └── game.js                     # 对局逻辑 + Canvas
├── login.html
├── hall.html
└── room.html
```

## 样式引用约定

| 页面 | CSS |
|------|-----|
| login | `login.css` → `@import tokens.css` |
| hall | `hall.css` → `@import tokens.css` |
| room | `game.css` → `@import tokens.css` |

禁止在三页内联 `<style>` 重复渐变或硬编码 `#1a1a2e` 旧深蓝底。

## 新增文件检查清单

- 五境 PNG 必须在 `realm-wallpapers.js` manifest 中注册
- hall 壁纸相关 DOM 类名与 `story-covers.md` 一致
- room 不得引入全屏背景图
