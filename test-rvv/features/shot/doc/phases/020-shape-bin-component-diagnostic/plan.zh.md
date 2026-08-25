# Phase 020 plan: shape-bin component diagnostic

## 阶段意图和边界

本阶段继续保持不修改 production（生产源码），只在 test-rvv 中拆出 `createBinDistanceShape` 的 component ablation（组件消融）。目标是验证 normal dot（法线点积）、finite mask（有限值掩码）、clamp（夹紧到 [-1, 1]）和 shape-bin 映射是否值得作为后续 production probe（生产探针）候选。

`validated_scope`：测试专用 SoA（structure of arrays，按字段拆开的数组）normal 输入，固定 `frame_z`，`nr_shape_bins=10`，有限 normal 与少量 NaN normal fallback，输出为 `double` bin distance，长度覆盖小数组、352 邻域批量和更大 batch。

`unvalidated_scope`：真实 `pcl::Normal` AoS（array of structures，结构数组）跨步加载、estimator protected state、`frames_` / `normals_` 对象生命周期、`PCL_WARN` nan_counter、public SHOT / SHOTColor 入口收益、production fallback / dispatch。

`phase_closeout_boundary`：本阶段最多关闭 shape-bin test-only helper 的 correctness、asm 和 board A/B；不能关闭 production `createBinDistanceShape` 或公开入口收益。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| Phase 010 | normalization component positive，但 production probe 需要显式授权。 | `doc/phases/010-normalize-component-diagnostic/result.zh.md` |
| production source | `createBinDistanceShape` 对每个邻域 normal 做 finite 检查、dot current frame z、clamp 并写 `std::vector<double>`。 | `features/include/pcl/features/impl/shot.hpp` |
| current helpers | 已有 fixtures 与 normalization helper；尚无 shape-bin component helper。 | `include/impl/` |
| current bench | 已有 public fixed-LRF 和 normalization component case；尚无 shape-bin case。 | `src/bench_shot.cpp` |

## 假设与候选族

| candidate family | hypothesis | risk |
| --- | --- | --- |
| shape-bin SoA RVV | 三个 normal 分量按 SoA 连续加载时，dot + clamp 可以形成低风险 RVV helper。 | production 当前是 `pcl::Normal` AoS，SoA 可能需要 staging（分阶段暂存）才适用；输出 double 可能抵消部分收益。 |
| shape-bin AoS RVV later | 若 SoA helper positive，再评估跨步加载 `pcl::Normal` 的真实 shape。 | AoS stride 和 NaN warning 计数会增加维护成本。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| shape-bin SoA RVV | neighbor normal batch | `float nx/ny/nz` SoA input, `double` output | test-only `computeShapeBinDistanceRVV` | `run_test_compare` shape-bin tests | `shape_bin_component` | planned | planned | planned | planned |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criterion |
| --- | --- | --- | --- |
| C1 red test | 修改 `src/test_shot.cpp` 先引用 shape-bin helper | 缺 helper 时编译失败。 | `run_test_compare` fail at expected missing symbols。 |
| C2 helper implementation | 新增 `include/impl/shot_shape_bin.hpp` 并从 `include/shot.h` 聚合 | scalar reference 与 RVV candidate 可编译。 | Std/RVV correctness 通过，NaN normal 输出 NaN。 |
| C3 component bench | 扩展 `src/bench_shot.cpp` 和 manifest case 字典 | `shape_bin_component` timing 和 checksum 可解析。 | board compare 能看到 shape-bin case。 |
| C4 asm | `make -C test-rvv/features/shot dump_bench_rvv` | `vfmacc` / clamp / store 指令能归因到 helper callsite。 | 无法归因则降级。 |
| C5 board evidence | `board_smoke` 或 `run_board_bench_compare fetch_board_logs` | component A/B、checksum、Evidence Doctor。 | 板卡可用时完成；异常按 doctor 降级。 |
| C6 docs | Phase 020 result、matrix、roadmap、evaluation、README | 可恢复 decision。 | 每个动作回填 done / partial / deferred / blocked。 |

## Evidence Doctor 和 freshness 规则

新增 `shape_bin_component` 后必须更新 `script/generate_shot_evidence_manifest.py` 的 case metadata，否则 doctor 会忽略新 case。若 current manifest（当前证据清单）仍只能表达单 run，阶段 result 需要把 run-labelled 复跑作为人工稳定性证据单独列出。

## 板卡复跑预算和决策桶

先跑一次 board smoke 或 board bench compare。若 `shape_bin_component` 大于 1.10x，最多追加 2 次复跑确认；`1.02x-1.10x` 记为 weak_positive；接近 1 或低于 1 记为 neutral / negative，不进入 production integration loop。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic。 |
| A/B boundary | test helper / SoA component bench。 |
| 当前决策问题 | implementation-shape and RVV-vs-scalar component A/B。 |
| diagnostic 是否可外推到 production | no for direct adoption。SoA 输入不等于 production AoS `pcl::Normal`，只能判断 dot/clamp 算法本身是否值得继续。 |
| comparison-boundary / baseline mismatch 风险 | yes。production 还有 protected state、normal cloud indexing、warning side effect 和 double vector output。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。需要 positive + AoS/layout audit 才能进入 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。 |

## 继续 / 停止条件

默认继续到 C1-C6。只有编译 / 工具 / 板卡失败且无法修复、correctness mismatch 无法收敛、dirty isolation 不安全，或继续需要修改 production 时才停止。
