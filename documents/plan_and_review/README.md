# plan_and_review 目录说明

## 目录结构

| 子目录 | 用途 | 文档类型 |
|---|---|---|
| `project_plan/current/` | 当前有效的项目总计划 | 架构、里程碑、治理规则 |
| `project_plan/history/` | 历史版本归档 | 被替代的旧计划 |
| `milestone_plan/` | 里程碑实施计划 | M1/M2/M3 等阶段计划 |
| `phase_plan/` | 阶段任务拆分 | 每个阶段的执行/验收计划 |
| `review/` | 阶段复盘 | 结果、问题、后续改进 |
| `handover/` | 交接文档 | 跨阶段与协作交接 |

## 当前工程事实（必须与代码一致）

- 服务器代码位于 `source/include/*.hpp`（当前为头文件内联实现为主）。
- 测试以 `source/tests/*.cpp` 可执行目标存在，通过 CMake + CTest 统一注册与分层运行。
- 当前阶段不做 `hpp -> cpp` 重构，避免引入行为变化。

## 文档同步规则

当 PR 涉及以下任一变更时，必须同步更新文档：

1. 架构边界或目录结构变更：同步 `project_plan/current/*` 与本文件。
2. 测试入口、运行命令或标签策略变更：同步 `project_plan/current/*` 与相关 `phase_plan/*`。
3. 里程碑状态变化：同步 `milestone_plan/*` 与 `review/*`。

未满足上述同步规则的 PR 不应合并。
