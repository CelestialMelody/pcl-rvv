# ROPS RVV 函数级评估

## 当前结论

`features/include/pcl/features/impl/rops_estimation.hpp` 已完成 diagnostic component ablation
（诊断组件消融）、production integration loop（生产接入闭环）和 Phase 050 point-type expansion
（点类型扩展）的证据闭环。用户已确认板卡正向即可采纳，当前生产源码中保留一份已采纳的有界
RVV（RISC-V Vector，可伸缩向量）production patch（生产补丁）：`ROPSEstimation::rotateCloud()` 与
`ROPSEstimation::getDistributionMatrix()` 在 `__RVV10__`、dense cloud、足够规模、有效
bins / projection / AABB extent，并且 `PointInT` 满足 `RVVXYZAoSFloatLayout` 的 xyz float
AoS（结构数组）布局时进入 RVV helper。

接入后 production-detail（生产私有 helper 直连）板卡 repeated bench（重复板卡性能测试）为正向：
`PointXYZ` median `1.700x`，`PointXYZI` median `1.520x`，`PointNormal` median `1.590x`；
三组均 `0/5` 退化且 checksum 一致。对应 Evidence Doctor（证据体检）均为
`0 Error / 0 Warning / 2 Suggestion`。这些接入后板卡结果支撑当前 patch 的生产采纳。
长期生产文档为 `doc-rvv/features/rops_estimation-RVV.zh.md`。

当前证据只覆盖 production private helper dispatch（生产私有 helper 分流），不覆盖完整 public
`computeFeature()`、真实 mesh local surface、LRF（Local Reference Frame，局部参考坐标系）、
central moments（中心矩）、descriptor normalization（描述子归一化）、`Scalar=double` 或所有自定义点型逐个性能。

## 标量路径重建

公开入口 `pcl::Feature::compute()` 调用 `ROPSEstimation::computeFeature()`。`computeFeature()` 对每个
`indices_` 中的 keypoint 执行：

1. `buildListOfPointsTriangles()` 为 surface 点建立 triangle 反向索引。
2. `getLocalSurface()` 用 KdTree radius search 找 local points，并收集关联 triangles。
3. `computeLRF()` 对 local triangles 计算 scatter matrix、Eigen eigenvectors 和 LRF。
4. `transformCloud()` 把 local points 平移到 keypoint 原点并乘 LRF。
5. 对 x/y/z 三个轴和多次旋转角度调用 `rotateCloud()`，同时得到 rotated cloud 的 AABB。
6. 每个旋转下对 XY、XZ、YZ 三个 projection 调用 `getDistributionMatrix()`，再调用
   `computeCentralMoments()` 生成 4 个中心矩和 1 个 entropy。
7. 拼接 135 维 feature，按绝对值和归一化写入 `PointOutT::histogram`。

RVV 当前接管第 5 步中的 xyz 旋转与 AABB min/max reduction（最小 / 最大规约），以及第 6 步中
distribution matrix 的 row/col bin staging（行列索引暂存）。matrix scatter 累加、central moments、
LRF、local surface search 和 descriptor normalization 仍保持标量。

## 可 RVV 化点和风险

| component | RVV potential | 当前结论 | risk / boundary |
| --- | --- | --- | --- |
| central moments | 小矩阵 mean、低阶乘积和 entropy 前置计算 | correctness-only，未单独接 production | 默认 5x5 工作量小，`std::log` 仍是标量，收益缺少同边界板卡证据。 |
| distribution matrix | row/col bin index 可批量化 | 已采纳 production patch 的一部分 | scatter 冲突和矩阵更新仍保留标量。 |
| rotateCloud / projection | AoS xyz load、矩阵乘法和 min/max 规约适合 RVV | 已采纳 production patch 的一部分 | 只覆盖 dense finite-contract 输入和 xyz float AoS layout。 |
| combined rotate + distribution | 组合计时边界能验证 buffer 写回与 scatter 是否抵消收益 | Phase 040/050 接入后仍 positive | 仍不是完整 public descriptor 性能证据。 |
| descriptor normalization | 135 个 float contiguous reduction + scale | deferred-low-priority | 长度固定且较小，建议先有 public workload/profile。 |

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ROPSEstimation::computeFeature()` | production public-derived entry | 完整 RoPS descriptor 生成 | `Feature::compute()` | `getLocalSurface()`、`computeLRF()`、`rotateCloud()`、`getDistributionMatrix()`、`computeCentralMoments()` | public boundary，当前未计入性能结论 | `features/include/pcl/features/impl/rops_estimation.hpp` |
| `ROPSEstimation::rotateCloud()` | production private helper | 旋转 transformed local cloud 并计算 AABB | `computeFeature()` | `getDistributionMatrix()` | adopted production-detail dispatch | `features/include/pcl/features/impl/rops_estimation.hpp` |
| `ROPSEstimation::getDistributionMatrix()` | production private helper | 把 rotated local cloud 投影到 2D bins 并归一化 | `computeFeature()` | `computeCentralMoments()` | adopted production-detail dispatch | `features/include/pcl/features/impl/rops_estimation.hpp` |
| `pcl::detail::rops::rotateCloudRVV()` / `getDistributionMatrixRVV()` | production RVV helper | xyz stride load/store、rotation、min/max reduction、row/col staging | production private helpers | rotated cloud、AABB、distribution matrix | asm attribution and board performance boundary | `features/include/pcl/features/impl/rops_estimation.hpp` |
| `test_rops_estimation.cpp` | correctness test | 对 test-only candidate、production helper、public non-dense fallback、helper invalid-input gate、layout fallback 和 typed scope 对拍 | `make run_test_compare` | production helper / candidate helper | correctness gate（正确性验收） | `test-rvv/features/rops_estimation/src/test_rops_estimation.cpp` |
| `bench_rops_estimation.cpp` | diagnostic and production-detail bench | 同一 wrapper 下比较 Std fallback 与 RVV production detail | board `run_board_bench_compare` | summary / Evidence Doctor | component / production-detail performance evidence | `test-rvv/features/rops_estimation/src/bench_rops_estimation.cpp` |
| `generate_rops_repeated_summary.py` | analysis script | 生成 repeated summary 和 manifest | Makefile summary targets | Evidence Doctor | evidence manifest | `test-rvv/features/rops_estimation/script/generate_rops_repeated_summary.py` |
| Phase 040 board summary | evidence output summary | 记录 `PointXYZ` production detail 接入后 repeated board 结果 | `doctor_phase040_board_summary` | evaluation / phase result / production doc | adopted production-detail evidence | `test-rvv/features/rops_estimation/log/board/phase040_production_direct_repeated/summary.md` |
| Phase 050 board summaries | evidence output summary | 记录代表性 typed scope 接入后 repeated board 结果 | typed Doctor targets | evaluation / phase result / production doc | adopted representative point-type expansion evidence | `test-rvv/features/rops_estimation/log/board/phase050_pointxyzi_production_detail_repeated/summary.md`、`test-rvv/features/rops_estimation/log/board/phase050_pointnormal_production_detail_repeated/summary.md` |
| production long-term topic doc | production topic doc | 保存已采纳生产行为、fallback、证据链和后续方向 | S11 closeout | reviewer / future worker | adopted production behavior | `doc-rvv/features/rops_estimation-RVV.zh.md` |

## 测试计划和 bench 计划

Phase 000 已完成 central moments correctness scaffold。Phase 010 已完成 distribution matrix correctness、asm 和
board repeated diagnostic bench：`phase010_distribution_matrix_repeated` 5-run median `1.440x`，0/5 退化，
checksum match；Evidence Doctor 为 0 Error / 0 Warning / 2 Suggestion。Phase 020 已完成 rotateCloud + AABB
correctness、asm 和 board repeated diagnostic bench：`phase020_rotate_cloud_repeated` 5-run median `1.220x`，
0/5 退化，checksum match；Evidence Doctor 为 0 Error / 0 Warning / 2 Suggestion。Phase 030 已完成
combined rotate + distribution production-shaped diagnostic bench：`phase030_combined_rotate_distribution_repeated`
5-run median `1.630x`，0/5 退化，checksum match；Evidence Doctor 为 0 Error / 0 Warning / 2 Suggestion。

Phase 040 完成 production detail 接入：`PointXYZ` 5-run median `1.700x`，
min `1.690x`，0/5 退化，checksum match；Evidence Doctor 为 0 Error / 0 Warning / 2 Suggestion。
Phase 050 完成 traits-gated point-type expansion 证据：`PointXYZI` 5-run median `1.520x`、
`PointNormal` 5-run median `1.590x`，均 0/5 退化且 Doctor 无 Error / Warning。

## 正确性与高效性证据链

当前证据链：

- Phase 000：central moments QEMU Std/RVV 3/3，通过；asm 可归属；不做 standalone production。
- Phase 010：distribution matrix QEMU Std/RVV 4/4，通过；bench asm 可归属；board repeated 5-run positive；Evidence Doctor 0 Error / 0 Warning / 2 Suggestion。
- Phase 020：rotateCloud + AABB QEMU Std/RVV 6/6，通过；bench asm 可归属；board repeated 5-run positive；Evidence Doctor 0 Error / 0 Warning / 2 Suggestion。
- Phase 030：combined rotate + distribution QEMU Std/RVV 7/7，通过；bench asm 可归属；board repeated positive，median `1.630x`，0/5 退化；Evidence Doctor 0 Error / 0 Warning / 2 Suggestion。
- Phase 040：production detail `PointXYZ` QEMU correctness 和 fallback 通过；bench asm 可归属；接入后 board repeated median `1.700x`，0/5 退化；Evidence Doctor 0 Error / 0 Warning / 2 Suggestion。
- Phase 050：typed production detail `PointXYZI` / `PointNormal` QEMU correctness、trace hit 和 fallback 边界通过；post-review correctness compare 为 Std 7/7、RVV 16/16，并补 public non-dense、helper invalid-input 和非 f32 layout fallback；typed bench asm 可归属；接入后 board repeated median 分别为 `1.520x` / `1.590x`，均 0/5 退化；Evidence Doctor 均 0 Error / 0 Warning / 2 Suggestion。

这些证据支持 `adopted_production_detail_traits_xyz_aos_representative_types`。它们授权当前 patch 作为
有界生产行为保留；完整 public `computeFeature()` 性能、自定义点型逐个性能和其它未覆盖路径仍不外推。

## Doc-suite role inventory

| role | status | 主归属 / 说明 |
| --- | --- | --- |
| topic_navigation | standalone:`README.zh.md` | 入口、命令、证据白名单和已采纳范围。 |
| testing_overview | merged:`README.zh.md#常用命令` + `rops_estimation-evaluation.zh.md#测试计划和-bench-计划` | 当前 gtest / bench target 可由合并章节定位。 |
| correctness_tests | merged:`rops_estimation-evaluation.zh.md#正确性与高效性证据链` | 逐 phase 记录 correctness 范围；细节在 `src/test_rops_estimation.cpp` 注释和 TEST 名。 |
| benchmark_and_evidence | merged:`README.zh.md#当前结论` + summary artifacts | summary / manifest / Doctor 是统计主归属。 |
| optimization_evidence | standalone:`doc/phases/optimization-matrix.zh.md` | candidate 到证据和 decision 的矩阵。 |
| optimization_roadmap | standalone:`doc/optimization-roadmap.zh.md` | 候选搜索空间、暂缓项和恢复条件。 |
| test_support_code_map | merged:`rops_estimation-evaluation.zh.md#Traceability Map可追踪性地图` | 当前测试支撑仍可通过 map 定位；继续扩张时再拆独立 role 文档。 |
| phase_index | standalone:`doc/phases/README.zh.md` | 阶段恢复入口。 |
| production_topic_doc | standalone:`doc-rvv/features/rops_estimation-RVV.zh.md` | 用户已确认板卡正向即可采纳；长期文档保存已采纳生产行为。 |

## S11 Closeout 状态

当前 production decision 为 `adopted_production_detail_traits_xyz_aos_representative_types`。
生产源码 patch 不改变公开 API；非 RVV 构建、未覆盖 layout、小规模和 non-dense 均保持标量路径；无效 bins / projection /
extent 在 RVV helper gate 被拒绝，production caller 只产生有效 projection。Phase 040/050 的板卡和 Doctor 结果支持当前生产接入，S11 production closeout
已经创建长期 `doc-rvv/features/rops_estimation-RVV.zh.md`。

默认不继续扩大当前 topic：完整 public `computeFeature()` 需要真实 mesh workload 和 profile；descriptor normalization
只有 135 个 float，预期收益可能较小；distribution scatter 去标量化有冲突与顺序语义风险；evidence metadata
hardening 属于归档质量，不属于下一步算法收益。若后续继续，应先另开 public workload/profile、scatter
或 metadata hardening 目标，并冻结新的 phase plan。
