# Phase 050 Plan: getDistances vfsqrt full-RVV diagnostic

## 阶段意图和边界

本阶段重开 `getDistancesToModel`，但只验证新的 test-only diagnostic（仅测试使用的诊断候选），不修改 production（生产源码）。Phase 020 已拒绝的是 `RVV sqr distance + scalar sqrt + dense double store` 形态；本阶段候选改为完整 RVV 链路：RVV gather（离散加载）`x/y`、RVV 计算平方距离、`__riscv_vfsqrt_v_f32m2`、RVV abs、`__riscv_vfwcvt_f_f_v_f64m4` 扩成 `double`，最后 `__riscv_vse64_v_f64m4` 密集写回 `std::vector<double>`。

本阶段参考 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` 的 dense double write-back（密集 double 写回）模式，但不复制 normal plane 的角度公式或 production 接入结论。

## S0 恢复和偏好冻结

| 字段 | 本阶段记录 |
| --- | --- |
| preferences_loaded | defaults loaded；local override absent；prompt override loaded，用户打开 `getDistancesToModel` vfsqrt full-RVV 新实现族。 |
| loaded_instruction_sources | `AGENTS.md`、`.agents/config/defaults.yaml`、`rvv-workflow/SKILL.md`、`rvv-workflow/references/s0-preferences-and-recovery.zh.md`、`rvv-workflow/references/reviewability-and-language.zh.md`、`rvv-test/SKILL.md`、`rvv-test/references/optimization-phase-loop.zh.md`、`rvv-test/references/entry-shapes-and-test-support.zh.md`、`rvv-test/references/numerical-consistency.zh.md`、`rvv-test/references/performance-and-ablation.zh.md`、`rvv-implementation/SKILL.md`、`rvv-documentation/SKILL.md`。 |
| work_preferences | test-rvv / diagnostic 注释使用中文说明证据边界；production 注释不新增；文档 current-state-first；证据 summary-only。 |
| commit_preferences | no commit without user request；topic、summary evidence、agent instruction patch 分开审查。 |
| dirty_isolation | 只编辑 `test-rvv/sample_consensus/sac_model_circle/**` 和必要的 circle topic-local Handoff；不回滚其它 topic / `.agents` / production 文件上的既有改动。 |

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| production boundary | `selectWithinDistance` / `countWithinDistance` gather-style RVV 已采纳；`getDistancesToModel` production 保持标量。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| Phase 020 | 旧 test-only helper 在 RVV 中只算平方距离，之后写临时 float buffer 并逐 lane 标量 `sqrt` / 写 `double`；板卡 B/A median `0.6590x`，稳定负向。 | `doc/phases/020-circle-getdistances-ablation/result.zh.md` |
| normal_plane 经验 | `getDistancesToModelRVV` 用 `vfwcvt` + `vse64` 直接写 `std::vector<double>`，可作为 circle 新候选的写回参考。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` |
| 旧 Handoff | `ready_for_review` 的停止条件基于“没有新实现族”；本阶段由用户明确打开新实现族，因此该停止决定对当前 goal 过期。 | `tmp/rvv-work-logs/sample_consensus/sac_model_circle/current-handoff/current-handoff.zh.md` |

## 测试支撑形态扫描

| shape | present | paths | roles found | risk if unchanged | decision | next action |
| --- | --- | --- | --- | --- | --- | --- |
| root test source | no | not_present | none | none | not_applicable with evidence | none |
| root bench source | no | not_present | none | none | not_applicable with evidence | none |
| `src/` source | yes | `src/test_sac_model_circle.cpp`、`src/bench_sac_model_circle.cpp` | public correctness、test-only candidate、bench harness、case output | 新增第二个 getDistances 实现族后，候选类和入口逻辑会在 test / bench 中重复。 | adopt structure split | 新增 `include/` 聚合头和 `include/impl/` 内部 helper。 |
| aggregator header | no | not_present | none | test / bench 直接复制候选入口。 | adopt | 新增 `include/sac_model_circle_test_support.h`；避免和 PCL production 头文件名混淆。 |
| internal helper | no | not_present | none | 新旧 getDistances 候选和后续 bench row 难以统一。 | adopt | 新增 `include/impl/sac_model_circle_candidates.hpp`。 |
| legacy `test_support/` | no | not_present | none | none | not_applicable with evidence | none |
| topic-local script | yes | `script/generate_circle_board_evidence_manifest.py` | manifest、case label、asm count | 需新增 full-RVV manifest mode。 | adopt narrow update | 新增 `getdistances-full-rvv` mode。 |
| Makefile / board target | yes | `Makefile`、`board.mk` | correctness、bench、board repeated、doctor / registry | 需为新候选单独 target，避免混用 Phase 020。 | adopt narrow update | 新增 Phase 050 target 和 evidence 路径。 |
| topic-local docs | yes | `doc/*.zh.md`、`doc/phases/**` | evaluation、roadmap、matrix、evidence index | 新候选会改变 roadmap 前沿。 | adopt refresh | result 后同步。 |
| `doc-rvv` long-term doc | yes | `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` | adopted production behavior | 本阶段未接 production，不能把 diagnostic 写成长期 production 行为。 | no edit unless production adopted | 若后续 PI5 用户确认采纳，再刷新。 |

## 候选矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | planned decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| getDistances vfsqrt full-RVV test-only candidate | direct indexed `indices_` | `PointXYZ` / `float x-y AoS` / output `double` | RVV binary 内 public `getDistancesToModel` companion vs test-only full-RVV helper | `run_circle_getdistances_full_rvv_candidate_test`，并纳入 `run_test_compare` | `bench_sac_model_circle` 新增 full-RVV row | 5-run board repeated，B/A = public RVV ms / full-RVV candidate ms | `getDistancesToModelFullRVV`，必须出现 `vfsqrt.v`、`vfwcvt`、`vse64` | Phase 050 manifest + doctor | positive 时进入 production integration loop；negative/unstable 时只拒绝本实现族。 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| phase plan | 本文件 | 写在任何 test/bench 代码修改之前。 |
| test support split | `include/sac_model_circle_test_support.h`、`include/impl/sac_model_circle_candidates.hpp` | test / bench 通过聚合头复用候选，保留旧候选和新 full-RVV 候选。 |
| correctness | `src/test_sac_model_circle.cpp` | 新增 full-RVV candidate 与 public `getDistancesToModel` 对拍；Std/RVV compare 通过。 |
| bench row | `src/bench_sac_model_circle.cpp` | 输出保留旧 label，并新增 `diagnostic full-rvv getDistancesToModel`。 |
| Makefile targets | `Makefile` | 新增 Phase 050 correctness、board repeated、manifest、doctor、registry 和 asm check target。 |
| manifest / doctor | `script/generate_circle_board_evidence_manifest.py` | 新模式只采集 public vs full-RVV row，不覆盖 Phase 020 文件。 |
| docs | topic-local docs 和 current Handoff | 结果后同步 Phase 050 证据、矩阵、roadmap 和恢复状态。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断），不是真实 production direct。 |
| A/B boundary | 同一 RVV binary 中 public `getDistancesToModel` companion 与 test-only full-RVV helper。 |
| 当前决策问题 | 新 full-RVV 实现族是否值得进入 bounded production probe（有界生产探针）。 |
| diagnostic 是否可外推到 production | 不能直接外推；若正向，只能支持进入 production integration loop。 |
| comparison-boundary / baseline mismatch 风险 | 有。baseline 是公开入口，candidate 是测试派生类 helper；wrapper 不同。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 asm 显示 full-RVV 链路命中且负向可归因于测试 wrapper / 可消除的边界成本时才允许；否则拒绝当前实现族。 |
| clean adoption 是否需要同一 production boundary 内证据 | 需要。任何采纳都必须另做 production patch、production direct correctness、asm、fallback 和 repeated board。 |

## 板卡复跑预算和决策桶

默认 5-run repeated board，每轮 65536 points、200 iterations、5 warmup。`B/A = public_getDistances_ms / full_rvv_candidate_ms`，`>1` 表示 full-RVV candidate 更快。若 5/5 同向且 median 明显大于 1，记录为 positive 或 weak-positive；若 5/5 低于 1，记录为 negative；若跨越 1 且差距很小，记录为 neutral/unstable。只有 Evidence Doctor 或日志显示边界污染时，最多追加一次同边界确认复跑。

## 继续 / 停止条件

本阶段完成后必须更新 result、optimization matrix、roadmap、test-support-code-map、benchmark/evidence、correctness docs 和 current Handoff。若 full-RVV 板卡 positive，停止在 production integration loop 入口，等待用户确认是否接入 production。若 full-RVV negative 或 unstable，拒绝本实现族并检查是否还有当前授权范围内的未阻塞新实现族；没有则暂停并说明原因。
