# sac_model_cylinder phase index

| phase | 状态 | 默认恢复动作 | 文档 |
| --- | --- | --- | --- |
| `000-cylinder-count-select-diagnostic` | completed / historical | 已完成 test-only count/select diagnostic、bench、asm、5-run board repeated、Doctor 和 registry；现在只作为实现族来源。 | `000-cylinder-count-select-diagnostic/plan.zh.md`、`000-cylinder-count-select-diagnostic/result.zh.md` |
| `010-cylinder-production-integration-plan` | completed | PI1 计划完成；历史阻塞为 PI2 production patch 授权，已由后续用户偏好解除。 | `010-cylinder-production-integration-plan/plan.zh.md`、`010-cylinder-production-integration-plan/result.zh.md` |
| `020-cylinder-production-integration` | completed / adopted | count/select production direct 接入，并由当前 fresh manifest 继续承载三入口 production repeated evidence。 | `020-cylinder-production-integration/plan.zh.md`、`020-cylinder-production-integration/result.zh.md` |
| `030-cylinder-getdistances-dense-output` | completed / adopted | getDistances dense output 接入并使用同一 production repeated manifest 闭合；下一默认恢复动作是覆盖面扩展或转下一 topic。 | `030-cylinder-getdistances-dense-output/plan.zh.md`、`030-cylinder-getdistances-dense-output/result.zh.md` |
| `040-cylinder-point-type-expansion` | completed / adopted | 代表点型扩展已完成；`PointXYZI + Normal`、`PointXYZRGB + Normal`、`PointXYZ + PointNormal` 三组三入口均为 positive。 | `040-cylinder-point-type-expansion/plan.zh.md`、`040-cylinder-point-type-expansion/result.zh.md` |

`ready_for_review` 当前对已授权范围有效：topic-local doc suite、production `doc-rvv`、matrix、roadmap、Evidence Doctor 和 registry
均已刷新到四组代表点型的三入口 production direct 证据。当前 cylinder topic 内没有仍建议默认推进的未阻塞优化方向。

## 文档归属

Phase plan / result 保存阶段事实和异常解释；`doc/optimization-roadmap.zh.md` 保存候选前沿和默认恢复队列；
`doc/phases/optimization-matrix.zh.md` 保存跨阶段证据状态；`doc/sac_model_cylinder-evaluation.zh.md` 是当前
EvidenceDecision 的审计主归属；`doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md` 保存长期 production 行为。
