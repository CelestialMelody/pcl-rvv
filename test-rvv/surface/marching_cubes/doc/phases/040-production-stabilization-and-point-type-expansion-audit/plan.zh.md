# Phase 040 计划：production stabilization and point type expansion audit

## 阶段意图和边界

本阶段发生在用户确认 narrow `PointNormal` production patch 之后。目标不是扩大生产门禁，而是先把接入后的证据做稳，并审计是否值得把 exact `PointNormal` gate 放宽成 traits-gated `PointNT`。

| question | answer |
| --- | --- |
| validated_scope | `performReconstruction()` / `PointNormal` / `float grid` / contiguous synthetic grid / RVV active-cell prepass + scalar `createSurface()` |
| unvalidated_scope | 其它 `PointNT`、真实 Hoppe/RBF 输入分布、generic xyz AoS layout、`Scalar=double`、edge interpolation RVV、triangle emission RVV |
| phase_closeout_boundary | 只能关闭 production stabilization 和 expansion audit；不能直接扩大 production gate。 |
| point_type_expansion_queue | 先补 non-`PointNormal` fallback correctness；再审计 `RVVXYZAoSFloatLayout<PointNT>` 是否足以表达 active-cell prepass 的输入字段和输出语义；若可行，下一 phase 才做 generic production patch。 |

## 当前状态清单

| area | status | evidence |
| --- | --- | --- |
| production patch | adopted / narrow | `surface/include/pcl/surface/impl/marching_cubes.hpp` 中 `if constexpr (std::is_same_v<PointNT, pcl::PointNormal>)` |
| correctness | done | `make run_test_compare`，Std/RVV 各 4 tests passed |
| asm | done | `make dump_bench_rvv`，可见 RVV load/store、`vmerge`、`vmflt`、`vmfeq` |
| board | done / quick repeated | `log/board/production_direct_repeated/summary.md`，`5.404x/6.425x/8.910x` |
| Evidence Doctor | done / warnings remain | `Errors=0`、`Warnings=4`；3 个 low_run_count，1 个 group_outlier |
| long-term doc | planned in this closeout | `doc-rvv/surface/marching_cubes-RVV.zh.md` |

## 候选族和假设

| candidate family | hypothesis | risk / unknown | decision target |
| --- | --- | --- | --- |
| production steady rerun | 5-run repeated 会保持 positive bucket，并解释 low_run_count warning | 板卡波动或 sparse case outlier 可能扩大 | keep adopted narrow production 或降级为 retained-candidate |
| non-`PointNormal` fallback correctness | exact-type gate 让其它 `PointNT` 在 RVV build 中自然走标量路径 | 当前 tests 只覆盖 `PointNormal` production direct | fallback coverage accepted |
| generic point type audit | active-cell prepass 只读取 `grid_` 和输出 `PointNT` 仍由 `createSurface()` 标量构造，可能不需要读取 point fields | `getBoundingBox()` 和 `PointNT` 输出语义仍要求 xyz 字段；额外字段默认未初始化是否与标量一致要验证 | 下一 phase 是否允许 traits-gated production probe |
| edge interpolation revisit | 若 profile 证明 scalar tail 中 edge interpolation 成为新瓶颈，再恢复 | 既有 board 为 attempted / not production candidate | keep paused |

## 优化矩阵

| candidate family | point type / layout | correctness | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| production-active-cell-prepass steady | `PointNormal` exact gate | rerun `run_test_compare` | 5-run repeated planned | rerun `dump_bench_rvv` if code changes | planned | continue adopted if positive |
| fallback correctness | non-`PointNormal` representative | planned | not_applicable | not_applicable | not_applicable | required before ready_for_review |
| generic point type expansion | traits-gated xyz AoS | audit only | deferred | deferred | deferred | next phase only if audit passes |

## 实现和测试动作

1. 更新 `doc-rvv/surface/marching_cubes-RVV.zh.md` 和 topic-local docs，把 Phase 030 写成用户确认采纳。
2. 补 `PointXYZ` 或等价 non-`PointNormal` synthetic public-path fallback correctness。
3. 在板卡上补 production-direct steady repeated，目标至少 5 run；如果 decision bucket 不变，刷新 summary / manifest / Evidence Doctor。
4. 审计 generic point type gate：读取 `RVV Generic Point Type Strategy`，确认 `MarchingCubes<PointNT>` 的 `getBoundingBox()`、`createSurface()` 和输出 `PointNT` 构造是否允许 traits-gated expansion。

## Board 复跑预算和决策桶

| item | value |
| --- | --- |
| target | `mc_prod_sphere_64`、`mc_prod_wave_72`、`mc_prod_sparse_sphere_80` |
| minimum runs | 5 |
| positive | median >= 1.10 and min >= 1.03 |
| weak-positive | median >= 1.03 and min >= 0.97 |
| stop / rerun | 若 5-run 仍 positive 且 checksum match，停止复跑；若 bucket 改变或 below 1.0 出现，降级 evidence 并暂停扩大门禁。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public stabilization + production-expansion audit |
| A/B boundary | `performReconstruction()` public wrapper |
| 当前决策问题 | adopted narrow path 是否保持；generic expansion 是否值得进入下一 phase |
| diagnostic 是否可外推到 production | steady rerun 是同一 production boundary；generic audit 不能外推，必须另做 production direct evidence |
| comparison-boundary / baseline mismatch 风险 | 仍有 synthetic grid 与真实 Hoppe/RBF 输入分布 mismatch |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 对现有 narrow path：降级或暂停；对 generic expansion：不进入 production patch |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | generic expansion 需要同边界 correctness、asm、board 和 doctor |

## 继续 / 停止条件

- 若 5-run production direct 仍 positive、fallback correctness 通过、generic audit 有可行门禁，下一 phase 做 traits-gated bounded production probe。
- 若 5-run 降级为 weak / unstable，保持 narrow patch 但不扩大门禁，先做 variance / case profile。
- 若 non-`PointNormal` fallback 失败，立即修生产 gate 或测试，不进入 generic audit closeout。
