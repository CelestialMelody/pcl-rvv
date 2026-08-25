# Phase 040 plan: shape-bin indexed gather diagnostic

## 阶段意图和边界

本阶段仍不修改 production（生产源码），只在 test-rvv 中评估 `createBinDistanceShape` 最关键的 production data shape（生产数据形态）：`pcl::Indices` 指向 normal cloud（法线点云）中的任意 `pcl::Normal`。目标是验证 indexed gather（按索引离散加载）加上 NaN normal count（非法法线计数）后，shape-bin RVV 候选是否仍值得作为 production probe（生产探针）输入。

`validated_scope`：测试专用 `pcl::PointCloud<pcl::Normal>` + `pcl::Indices`，索引允许乱序和重复但必须在合法范围内；固定 `frame_z`，`nr_shape_bins=10`，输出 `double` bin distance，同时返回 NaN normal count。

`unvalidated_scope`：estimator protected state（受保护对象状态）、`frames_` / `normals_` lifetime（生命周期）、真实 `PCL_WARN` 文本输出、公开入口收益、indices 越界行为、production dispatch（生产分流）。

`phase_closeout_boundary`：本阶段最多关闭 test-only indexed gather helper 的 correctness、asm 和 board A/B；不能关闭 production helper 或公开入口收益。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| Phase 030 | `pcl::Normal` 连续 AoS component 三次板卡 run 稳定 1.76x-1.90x。 | `doc/phases/030-shape-bin-aos-layout-diagnostic/result.zh.md` |
| production source | `createBinDistanceShape` 使用 `(*normals_)[indices[i_idx]]`，并在 NaN normal 存在时发出 `PCL_WARN`。 | `features/include/pcl/features/impl/shot.hpp` |
| current helper | 已有 SoA 和连续 AoS shape-bin helper；尚无 indexed gather helper。 | `include/impl/shot_shape_bin.hpp` |
| current bench | 已有 `shape_bin_component` 和 `shape_bin_aos_component`；尚无 indexed gather case。 | `src/bench_shot.cpp` |

## 假设与候选族

| candidate family | hypothesis | risk |
| --- | --- | --- |
| shape-bin indexed gather RVV | 若索引 offset staging 成本可控，`vluxei32` gather 后仍可能 positive。 | gather、offset staging 和 NaN count scalar side work 可能抵消 AoS 收益。 |
| production probe later | 若 indexed component positive，再写 PI1 计划冻结 fallback、dispatch、warning side effect 和 public-entry evidence。 | 需要显式 production 授权，且仍要 public-entry direct test / bench。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| shape-bin indexed gather RVV | indexed normal cloud batch | `pcl::Normal` AoS + `pcl::Indices`, `double` output + NaN count | test-only `computeShapeBinDistanceIndexedRVV` | planned `run_test_compare` indexed test | `shape_bin_indexed_component` | planned | planned | planned | planned |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criterion |
| --- | --- | --- | --- |
| E1 red test | 修改 `src/test_shot.cpp` 先引用 indexed helper | 缺 helper 时编译失败。 | `run_test_compare` fail at expected missing symbols。 |
| E2 helper implementation | 扩展 `include/impl/shot_shape_bin.hpp` | scalar reference 与 RVV gather candidate 可编译，返回 NaN count。 | Std/RVV correctness 通过。 |
| E3 component bench | 扩展 `src/bench_shot.cpp` 和 manifest case 字典 | `shape_bin_indexed_component` timing 和 checksum 可解析。 | board compare 能看到 indexed case。 |
| E4 asm | `make -C test-rvv/features/shot dump_bench_rvv` | 能看到 `vluxei32` gather、widen、FMA 和 double store。 | 无法归因则降级。 |
| E5 board evidence | `run_board_bench_compare fetch_board_logs` with indexed case-filter | component A/B、checksum、Evidence Doctor。 | 若 `>1.10x`，最多追加 2 次复跑；异常按 doctor 降级。 |
| E6 docs | Phase 040 result、matrix、roadmap、evaluation、README | 可恢复 decision。 | 每个动作回填 done / partial / deferred / blocked。 |

## Evidence Doctor 和 freshness 规则

新增 `shape_bin_indexed_component` 后必须更新 manifest case metadata。裸 `log/board/*` 会被覆盖，当前证据必须复制到 `phase040_*` run-labelled 目录。

## 板卡复跑预算和决策桶

先跑一次 targeted board compare。若 `shape_bin_indexed_component` 大于 1.10x，最多追加 2 次复跑确认；`1.02x-1.10x` 记为 weak_positive；接近 1 或低于 1 记为 neutral / negative。复跑预算耗尽后若方向稳定，按该桶关闭当前 diagnostic；若方向摇摆则标为 unstable。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic。 |
| A/B boundary | test helper / indexed gather component bench。 |
| 当前决策问题 | implementation-shape and RVV-vs-scalar component A/B。 |
| diagnostic 是否可外推到 production | no for direct adoption。它覆盖 indices gather 和 NaN count，但不覆盖 estimator state、真实 warning 输出和 public entry。 |
| comparison-boundary / baseline mismatch 风险 | yes。production helper 还要 resize vector、读 `frames_`、调用 `getClassName()` 并可能发出 warning。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no，除非用户明确要求只做受限 PI1。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。 |

## 继续 / 停止条件

默认继续到 E1-E6。只有编译 / 工具 / 板卡失败且无法修复、correctness mismatch 无法收敛、dirty isolation 不安全，或继续需要修改 production 时才停止。
