# transformation_estimation_point_to_plane_lls_weighted 正确性测试说明

## 本文职责

本文解释 gtest 测试代码。读者可以不先阅读 C++ 实现。每个测试条目说明中文含义、输入、被测路径、断言内容、证明范围和代码位置。

测试源码位于：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/src/
```

gtest 聚合入口是：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/include/test_teptplw.h
```

## 测试文件分工

| 文件 | target | 职责 | 读者应关注 |
| --- | --- | --- | --- |
| `src/test_teptplw_public_semantics.cpp` | `run_test_public_semantics` | 公开入口标量语义。 | test-only reference 是否复刻 public overload 有效输入语义。 |
| `src/test_teptplw_input_semantics.cpp` | `run_test_input_semantics` | 公开入口输入语义。 | 数量不匹配、0 权重和负权重如何处理。 |
| `src/test_teptplw_row_sources.cpp` | `run_test_row_sources` | 行来源 candidate、fallback gate、finite/weight 语义。 | full/source/dual/correspondences 数据来源如何区分。 |
| `src/test_teptplw_candidates.cpp` | `run_test_candidates` | staged-row、block-reduction、fused formula、代表点型、production default 对拍。 | RVV 候选与标量 reference 的数值预算。 |
| `src/test_teptplw_production_direct.cpp` | `run_test_production_direct` | 真实 production full-cloud public overload、source-indexed public overload、layout gate 和 fallback。 | production dispatch 是否命中，gate miss 是否回标量。 |

## 共同输入和断言

多数测试使用 `makeSurfaceCloud(grid_radius, step)`。第一个参数是 grid radius。实际点数为 `(2 * grid_radius + 1)^2`。例如 `makeSurfaceCloud(28, 0.12f)` 生成 3249 个点。

共同断言包括：

| 断言 helper | 检查内容 |
| --- | --- |
| `expectMatrixNear(actual, expected, tolerance)` | 逐元素检查 4x4 matrix。 |
| `expectNormalEquationWithinBudget(actual, expected, ...)` | 检查 `accepted_points`、`ATA` 和 `ATb` 的绝对/相对误差预算。 |
| `expectProductionEquationWithinBudget(actual, expected, ...)` | production detail normal-equation 的预算检查。 |
| `EXPECT_TRUE(stats.used_rvv)` | RVV 构建中应命中 RVV candidate 或 production helper。 |
| `EXPECT_FALSE(stats.used_rvv)` | std 构建或 fallback case 不应使用 RVV。 |
| `EXPECT_FALSE(std::isfinite(eq.ata.norm()))` | 非有限 weight 应传播到法方程。 |

## 公开入口语义测试

这些测试验证 test-rvv 标量 reference 是否复刻公开入口的有效输入语义。它们是其它 candidate 对拍的基础。

| TEST 名称 | 中文含义 | 输入 | 被测路径 | 断言 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- | --- | --- |
| `StdDiagnosticMatchesPublicEstimator` | 全云公开入口与 test-only 标量 full-cloud reference 一致。 | `PointNormal -> PointNormal`；小规模 `grid_radius=3`；连续 weights。 | public full-cloud overload 和 `diag::estimate_std_full`。 | matrix 逐元素接近；stats 输入点和 accepted 点一致。 | `diag::accumulate_std_full` 可作为 full-cloud 标量 reference。 | 大规模 RVV dispatch；source-indexed、dual-indices、correspondences。 |
| `StdCorrespondencesMatchesPublicEstimator` | correspondences 公开入口使用 `correspondence.weight`。 | `grid_radius=16`；有效、乱序、重复 correspondences。 | public correspondences overload 和 `diag::estimate_std_correspondences`。 | matrix 接近；输入行数等于 correspondences 数。 | correspondence weight 来源不同于 `weights_`。 | 非法 correspondence index 的 public API 行为。 |
| `StdSourceIndexedMatchesPublicEstimator` | source-indexed 公开入口按 `source[indices[k]] + target[k] + weights[k]` 取行。 | `grid_radius=16`；有效 source indices；连续 weights。 | public source-indexed overload 和 `diag::estimate_std_source_indices`。 | matrix 接近；input/accepted 行数一致。 | source-indexed row source 语义。 | production RVV 接入。 |
| `StdDualIndicesMatchesPublicEstimator` | dual-indices 公开入口使用两条 index stream。 | `grid_radius=16`；有效 source/target indices；连续 weights。 | public dual-indices overload 和 `diag::estimate_std_dual_indices`。 | matrix 接近；input/accepted 行数一致。 | dual-indices row source 和 weight 读取语义。 | correspondences 的 query/match 展开成本。 |

## 公开入口输入语义测试

这些测试验证公开入口对输入数量和权重取值的处理。数量不匹配路径来自 production public overload。该路径打印 `PCL_ERROR` 后直接 `return`，不进入 RVV helper，也不进入标量 solver。测试用 sentinel matrix 检查输出矩阵保持调用前状态。

| TEST 名称 | 中文含义 | 输入 | 被测路径 | 断言 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- | --- | --- |
| `PublicFullCloudTargetSizeMismatchKeepsOutputMatrix` | full-cloud 的 source/target 点数不一致时提前返回。 | source 为 81 点，target 删去 1 点，weights 仍为 81 个。 | public full-cloud overload。 | matrix 与调用前 sentinel 完全一致。 | 原 public error path 保持输出不变。 | error message 文本稳定性。 |
| `PublicFullCloudWeightSizeMismatchKeepsOutputMatrix` | full-cloud 的 `weights_` 数量不等于 source 点数时提前返回。 | source/target 点数一致，weights 删去 1 个。 | public full-cloud overload。 | matrix 与 sentinel 一致。 | weights size gate 位于 RVV dispatch 前。 | 业务层是否应传空权重。 |
| `PublicSourceIndexedTargetSizeMismatchKeepsOutputMatrix` | source-indexed 的 target 点数必须等于 source index stream 长度。 | `indices_src.size()` 为 1089，target 删去 1 点。 | public source-indexed overload。 | matrix 与 sentinel 一致。 | indexed public size gate 行为。 | 非法 source index 行为。 |
| `PublicSourceIndexedWeightSizeMismatchKeepsOutputMatrix` | source-indexed 的 `weights_` 数量必须等于 source index stream 长度。 | source/target/indices 有效，weights 删去 1 个。 | public source-indexed overload。 | matrix 与 sentinel 一致。 | indexed weights gate 行为。 | production RVV indexed 接入。 |
| `PublicDualIndicesTargetIndexSizeMismatchKeepsOutputMatrix` | dual-indices 的两条 index stream 长度必须一致。 | source indices 为 1089，target indices 删去 1 个。 | public dual-indices overload。 | matrix 与 sentinel 一致。 | dual-indices index-count gate 行为。 | 非法 index 值行为。 |
| `PublicDualIndicesWeightSizeMismatchKeepsOutputMatrix` | dual-indices 的 `weights_` 数量必须等于 source index stream 长度。 | 两条 index stream 长度一致，weights 删去 1 个。 | public dual-indices overload。 | matrix 与 sentinel 一致。 | dual-indices weights gate 行为。 | correspondences 权重语义。 |
| `ProductionFullCloudZeroWeightsMatchStdWithinBudget` | 0 权重是有效输入，贡献为 0。 | `grid_radius=28`；每 7 个权重置 0。 | public full-cloud overload、default helper、std helper。 | input/accepted 一致；normal-equation 和 matrix 预算内。 | 0 权重不触发 error path，并与 production std 对齐。 | 全部权重为 0 的奇异 solve 行为。 |
| `ProductionFullCloudNegativeWeightsMatchStdWithinBudget` | 负权重是 production 当前接受的数值输入。 | `grid_radius=28`；每 11 个权重取负。 | public full-cloud overload、default helper、std helper。 | input/accepted 一致；normal-equation 和 matrix 预算内。 | 负权重不参与 finite mask，也不触发输入拒绝。 | 业务层是否应该使用负权重。 |

## Row Source 与 Finite 语义测试

这些测试验证 test-only RVV candidate 的 row source、finite mask 和 fallback。source-indexed 的 production direct 证据放在后面的真实生产路径测试；dual-indices 和 correspondences 仍只有诊断证据。

| TEST 名称 | 中文含义 | 输入 | 被测路径 | 断言 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- | --- | --- |
| `SourceIndexedCandidateMatchesStd` | source-indexed candidate 与标量 reference 一致。 | `grid_radius=32`；有效 source indices；连续 weights。 | `diag::estimate_candidate_source_indices` 对 `diag::estimate_std_source_indices`。 | input/accepted 一致；RVV 构建命中 candidate；matrix 接近。 | 单侧 source gather、target stride load、continuous weight 的 candidate correctness。 | 真实 public production dispatch。 |
| `DualIndicesCandidateMatchesStd` | dual-indices candidate 与标量 reference 一致。 | `grid_radius=34`；有效 source/target indices；连续 weights。 | `diag::estimate_candidate_dual_indices` 对 `diag::estimate_std_dual_indices`。 | input/accepted 一致；RVV 构建命中 candidate；matrix 接近。 | 双侧 gather candidate correctness。 | correspondences weight 展开。 |
| `CorrespondenceCandidateMatchesStd` | correspondences candidate 与标量 reference 一致。 | `grid_radius=32`；有效、乱序、重复 correspondences。 | `diag::estimate_candidate_correspondences` 对 `diag::estimate_std_correspondences`。 | input/accepted 一致；RVV 构建命中 candidate；matrix 接近。 | query/match/weight 展开后 gather path correctness。 | production correspondences RVV。 |
| `SmallInputFallsBackForIsolatedSizeGate` | 小规模 candidate 不进入 RVV。 | `grid_radius=3`；连续 weights。 | `diag::accumulate_candidate_full`。 | `used_rvv=false`；normal-equation 与标量完全接近。 | size gate fallback。 | public full-cloud overload 的所有 fallback。 |
| `InvalidLaneMaskMatchesStd` | 非有限 point/normal lane 被剔除。 | `grid_radius=20`；source x NaN、target normal Inf、target y NaN；权重有限。 | `diag::accumulate_candidate_full` 对 `diag::accumulate_std_full`。 | accepted points 一致；`ATA/ATb` 预算内。 | finite mask 覆盖 source xyz、target xyz、target normal。 | 非有限 weight 语义。 |
| `NonFiniteWeightsAreNotMaskedWhenPointsAreFinite` | 非有限 weight 不参与 finite mask。 | `grid_radius=18`；point/normal 有限；weight NaN/Inf。 | `diag::accumulate_candidate_full` 对 `diag::accumulate_std_full`。 | accepted points 一致；两侧法方程变为非有限。 | weighted 标量合同：weight 非有限仍传播。 | 业务层是否接受非有限输出。 |

## Dual / Correspondence Family Carry-over

Phase 020 额外补了 `run_test_dual_correspondence_family` 对应的四个实现族比较测试。它们都属于 pre-production diagnostic：把 full-cloud adopted family 迁移到 dual-indices 和 correspondences row source，先看同边界 correctness，再看 bench / board 是否仍然负向。

| TEST 名称 | 中文含义 | 输入 | 被测路径 | 断言 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- | --- | --- |
| `DualIndicesBlockReductionMatchesStdWithinBudget` | dual-indices 的 block-baseline carry-over。 | `PointNormal`；有效 source/target indices；连续 weights。 | `diag::accumulate_candidate_dual_indices_block_reduction` 对 `diag::accumulate_std_dual_indices`。 | input/accepted 一致；`ATA/ATb` 预算内；matrix 接近。 | 同边界下的 dual-indices block-reduction 正确性。 | production dual-indices RVV。 |
| `DualIndicesBlockFusedAbcdIlpMatchesBlockAndStdWithinBudget` | dual-indices 的 fused-abcd-ilp carry-over。 | `PointNormal`；有效 source/target indices；连续 weights。 | `diag::accumulate_candidate_dual_indices_block_fused_abcd_ilp` 对 block / std。 | input/accepted 一致；`ATA/ATb` 预算内；matrix 接近。 | dual-indices fused formula / ILP 在同边界下可对拍。 | production dual-indices RVV。 |
| `CorrespondenceBlockReductionMatchesStdWithinBudget` | correspondences 的 block-baseline carry-over。 | `PointNormal`；有效、乱序、重复 correspondences。 | `diag::accumulate_candidate_correspondences_block_reduction` 对 `diag::accumulate_std_correspondences`。 | input/accepted 一致；`ATA/ATb` 预算内；matrix 接近。 | correspondences 的 query/match/weight 展开后仍可对拍 block-baseline。 | production correspondences RVV。 |
| `CorrespondenceBlockFusedAbcdIlpMatchesBlockAndStdWithinBudget` | correspondences 的 fused-abcd-ilp carry-over。 | `PointNormal`；有效、乱序、重复 correspondences。 | `diag::accumulate_candidate_correspondences_block_fused_abcd_ilp` 对 block / std。 | input/accepted 一致；`ATA/ATb` 预算内；matrix 接近。 | correspondences 的 fused formula / ILP 在同边界下可对拍。 | production correspondences RVV。 |

## Candidate 与 Reduction 测试

这些测试验证 full-cloud candidate、block-reduction 和 fused formula。它们解释 production 采用路径的候选来源。

| TEST 名称 | 中文含义 | 输入 | 被测路径 | 断言 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- | --- | --- |
| `FullCloudCandidateMatchesStd` | 早期 full-cloud staged-row candidate 与标量一致。 | `grid_radius=28`；`PointNormal -> PointNormal`；连续 weights。 | `diag::estimate_candidate_full` 对 `diag::estimate_std_full`。 | input/accepted 一致；RVV 构建命中；matrix 接近。 | stride load、weight load、finite mask、staging tail 的基础 correctness。 | block-reduction production path。 |
| `FullCloudBlockReductionMatchesStdWithinBudget` | block-reduction 与标量 reference 在预算内。 | `grid_radius=28`；放入 source NaN 和 target normal Inf。 | staged-row、block-reduction、std 三方对拍。 | accepted 一致；`ATA/ATb` 预算内；matrix 接近。 | A/B/C/N block-reduction 的 correctness。 | fused formula 的独立收益。 |
| `FullCloudBlockReductionNearCancellationStressMatchesStd` | near-cancellation 样本保护 reduction tree 风险。 | `grid_radius=31`；target 沿 normal 做正负小位移；周期 weights。 | block-reduction 对 std。 | normal-equation 和 matrix 预算内。 | 近抵消输入下的数值预算。 | 所有真实 ICP 分布。 |
| `FullCloudBlockReductionScaleStressMatchesStd` | 大坐标和宽权重动态范围压力测试。 | `grid_radius=33`；source 放大；weights 覆盖 0.015、3.5、48 附近。 | block-reduction 对 std。 | normal-equation 和 matrix 预算内。 | 大量级 `ATA/ATb` 下的预算。 | bitwise 一致。 |
| `FullCloudBlockReductionPreservesNonFiniteWeightSemantics` | block-reduction 保留非有限 weight 语义。 | `grid_radius=20`；point/normal 非有限和 weight 非有限同时存在。 | block-reduction 对 std。 | accepted 一致；两侧法方程非有限。 | point/normal finite mask 与 weight 非 mask 分离。 | 业务层处理非有限输出。 |
| `FullCloudBlockFusedFormulaCandidatesMatchBlockAndStdWithinBudget` | fused formula 候选与 block baseline 和 std 对齐。 | `grid_radius=28`；source NaN、target normal Inf。 | 八个 fused formula candidate。 | 每个 candidate 的 input/accepted、normal-equation、matrix 均在预算内。 | 公式树候选 correctness。 | 板卡性能。 |
| `FullCloudBlockFusedFormulaNearCancellationStressMatchesBlockAndStd` | near-cancellation 样本保护 D 项公式树。 | `grid_radius=31`；target 沿 normal 做小位移。 | fused formula candidates 对 block 和 std。 | normal-equation 和 matrix 预算内。 | `d-six-term`、`d-displacement` 舍入风险。 | production 采用。 |
| `FullCloudBlockFusedFormulaScaleStressMatchesBlockAndStd` | fused formula 大尺度压力测试。 | `grid_radius=33`；大坐标和宽权重。 | fused formula candidates 对 block 和 std。 | normal-equation 和 matrix 预算内。 | 大量级输入下候选不越过预算。 | 性能稳定性。 |
| `FullCloudBlockFusedFormulaPreservesNonFiniteWeightSemantics` | fused formula 保留非有限 weight 语义。 | `grid_radius=20`；point/normal 非有限和 weight 非有限。 | 八个 fused formula candidate。 | accepted 一致；candidate 法方程非有限。 | fused formula 没有把 weight 纳入 mask。 | 非有限输出的业务处理。 |
| `FullCloudGenericAbcFusedRepresentativePointTypesMatchStd` | `abc-fused` 和 `abc-fused-ilp` 覆盖三类代表点型。 | `PointNormal -> PointNormal`、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal`。 | generic layout-gated abc candidates。 | 三类点型 normal-equation 和 matrix 预算内。 | source/target layout gate 的 representative correctness。 | 所有 gate-allowed 点型性能。 |
| `FullCloudGenericDAndAbcdFusedRepresentativePointTypesMatchStd` | D 项和 abcd 组合覆盖三类代表点型。 | 同上。 | generic layout-gated D/ABCD candidates。 | 六类 candidate 对 std 预算内。 | D 项和 abcd 公式树的 representative correctness。 | production direct performance。 |
| `ProductionDefaultFusedAbcdIlpRepresentativePointTypesMatchStd` | 当前默认 production RVV helper 覆盖三类代表点型。 | 同上；RVV-only。 | `prod_detail::buildPointToPlaneLLSWeightedFullCloudBlockRVV` 对 std detail helper。 | RVV 构建必须命中；normal-equation 和 matrix 预算内。 | production 默认 fused-abcd-ilp helper correctness。 | std 构建中该 TEST 会 skip。 |

## 真实生产路径测试

这些测试直接触达 production 文件中的 public full-cloud overload、source-indexed overload 或 production detail helper。它们是 production 接入判断的 correctness 主证据。

| TEST 名称 | 中文含义 | 输入 | 被测路径 | 断言 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- | --- | --- |
| `ProductionFullCloudPublicOverloadMatchesStdWithinBudget` | 真实 public full-cloud overload 与 production std helper 对齐。 | `grid_radius=28`；`PointNormal -> PointNormal`；连续 weights。 | public overload 对 `prod_detail::buildPointToPlaneLLSWeightedFullCloudStd`。 | matrix 接近。 | public dispatch 后输出符合 production std reference。 | normal-equation 中间态。 |
| `ProductionFullCloudNormalEquationMatchesStdWithinBudget` | production default normal-equation 与 std helper 对齐。 | 同上。 | `buildPointToPlaneLLSWeightedFullCloudDefault` 对 std helper。 | input/accepted 一致；RVV 构建命中；normal-equation 和 matrix 预算内。 | production helper 的中间态 correctness。 | board 性能。 |
| `ProductionFullCloudScaleStressMatchesStdWithinBudget` | production path 的大尺度压力测试。 | `grid_radius=33`；大坐标和宽权重。 | public overload、default helper、std helper。 | accepted 一致；RVV 构建命中；normal-equation 和 matrix 预算内。 | production default 在压力样本下保持预算。 | 所有输入分布。 |
| `ProductionFullCloudPreservesNonFiniteWeightSemantics` | production default 保留非有限 weight 语义。 | `grid_radius=20`；point/normal 非有限和 weight 非有限。 | default helper 对 std helper。 | input/accepted 一致；两侧法方程非有限。 | production path 的 finite mask 与 weight 语义。 | 业务层输出是否可用。 |
| `ProductionFullCloudSmallInputFallsBackToScalar` | 小规模 full-cloud public overload 回标量。 | `grid_radius=3`；连续 weights。 | public overload 和 default helper。 | `used_rvv=false`；accepted 等于输入点；matrix 接近。 | `nr_points < 64` fallback。 | layout miss fallback。 |
| `ProductionFullCloudPredicateGatesAreNarrow` | production RVV predicate 是窄门。 | 直接调用 predicate helper。 | `canUsePointToPlaneLLSWeightedFullCloudRVV`。 | size、target size、weights size、VLEN、byte-offset miss 均 false。 | predicate 条件本身。 | public overload early-return 行为。 |
| `ProductionSourceIndexedPublicOverloadMatchesStdWithinBudget` | 真实 public source-indexed overload 与 production std helper 对齐。 | `grid_radius=32`；`PointNormal -> PointNormal`；有效 source indices；连续 weights。 | public source-indexed overload 对 `prod_detail::buildPointToPlaneLLSWeightedSourceIndicesStd`。 | matrix 接近。 | source-indexed public dispatch 后输出符合 production std reference。 | normal-equation 中间态。 |
| `ProductionSourceIndexedNormalEquationMatchesStdWithinBudget` | production source-indexed default normal-equation 与 std helper 对齐。 | 同上。 | `buildPointToPlaneLLSWeightedSourceIndicesDefault` 对 std helper。 | input/accepted 一致；RVV 构建命中；normal-equation 和 matrix 预算内。 | source-indexed 默认 staged-gather / compressed-tail 路径的中间态 correctness。 | board 性能和 block-fused family selection。 |
| `ProductionSourceIndexedDefaultUsesStagedGatherRVV` | source-indexed 默认 RVV path 固定为 staged-gather。 | 同上；RVV-only。 | `buildPointToPlaneLLSWeightedSourceIndicesDefault` 对 `buildPointToPlaneLLSWeightedSourceIndicesStagedRVV`。 | 两侧 `used_rvv=true`；input/accepted 一致；normal-equation 完全一致。 | Phase 033 的默认策略护栏：block-fused helper 不会无意间回到 public 默认优先路径。 | staged 是否在板卡上最快；block-fused 显式 probe 的正确性。 |
| `ProductionSourceIndexedSmallInputFallsBackToScalar` | 小规模 source-indexed public overload 回标量。 | `grid_radius=3`；连续 weights。 | public overload 和 default helper。 | `used_rvv=false`；matrix 接近。 | `nr_points < 64` fallback。 | layout miss fallback。 |
| `ProductionSourceIndexedPredicateGatesAreNarrow` | source-indexed production RVV predicate 是窄门。 | 直接调用 predicate helper。 | `canUsePointToPlaneLLSWeightedSourceIndicesRVV`。 | size、target size、weights size、VLEN、byte-offset miss 均 false。 | predicate 条件本身。 | public overload early-return 行为。 |
| `ProductionSourceIndexedInvalidIndexRejectsRVVBeforeGather` | source-indexed RVV helper 在 gather 前拒绝非法 source index。 | `grid_radius=32`；一个负索引和一个越界索引；RVV-only。 | `buildPointToPlaneLLSWeightedSourceIndicesBlockFusedAbcdIlpRVV` 和 `buildPointToPlaneLLSWeightedSourceIndicesStagedRVV`。 | 两个 helper 都返回 false；`used_rvv=false`；accepted 为 0。 | 非法 index 不会进入 block-fused 或 staged-gather RVV gather。 | public API 对非法 index 的业务合同。 |
| `ProductionSourceIndexedPointXYZSourceMatchesStdWithinBudget` | `PointXYZ -> PointNormal` source generic gate。 | source 从 `PointNormal` 复制为 `PointXYZ`。 | public overload、default helper、std helper。 | accepted 一致；RVV 构建命中；normal-equation 和 matrix 预算内。 | source 只需要 xyz f32 AoS layout。 | 所有 source 点型。 |
| `ProductionSourceIndexedPointXYZToPointXYZINormalMatchesStdWithinBudget` | `PointXYZ -> PointXYZINormal` target generic gate。 | source `PointXYZ`，target `PointXYZINormal`。 | public overload、default helper、std helper。 | accepted 一致；RVV 构建命中；normal-equation 和 matrix 预算内。 | target 需要 xyz+normal f32 AoS layout，额外 intensity 不参与公式。 | 所有 target normal 点型。 |
| `ProductionFullCloudPointXYZSourceMatchesStdWithinBudget` | `PointXYZ -> PointNormal` source generic gate。 | source 从 `PointNormal` 复制为 `PointXYZ`。 | public overload、default helper、std helper。 | accepted 一致；RVV 构建命中；normal-equation 和 matrix 预算内。 | source 只需要 xyz f32 AoS layout。 | 所有 source 点型。 |
| `ProductionFullCloudPointXYZToPointXYZINormalMatchesStdWithinBudget` | `PointXYZ -> PointXYZINormal` target generic gate。 | source `PointXYZ`，target `PointXYZINormal`。 | public overload、default helper、std helper。 | input/accepted 一致；RVV 构建命中；normal-equation 和 matrix 预算内。 | target 需要 xyz+normal f32 AoS layout，额外 intensity 不参与公式。 | 所有 target normal 点型。 |
| `ProductionFullCloudDoubleNormalTargetFallsBackToScalar` | target normal 是 double 时回标量。 | target 使用 `TEPTPLWDoubleNormalTarget`。 | public overload 和 default helper。 | static_assert layout gate false；`used_rvv=false`；matrix 接近。 | target normal f32 gate。 | source layout miss。 |
| `ProductionFullCloudScalarDoubleFallsBackToScalar` | `Scalar=double` 回标量。 | `PointNormal -> PointNormal`；weights 转 double。 | public overload 模板 `Scalar=double`。 | public matrix finite。 | `Scalar=double` 不进入 RVV。 | 与 std helper 的严格数值对拍，后续可补。 |

## 输入语义覆盖与暂缓项

本轮已补 public 输入数量和权重取值测试。下列暂缓项需要先确认 upstream 公开 API 语义或 solver 行为。

| 输入类型 | 当前状态 | 建议 |
| --- | --- | --- |
| full-cloud target size mismatch | 已覆盖。 | `PublicFullCloudTargetSizeMismatchKeepsOutputMatrix`。 |
| full-cloud weights size mismatch | 已覆盖。 | `PublicFullCloudWeightSizeMismatchKeepsOutputMatrix`。 |
| source-indexed target size mismatch | 已覆盖。 | `PublicSourceIndexedTargetSizeMismatchKeepsOutputMatrix`。 |
| source-indexed weights size mismatch | 已覆盖。 | `PublicSourceIndexedWeightSizeMismatchKeepsOutputMatrix`。 |
| dual-indices index stream size mismatch | 已覆盖。 | `PublicDualIndicesTargetIndexSizeMismatchKeepsOutputMatrix`。 |
| dual-indices weights size mismatch | 已覆盖。 | `PublicDualIndicesWeightSizeMismatchKeepsOutputMatrix`。 |
| zero weights | 已覆盖。 | `ProductionFullCloudZeroWeightsMatchStdWithinBudget`。 |
| negative weights | 已覆盖。 | `ProductionFullCloudNegativeWeightsMatchStdWithinBudget`。 |
| empty input | 未专项覆盖。 | 先确认 upstream 期望和 solver 行为。 |
| invalid indices / invalid correspondences | public iterator 未做边界检查；source-indexed RVV helper 已有 pre-gather gate 测试。 | 不把 test-only reference 的 defensive skip 写成 public API 合同。 |
| `Scalar=double` 严格 reference 对拍 | 当前有 fallback smoke。 | 可补 double reference 或更窄的 matrix 对拍，前提是先确认误差预算。 |

## 验证命令

当前 correctness 主入口：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_compare
```

当前提交证据：

```text
log/qemu/run_test_std.log
log/qemu/run_test_rvv.log
log/qemu/run_test_source_indices_std.log
log/qemu/run_test_source_indices_rvv.log
```

本轮重跑结果：std 为 `52 passed + 1 skipped`，RVV 为 `55 passed`。`run_test_source_indices_compare` 的细粒度结果为 std 6 passed、RVV 8 passed；`run_test_production_direct_compare` 为 std 18 passed、RVV 20 passed。如果继续新增 gtest，应重跑对应 compare target 并更新这些日志。
