# 正确性测试说明

## 本文职责

本文逐项说明 `src/test_te2d.cpp` 中的 gtest（单元测试）输入、被测路径、断言和证明范围。测试分成 public semantics（公开入口语义）、row-source scalar boundary（行来源标量边界）和 fused candidate correctness（候选正确性）三组。

## 测试文件分工

| 文件 | 职责 |
| --- | --- |
| `src/test_te2d.cpp` | gtest 主入口，列出 public semantics、row-source scalar boundary 和 fused candidate case。 |
| `include/te2d.h` | 稳定聚合入口。 |
| `include/impl/te2d_candidates.hpp` | fixtures、公开入口 wrapper、row-source materialization helper、标量 reference、RVV candidate 和 checksum helper。 |

## 共同输入和断言

| helper / assertion | 作用 | 证据边界 |
| --- | --- | --- |
| `makePointXYZCloud` | 构造 deterministic dense `PointXYZ` source cloud。 | 常规顺序点云对 correctness / bench input。 |
| `transformCloud2D` | 用固定 2D transform 构造 target cloud。 | 只服务输入构造，不是 production transform evidence。 |
| `makeNearCancellationCloud` | 构造较大公共偏移和小扰动样本。 | 暴露 raw sums 公式的近抵消风险，保护两遍中心化候选。 |
| `makePrefixIndices` | 构造有效前缀 index list。 | 保护 indexed overload 当前标量 row pairing。 |
| `makePrefixCorrespondences` | 构造 identity correspondence list。 | 保护 correspondence overload 当前标量 row pairing。 |
| `expectMatrixNear` | 逐元素比较 4x4 matrix。 | 容忍 reduction tree 造成的小误差。 |
| `expectMatrixExactlySame` | 逐元素严格比较输出矩阵。 | 保护 size mismatch 早返回不改输出。 |

## TEST / 测试族字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `PublicOrderedCloudPairRecoversRigid2DTransform` | 4096 个 dense finite `PointXYZ` 点对 | 真实 `TransformationEstimation2D::estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | 输出矩阵接近构造用 2D transform，z translation 为 0。 | 顺序点云对公开入口基础语义。 |
| `PublicOrderedCloudPairSizeMismatchKeepsOutputMatrix` | source 9 点、target 8 点 | 真实顺序点云对公开入口 | 输出矩阵保持调用前常量矩阵。 | size mismatch 早返回语义。 |
| `PublicSourceIndexedSizeMismatchKeepsOutputMatrix` | source indices 5 个、target 4 点 | 真实 source-indexed-cloud-pair 公开入口 | 输出矩阵保持调用前状态。 | source indexed 的数量检查；不覆盖非法 index。 |
| `PublicSourceIndexedValidCaseStaysOnScalarBoundary` | source 4096 点、前缀 indices 1024 个、target 为 selected source 的 2D transform | 真实 source-indexed-cloud-pair 公开入口 | 输出矩阵接近构造用 transform。 | source-indexed overload 当前保持标量 row pairing；不证明 RVV gather。 |
| `SourceIndexedFusedCandidateMatchesPublic` | source 4096 点、有效 source indices 1536 个、target 为选中 source 的 2D transform | source-indexed row source materialize-to-ordered candidate vs public overload | input / accepted points 与 indices 数量一致；矩阵接近 public 结果。 | 证明 source row pairing 和 materialize-to-ordered 数学链路一致；不证明 gather kernel 或 production dispatch。 |
| `PublicDualIndexedSizeMismatchKeepsOutputMatrix` | source indices 5 个、target indices 4 个 | 真实 dual-indexed-cloud-pair 公开入口 | 输出矩阵保持调用前状态。 | dual indices 的数量检查；不覆盖有效乱序 / 重复 index。 |
| `PublicDualIndexedValidCaseStaysOnScalarBoundary` | source / target 各 4096 点，双侧前缀 indices 1536 个 | 真实 dual-indexed-cloud-pair 公开入口 | 输出矩阵接近构造用 transform。 | dual-indexed overload 当前保持标量 row pairing；不证明双侧 gather RVV。 |
| `DualIndexedFusedCandidateMatchesPublic` | source / target 各 4096 点，双侧有效 indices 1536 个 | dual-indexed row source materialize-to-ordered candidate vs public overload | input / accepted points 与 indices 数量一致；矩阵接近 public 结果。 | 证明双侧行配对和物化后的数学链路一致；不证明双 gather 的硬件收益或 production dispatch。 |
| `PublicCorrespondencePairValidCaseStaysOnScalarBoundary` | source / target 各 4096 点，identity correspondences 2048 个 | 真实 correspondence-pair 公开入口 | 输出矩阵接近构造用 transform。 | correspondence overload 当前保持标量 row pairing；不证明 query/match RVV。 |
| `CorrespondenceFusedCandidateMatchesPublic` | source / target 各 4096 点，identity correspondences 2048 个 | correspondence row source materialize-to-ordered candidate vs public overload | input / accepted points 与 correspondence 数量一致；矩阵接近 public 结果。 | 证明 query/match 展开后的行配对和数学链路一致；不证明 correspondence gather 的硬件收益或 production dispatch。 |
| `PublicNonFiniteXYProducesNonFiniteMatrix` | source 中一个 x 为 NaN | 真实顺序点云对公开入口 | 输出矩阵含非有限值。 | 当前 centroid 过滤和 demean 全量写出的组合语义。 |
| `PublicNonFiniteZForcesCandidateFallback` | source 中一个 z 为 NaN | 真实公开入口 + test-only candidate | public 输出有限；candidate 走 fallback；finite count 少 1。 | z 有限性会影响 centroid，candidate 不能只看 x/y。 |
| `FusedStdMatchesPublicOrderedCloudPair` | 4096 个 dense finite 点对 | test-only 标量 fused reference vs 真实公开入口 | 矩阵误差在 `4e-4` 内。 | 两遍中心化 fused 公式对齐当前 public path。 |
| `FusedCandidateMatchesScalarOrderedCloudPair` | 8192 个 dense finite 点对 | RVV candidate vs 标量 fused reference | input / accepted points 相同；RVV 构建命中 RVV；矩阵误差在 `6e-4` 内。 | 顺序点云对 candidate correctness。 |
| `FusedCandidateSmallInputFallsBack` | 8 个点 | test-only candidate gate | 不使用 RVV，走 fallback，矩阵对齐标量 reference。 | 小规模 gate。 |
| `FusedCandidateNearCancellationWithinBudget` | 2048 个 near-cancellation 点对 | RVV candidate vs 标量 fused reference | 矩阵误差在 `5e-3` 内。 | 两遍中心化 reduction 在近抵消样本下可用。 |

## 边界和随机样本策略

当前有 deterministic corpus（确定性样本集）和 near-cancellation corpus（近抵消样本集），没有 seeded random stress（带种子随机压力样本）。Phase 030 已按三类 row source 补合法 index / correspondence 的确定性 candidate 对拍；如果后续板卡证据显示某一 policy 值得继续，再按该 policy 补固定 seed 的乱序、重复 index 或局部性样本。

非法 index / correspondence 未写成可运行测试，因为 `ConstCloudIterator` 会直接用传入 index 访问 cloud，当前源码没有安全 bounds check（边界检查）。本阶段只记录安全数量检查、有效输入边界和标量 row pairing。

## 验证命令

```bash
make -C test-rvv/registration/transformation_estimation_2D run_test_compare
make -C test-rvv/registration/transformation_estimation_2D run_test_public_semantics
make -C test-rvv/registration/transformation_estimation_2D run_test_candidates
```

当前 `run_test_compare`：Std 构建 16/16 通过，RVV 构建 16/16 通过。日志位于 `log/qemu/run_test_std.log` 和 `log/qemu/run_test_rvv.log`，属于本地生成证据，默认不提交。
