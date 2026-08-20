# Phase 010 结果：cube-index prepass diagnostic

## 实际执行范围

本阶段继续保持 `test-rvv/surface/marching_cubes` 只做测试专用诊断，不修改 `surface/include/pcl/surface/impl/marching_cubes.hpp`。候选 `cube-index-prepass` 先用 RVV 批量扫描 `z` 方向 active cell，再把 active cell 交给标量 `emitSurfaceStd` 输出三角形，因此仍属于 `production-shaped diagnostic`，不是 production patch。

## 动作回填

| action | status | evidence | result |
| --- | --- | --- | --- |
| helper | done | `include/impl/marching_cubes_core.hpp` 中 `scanActiveCellsZRVV`、`runPrepassCandidate` | RVV 只负责 active-cell 预扫描，标量 emit 仍保留。 |
| correctness | done | `make run_test_compare` | Std / RVV 各 3 tests passed，reference、edge candidate、prepass candidate 统计一致。 |
| bench / asm | done | `make dump_bench_rvv`、`src/bench_marching_cubes.cpp` | RVV bench 保持可构建，边界内可看到 RVV 指令。 |
| board repeated | done | `log/board/prepass_smoke1/board`、`log/board/prepass_smoke2/board` | 复跑两次，prepass 三个 case 全部明显快于标量。 |
| Evidence Doctor | done | `log/board/prepass_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | Doctor：`Errors=0`、`Warnings=3`、`Suggestions=0`。 |

## Board 结果

| case | median | values | bucket | 解释 |
| --- | ---: | --- | --- | --- |
| `mc_prepass_sphere_64` | `2.431x` | `2.427x, 2.435x` | positive | 稳定正向。 |
| `mc_prepass_wave_72` | `2.681x` | `2.673x, 2.690x` | positive | 稳定正向。 |
| `mc_prepass_sparse_sphere_80` | `3.217x` | `3.200x, 3.233x` | positive | 稳定正向。 |

## Evidence Doctor

`log/board/prepass_repeated/evidence_doctor.md` 只剩 `low_run_count` 警告，没有 Error。说明这条 diagnostic 在当前边界上是强正向的，但重复次数仍然偏少，不能直接升级成严格 production performance 结论。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`。 |
| A/B boundary | `test helper`。 |
| 当前决策问题 | `RVV-vs-scalar` / `implementation-shape`。 |
| diagnostic 是否可外推到 production | 不能直接外推。它验证的是测试 helper 中 active-cell 预扫描是否划算，尚未覆盖真实 `performReconstruction()` 的 `voxelizeData()`、`getNeighborList1D()`、`createSurface()` 和 `PointNT` 输出成本。 |
| comparison-boundary / baseline mismatch 风险 | 有。当前 A/B 共享 synthetic grid 和 checksum 规则，但 production boundary 还没冻结。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段结果已是正向，因此可进入下一阶段的有界生产边界审计；仍不能直接写成采纳。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。若后续要采纳，必须在同一 production boundary 内补 detail A/B。 |

## Optimization matrix 更新

| candidate family | decision | evidence | unblocked next action |
| --- | --- | --- | --- |
| edge-interpolation-rvv | attempted / neutral-to-weak-positive / not production candidate | QEMU correctness 通过；asm 有 RVV；board repeated `0.992x/1.024x/1.049x`；doctor `1/4/2` | 暂停该 family。 |
| cube-index-prepass | attempted / strong-positive / not production candidate | QEMU correctness 通过；asm 可见 RVV；board repeated `2.431x/2.681x/3.217x`；doctor `0/3/0` | 进入生产边界与文档套件对齐阶段。 |
| full-public-probe | deferred | 仍缺 production boundary 和 doc-suite parity 证据 | Phase 020。 |

## 阶段反思新增路线

Phase 010 的结果说明，单纯把 12 条 edge 插值向量化还不够，真正有收益的是先筛 active cell。下一阶段的高优先级动作不再是继续扩大 helper-only benchmark，而是把当前 topic 的 doc suite、公开入口边界和 production integration 冻结条件补齐，准备进入 PI1。

## continue_stop_decision

`continue`。当前 phase 已完成，但 roadmap 仍有未阻塞动作：`structure-parity-doc-suite`、`production-boundary-audit`、以及后续的 PI1 生产接入计划。板卡和 correctness 已经闭合，因此下一步继续 Phase 020。
