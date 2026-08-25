# PI3 plan: shape-bin production rollback closeout

## 阶段意图和边界

本阶段执行用户确认的 PI5 回滚：移除 `features/include/pcl/features/impl/shot.hpp` 中 PI2 shape-bin indexed gather production patch（生产补丁），让 SHOT production（生产源码）恢复为原标量 `createBinDistanceShape` 路径。

本阶段不删除 PI2 的 topic-local tests、bench case、manifest wrapper 或 phase result。它们保留为历史 production probe（生产探针）证据，用来说明为什么不采纳该 patch，并继续作为回滚后标量语义的 regression（回归）检查入口。

## 当前状态清单

| item | current state |
| --- | --- |
| production patch | `shot.hpp` 当前含 `__RVV10__` include、`SHOTRVVNormalAoSLayout`、`shotCreateBinDistanceShapeRVV`、`shotCreateBinDistanceShapeStd` 和 `createBinDistanceShape` dispatch。 |
| PI2 evidence | `production_shape_bin_direct` 为 1.07x；`public_shot352_fixed_lrf` / `public_shot1344_fixed_lrf` 为 0.98x / 0.99x，两个 public Doctor 均有退化 Error。 |
| user decision | 用户已明确“确认回滚当前 `shot.hpp` PI2 patch”。 |
| doc-rvv | `doc-rvv/features/shot-RVV.zh.md` 未创建，且本阶段仍不创建。 |
| topic docs | 当前多处文档写的是 `pending_user_confirmation_rollback`，回滚后需改为 `rollback/no-production`。 |

## 假设与候选族

本阶段不是新优化 candidate（候选实现）阶段，而是 rollback/no-production closeout（回滚后不接生产收尾）。需要验证的假设是：撤掉 production RVV helper 后，现有 SHOT correctness tests（正确性测试）仍通过，topic-local 文档能恢复到“生产源码未接 RVV，诊断证据保留”的一致状态。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | board evidence | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| shape-bin indexed production probe rollback | indexed normal cloud / public fixed-LRF | `PointNT` normal traits + AoS layout, f32 normal -> f64 bin distance | `run_test_shape_bin`、`run_test_compare` | PI2 public 0.98x / 0.99x historical evidence | PI2 public Errors=1 each | rollback/no-production |

## 实现和测试动作

| action | artifact / command | done criteria |
| --- | --- | --- |
| 回滚 production patch | `features/include/pcl/features/impl/shot.hpp` | `git diff -- shot.hpp` 不再包含 PI2 RVV helper / dispatch；原标量 loop 恢复。 |
| 保留历史测试支撑 | `test-rvv/features/shot/{Makefile,src,script,doc}` | PI2 direct tests / bench case 保留，但文档说明它们是 historical probe / scalar regression，不代表当前 production RVV path。 |
| correctness 验证 | `make -C test-rvv/features/shot run_test_shape_bin`、`make -C test-rvv/features/shot run_test_compare` | Std/RVV 双侧通过。 |
| evidence freshness | `make -C test-rvv/features/shot evidence_status`、`check_evidence_doc_refs` | registry / doc refs fresh。 |
| closeout 检查 | `git diff --check`、路径限定 `git status --short --untracked-files=all` | 无 whitespace error；dirty isolation 清楚。 |

## Evidence Doctor 和 registry 规则

本阶段不新增 board run，因此不新增 Evidence Doctor 输入。PI2 per-run evidence 保留为 historical evidence（历史证据），`log/evidence_registry.json` 应继续 fresh；若 doc refs 因文档改写丢失，先修正文档引用，不重跑板卡。

## 阶段完成条件

- `shot.hpp` production RVV patch 已移除。
- QEMU correctness（QEMU 正确性）通过。
- evaluation、README、benchmark/evidence、optimization evidence、code map、phase index、matrix、roadmap、queue row 和 Handoff 均写成 `rollback/no-production`，不再写 `pending_user_confirmation_rollback` 作为当前状态。
- `doc-rvv/features/shot-RVV.zh.md` 保持 not_applicable。
- 当前 roadmap 和 matrix 无授权、未阻塞、值得继续推进的 high-priority candidate。

## 板卡复跑预算和决策桶

本阶段 run budget 为 0：PI2 public evidence 已经提供回滚决策所需的板卡证据；回滚后没有 production RVV path 可做 production-public A/B。若未来恢复 SHOT，需要新的 phase plan 和独立 board budget。

## 继续 / 停止条件

完成回滚和文档同步后，默认停止。停止理由不是“已经做完一个小任务”，而是当前候选空间已经被证据关闭：

- shape-bin production probe 接入后 public negative；
- interpolation geometry / bin-selection 已是 negative 或 unstable；
- color RGB/LUT staging neutral-weak；
- normalization 虽有 component positive，但 PI2 已证明局部收益可能被 public entry 成本吞掉，继续需要新的 production profile 或用户明确授权的新 PI1，而不是当前 topic 的默认 next action。

## 文档更新清单

- `test-rvv/features/shot/README.zh.md`
- `test-rvv/features/shot/doc/shot-evaluation.zh.md`
- `test-rvv/features/shot/doc/benchmark-and-evidence.zh.md`
- `test-rvv/features/shot/doc/optimization-evidence.zh.md`
- `test-rvv/features/shot/doc/test-support-code-map.zh.md`
- `test-rvv/features/shot/doc/testing-overview.zh.md`
- `test-rvv/features/shot/doc/correctness-tests.zh.md`
- `test-rvv/features/shot/doc/phases/README.zh.md`
- `test-rvv/features/shot/doc/phases/optimization-matrix.zh.md`
- `test-rvv/features/shot/doc/optimization-roadmap.zh.md`
- `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md`
- `tmp/rvv-work-logs/features/shot/current-handoff/current-handoff.zh.md`

## Roadmap 同步动作

将当前默认恢复动作改为 `rollback/no-production closeout done`，并把 remaining candidates 全部标为 `rejected with evidence`、`not_applicable with evidence` 或需要未来独立 profile / user authorization 的低优先恢复条件。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | PI2 为 production-detail + production-public；PI3 为 rollback closeout。 |
| A/B boundary | PI2 包含 production detail helper 和 public overload；PI3 不新增 A/B。 |
| 当前决策问题 | 是否保留 PI2 production patch。 |
| diagnostic 是否可外推到 production | 否。Phase 040 component positive 已被 PI2 production-public negative 纠正。 |
| comparison-boundary / baseline mismatch 风险 | 已通过 PI2 public cases 暴露；detail 1.07x 不能替代 public 0.98x / 0.99x。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已允许并执行一次有界 probe；结果不支持采纳。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不适用；当前没有 adopted RVV family，且 public Std/RVV 已负向。 |
