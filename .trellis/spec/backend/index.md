# 后端开发规范

> 后端开发最佳实践

---

## 概述

本目录包含后端开发规范，基于代码库中的实际模式填充（C++11、纯头文件、WebSocket++、MySQL、GTest）。

---

## 规范索引

| 规范 | 描述 | 状态 |
|------|------|------|
| [目录结构](./directory-structure.md) | 模块组织与文件布局 | 已填充 |
| [数据库规范](./database-guidelines.md) | 原生 MySQL C API、预处理语句、RAII 连接池 | 已填充 |
| [错误处理](./error-handling.md) | JSON 响应、LOG_ERROR + return 模式 | 已填充 |
| [质量规范](./quality-guidelines.md) | GTest、CTest 标签、线程安全、代码审查 | 已填充 |
| [日志规范](./logging-guidelines.md) | 异步单例日志器、LOG_* 宏 | 已填充 |
| [游戏逻辑规范](./game-logic-spec.md) | 落子流程、GameResult 枚举、胜负判定 | 已填充 |
| [WebSocket 断线 Grace](./websocket-disconnect-grace.md) | pending 断线状态机、grace 与重连 | 已填充 |
| [服务器初始化与定时器](./server-init-and-timers.md) | init_asio 顺序、Asio 回合 timer、grace driver | 已填充 |
