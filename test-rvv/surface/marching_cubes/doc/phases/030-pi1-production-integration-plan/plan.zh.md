# Phase 030 计划：PI1 production integration plan

## 阶段意图和边界

本阶段只冻结进入 PI1 的生产接入计划，不直接修改 `surface/include/pcl/surface/impl/marching_cubes.hpp`。当前证据已经证明 `cube-index-prepass` 在 `PointNormal` / `float` 的 test helper 边界上强正向，但还没有任何 production direct 证据。

| scope item | 本阶段范围 |
| --- | --- |
| evidence role | `production-integration-plan`。 |
| A/B boundary | 从 `test helper` 过渡到 `performReconstruction()` public boundary。 |
| 当前决策问题 | `PI1 readiness`、`fallback correctness`、`point type scope`。 |
| point type / Scalar | 先冻结为 `PointNormal` / `float` 的 phase-local exception 候选。 |
| layout | 先按 contiguous AoS / dense grid 冻结；generic point type 不在本阶段直接接入。 |
| 不覆盖范围 | 生产补丁提交、generic point type 扩展、doc-rvv 长期主题文档采纳。 |

## 当前状态清单

| item | 状态 | evidence |
| --- | --- | --- |
| Phase 000 | done / attempted / neutral-to-weak-positive | `doc/phases/000-current-state-and-edge-interpolation-diagnostic/result.zh.md` |
| Phase 010 | done / strong-positive diagnostic | `doc/phases/010-cube-index-prepass-diagnostic/result.zh.md` |
| Phase 020 | done / boundary-audited | `doc/phases/020-structure-parity-doc-suite-and-production-boundary-audit/result.zh.md` |
| 生产源码 | 尚未修改 | `surface/include/pcl/surface/impl/marching_cubes.hpp` |
| 泛型点类型策略 | 已读，待按 PI1 决定是否需要 | `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md` |

## 假设与候选族

| candidate family | 目标 | 当前判断 | 恢复条件 |
| --- | --- | --- | --- |
| point-normal narrow PI1 | 先做 phase-local `PointNormal` 生产接入 | unblocked | 用户确认进入 production integration loop |
| generic point type expansion | 若 production gate 需要模板泛型 | deferred | 必须先有 phase-local narrow 生产计划或 traits gate 方案 |
| public-probe-prep | 冻结最终 public boundary、fallback 和 asm contract | unblocked | 生产接入前完成 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| cube-index-prepass | dense-grid-cell | `PointNormal` / `float` / contiguous grid | test helper `runPrepassCandidate` | `run_test_compare` passed | board repeated `2.431x/2.681x/3.217x` | `prepass_repeated` | `dump_bench_rvv` showed RVV scan | `0/3/0` | strong-positive / diagnostic complete | use as PI1 motivation |
| point-normal narrow PI1 | public overload | `PointNormal` / `float` / contiguous AoS | `performReconstruction()` | not yet covered | not yet covered | missing | missing | missing | user-checkpoint | await user confirmation |
| generic point type expansion | public template | `PointNT` / `Scalar` / traits-gated layout | `performReconstruction()` | not yet covered | not yet covered | missing | missing | missing | deferred | require traits proof |

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| PI1 boundary spec | write narrow production boundary for `PointNormal` | public API, fallback, and phase-local exception are explicit |
| traits audit | if generic expansion is requested, map `PointNT` to `RVV Generic Point Type Strategy` | source / target / layout / stride gates are explicit |
| asm contract | define what RVV instruction attribution must remain visible after PI1 | helper and public path symbols remain separable |
| user checkpoint | pause before any production source change | user confirms whether to enter production integration loop |

## 继续 / 停止条件

本阶段必须在用户确认前停止在计划层。若用户确认进入 PI1，就按 narrow `PointNormal` boundary 继续；若用户要求泛型接入，则先补 traits gate 方案，不直接把 `PointNormal` 诊断证据外推成泛型 production 结论。
