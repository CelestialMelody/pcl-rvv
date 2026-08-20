# Phase 000 结果：edge interpolation diagnostic

## 实际执行范围

本阶段已建立 `test-rvv/surface/marching_cubes` topic-local scaffold（脚手架），不修改 production 源码。诊断边界保持为 `production-shaped diagnostic`（生产形态诊断）：测试专用 helper 复刻 `createSurface` 的 active cell 输出语义，Std/RVV 两侧共享 synthetic signed-distance grid（合成有符号距离网格）。

## 动作回填

| action | status | evidence | result |
| --- | --- | --- | --- |
| scaffold | done | `Makefile`、`board.mk`、`include/marching_cubes.h`、`include/impl/marching_cubes_core.hpp`、`src/test_marching_cubes.cpp`、`src/bench_marching_cubes.cpp` | 采用 `src/` + `include/impl` 布局，生产源码未改。 |
| QEMU correctness | done | `make run_test_compare`，`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Std/RVV 各 3 tests passed。 |
| QEMU bench smoke | done / qemu_smoke_only | `make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 0 --case-filter mc_edge_sphere_64'` | 只验证 RVV bench 可运行和日志格式；不作为性能结论。 |
| asm | done | `make dump_bench_rvv`、`build/asm/riscv/bench_marching_cubes_rvv.asm` | 可见 `vfdiv.vv`、`vfmacc.vv`、`vle32.v`、`vse32.v`，归属到 bench RVV binary。 |
| board smoke + bounded rerun | done | `log/board/analyze_bench_compare.log`、`log/board/edge_interpolation_smoke2/board/analyze_bench_compare.log` | 两次同边界 board run 完成。 |
| board repeated summary / manifest / doctor | done | `log/board/edge_interpolation_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | Evidence Doctor：Errors=1、Warnings=4、Suggestions=2。 |

## Board 结果

| case | median | values | bucket | 解释 |
| --- | ---: | --- | --- | --- |
| `mc_edge_sphere_64` | `0.992x` | `0.995x, 0.989x` | neutral / degradation error | 两次都低于 1，不能外推为正向。 |
| `mc_edge_wave_72` | `1.024x` | `1.020x, 1.027x` | neutral | 接近 1.0，Evidence Doctor 给 near-threshold suggestion。 |
| `mc_edge_sparse_sphere_80` | `1.049x` | `1.046x, 1.052x` | weak_positive | 稳定弱正向，但收益不足以单独进入 production probe。 |

## Evidence Doctor

`log/board/edge_interpolation_repeated/evidence_doctor.md` 报告 `Errors=1`、`Warnings=4`、`Suggestions=2`。Error 是 `mc_edge_sphere_64` 的 `ba_degradation_frequency`，两次 B/A 都低于 1。Warnings 包含 2-run 低样本数和 `mc_edge_sphere_64` 组内偏离。Suggestions 指出 wave / sparse sphere 都接近阈值。

处理动作：本阶段不把 `edge-interpolation-rvv` 写成 production candidate（生产候选）。它只保留为 `attempted / weak-or-neutral` 诊断结果；如果后续有 full-public evidence（真实公开入口证据）说明 active cell emission 是主成本，可恢复做更低 staging 的实现族比较。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`，实际 board summary manifest 用 strict A/B 描述同一 test helper 边界。 |
| A/B boundary | `test helper`，不是 public overload，也不是 production dispatch。 |
| 当前决策问题 | `RVV-vs-scalar` helper 候选筛选。 |
| diagnostic 是否可外推到 production | 不能。它只覆盖 `createSurface` 风格 helper，不覆盖 `voxelizeData()`、真实对象状态和 polygon 输出整体成本。 |
| comparison-boundary / baseline mismatch 风险 | 同一 helper、同一 synthetic grid、同一 checksum policy；但 production boundary 缺失。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前不允许。一个 case 有退化 Error，其它 case 只是 near-threshold 弱信号。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要 production direct Std/RVV positive 和用户确认；Phase 000 不满足。 |

## Optimization matrix 更新

| candidate family | decision | evidence | unblocked next action |
| --- | --- | --- | --- |
| edge-interpolation-rvv | attempted / neutral-to-weak-positive / not production candidate | QEMU correctness 通过；asm 有 RVV；board median `0.992x/1.024x/1.049x`；doctor `1/4/2` | 暂停该 family，除非后续 full-public profile 指向 edge interpolation 为主成本。 |
| cube-index-prepass | planned / unblocked | 当前 active cell ratios 暴露大量 inactive / NaN skip 工作 | 进入 Phase 010，批量判断 active cell 并只对 active cell 调 scalar emit。 |
| full-public-probe | deferred | helper 未形成足够正向证据 | 需要 Phase 010 或其它 candidate positive 后再考虑 PI1。 |

## 阶段反思新增路线

Phase 000 说明“只优化 12 条 edge 插值”太窄，收益被每个 cell 的 staging 和三角输出稀释。下一条更值得验证的路线是 `cube-index-prepass`：把 `leaf < iso`、NaN skip 和 `edgeTable[cubeindex] != 0` 提前批量化，减少进入 surface emission 的 cell 数量。它仍是 diagnostic，不修改 production。

## continue_stop_decision

`micro_stop_guard` 触发继续：当前 phase 已完成，但 roadmap 仍有授权范围内、未阻塞的 `cube-index-prepass` 诊断动作。板卡可用，dirty isolation 也可限定在当前 topic，因此继续 Phase 010。
