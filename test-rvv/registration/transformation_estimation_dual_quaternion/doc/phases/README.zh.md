# 阶段索引

## 当前恢复入口

当前恢复入口是 Phase 016 后的提交前检查点。生产提交候选保留
`ordered-cloud-pair`、`source-indexed-cloud-pair`、`dual-indexed-cloud-pair`
三类 RVV path；`correspondence-pair` production RVV 已移除。

下一步只剩提交前验证和用户判断：提交当前接入，或取消接入并回滚生产补丁 / 长期文档。

## 阶段表

| phase | 状态 | plan | result | 说明 |
| --- | --- | --- | --- | --- |
| `000-current-state-and-gaps` | done / diagnostic | `doc/phases/000-current-state-and-gaps/plan.zh.md` | `doc/phases/000-current-state-and-gaps/result.zh.md` | 建立 evaluation、test support、QEMU correctness、QEMU smoke、doctor 和 registry。 |
| `001-board-diagnostic` | done / pre-PI1 positive | `doc/phases/001-board-diagnostic/plan.zh.md` | `doc/phases/001-board-diagnostic/result.zh.md` | ordered helper 5-run repeated diagnostic positive。 |
| `002-production-integration-plan` | done / rejected | `doc/phases/002-production-integration-plan/plan.zh.md` | `doc/phases/002-production-integration-plan/result.zh.md` | 早期临时 ordered production direct neutral，patch 撤回。 |
| `003-component-ablation` | done / component positive | `doc/phases/003-component-ablation/plan.zh.md` | `doc/phases/003-component-ablation/result.zh.md` | C1/C2 accumulation-only strict A/B positive；solve-only neutral。 |
| `004-production-boundary-probe` | done / contract defined | `doc/phases/004-production-boundary-probe/plan.zh.md` | `doc/phases/004-production-boundary-probe/result.zh.md` | 定义 path-hit、fallback、asm 和板卡 production 证据合同。 |
| `005-row-source-expansion` | done / diagnostic positive | `doc/phases/005-row-source-expansion/plan.zh.md` | `doc/phases/005-row-source-expansion/result.zh.md` | source-indexed positive；dual / correspondence weak-positive。 |
| `006-test-support-split-and-row-source-family-comparison` | done / diagnostic positive | `doc/phases/006-test-support-split-and-row-source-family-comparison/plan.zh.md` | `doc/phases/006-test-support-split-and-row-source-family-comparison/result.zh.md` | 完成 test-support 拆分；source-indexed direct gather family positive。 |
| `007-indexed-direct-gather-family-expansion` | done / diagnostic weak-positive | `doc/phases/007-indexed-direct-gather-family-expansion/plan.zh.md` | `doc/phases/007-indexed-direct-gather-family-expansion/result.zh.md` | dual positive；correspondence weak-positive。 |
| `008-correspondence-direct-index-stream` | done / diagnostic positive | `doc/phases/008-correspondence-direct-index-stream/plan.zh.md` | `doc/phases/008-correspondence-direct-index-stream/result.zh.md` | correspondence direct index stream 作为 test-rvv baseline 保留。 |
| `009-correspondence-segment-load` | done / rejected | `doc/phases/009-correspondence-segment-load/plan.zh.md` | `doc/phases/009-correspondence-segment-load/result.zh.md` | `vlseg3e32` 无稳定收益。 |
| `010-correspondence-index-locality` | done / rejected | `doc/phases/010-correspondence-index-locality/plan.zh.md` | `doc/phases/010-correspondence-index-locality/result.zh.md` | local-window 三个规模 negative；不做 locality-aware production dispatch。 |
| `011-row-source-taxonomy-and-pattern-boundary` | done / taxonomy adopted | `doc/phases/011-row-source-taxonomy-and-pattern-boundary/plan.zh.md` | `doc/phases/011-row-source-taxonomy-and-pattern-boundary/result.zh.md` | 固定四类 row-source policy；pattern 只作为内部维度。 |
| `012-point-type-layout-expansion` | done / diagnostic positive with warnings | `doc/phases/012-point-type-layout-expansion/plan.zh.md` | `doc/phases/012-point-type-layout-expansion/result.zh.md` | correspondence `PointXYZI` / `PointXYZRGB` diagnostic positive。 |
| `013-indexed-direct-gather-point-type-layout` | done / diagnostic positive with warnings | `doc/phases/013-indexed-direct-gather-point-type-layout/plan.zh.md` | `doc/phases/013-indexed-direct-gather-point-type-layout/result.zh.md` | source / dual `PointXYZI` / `PointXYZRGB` diagnostic positive。 |
| `014-production-reentry-decision-packet` | done / superseded | `doc/phases/014-production-reentry-decision-packet/plan.zh.md` | `doc/phases/014-production-reentry-decision-packet/result.zh.md` | 被 Phase 015 用户授权 supersede。 |
| `015-production-integration-all-row-sources` | done / mixed result | `doc/phases/015-production-integration-all-row-sources/plan.zh.md` | `doc/phases/015-production-integration-all-row-sources/result.zh.md` | 四类 production probe：前三类 positive clean，correspondence negative。 |
| `016-production-adoption-scope-finalization` | done / ready for user submit-or-cancel | `doc/phases/016-production-adoption-scope-finalization/plan.zh.md` | `doc/phases/016-production-adoption-scope-finalization/result.zh.md` | 保留前三类，移除 correspondence production RVV，重跑 retained-only 证据。 |

## 文档归属

phase 文档记录阶段动作和证据解释；`doc/optimization-roadmap.zh.md` 记录恢复队列；
`doc/transformation_estimation_dual_quaternion-evaluation.zh.md` 记录当前 EvidenceDecision；
`doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md` 记录长期 production 行为。

## 当前早停规则

没有未阻塞的继续优化动作需要在本轮继续执行。合法停止点是提交前用户判断：
提交当前 retained 三类接入，或取消接入并回滚。
