# plan_and_review 目录说明

## 目录结构

| 子目录 | 用途 | 文档类型 |
|--------|------|----------|
| project_plan/current/ | 当前有效的项目整体规划 | 项目架构、里程碑总览 |
| project_plan/history/ | 历史版本归档 | 被替代的旧版规划 |
| milestone_plan/ | 里程碑开发计划 | M1/M2/M3 等阶段计划 |
| phase_plan/ | 具体任务计划 | Phase 1/2/3 等实现计划 |
| review/ | 复盘报告 | 阶段完成后的总结 |
| handover/ | 交接文档 | 跨阶段/跨对话交接记录 |

## 文档命名规范

- **里程碑计划**: `M{N}_开发计划.md` (如 `M2_开发计划.md`)
- **任务计划**: `M{N}_Phase{N}_{task}_plan.md` (如 `M2_Phase1_db_pool_plan.md`)
- **复盘报告**: `M{N}_复盘报告.md` 或 `M{N}_Phase{N}_复盘报告.md`
- **交接文档**: `M{N}-M{N}_开发交接.md` (如 `M1-M2_开发交接.md`)

## 版本管理

- 项目整体规划使用 `current/` + `history/` 子目录管理版本
- 当前有效版本始终在 `current/` 目录
- 被替代的版本移至 `history/` 目录

## 当前文档清单

### project_plan/current/
- `project_plan_v2.2.md` - 当前有效的项目整体规划（10 周里程碑）

### project_plan/history/
- `project_plan_v1.md` - 历史版本（6 周里程碑，已被 v2.2 替代）

### milestone_plan/
- `M2_开发计划.md` - M2 阶段详细开发计划

### phase_plan/
- `M2_Phase1_db_pool_plan.md` - Phase 1 数据库连接池实现计划

### review/
- `M1_复盘报告.md` - M1 环境搭建阶段复盘
- `M2_Phase1_复盘报告.md` - M2 Phase 1 数据库连接池复盘

### handover/
- `M1-M2_开发交接.md` - M1 到 M2 的开发交接文档
