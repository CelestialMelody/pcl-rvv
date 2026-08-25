# Phase 030 Direct AoS Pair Feature Diagnostic Plan

## 阶段意图和边界

本阶段尝试 `pfh-direct-point-load-rvv` test-only diagnostic（测试专用诊断）候选，用来回答：Phase 010 的 staged pair-feature RVV（先标量暂存再 RVV 计算）是否因为 staging（暂存）成本留下了可改进空间。新候选直接在 RVV chunk（向量分块）内从 `PointNormal` AoS（结构数组）点云按 pair index 读取 `x/y/z/normal_x/normal_y/normal_z`，再复用同一 pair tuple math（成对特征数学）和标量 histogram scatter（直方图离散累加）。

本阶段仍不修改 `features/include/pcl/features/impl/pfh.hpp` production（生产源码）。候选只服务 `test-rvv/features/pfh`；它不能证明 production dispatch（生产分流）或泛型点类型已闭合。

## 当前状态清单

| 项目 | 当前事实 |
| --- | --- |
| Phase 010 staged candidate | `candidate_pfh_pair_batch_rvv` 在 Milkv-Jupiter 5-run repeated 为 `2.33x, 2.33x, 2.34x, 2.34x, 2.35x`，Doctor Errors=0。 |
| production authorization | PI2 生产补丁仍需用户明确确认；本阶段不进入 production。 |
| 公共 traits / load helper | `pcl/rvv_point_traits.h` 有 `RVVXYZNormalFloatLayout<PointT>`；`pcl/rvv_point_load.h` 有 field-tag strided/indexed load helper。 |
| 本阶段输入 | `PointNormal` dense finite synthetic cloud，fixed wrapped neighborhood indices，`nr_split=5`。 |

## 候选假设

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `pfh-direct-point-load-rvv` | 直接用 pair index 的 RVV gather / field load 可减少 Phase 010 的 SoA staging 写入和内存流量。 | pair index staging 仍存在；gather 访存可能比连续 SoA 慢；normal field traits 和 production fallback 仍未闭合。 |
| `pfh-pair-feature-staged-rvv` | 连续 SoA 加载可能比 AoS gather 更快，即使 staging 有成本。 | 如果 direct AoS 更快，生产探针应优先比较 direct family，而不是直接接 staged family。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pfh-direct-point-load-rvv` | fixed neighborhood indices | `PointNormal`, float, AoS gather by pair index | test-only `computePointPFHSignatureDirectAoSRVV` | new gtest vs production helper | `candidate_pfh_direct_aos_rvv` | 5-run repeated if correctness + smoke pass | `dump_bench_rvv`, look for gather/field-load RVV instructions in candidate binary | repeated Doctor | planned | RED/GREEN direct-load candidate |

## 实现和测试动作

| action | 产物 | 命令 / 验收 |
| --- | --- | --- |
| RED-030 | 新增 gtest 调用尚不存在的 `computePointPFHSignatureDirectAoSRVV`。 | `make -B -C test-rvv/features/pfh run_test_rvv` 先因候选缺失失败。 |
| GREEN-030 | 在 test-only candidate header 中实现 direct AoS pair tuple RVV。 | `make -B -C test-rvv/features/pfh run_test_compare` 通过；histogram 逐 bin 阈值 `2e-3`。 |
| BENCH-030 | `bench_pfh.cpp` 新增 `candidate_pfh_direct_aos_rvv`。 | `dump_bench_rvv` 生成可归属 RVV 指令。 |
| BOARD-030 | 5-run repeated board。 | 与 staged candidate 同一 repeated manifest 中比较；若 direct > staged，PI1 应优先 direct family；若 direct ≤ staged，保持 staged family 为 production probe 候选。 |

## Evidence Doctor 和 registry

沿用 `script/generate_pfh_evidence_manifest.py`，新增 `candidate_pfh_direct_aos_rvv` case metadata。Repeated 输出仍放 `log/board/repeated`，覆盖旧 repeated 时必须刷新 registry，并在 result 中把 Phase 010 run 标为 historical。

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | diagnostic。 |
| A/B boundary | test helper；candidate 与 staged family 都不是 production detail helper。 |
| 当前决策问题 | RVV-family-selection（实现族选择）前的 test-only family comparison。 |
| diagnostic 是否可外推到 production | 只能外推为生产探针的候选排序，不能外推为 production adoption。 |
| comparison-boundary / baseline mismatch 风险 | 有：direct AoS 在测试里用 `PointNormal` exact layout；production 模板入口还需 traits/fallback。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许 staged family 继续作为候选；direct family 若弱/负/不稳定则 rejected/attempted，不阻塞 staged PI1。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要；若 PI2 引入 direct 和 staged 两个 family，PI4 必须同 production boundary 比较。 |

## 继续 / 停止条件

默认继续到 correctness、asm、board repeated 和 Doctor。若 direct candidate correctness 因 gather 或 traits 失败而无法闭合，停止 direct family 并保留 staged family PI1；若 direct 性能不优于 staged，拒绝 direct family 作为首选生产探针。若继续需要修改 production，则停在 production authorization boundary。
