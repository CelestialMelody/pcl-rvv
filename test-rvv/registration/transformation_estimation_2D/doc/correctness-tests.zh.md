# 正确性测试说明

## 本文职责

本文逐项说明 `src/test_te2d.cpp` 中的 gtest（单元测试）输入、被测路径、断言和证明范围。测试分成 public semantics（公开入口语义）、source-indexed / dual-indexed / correspondence production probe 与 fallback、row-source scalar boundary（行来源标量边界）、generic point-type gate/fallback、source-indexed generic、dual-indexed generic、correspondence generic、materialize-to-ordered row-source candidate、direct gather row-source candidate、correspondence locality/order profile（对应关系局部性 / 顺序剖析）和 correspondence chunked staging candidate 几组。

## 测试文件分工

| 文件 | 职责 |
| --- | --- |
| `src/test_te2d.cpp` | gtest 主入口，列出 public semantics、row-source scalar boundary、generic gate/fallback、materialize row-source 和 direct gather case。 |
| `include/te2d.h` | 稳定聚合入口。 |
| `include/impl/te2d_candidates.hpp` | fixtures、公开入口 wrapper、row-source materialization helper、标量 reference、RVV candidate 和 checksum helper。 |

## 共同输入和断言

| helper / assertion | 作用 | 证据边界 |
| --- | --- | --- |
| `makePointXYZCloud` | 构造 deterministic dense `PointXYZ` source cloud。 | 常规顺序点云对 correctness / bench input。 |
| `transformCloud2D` | 用固定 2D transform 构造 target cloud。 | 只服务输入构造，不是 production transform evidence。 |
| `makeNearCancellationCloud` | 构造较大公共偏移和小扰动样本。 | 暴露 raw sums 公式的近抵消风险，保护两遍中心化候选。 |
| `makeXYZLikeCloud<PointT>` | 构造带非零额外字段的代表性 PointXYZ-like source cloud。 | traits gate、same-type/mixed candidate 和额外字段语义。 |
| `transformCloud2DTo<Source,Target>` | 按 source/target 点型分别生成变换后的 target cloud。 | mixed stride 和 source/target 独立 layout correctness。 |
| `makePrefixIndices` | 构造有效前缀 index list。 | 保护 indexed overload 当前标量 row pairing。 |
| `makePrefixCorrespondences` | 构造 identity correspondence list。 | 保护 correspondence public probe 和 materialize family 的 query/match 行配对。 |
| `makeStridedIndices` | 构造有效但带 stride / offset 的 index list。 | 保护 dual-indexed / correspondence source 和 target index stream 不被混用。 |
| `makeStridedCorrespondences` | 由 query / match index stream 构造 correspondence list。 | 保护 correspondence direct gather 的两条索引流。 |
| `makeReverseIndices` | 构造逆序但有效的 index list。 | Phase 096 用于检查访问方向变化对 direct / staged / materialize 三条 public RVV 路径的语义影响。 |
| `makeShuffledIndices` | 构造固定伪随机 permutation。 | Phase 096 用于放大 locality 差异；随机序列 deterministic，便于 board repeated 复现。 |
| `expectMatrixNear` | 逐元素比较 4x4 matrix。 | 容忍 reduction tree 造成的小误差。 |
| `expectMatrixExactlySame` | 逐元素严格比较输出矩阵。 | 保护 size mismatch 早返回不改输出。 |

## TEST / 测试族字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `PublicOrderedCloudPairRecoversRigid2DTransform` | 4096 个 dense finite `PointXYZ` 点对 | 真实 `TransformationEstimation2D::estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | 输出矩阵接近构造用 2D transform，z translation 为 0。 | 顺序点云对公开入口基础语义。 |
| `PublicOrderedCloudPairSizeMismatchKeepsOutputMatrix` | source 9 点、target 8 点 | 真实顺序点云对公开入口 | 输出矩阵保持调用前常量矩阵。 | size mismatch 早返回语义。 |
| `PublicSourceIndexedSizeMismatchKeepsOutputMatrix` | source indices 5 个、target 4 点 | 真实 source-indexed-cloud-pair 公开入口 | 输出矩阵保持调用前状态。 | source indexed 的数量检查；不覆盖非法 index。 |
| `PublicSourceIndexedProductionProbeRecoversRigid2DTransform` | source 4096 点、前缀 indices 4096 个、target 为 selected source 的 2D transform | 真实 source-indexed-cloud-pair 公开入口 | 输出矩阵接近构造用 transform；RVV 构建命中 Phase 091 bounded probe。 | source-indexed `PointXYZ -> PointXYZ` production probe correctness；不覆盖其它 row source 或点型。 |
| `PublicSourceIndexedSmallInputUsesScalarFallback` | source 16 点、8 个有效 indices | 真实 source-indexed-cloud-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | 小输入回退，不命中 Phase 091 RVV gate。 |
| `PublicSourceIndexedNonDenseFiniteUsesScalarFallback` | `is_dense=false` 但 selected source / target rows 全部 finite | 真实 source-indexed-cloud-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | dense flag 是 runtime gate，不能只按 finite 判断命中 RVV。 |
| `PublicSourceIndexedNonFiniteSelectedZUsesScalarFallback` | selected source row 中 z 为 NaN | 真实 source-indexed-cloud-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | Phase 091 gate 扫描 selected source rows，z 非有限必须回退。 |
| `PublicSourceIndexedGenericXYZMatchesExpected` | `PointXYZI -> PointXYZI` source-indexed 有效输入 | 真实 source-indexed-cloud-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐；RVV build 命中 Phase 112 exact gate。 | Phase 112 只覆盖 exact `PointXYZI -> PointXYZI`；不证明 source-indexed generic widening 或 Normal 类 dispatch。 |
| `PublicSourceIndexedFamilyMaterializedOrderedMatchesDirectPublic` | source 4096 点、有效 source indices、target 为 selected source 的 2D transform | direct source-indexed public overload vs materialize selected source rows 后的 ordered-cloud-pair public overload | 两条 public RVV family 的矩阵和 checksum 对齐。 | Phase 092 family A/B correctness；只覆盖 `PointXYZ -> PointXYZ` source-indexed family selection，不扩大 production dispatch。 |
| `SourceIndexedGenericXYZCandidateMatchesSameTypePairs` | 四类代表性 PointXYZ-like 点型各 4096 点，source 使用 strided valid indices，target 为 selected source 的 2D transform | source-indexed generic direct gather candidate vs same-chain scalar reference 和 public scalar boundary | same-type 矩阵在误差预算内一致；RVV build 命中 candidate。 | Phase 099 source-indexed generic candidate correctness；不修改 production dispatch。 |
| `SourceIndexedGenericXYZCandidateMatchesMixedPairs` | `PointXYZI -> PointXYZ`、`PointXYZ -> PointXYZI`、`PointNormal -> PointXYZINormal`、`PointXYZINormal -> PointNormal` | source-indexed generic direct gather candidate | source indexed gather 与 target prefix strided load 分别使用各自 traits layout，矩阵一致。 | Phase 099 mixed source/target 独立 layout correctness。 |
| `SourceIndexedGenericXYZCandidateIgnoresExtraFields` | 额外字段被改成不同有限值的 source-indexed generic 点对 | source-indexed generic direct gather candidate | 改变 intensity / normal 不改变输出矩阵。 | 当前 2D 算法只读 x/y/z；不授权其它整点语义算法复用 gate。 |
| `SourceIndexedGenericXYZSmallInputFallsBack` | 8 个有效 source indices | source-indexed generic candidate runtime gate | 小输入 `used_rvv=false`、`used_fallback=true`，结果对齐 scalar reference。 | Phase 099 size fallback。 |
| `SourceIndexedGenericXYZNonDenseFiniteFallsBack` | `is_dense=false` 但 selected source / target rows finite | source-indexed generic candidate runtime gate | 不使用 RVV，结果对齐 scalar reference。 | dense flag 是 runtime gate。 |
| `SourceIndexedGenericXYZNonFiniteSelectedSourceFallsBack` | selected source row 中含 NaN | source-indexed generic candidate runtime gate | 不使用 RVV，selected source finite count 下降。 | source-indexed generic 必须扫描 selected source rows。 |
| `SourceIndexedGenericXYZNonFiniteTargetPrefixFallsBack` | target prefix row 中含 Inf/NaN | source-indexed generic candidate runtime gate | 不使用 RVV，target finite count 下降。 | target 顺序 prefix rows 也必须独立参与 gate。 |
| `PublicSourceIndexedGenericXYZDoubleUsesScalarBoundary` | `TransformationEstimation2D<PointXYZI, PointXYZINormal, double>` source-indexed 有效输入 | 真实 public scalar API | 输出正确，当前 float-only RVV gate 不命中。 | `Scalar=double` public fallback；不证明 double RVV。 |
| `SourceIndexedFusedCandidateMatchesPublic` | source 4096 点、有效 source indices 1536 个、target 为选中 source 的 2D transform | source-indexed row source materialize-to-ordered candidate vs public overload | input / accepted points 与 indices 数量一致；矩阵接近 public 结果。 | 证明 source row pairing 和 materialize-to-ordered 数学链路一致；不证明 gather kernel 或 production dispatch。 |
| `PublicDualIndexedSizeMismatchKeepsOutputMatrix` | source indices 5 个、target indices 4 个 | 真实 dual-indexed-cloud-pair 公开入口 | 输出矩阵保持调用前状态。 | dual indices 的数量检查；不覆盖有效乱序 / 重复 index。 |
| `PublicDualIndexedProductionProbeRecoversRigid2DTransform` | source / target 各 4096 点，双侧有效 indices | 真实 dual-indexed-cloud-pair 公开入口 | 输出矩阵接近构造用 transform；RVV 构建命中 Phase 093 bounded probe。 | dual-indexed `PointXYZ -> PointXYZ` production probe correctness；不覆盖 correspondence 或其它点型。 |
| `PublicDualIndexedFamilyMaterializedOrderedMatchesDirectPublic` | source / target 各 4096 点，双侧有效 indices | direct dual-indexed public overload vs materialize selected source/target rows 后的 ordered-cloud-pair public overload | 两条 public RVV family 的矩阵和 checksum 对齐。 | Phase 093 family A/B correctness；只覆盖 `PointXYZ -> PointXYZ` dual-indexed family selection。 |
| `PublicDualIndexedSmallInputUsesScalarFallback` | source / target 各 16 点、8 对有效 indices | 真实 dual-indexed-cloud-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | 小输入回退，不命中 Phase 093 RVV gate。 |
| `PublicDualIndexedNonDenseFiniteUsesScalarFallback` | `is_dense=false` 但 selected rows 全部 finite | 真实 dual-indexed-cloud-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | dense flag 是 runtime gate，不能只按 finite 判断命中 RVV。 |
| `PublicDualIndexedNonFiniteSelectedZUsesScalarFallback` | selected source 或 target row 中 z 为 NaN | 真实 dual-indexed-cloud-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | Phase 093 gate 扫描双侧 selected rows，z 非有限必须回退。 |
| `PublicDualIndexedPointTypeOutsidePhaseScopeUsesScalarFallback` | `PointXYZI -> PointXYZI` dual-indexed 有效输入 | 真实 dual-indexed-cloud-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | Phase 093 不覆盖 dual-indexed generic PointXYZ-like 点型。 |
| `DualIndexedGenericXYZCandidateMatchesSameTypePairs` | 四类代表性 PointXYZ-like 点型各 4096 个 row，source/target 使用不同 stride / offset 的有效 index stream | dual-indexed generic direct gather candidate vs same-chain scalar reference 和 public boundary | same-type 矩阵在误差预算内一致；RVV build 命中 candidate。 | Phase 100 dual-indexed generic candidate correctness；不修改 production dispatch。 |
| `DualIndexedGenericXYZCandidateMatchesMixedPairs` | `PointXYZI -> PointXYZ`、`PointXYZ -> PointXYZI`、`PointNormal -> PointXYZINormal`、`PointXYZINormal -> PointNormal` | dual-indexed generic direct gather candidate | source indexed gather 与 target indexed gather 分别使用各自 traits layout，矩阵一致。 | Phase 100 mixed source/target 独立 layout correctness。 |
| `DualIndexedGenericXYZCandidateIgnoresExtraFields` | 额外字段被改成不同有限值的 dual-indexed generic 点对 | dual-indexed generic direct gather candidate | 改变 intensity / normal 不改变输出矩阵。 | 当前 2D 算法只读 x/y/z；不授权其它整点语义算法复用 gate。 |
| `DualIndexedGenericXYZSmallInputFallsBack` | 8 对有效 source/target indices | dual-indexed generic candidate runtime gate | 小输入 `used_rvv=false`、`used_fallback=true`，结果对齐 scalar reference。 | Phase 100 size fallback。 |
| `DualIndexedGenericXYZNonDenseFiniteFallsBack` | `is_dense=false` 但 selected source / target rows finite | dual-indexed generic candidate runtime gate | 不使用 RVV，返回 fallback 结果。 | dense flag 是 runtime gate。 |
| `DualIndexedGenericXYZNonFiniteSelectedSourceFallsBack` | selected source row 中含 NaN | dual-indexed generic candidate runtime gate | 不使用 RVV，selected source finite count 下降。 | dual-indexed generic 必须独立扫描 selected source rows。 |
| `DualIndexedGenericXYZNonFiniteSelectedTargetFallsBack` | selected target row 中含 NaN | dual-indexed generic candidate runtime gate | 不使用 RVV，selected target finite count 下降。 | dual-indexed generic 必须独立扫描 selected target rows。 |
| `PublicDualIndexedGenericXYZDoubleUsesScalarBoundary` | `TransformationEstimation2D<PointNormal, PointXYZINormal, double>` dual-indexed 有效输入 | 真实 public scalar API | 输出正确，当前 float-only RVV gate 不命中。 | `Scalar=double` public fallback；不证明 double RVV。 |
| `DualIndexedFusedCandidateMatchesPublic` | source / target 各 4096 点，双侧有效 indices 1536 个 | dual-indexed row source materialize-to-ordered candidate vs public overload | input / accepted points 与 indices 数量一致；矩阵接近 public 结果。 | 证明双侧行配对和物化后的数学链路一致；不证明双 gather 的硬件收益或 production dispatch。 |
| `PublicCorrespondenceProductionProbeRecoversRigid2DTransform` | source / target 各 4096 点，identity correspondences 4096 个 | 真实 correspondence-pair 公开入口 | 输出矩阵接近构造用 transform；RVV 构建命中 Phase 094 guarded public probe。 | correspondence `PointXYZ -> PointXYZ` production-public probe correctness；不覆盖其它点型或 row source。 |
| `PublicCorrespondenceFamilyMaterializedOrderedMatchesDirectPublic` | source / target 各 4096 点，identity correspondences 4096 个 | direct correspondence public overload vs materialize query/match rows 后的 ordered-cloud-pair public overload | 两条 public RVV family 的矩阵和 checksum 对齐。 | Phase 094 family A/B correctness；只覆盖 `PointXYZ -> PointXYZ` correspondence family selection。 |
| `PublicCorrespondenceFamilyStagedDualIndexedMatchesDirectPublic` | source / target 各 4096 点，query / match 使用不同 stride / offset 的 valid correspondences | direct correspondence public overload vs staged query/match indices 后的 dual-indexed public overload | staged-dual 与 direct correspondence public 的矩阵和 checksum 对齐。 | Phase 095 staging/profile correctness；只覆盖 staged-dual profile，不证明 production dispatch switch。 |
| `PublicCorrespondenceLocalityProfilesMatchAcrossFamilies` | source 8192 点；identity、strided、reverse、shuffled 四类 valid query/match streams | direct correspondence public、staged-dual-indexed public、materialize+ordered public | 四类 profile 下三条 public RVV family 的矩阵和 checksum 对齐。 | Phase 096 locality/order profile correctness；只证明 row pairing 和数学结果一致，不证明 staged-dual 或 materialize 性能可采纳。 |
| `CorrespondenceChunkedXYZStagingCandidateMatchesPublicOnStridedInput` | source / target 各 4096 点，query / match 使用不同 stride / offset 的 valid correspondences | topic-local chunked xyz staging candidate vs direct correspondence public overload | chunked candidate 与 public correspondence 输出矩阵一致；RVV 构建命中 chunked helper。 | Phase 098 chunked staging correctness；不证明 production dispatch。 |
| `CorrespondenceChunkedXYZStagingCandidateMatchesPublicOnShuffledInput` | source / target 各 4096 点，query / match 使用 deterministic shuffled streams | chunked xyz staging candidate vs direct correspondence public overload | shuffled profile 下矩阵一致。 | 保护非连续 query/match 访问下的 chunked staging 行配对。 |
| `CorrespondenceChunkedXYZStagingSmallInputFallsBack` | source / target 各 16 点、8 个有效 correspondences | chunked xyz staging runtime gate | 小输入回退，结果对齐 direct scalar reference。 | Phase 098 size gate fallback。 |
| `CorrespondenceChunkedXYZStagingNonFiniteSelectedRowFallsBack` | selected source 或 target row 中含 NaN | chunked xyz staging runtime gate | 不使用 RVV，fallback 被触发。 | chunked staging 必须扫描 selected query/match rows 的有限性。 |
| `PublicCorrespondenceSmallInputUsesScalarFallback` | source / target 各 16 点、8 对有效 correspondences | 真实 correspondence-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | 小输入回退，不命中 Phase 094 RVV gate。 |
| `PublicCorrespondenceNonDenseFiniteUsesScalarFallback` | `is_dense=false` 但 selected query / match rows 全部 finite | 真实 correspondence-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | dense flag 是 runtime gate，不能只按 finite 判断命中 RVV。 |
| `PublicCorrespondenceNonFiniteSelectedSourceZUsesScalarFallback` | selected source row 中 z 为 NaN | 真实 correspondence-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | Phase 094 gate 扫描 selected query rows，source z 非有限必须回退。 |
| `PublicCorrespondenceNonFiniteSelectedTargetZUsesScalarFallback` | selected target row 中 z 为 NaN | 真实 correspondence-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | Phase 094 gate 扫描 selected match rows，target z 非有限必须回退。 |
| `PublicCorrespondencePointTypeOutsidePhaseScopeUsesScalarFallback` | `PointXYZI -> PointXYZI` correspondence 有效输入 | 真实 correspondence-pair 公开入口 | 输出矩阵与 double public scalar reference 对齐。 | Phase 094 不覆盖 correspondence generic PointXYZ-like 点型。 |
| `CorrespondenceGenericXYZCandidateMatchesSameTypePairs` | `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` 四类 same-type correspondence 点对 | correspondence generic direct-gather candidate vs same-chain scalar reference | 四类矩阵在误差预算内一致，RVV 构建命中 candidate。 | Phase 101 代表性点型 correctness；不证明 production dispatch。 |
| `CorrespondenceGenericXYZCandidateMatchesMixedPairs` | `PointXYZI -> PointXYZ`、`PointXYZ -> PointXYZI`、`PointNormal -> PointXYZINormal`、`PointXYZINormal -> PointNormal` | correspondence generic direct-gather candidate | query/source 与 match/target 使用各自 traits layout，矩阵一致。 | Phase 101 mixed source/target correctness；不继承其它 row source。 |
| `CorrespondenceGenericXYZCandidateIgnoresExtraFields` | same/mixed 点对的 intensity、normal 改成不同有限值 | correspondence generic direct-gather candidate | 改变额外字段不改变 2D 估计矩阵。 | 当前算法只读取 x/y/z；不证明整点语义。 |
| `CorrespondenceGenericXYZSmallInputFallsBack` | 四类点型、8 个有效 correspondence | correspondence generic runtime gate | `used_rvv=false`、`used_fallback=true`，结果对齐 scalar reference。 | Phase 101 小输入 fallback。 |
| `CorrespondenceGenericXYZNonDenseFiniteFallsBack` | `is_dense=false` 但选中 query/match rows 全部 finite | correspondence generic runtime gate | 不使用 RVV，结果对齐 scalar reference。 | dense flag fallback。 |
| `CorrespondenceGenericXYZNonFiniteQueryFallsBack` | selected query/source row 含 NaN | correspondence generic runtime gate | 不使用 RVV，query finite count 下降。 | query/source selected-row finite gate。 |
| `CorrespondenceGenericXYZNonFiniteMatchFallsBack` | selected match/target row 含 NaN | correspondence generic runtime gate | 不使用 RVV，match finite count 下降。 | match/target selected-row finite gate。 |
| `PublicCorrespondenceGenericXYZDoubleUsesScalarBoundary` | `TransformationEstimation2D<PointNormal, PointXYZINormal, double>` 有效 correspondence | 真实 correspondence public scalar API | 输出正确，当前 float-only RVV gate 不命中。 | `Scalar=double` public fallback；不证明 double RVV。 |
| `CorrespondenceFusedCandidateMatchesPublic` | source / target 各 4096 点，identity correspondences 2048 个 | correspondence row source materialize-to-ordered candidate vs public overload | input / accepted points 与 correspondence 数量一致；矩阵接近 public 结果。 | 证明 query/match 展开后的行配对和数学链路一致；不证明 correspondence gather 的硬件收益或 production dispatch。 |
| `SourceIndexedDirectGatherCandidateMatchesPublic` | source 4096 点、有效 source indices 1536 个、target 为 selected source 的 2D transform | source-indexed direct gather candidate vs public overload | input / accepted points 与 indices 数量一致；RVV 构建命中 direct gather；矩阵接近 public 结果。 | 证明 source indexed direct gather 行配对和数学链路一致；不证明 production dispatch。 |
| `DualIndexedDirectGatherCandidateMatchesPublic` | source / target 各 4096 点，source 与 target 使用不同 stride / offset index stream | dual-indexed direct gather candidate vs public overload | 双侧 index stream 均被使用；矩阵接近 public 结果。 | 证明 source / target 两侧 gather stream 不混用；不覆盖非法 index。 |
| `CorrespondenceDirectGatherCandidateMatchesPublic` | source / target 各 4096 点，query / match 使用不同 stride / offset correspondence stream | correspondence direct gather candidate vs public overload | query / match 两列均被使用；矩阵接近 public 结果。 | 证明 correspondence query/match direct gather 行配对；不证明 production dispatch。 |
| `SourceIndexedDirectGatherSmallInputFallsBack` | source 16 点、8 个有效 indices | source-indexed direct gather candidate runtime gate | 小输入 `used_rvv=false`、`used_fallback=true`，结果对齐 direct scalar reference。 | 保护 direct gather size gate。 |
| `SourceIndexedDirectGatherNonFiniteSelectedRowFallsBack` | selected source row 中 z 为 NaN | source-indexed direct gather candidate runtime gate | 不使用 RVV，`dense_finite_input=false`，finite count 少 1，矩阵保持 identity。 | direct gather 必须扫描 selected rows，不能只信 `is_dense`。 |
| `DualIndexedDirectGatherNonFiniteSelectedRowFallsBack` | selected target row 中 x 为 NaN | dual-indexed direct gather candidate runtime gate | 不使用 RVV，target finite count 少 1，矩阵保持 identity。 | 双侧 selected rows 的有限性都参与 gate。 |
| `CorrespondenceDirectGatherNonFiniteSelectedRowFallsBack` | matched target row 中 y 为 NaN | correspondence direct gather candidate runtime gate | 不使用 RVV，target finite count 少 1，矩阵保持 identity。 | correspondence query/match selected rows 的有限性都参与 gate。 |
| `PublicNonFiniteXYProducesNonFiniteMatrix` | source 中一个 x 为 NaN | 真实顺序点云对公开入口 | 输出矩阵含非有限值。 | 当前 centroid 过滤和 demean 全量写出的组合语义。 |
| `PublicNonFiniteZForcesCandidateFallback` | source 中一个 z 为 NaN | 真实公开入口 + test-only candidate | public 输出有限；candidate 走 fallback；finite count 少 1。 | z 有限性会影响 centroid，candidate 不能只看 x/y。 |
| `GenericXYZTraitsGateCoversRepresentativePointTypes` | `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` 分别作为 source/target | `RVVXYZAoSFloatLayout<PointT>` source/target gate probe | traits、POD、sizeof、x/y/z offset 和 alignment gate 全部通过。 | 只证明四类代表性点型满足当前 AoS float xyz 输入前提。 |
| `GenericXYZCandidateMatchesRepresentativeSameTypePairs` | 四类点型各自的 dense finite 4096 点对 | generic candidate vs scalar fused reference | same-type 矩阵在误差预算内一致；RVV build 命中 candidate。 | 四类代表性点型的 ordered-cloud-pair 数学正确性。 |
| `GenericXYZCandidateMatchesMixedSourceTargetPairs` | `PointXYZI -> PointXYZ`、`PointXYZ -> PointXYZI`、`PointNormal -> PointXYZINormal`、`PointXYZINormal -> PointNormal` | source/target 独立 layout generic candidate vs scalar reference | 不同 stride 和 offset 组合矩阵一致。 | mixed source/target gate 与加载正确性；不覆盖其它自定义点型。 |
| `GenericXYZCandidateIgnoresExtraFields` | intensity、normal 使用不同非零有限值的 same/mixed 点对 | generic candidate | 改变额外字段不改变估计矩阵。 | 当前 2D 算法只读取 x/y/z；不授权其它整点语义算法复用 gate。 |
| `GenericXYZCandidateSmallInputFallsBack` | 8 点的四类代表性点型 | generic candidate runtime gate | `used_rvv=false`、`used_fallback=true`，结果对齐 scalar reference。 | 小规模 fallback。 |
| `GenericXYZCandidateNonDenseFiniteInputFallsBack` | `is_dense=false` 但所有 x/y/z finite | generic candidate runtime gate | 不使用 RVV，结果对齐 scalar reference。 | dense flag 是 runtime predicate，不是 traits gate。 |
| `GenericXYZCandidateNonFiniteInputFallsBack` | source/target 任一 x/y/z 为 NaN 或 Inf | generic candidate runtime gate | 不使用 RVV，保持当前 public scalar semantics。 | 非有限输入 fallback。 |
| `GenericXYZPublicDoubleScalarUsesFallbackBoundary` | `TransformationEstimation2D<PointXYZI, PointXYZINormal, double>` | 真实 public scalar API | 输出正确，当前 float-only RVV gate 不命中。 | `Scalar=double` public fallback；不证明 double RVV。 |
| `FusedStdMatchesPublicOrderedCloudPair` | 4096 个 dense finite 点对 | test-only 标量 fused reference vs 真实公开入口 | 矩阵误差在 `4e-4` 内。 | 两遍中心化 fused 公式对齐当前 public path。 |
| `FusedCandidateMatchesScalarOrderedCloudPair` | 8192 个 dense finite 点对 | RVV candidate vs 标量 fused reference | input / accepted points 相同；RVV 构建命中 RVV；矩阵误差在 `6e-4` 内。 | 顺序点云对 candidate correctness。 |
| `FusedCandidateSmallInputFallsBack` | 8 个点 | test-only candidate gate | 不使用 RVV，走 fallback，矩阵对齐标量 reference。 | 小规模 gate。 |
| `FusedCandidateNearCancellationWithinBudget` | 2048 个 near-cancellation 点对 | RVV candidate vs 标量 fused reference | 矩阵误差在 `5e-3` 内。 | 两遍中心化 reduction 在近抵消样本下可用。 |

## 边界和随机样本策略

当前有 deterministic corpus（确定性样本集）、generic PointXYZ-like corpus、strided row-source corpus、shuffled correspondence corpus 和 near-cancellation corpus（近抵消样本集），没有 seeded random stress（带种子随机压力样本）。Phase 070 已按代表性点型和 mixed source/target 补固定输入的 traits、额外字段和 fallback 对拍；Phase 090 已按三类 row source 补 direct gather 的合法 index / correspondence 对拍，并让 dual-indexed / correspondence 使用不同 source/target stride；Phase 091、093 和 094 分别补 source-indexed、dual-indexed、correspondence 的 public probe / fallback correctness；Phase 098 为 chunked staging 补 strided、shuffled、小规模和 selected non-finite fallback；Phase 099 为 source-indexed generic point types 补 same-type、mixed、extra-field、small/non-dense/selected non-finite 和 double boundary；Phase 100 为 dual-indexed generic point types 补 same-type、mixed、extra-field、small/non-dense、selected source / selected target non-finite 和 double boundary；Phase 101 为 correspondence generic point types 补 same-type、mixed、extra-field、small/non-dense、selected query / selected match non-finite 和 double boundary。若后续 PI1 某一组合出现不稳定，再按具体点型/row source 补固定 seed 的乱序、重复 index 或局部性样本。

非法 index / correspondence 未写成可运行测试，因为 `ConstCloudIterator` 会直接用传入 index 访问 cloud，当前源码没有安全 bounds check（边界检查）。当前只记录安全数量检查、有效输入边界、selected-row fallback 和 query/match 行配对。

## 验证命令

```bash
make -C test-rvv/registration/transformation_estimation_2D run_test_compare
make -C test-rvv/registration/transformation_estimation_2D run_test_public_semantics
make -C test-rvv/registration/transformation_estimation_2D run_test_candidates
```

当前 `run_test_compare`：Std 构建 84/84 通过，RVV 构建 84/84 通过。日志位于 `log/qemu/run_test_std.log` 和 `log/qemu/run_test_rvv.log`，属于本地生成证据，默认不提交。Phase 070 的 24/24、Phase 080 的 27/27、Phase 090 的 34/34、Phase 091 的 38/38、Phase 092 的 39/39、Phase 093 的 44/44、Phase 094 的 50/50、Phase 095 的 51/51、Phase 096 的 52/52、Phase 098 的 56/56、Phase 099 的 64/64、Phase 100 的 72/72 和 Phase 101 的 80/80 是历史阶段快照；Phase 103/104 新增 source-indexed generic public same-type、mixed pair、extra-field ignored、fallback 和 double boundary 测试，证明当前 guarded production-public probe 与 scalar/reference 边界对齐，但不证明 source-indexed generic widening 已可 clean-adopt。Phase 106 沿用同一 84/84 correctness 边界，只新增独立 QEMU smoke / asm / 20-run board variance evidence；其 board negative 结论不影响 correctness pass，但阻止 full source-indexed generic clean adoption。
