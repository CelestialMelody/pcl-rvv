# correspondence_rejection_poly 阶段索引

本文保存 phase loop（阶段循环）的恢复入口。阶段计划和结果保存在各自目录；跨阶段候选搜索空间保存在 `../optimization-roadmap.zh.md`；证据状态矩阵保存在 `optimization-matrix.zh.md`。

| phase | 状态 | 作用 | plan | result |
| --- | --- | --- | --- | --- |
| `000-current-state-and-gaps` | completed | 重建标量路径，建立诊断 scaffold（脚手架）、测试 / bench / Evidence Doctor（证据体检）计划。 | `000-current-state-and-gaps/plan.zh.md` | `000-current-state-and-gaps/result.zh.md` |
| `010-board-and-production-boundary` | completed | 补 board evidence（板卡证据）和 no-production / continue 边界。 | `010-board-and-production-boundary/plan.zh.md` | `010-board-and-production-boundary/result.zh.md` |
| `020-production-shaped-gather-diagnostic` | completed | 把 correspondence index gather（对应关系索引离散加载）和 squared distance staging（平方距离暂存）纳入诊断。 | `020-production-shaped-gather-diagnostic/plan.zh.md` | `020-production-shaped-gather-diagnostic/result.zh.md` |
| `030-pi1-production-integration-plan` | completed | 执行受控 production direct probe，证据负向后回滚生产补丁。 | `030-pi1-production-integration-plan/plan.zh.md` | `030-pi1-production-integration-plan/result.zh.md` |
| `040-structure-parity-doc-suite` | completed | 按 canonical doc-suite quality bar（规范文档套件质量门槛）补齐 README、testing overview、correctness、benchmark/evidence、optimization evidence、code map、evaluation 和 Handoff。 | `040-structure-parity-doc-suite/plan.zh.md` | `040-structure-parity-doc-suite/result.zh.md` |

## 文档归属

| 信息类型 | 主归属 |
| --- | --- |
| 阶段计划和动作完成情况 | 各 phase 目录下的 `plan.zh.md` / `result.zh.md` |
| 跨阶段候选搜索空间 | `../optimization-roadmap.zh.md` |
| candidate × evidence 状态 | `optimization-matrix.zh.md` |
| no-production 诊断证据链 | `../correspondence_rejection_poly-evaluation.zh.md` 和 Phase 030 / 040 result |
| 恢复动作和 dirty isolation | `tmp/rvv-work-logs/registration/correspondence_rejection_poly/current-handoff/` |

## 当前早停规则

`ready_for_review` 只有在 Phase 040 的 doc-suite 审计表全部为 `adopted`、`not_applicable with evidence` 或 `rejected with evidence`，且 artifact tracking（产物跟踪）扫描没有未解释缺口时才有效。若仍有 topic-local 文档、evidence registry（证据登记表）或 legacy compatibility（旧路径兼容）缺口，下一轮默认恢复到对应补齐 phase。

## 默认恢复动作

默认恢复动作为 `ready_for_review`。Phase 040 已完成 doc-suite 审计和 topic-local 文档补齐。当前 EvidenceDecision 是 `rollback/no-production`，生产目标文件已恢复为标量路径。若继续性能探索，应先创建新的窄范围 profile / ablation phase，不能重新应用 Phase 030 的生产补丁作为默认动作。
