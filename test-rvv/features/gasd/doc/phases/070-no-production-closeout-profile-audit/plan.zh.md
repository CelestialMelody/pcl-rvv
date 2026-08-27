# Phase 070 plan: no-production closeout / profile recovery audit

## 阶段意图和边界

本阶段把 Phase 050 和 Phase 060 的负向证据整理成 topic-local no-production closeout（不接入生产收尾）
和 profile recovery audit（性能剖析恢复条件审计）。本阶段不修改 production（生产源码），不创建
`doc-rvv/features/gasd-RVV.zh.md`，不把 diagnostic negative（诊断负向）写成全局 rejected（拒绝）。

本阶段只回答三个问题：

1. 当前 staged shape family 为什么不进入 production patch（生产补丁）。
2. 如果后续继续当前 topic，哪些 profile（性能剖析）、消融或 bounded production probe（有界生产探针）能改变判断。
3. 当前 topic-local 文档是否能让下一轮 worker 从 README、evaluation、phase index、matrix 和 roadmap 恢复。

## 当前状态清单

| area | state | evidence |
| --- | --- | --- |
| Phase 050 Eigen-backed write | attempted / diagnostic-negative | board median 0.820x，Doctor Errors=1；见 `doc/phases/050-eigen-backed-histogram-write-probe/result.zh.md` |
| Phase 060 shape combined | attempted / production-shaped-diagnostic-negative | board median 0.590x，Doctor Errors=1；见 `doc/phases/060-production-shaped-shape-combined-diagnostic/result.zh.md` |
| correctness | pass | QEMU `run_test_compare` 12/12 + 12/12；board `run_board_test` 12/12 |
| production state | 未修改 production | `features/include/pcl/features/impl/gasd.hpp` 不在当前 topic diff 中 |
| doc-rvv applicability | not_applicable | 没有 adopted production behavior 或 PI5 production patch |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic + production-shaped diagnostic |
| A/B boundary | test helper；没有 public overload 或 production detail helper |
| 当前决策问题 | production decision audit：当前 staged shape family 是否应进入 production integration loop |
| diagnostic 是否可外推到 production | no；当前证据只支持暂不接入 production patch |
| comparison-boundary / baseline mismatch 风险 | yes；未覆盖 public dispatch、alignment transform、indices、object state 和 fallback |
| negative 时是否允许 bounded production probe | yes，但需要用户显式授权；probe 必须先写 PI1 scope、fallback matrix 和 production direct evidence plan |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；当前没有 family-selection 证据 |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| N1 no-production closeout | `doc/gasd-evaluation.zh.md` | 当前 staged shape family 不进入 production 的证据边界 | 写清诊断证据链、不能证明范围和恢复条件 |
| N2 roadmap / matrix 收口 | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | Phase 070 状态和剩余候选边界 | 不把 deferred 候选伪装成 adopted |
| N3 README / phase index 恢复入口 | `README.zh.md`、`doc/phases/README.zh.md` | 默认恢复动作可执行 | 下一轮能定位 Phase 070 result 和后续选项 |
| N4 artifact tracking | `git status --short --untracked-files=all -- test-rvv/features/gasd features/include/pcl/features/impl/gasd.hpp` | topic-local 产物边界清楚 | 生产文件未改；新增文档在 topic artifact 集合中 |
| N5 validation | `git diff --check -- test-rvv/features/gasd` | Markdown / code diff 没有 whitespace error | 通过或记录阻塞 |

## 阶段完成条件

本阶段完成后，当前 staged shape family 的 production decision（生产接入判断）应为
`no-production for current staged shape family`。这不是整个 GASD topic 的数学结论；color quadrilinear、
profile-driven rewrite（由性能剖析驱动的重写）或用户授权的 bounded production probe 仍可作为后续选择。

## 继续 / 停止条件

默认继续到 N1-N5。合法停止条件：dirty isolation（脏工作区隔离）显示 production 文件有非本轮修改、
文档与证据路径矛盾无法在本阶段修复，或继续需要用户授权 production integration loop。

若 N1-N5 闭合且没有授权的高优先级 test-only 动作，`next_phase_default` 写成
`stop_for_user_review_no_production_closeout`。如果 roadmap 仍有当前 topic 内的高优先级 unblocked 动作，
则继续创建下一 phase。
