# Phase 020 结果：structure parity doc-suite + production boundary audit

## 实际执行范围

本阶段仍不修改 `surface/include/pcl/surface/impl/marching_cubes.hpp`。它只做两件事：一是补齐 topic-local doc suite，二是冻结进入 PI1 前必须明确的 production boundary。Phase 010 的 `cube-index-prepass` 仍然只算 helper-level strong-positive diagnostic，不被写成 production evidence。

## 动作回填

| action | status | evidence | result |
| --- | --- | --- | --- |
| doc suite | done | `doc/README.zh.md`、`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | reviewer 现在可以从 topic-local 文档找到命令、证据和代码地图。 |
| boundary scan | done | `surface/include/pcl/surface/impl/marching_cubes.hpp`、`surface/include/pcl/surface/marching_cubes.h` | `performReconstruction()`、`voxelizeData()`、`getNeighborList1D()`、`createSurface()` 的职责链条已厘清。 |
| mismatch audit | done | `doc/marching_cubes-evaluation.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md` | 诊断证据仍不能直接外推到 public production path。 |
| roadmap / matrix refresh | done | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | Phase 020 作为下一恢复入口已明确。 |

## 生产边界审计摘要

- `performReconstruction()` 是 public 入口，先做 `iso_level_` 检查，再建 `grid_` 和 voxel size。
- 真正的 production 成本集中在 `voxelizeData()`、三重 cell 扫描、`getNeighborList1D()`、`createSurface()` 和最终 `points` / `polygons` 输出。
- 当前 diagnostic 的收益来自 helper-level active-cell 预扫描，不覆盖真实 `voxelizeData()` 或 public dispatch 成本。
- `MarchingCubes<PointNT>` 是模板入口。当前证据只覆盖 `PointNormal` / `float` 形态；若后续 production probe 先用 exact `PointNormal` gate，必须写成阶段性收窄，其它 `PointNT` 自然 fallback。若要泛型接入，需要按 `RVV Generic Point Type Strategy` 证明当前 `PointNT` 的 `x/y/z` 单个 `float` 字段、POD / standard-layout、字段 offset 和 stride 前提。
- 由于还没进入 PI1，本阶段不写 production patch，也不更新 `doc-rvv` 长期主题文档。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`。 |
| A/B boundary | `test helper`。 |
| 当前决策问题 | `implementation-shape`，并准备 `diagnostic-to-production mismatch` 的下一步审计。 |
| diagnostic 是否可外推到 production | 不能直接外推；helper prepass 的强正向不证明 public path 采纳后仍能保持收益。 |
| comparison-boundary / baseline mismatch 风险 | 有。当前基线是 synthetic grid 和 test helper，未覆盖真实 `performReconstruction()` public boundary。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段结果不是弱/负/中性，但仍只允许进入 PI1 计划，不允许直接修改 production。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；若要采纳，后续必须在同一 public boundary 内补齐对照。 |

## 下一步

默认恢复入口指向 PI1 生产边界计划。当前 topic 仍处于 `phase_deferred + unblocked`，但没有任何证据足以跳过用户检查点直接落 production patch。
