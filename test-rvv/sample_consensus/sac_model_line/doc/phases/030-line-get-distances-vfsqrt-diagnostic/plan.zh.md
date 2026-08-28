# Phase 030: line getDistancesToModel vfsqrt diagnostic plan

## 阶段意图和边界

Phase 020 证明 `getDistancesToModelCandidateRVV` 的当前形状在 board repeated 中退化：RVV 只计算平方距离，sqrt（平方根）和 double store（双精度写回）保留标量，5-run B/A median 为 `0.9340x`。本阶段新增独立候选 `getDistances-vfsqrt-store`，验证把 sqrt 移入 RVV chunk（可变向量长度分块）是否能消除 Phase 020 的后处理瓶颈。

范围仍限于 test-rvv（测试资产）和 topic-local docs。production（生产源码）不修改。本阶段只覆盖 `PointXYZ + direct indexed indices_ + float xyz AoS + dense distance output`。

## 当前状态清单

| area | current state | evidence |
| --- | --- | --- |
| Phase 020 scalar-sqrt shape | attempted / negative diagnostic。 | `../020-line-get-distances-diagnostic/result.zh.md` |
| correctness baseline | public `getDistancesToModel` 与 Phase 020 candidate 在 QEMU / board gtest 中逐项距离一致。 | `run_test_compare`, `run_line_get_distances_tests` |
| asm | Phase 020 helper 有 indexed gather、FMA 和 store，但没有 `vfsqrt`。 | `dump_bench_rvv` |
| RVV sqrt helper audit | 没有通用 `sqrt_RVV_f32m2` helper；仓库已有直接使用 `__riscv_vfsqrt_v_f32m2` 的先例。 | `common/include/pcl/common/impl/norms.hpp`, `common/include/pcl/common/impl/rvv_math.hpp` |

## 候选族和假设

| candidate family | hypothesis | risk / unknown |
| --- | --- | --- |
| `getDistances-vfsqrt-store` | 在 RVV chunk 中对平方距离执行 `__riscv_vfsqrt_v_f32m2`，再暂存 float distance，最后标量转 double 写 dense output，可能减少 scalar sqrt 调用成本。 | `vfsqrt` latency（延迟）可能仍高；float sqrt 与 public Eigen/std sqrt 的 1e-6 级误差必须由 gtest 证明；double store 和 buffer 写回仍可能主导。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `getDistances-vfsqrt-store` | direct indexed `indices_` | `PointXYZ`, float xyz AoS, dense `std::vector<double>` output | test-only `getDistancesToModelVFSqrtCandidate` vs public `getDistancesToModel` | `run_line_get_distances_vfsqrt_tests`, `run_test_compare` | `collect_repeated_board_get_distances_vfsqrt_evidence` | planned 5-run board repeated | `getDistancesToModelVFSqrtCandidateRVV` | planned manifest / doctor / registry | planned |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| RED test | 新增 `GetDistancesVFSqrtCandidateMatchesPublicEntryOnBenchScaleInput` 调用缺失候选。 | `make run_line_get_distances_vfsqrt_tests` 因缺少候选入口失败。 |
| GREEN helper | 新增 scalar fallback 和 RVV vfsqrt helper，不替换 Phase 020 candidate。 | QEMU RVV 侧 vfsqrt correctness 通过。 |
| bench / manifest | 新增 `diagnostic candidate getDistancesToModel vfsqrt` label、`--items getdist_vfsqrt` alias 和 Phase 030 Make target。 | manifest 可解析 vfsqrt comparison，checksum 一致。 |
| asm | `make dump_bench_rvv`。 | vfsqrt helper 内出现 `vfsqrt.v`，并保留 indexed load / FMA / store 归属。 |
| board / doctor / registry | 5-run board repeated 后 record/status。 | doctor 无未处理 Error，或 Error 被降级为 rejected / blocked 结论。 |
| docs | 更新 Phase 030 result、roadmap、matrix 和 role docs。 | 下一轮可从 phase index 恢复。 |

## Evidence Doctor 和 registry 规则

Phase 030 输出到 `doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-manifest.json`、`repeated-evidence-doctor.md` 和 `repeated-evidence-doctor.json`。registry run label 使用 `line-phase030-get-distances-vfsqrt-repeated-board`。count、select、Phase 020 和 Phase 030 的 registry target 必须顺序执行。

## 板卡复跑预算和决策桶

默认 5-run repeated board。若 checksum、doctor 或 B/A 方向异常，最多追加 1 次同边界确认复跑。`B/A > 1` 表示 vfsqrt candidate 比 Std build 同 wrapper 更快；`B/A < 1` 表示退化。若 5-run 全部小于 1，本候选进入 negative diagnostic，不继续 production probe。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper；baseline 是 Std build fallback，candidate 是 RVV build vfsqrt helper。 |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape。 |
| diagnostic 是否可外推到 production | 只能作为有界 production probe 输入。production dispatch / fallback / direct bench 仍未证明。 |
| comparison-boundary / baseline mismatch 风险 | 有。candidate 仍在测试派生类中，double store 和 helper 边界可能不同于 production。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若本阶段负向，当前 getDistances 不进入 production probe；count/select 不受影响。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。若 vfsqrt positive，仍需 PI1 / PI2 后在 production boundary 内复证。 |

## 继续 / 停止条件

若 vfsqrt 仍 negative，`getDistancesToModel` 当前 topic 下的已尝试实现族均不支持 production probe，下一默认动作回到用户授权 count/select PI1 或转下一个筛选 topic。若 vfsqrt positive，则把 getDistances 加入 line production integration scope 候选，但 PI1 / PI2 仍需要用户授权。
