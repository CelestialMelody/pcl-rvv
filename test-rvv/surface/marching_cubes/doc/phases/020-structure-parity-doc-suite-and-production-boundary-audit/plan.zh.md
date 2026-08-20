# Phase 020 计划：structure parity doc-suite + production boundary audit

## 阶段意图和边界

本阶段不修改 production 源码，先把当前 topic 的 doc suite 对齐到可恢复质量，并冻结进入 PI1 前必须确认的生产边界。前一阶段已经证明 `cube-index-prepass` 在 test helper 边界上强正向，但仍不能直接外推到 `performReconstruction()` 公开入口。

| scope item | 本阶段范围 |
| --- | --- |
| evidence role | `production-shaped diagnostic` + structure parity audit。 |
| A/B boundary | `test helper` 证据继续保留，同时审计 `performReconstruction()` 公开边界。 |
| 当前决策问题 | `diagnostic-to-production mismatch`、`doc-suite parity`、`implementation-shape`。 |
| point type / Scalar | 仍以 `PointNormal` / `float` 为当前已验证范围。 |
| layout | 继续按 contiguous grid / dense cell 形态审计，不扩大到 generic point type。 |
| 不覆盖范围 | production patch、fallback 采纳、generic 点类型扩展。 |

## 当前状态清单

| item | 状态 | evidence |
| --- | --- | --- |
| Phase 000 | done / attempted / neutral-to-weak-positive | `doc/phases/000-current-state-and-edge-interpolation-diagnostic/result.zh.md` |
| Phase 010 | done / strong-positive diagnostic | `doc/phases/010-cube-index-prepass-diagnostic/result.zh.md` |
| board repeated | 可复用但不再作为本阶段主证据 | `log/board/edge_interpolation_repeated/`、`log/board/prepass_repeated/` |
| production source | 尚未修改 | `surface/include/pcl/surface/impl/marching_cubes.hpp` |
| doc suite | 仍缺 README / testing-overview / correctness-tests / benchmark-and-evidence / optimization-evidence / code map 主路径 | `test-rvv/surface/marching_cubes/doc/` |

## 假设与候选族

| candidate family | 目标 | 当前判断 | 恢复条件 |
| --- | --- | --- | --- |
| structure-parity-doc-suite | 补齐 topic-local 读者路径和证据分工 | unblocked | 当前 phase 可直接执行 |
| production-boundary-audit | 冻结 public entry、fallback、layout / point type gate | unblocked | 读取 production source 与相关 test assets |
| public-probe-prep | 为 PI1 冻结可改动边界 | deferred | doc suite 与边界审计完成后继续 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| cube-index-prepass | dense-grid-cell | `PointNormal` / `float` / contiguous grid | test helper `runPrepassCandidate` | `run_test_compare` 已通过 | board repeated 已通过 | `prepass_repeated` | `dump_bench_rvv` 已通过 | `0/3/0` | strong-positive / diagnostic complete | prepare PI1 boundary audit |
| edge-interpolation-rvv | dense-grid-cell | `PointNormal` / `float` / contiguous grid | test helper `runCandidate` | `run_test_compare` 已通过 | board repeated 已完成 | `edge_interpolation_repeated` | `dump_bench_rvv` 已通过 | `1/4/2` | attempted / not production candidate | pause this family |
| full-public-probe | public overload | `PointNT` / `Scalar` / production layout | `performReconstruction()` | not yet covered | not yet covered | missing | missing | missing | deferred | PI1 after parity audit |

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| doc suite | add README / testing-overview / correctness-tests / benchmark-and-evidence / optimization-evidence / test-support-code-map | reviewer can find commands, evidence, and code map without opening phase logs first |
| boundary scan | inspect `surface/include/pcl/surface/impl/marching_cubes.hpp` and public declarations | freeze public entry, fallback, `PointNT`, `Scalar`, and layout gate |
| mismatch audit | keep `diagnostic-to-production mismatch audit` current | result / evaluation / roadmap agree on what diagnostic can and cannot prove |
| roadmap refresh | update candidate frontier and recovery queue | next phase is explicit, not implied |
| docs refresh | result, roadmap, matrix, evaluation, README | no stale recovery path remains in current topic docs |

## Evidence Doctor 和 registry 规则

本阶段不新增 board repeated 结论，但如果 doc suite 或 boundary scan 产出新的摘要文件，必须把新产物纳入 topic-local 记录，避免 summary / doctor / result 互相指向不同 truth。若后续正式进入 PI1，manifest 和 doctor 仍要保留独立文件，不把 parse 结果写回 production source。

## 阶段完成条件

1. topic-local doc suite 补齐，并且 README 能指向正确的测试、bench、板卡和 phase 文档。
2. production boundary audit 冻结 `performReconstruction()`、`voxelizeData()`、`getNeighborList1D()` 和 `createSurface()` 的职责边界。
3. `README.zh.md`、`optimization-roadmap.zh.md`、`optimization-matrix.zh.md`、`marching_cubes-evaluation.zh.md` 与 phase index / result 互相一致。
4. 下一阶段默认入口明确指向 PI1 生产边界计划，而不是继续重复 helper-only board run。

## 继续 / 停止条件

当前阶段默认继续。只有遇到需要用户判断的 production 采纳 / 回滚、无法安全读取 production source、或 doc suite 需要外部依赖时才停止。否则本 phase 结束后直接进入 PI1 生产边界计划。
