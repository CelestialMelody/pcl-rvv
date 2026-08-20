# Phase 030 结果：PI1 production integration plan

## 实际执行范围

本阶段把 `cube-index-prepass` 从 helper 诊断接到 production public path：`pcl::MarchingCubes<PointNT>::performReconstruction()` 在 `__RVV10__` 下先走 RVV active-cell prepass，再对 active cell 调回原有标量 `createSurface()`。用户已经确认“如果有收益，先接入”，因此当前生产 patch 进入 adopted production behavior（已采纳生产行为），范围仍收窄到 `PointNormal`。

## 动作回填

| action | status | evidence | result |
| --- | --- | --- | --- |
| production integration | done / adopted | `surface/include/pcl/surface/impl/marching_cubes.hpp`、`surface/include/pcl/surface/marching_cubes.h` | public path 采用 RVV prepass + scalar emit 的最小生产形态。 |
| exact-type gate | done / adopted narrow gate | `std::is_same_v<PointNT, pcl::PointNormal>` | 这轮 production adoption 只让 `PointNormal` 命中 RVV，其他模板实例继续回退标量。 |
| correctness | done | `make run_test_compare` | Std/RVV 各 4 tests passed，包含 production-direct synthetic public path。 |
| asm | done | `make dump_bench_rvv` | `bench_marching_cubes_rvv` 可见 RVV load/store、`vmerge`、`vmflt`、`vmfeq` 等指令。 |
| board repeated | done / refreshed to 3-run | `log/board/production_direct_smoke1/board`、`log/board/production_direct_smoke2/board`、`log/board/production_direct_smoke3/board` | production-direct repeated board 三轮完成。 |
| Evidence Doctor | done / needs steady rerun follow-up | `log/board/production_direct_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | `Errors=0`、`Warnings=4`、`Suggestions=0`；Warnings 主要来自 3-run 低重复数和 sparse case 组内偏离。 |

## Board 结果

| case | median | values | bucket | 解释 |
| --- | ---: | --- | --- | --- |
| `mc_prod_sphere_64` | `5.364x` | `5.364x, 5.443x, 5.324x` | positive | public path 稳定正向。 |
| `mc_prod_wave_72` | `6.388x` | `6.386x, 6.464x, 6.388x` | positive | public path 稳定正向。 |
| `mc_prod_sparse_sphere_80` | `8.840x` | `8.840x, 8.980x, 8.806x` | positive | public path 稳定正向，但偏离组中位较大，需要单列说明。 |

## 生产边界审计

- 当前 public entry 仍是 `performReconstruction()`，但 `__RVV10__` 下会先尝试 RVV active-cell prepass。
- `voxelizeData()` 仍然由子类提供，未改 public 语义。
- `createSurface()`、三角输出和 `PointNT` 构造仍保留原标量语义。
- 这轮证据只支持 `PointNormal` 作为 phase-local exception；若后续要放宽到泛型点类型，必须再做 traits / layout gate 证明。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-public diagnostic`。 |
| A/B boundary | `public reconstruction wrapper`。 |
| 当前决策问题 | `RVV-vs-scalar` 的 production-direct 接入。 |
| diagnostic 是否可外推到 production | 这轮已经进入 public boundary，但仍只对当前 synthetic public probe 成立；generic PointT 还不能外推。 |
| comparison-boundary / baseline mismatch 风险 | 有。当前 board 仍是 synthetic grid，不能直接替代真实 Hoppe/RBF 输入分布。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前为正向，且用户已确认先接入；后续要补 5-run steady board，降低 low_run_count 风险。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；后续若扩大范围，仍需同一 public boundary 的更细对照。 |

## 下一步

当前状态是 `production adopted / narrow PointNormal gate`。默认下一阶段是 Phase 040：

1. 先补 production-direct 5-run steady board summary，处理 Evidence Doctor 的低重复数 warning。
2. 增加非 `PointNormal` 模板实例 fallback correctness，确保 exact-type gate 没把其它 `PointNT` 意外带入 RVV。
3. 做 generic point type traits / layout 审计，判断能否把 active-cell prepass 从 exact `PointNormal` 放宽到 `RVVXYZAoSFloatLayout<PointNT>` 或继续保持窄门禁。
