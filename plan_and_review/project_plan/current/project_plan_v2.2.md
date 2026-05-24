# C++ 在线五子棋对战项目计划 v2.2（治理同步版）

## 1. 范围与目标

本版本聚焦 M3 工程治理与稳定性，不改动 M3 核心业务语义（在线状态、匹配流、WebSocket 事件字段）。

目标：

- 可复现构建
- 可分层运行测试（unit/integration/smoke）
- 文档与代码事实一致
- 默认弱密钥不可启动

## 2. 当前代码事实

- 代码组织：当前以 `source/include/*.hpp` 内联实现为主。
- 测试组织：`source/tests/*.cpp` 构建为可执行测试目标。
- 测试入口：CTest 已注册统一入口，支持标签分层。

## 3. 构建与测试命令

```bash
cmake -S source -B source/build
cmake --build source/build -j
ctest --test-dir source/build -N
ctest --test-dir source/build -L unit --output-on-failure
ctest --test-dir source/build -L integration --output-on-failure
ctest --test-dir source/build -L smoke --output-on-failure
```

## 4. M3 到 M4 的约束

- M3 事件字段在 M4 前冻结：只允许新增可选字段，不允许删除/改名/改语义。
- 在线状态流转语义保持不变。
- 仅做治理和安全增强，不进入 M4 房间对局逻辑实现。

## 5. 文档同步规则

凡涉及架构、目录、测试入口、接口契约变更，必须同步：

1. `plan_and_review/project_plan/current/project_plan_v2.2.md`
2. `plan_and_review/README.md`

PR 缺少同步更新视为不完整变更。
