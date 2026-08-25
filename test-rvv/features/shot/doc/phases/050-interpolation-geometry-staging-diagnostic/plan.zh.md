# Phase 050 plan: interpolation geometry staging diagnostic

## 阶段意图和边界

本阶段继续保持 test-only diagnostic（测试专用诊断），不修改 `features/include/pcl/features/impl/shot.hpp`。Phase 020-040 证明 shape-bin 前置距离组件有收益，但 public fixed-LRF 入口仍约 1x，说明后续 `interpolateSingleChannel` / `interpolateDoubleChannel` 的投影、数学函数和 histogram scatter（直方图离散写入）可能稀释收益。

本阶段只评估 interpolation geometry staging（插值几何暂存）：按 `pcl::Indices` gather surface point，减中心点，投影到 local reference frame（局部参考系）三轴，计算 distance，并生成有效 lane 标记。它不实现完整 quadrilinear interpolation（四线性插值），也不写 histogram。

`validated_scope`：`pcl::PointCloud<pcl::PointXYZ>` surface + `pcl::Indices`，contiguous `sqr_dists` 和 `binDistance`，固定 central point / frame axes，输出 `double x/y/z/distance` 和 `uint8_t valid`。

`unvalidated_scope`：`acos` / `atan2`、radius / inclination / azimuth 相邻 bin 投票、histogram scatter、SHOT1344 double-channel stride、真实 estimator state、production dispatch。

`phase_closeout_boundary`：最多关闭 test-only geometry staging helper 的 correctness、asm 和 board component A/B；不能关闭完整 interpolation 或 production 入口收益。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| Phase 040 | indexed normal gather component 1.65x-1.86x，仍未接 production。 | `doc/phases/040-shape-bin-indexed-gather-diagnostic/result.zh.md` |
| PI1 | shape-bin indexed production probe plan 已写好，但 PI2 需要生产授权。 | `doc/phases/PI1-shape-bin-indexed-production-probe-plan/plan.zh.md` |
| production interpolation | `interpolateSingleChannel` / `interpolateDoubleChannel` 每邻域点执行 indexed surface load、三轴 dot、sqrt、`acos` / `atan2` 和多处 histogram update。 | `features/include/pcl/features/impl/shot.hpp` |
| current test support | 尚无 interpolation staging helper / test / bench case。 | `test-rvv/features/shot/include/impl` |

## 假设与候选族

| candidate family | hypothesis | risk |
| --- | --- | --- |
| interpolation geometry staging RVV | indexed gather + 三轴 dot + distance 可以批量化，若组件正向，后续可考虑把前置投影暂存给标量 histogram tail。 | gather、offset staging、double sqrt 和四路输出 store 可能抵消收益。 |
| full interpolation RVV | 若 geometry staging 正向，后续可以单独诊断 bin selection、`acos` / `atan2` 或 scatter tail。 | 完整 histogram scatter 分支多、写入冲突多，生产维护风险高。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| interpolation geometry staging RVV | indexed surface point batch | `pcl::PointXYZ` AoS + `pcl::Indices`, f32 input, double staging output | planned test-only `computeInterpolationGeometryIndexedRVV` | planned `run_test_compare` interpolation test | planned `interpolation_geometry_component` | planned | planned | planned | planned |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criterion |
| --- | --- | --- | --- |
| F1 red test | 修改 `src/test_shot.cpp` 引用 interpolation geometry helper | 缺 helper 时编译失败。 | `run_test_compare` fail at expected missing symbols。 |
| F2 helper implementation | 新增 `include/impl/shot_interpolate.hpp` 并接入 `include/shot.h` | scalar reference 与 RVV candidate 可编译，输出 arrays 和 valid mask。 | Std/RVV correctness 通过。 |
| F3 component bench | 扩展 `src/bench_shot.cpp` 和 manifest case 字典 | `interpolation_geometry_component` timing 和 checksum 可解析。 | board compare 能看到新 case。 |
| F4 asm | `make -C test-rvv/features/shot dump_bench_rvv` | 能看到 indexed gather、widen、FMA / subtract、sqrt 和 stores。 | 无法归因则降级。 |
| F5 board evidence | targeted `run_board_bench_compare fetch_board_logs` | component A/B、checksum、Evidence Doctor。 | 若 `>1.10x`，最多追加 2 次复跑；否则按桶降级。 |
| F6 docs | Phase 050 result、matrix、roadmap、evaluation、README | 可恢复 decision。 | 每个动作回填 done / partial / deferred / blocked。 |

## Evidence Doctor 和 freshness 规则

新增 `interpolation_geometry_component` 后必须更新 `script/generate_shot_evidence_manifest.py` 的 case metadata。当前 board compare 裸路径会被覆盖，采集结果必须复制到 `phase050_*` run-labelled 目录后再写入 result。

## 板卡复跑预算和决策桶

先跑一次 targeted board compare。若 `interpolation_geometry_component` 大于 1.10x，最多追加 2 次复跑确认；`1.02x-1.10x` 记为 weak_positive；接近 1 或低于 1 记为 neutral / negative。预算耗尽后若方向稳定，按该桶关闭当前 diagnostic；若方向摇摆则标为 unstable。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic。 |
| A/B boundary | test helper / interpolation geometry staging component bench。 |
| 当前决策问题 | implementation-shape and RVV-vs-scalar component A/B。 |
| diagnostic 是否可外推到 production | no for direct adoption。它只覆盖插值前置几何暂存，不覆盖完整 histogram 更新、`acos` / `atan2` 和 public entry。 |
| comparison-boundary / baseline mismatch 风险 | yes。production interpolation 会在同一循环里直接更新 descriptor，staging arrays 会引入额外 store/load。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。弱或负向结果只说明该 geometry staging 形态不值得直接接 production，不拒绝其它 interpolation 子候选。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。 |

## 继续 / 停止条件

默认继续到 F1-F6。只有编译 / 工具 / 板卡失败且无法修复、correctness mismatch 无法收敛、dirty isolation 不安全，或继续需要修改 production 时才停止。
