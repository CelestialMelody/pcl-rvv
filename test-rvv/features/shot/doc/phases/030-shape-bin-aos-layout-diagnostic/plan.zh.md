# Phase 030 plan: shape-bin AoS layout diagnostic

## 阶段意图和边界

本阶段仍不修改 production（生产源码），只在 `test-rvv/features/shot` 中把 Phase 020 的 shape-bin SoA（structure of arrays，按字段拆开的数组）候选推进到更接近 production 的 AoS（array of structures，结构数组）布局。目标是回答：当输入来自连续 `pcl::Normal` normal cloud（法线点云）时，RVV stride load（跨步加载）是否仍保留 dot + clamp 的收益。

`validated_scope`：测试专用连续 `pcl::PointCloud<pcl::Normal>`，按起点和 count 连续读取 normal，固定 `frame_z`，`nr_shape_bins=10`，输出为 `double` bin distance。

`unvalidated_scope`：production 中 `indices` 指向的任意邻域 gather（离散加载）、非连续 normal index、`frames_` / `normals_` protected state（受保护对象状态）、`PCL_WARN` nan_counter 副作用、真实 `createBinDistanceShape` 分流和公开入口收益。

`phase_closeout_boundary`：本阶段最多关闭 contiguous AoS test-only helper 的 correctness、asm 和 board A/B；不能关闭 arbitrary indices gather 或 production helper。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| Phase 020 | shape-bin SoA component 三次板卡 run 稳定 2.38x-2.47x，但不能外推到 AoS / indices。 | `doc/phases/020-shape-bin-component-diagnostic/result.zh.md` |
| production source | `createBinDistanceShape` 从 `(*normals_)[indices[i_idx]].getNormalVector4fMap()` 读取 normal。 | `features/include/pcl/features/impl/shot.hpp` |
| current helper | `computeShapeBinDistanceRVV` 使用 SoA 连续输入。 | `include/impl/shot_shape_bin.hpp` |
| current bench | 有 `shape_bin_component`，尚无 AoS layout case。 | `src/bench_shot.cpp` |

## 假设与候选族

| candidate family | hypothesis | risk |
| --- | --- | --- |
| shape-bin contiguous AoS RVV | `pcl::Normal` 的 `normal_x/y/z` 字段可用 stride load 批量读取，收益可能低于 SoA 但仍为 positive。 | stride load 比连续 load 更贵；`pcl::Normal` layout 需要用字段 offset 审慎读取。 |
| shape-bin indexed gather RVV later | 若 AoS contiguous 仍 positive，再评估 arbitrary indices gather。 | gather 成本和 warning side effect 可能抵消收益，且更接近 production 但维护风险更高。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| shape-bin AoS layout RVV | contiguous normal cloud batch | `pcl::Normal` AoS input, `double` output | test-only `computeShapeBinDistanceAoSRVV` | planned `run_test_compare` AoS test | `shape_bin_aos_component` | planned | planned | planned | planned |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criterion |
| --- | --- | --- | --- |
| D1 red test | 修改 `src/test_shot.cpp` 先引用 AoS helper | 缺 helper 时编译失败。 | `run_test_compare` fail at expected missing symbols。 |
| D2 helper implementation | 扩展 `include/impl/shot_shape_bin.hpp` | scalar reference 与 RVV stride-load candidate 可编译。 | Std/RVV correctness 通过，NaN normal 输出 NaN。 |
| D3 component bench | 扩展 `src/bench_shot.cpp` 和 manifest case 字典 | `shape_bin_aos_component` timing 和 checksum 可解析。 | board compare 能看到 AoS case。 |
| D4 asm | `make -C test-rvv/features/shot dump_bench_rvv` | 能看到 stride load / widen / FMA / double store 指令。 | 无法归因则降级。 |
| D5 board evidence | `run_board_bench_compare fetch_board_logs` | component A/B、checksum、Evidence Doctor。 | 若 `>1.10x`，最多追加 2 次复跑；异常按 doctor 降级。 |
| D6 docs | Phase 030 result、matrix、roadmap、evaluation、README | 可恢复 decision。 | 每个动作回填 done / partial / deferred / blocked。 |

## Evidence Doctor 和 freshness 规则

新增 `shape_bin_aos_component` 后必须更新 `script/generate_shot_evidence_manifest.py` 的 case metadata。裸 `log/board/*` 会被每次板卡 run 覆盖，当前证据必须复制到 run-labelled 目录后再写入 result。

## 板卡复跑预算和决策桶

先跑一次 board compare。若 `shape_bin_aos_component` 大于 1.10x，最多追加 2 次复跑确认；`1.02x-1.10x` 记为 weak_positive；接近 1 或低于 1 记为 neutral / negative。复跑预算耗尽后，若方向稳定就按该桶关闭当前 diagnostic；若方向摇摆则标为 unstable。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic。 |
| A/B boundary | test helper / contiguous AoS component bench。 |
| 当前决策问题 | implementation-shape and RVV-vs-scalar component A/B。 |
| diagnostic 是否可外推到 production | no for direct adoption。连续 AoS 比 SoA 更接近 production，但仍不覆盖 arbitrary `indices` gather、对象状态和 warning side effect。 |
| comparison-boundary / baseline mismatch 风险 | yes。production 按邻域 index 访问 normal cloud，本阶段只验证连续 layout。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。需要 positive + indexed gather / side-effect audit，或用户明确要求仅做受限生产探针。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。 |

## 继续 / 停止条件

默认继续到 D1-D6。只有编译 / 工具 / 板卡失败且无法修复、correctness mismatch 无法收敛、dirty isolation 不安全，或继续需要修改 production 时才停止。
