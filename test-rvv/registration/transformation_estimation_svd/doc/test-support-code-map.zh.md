# 测试支撑代码地图

## 本文职责

本文说明 `artifact_layout` 和 `test_support` 解析出的 source、aggregator（聚合入口）和 internal helper（内部 helper）职责，并把 test-only diagnostic（测试专用诊断）、production direct（真实生产路径证据）和 evidence output（证据输出）分开定位。

## 总调用图

```text
src/test_tesvd.cpp
  -> include/tesvd.h
    -> include/impl/tesvd_support.hpp
    -> include/impl/tesvd_candidates.hpp
      -> estimatePublicUmeyama()
      -> estimatePublicSourceIndexedUmeyama()
      -> estimatePublicDualIndicesUmeyama()
      -> estimatePublicCorrespondencesUmeyama()
      -> estimateFusedStd()
      -> estimateFusedSourceIndexedStd()
      -> estimateFusedDualIndicesStd()
      -> estimateFusedCorrespondencesStd()
      -> estimateFusedCandidate()
      -> estimateFusedSourceIndexedCandidate()
      -> estimateFusedDualIndicesCandidate()
      -> estimateFusedCorrespondencesCandidate()
      -> production direct helper probes

src/bench_tesvd.cpp
  -> include/tesvd.h
    -> public production direct ordered-cloud-pair cases
    -> public production direct source-indexed-cloud-pair cases
    -> public production direct dual-indices-cloud-pair cases
    -> public production direct correspondence-pair cases
    -> fused diagnostic ordered-cloud-pair cases
    -> fused diagnostic source-indexed-cloud-pair cases
    -> fused diagnostic dual-indices-cloud-pair cases
    -> fused diagnostic correspondence-pair cases

registration/include/pcl/registration/impl/transformation_estimation_svd.hpp
  -> public ordered-cloud-pair overload
    -> estimateRigidTransformationSVDOrderedCloudPairRVV()
    -> scalar ConstCloudIterator fallback
  -> public source-indexed-cloud-pair overload
    -> estimateRigidTransformationSVDSourceIndexedCloudPairRVV()
    -> scalar ConstCloudIterator fallback
  -> public dual-indices-cloud-pair overload
    -> estimateRigidTransformationSVDDualIndicesCloudPairRVV()
    -> scalar ConstCloudIterator fallback
  -> public correspondence-pair overload
    -> estimateRigidTransformationSVDCorrespondencePairRVV()
    -> scalar ConstCloudIterator fallback
```

## 稳定聚合入口

| 文件 | 作用 | 边界 |
| --- | --- | --- |
| `include/tesvd.h` | 测试和 bench 的唯一稳定 include 入口。 | 不暴露给 production；不承载候选实现正文。 |
| `include/impl/tesvd_support.hpp` | fixture、统计结构和样本构造 helper。 | 只负责确定性样本、indices、correspondences 和 shared stats。 |
| `include/impl/tesvd_candidates.hpp` | 标量 reference、test-only RVV candidate 和 checksum helper。 | 专注 row source reference、fallback gate 和 RVV 累加形状。 |

## Fixtures 与输入构造

| helper | 作用 | 证据角色 |
| --- | --- | --- |
| `makePointXYZCloud` | 构造 deterministic dense `PointXYZ` corpus（确定性稠密样本集）。 | correctness / bench input。 |
| `makePointXYZICloud` | 构造带 intensity 额外字段的 xyz AoS layout。 | representative mixed-field layout smoke。 |
| `makePointXYZRGBCloud` | 构造带颜色字段的 xyz AoS layout。 | representative mixed-field layout smoke。 |
| `makePointXYZLikeCloud` | 构造任意 xyz AoS layout 的样本。 | 双索引 / correspondence 目标构造。 |
| `makeSourceIndices` | 构造 source-indexed row source 的 deterministic indices。 | source-indexed correctness / bench input。 |
| `makeTargetIndices` | 构造 dual-indices row source 的 deterministic indices。 | dual-indices correctness / bench input。 |
| `makeCorrespondences` | 构造 correspondence row source 的 deterministic correspondences。 | correspondence correctness / bench input。 |
| `makeRigidTransform` | 固定刚体矩阵。 | public Umeyama semantic anchor。 |
| `transformCloudXYZ` | 用固定矩阵生成 ordered target。 | 输入构造，不是 production transform evidence。 |
| `transformCloudXYZBySourceIndices` | 用 `source[indices[i]]` 生成 source-indexed target。 | source-indexed row semantics evidence。 |
| `transformCloudXYZByIndexedPairs` | 用 source / target 双 indices 生成 dual-indices target。 | dual-indices row semantics evidence。 |

## 标量 Reference

| helper | 作用 | 证据角色 |
| --- | --- | --- |
| `estimatePublicUmeyama` | 调用当前 production public ordered-cloud-pair path。 | scalar truth / production direct bench entry。 |
| `estimatePublicSourceIndexedUmeyama` | 调用当前 production public source-indexed overload。 | source-indexed scalar truth / production direct bench entry。 |
| `estimatePublicDualIndicesUmeyama` | 调用当前 production public dual-indices overload。 | dual-indices scalar truth / production direct bench entry。 |
| `estimatePublicCorrespondencesUmeyama` | 调用当前 production public correspondence overload。 | correspondence scalar truth / production direct bench entry。 |
| `estimateFusedStd` | 标量 ordered-cloud-pair fused accumulation reference。 | ordered RVV candidate 的 same-chain reference。 |
| `estimateFusedSourceIndexedStd` | 标量 source-indexed fused accumulation reference。 | source-indexed RVV candidate 的 same-chain reference。 |
| `estimateFusedDualIndicesStd` | 标量 dual-indices fused accumulation reference。 | dual-indices RVV candidate 的 same-chain reference。 |
| `estimateFusedCorrespondencesStd` | 标量 correspondence fused accumulation reference。 | correspondence RVV candidate 的 same-chain reference。 |
| `solveUmeyamaNoScaleFromAccumulation` | 从 fused sums 构造 no-scale rigid transform。 | SVD tail reference。 |

## Candidate / Diagnostic Helper

| helper | 作用 | 证据角色 |
| --- | --- | --- |
| `estimateFusedCandidate` | RVV 构建下尝试 dense ordered-cloud-pair fused accumulation；其它情况 fallback。 | pre-production diagnostic candidate。 |
| `accumulateFusedRVV` | test-support ordered RVV load / reduction 的核心 helper。 | diagnostic asm attribution input。 |
| `estimateFusedSourceIndexedCandidate` | RVV 构建下尝试 source-indexed fused accumulation。 | row-source diagnostic candidate。 |
| `accumulateFusedSourceIndexedRVV` | test-support source-indexed RVV load / reduction helper。 | gather asm attribution input。 |
| `estimateFusedDualIndicesCandidate` | RVV 构建下尝试 dual-indices fused accumulation。 | row-source diagnostic candidate。 |
| `accumulateFusedDualIndicesRVV` | test-support dual-indices RVV load / reduction helper。 | double gather asm attribution input。 |
| `estimateFusedCorrespondencesCandidate` | RVV 构建下尝试 correspondence fused accumulation。 | row-source diagnostic candidate。 |
| `accumulateFusedCorrespondencesRVV` | test-support correspondence RVV load / reduction helper。 | correspondence gather asm attribution input。 |
| `sourceIndicesInRange` / `sourceIndicesFitRVVGather` | source-indexed 诊断 gate，检查 index 合法性和 32-bit byte offset 边界。 | fallback boundary evidence。 |
| `indicesInRange` / `correspondencesInRange` | dual-indices / correspondence 诊断 gate。 | fallback boundary evidence。 |
| `pcl::registration::detail::estimateRigidTransformationSVDOrderedCloudPairRVV` | production 内部 helper，按 `Scalar=float`、dense、`use_umeyama_`、size 和 `RVVXYZAoSFloatLayout` gate 尝试 ordered RVV。 | production direct path-hit / fallback evidence。 |
| `pcl::registration::detail::estimateRigidTransformationSVDSourceIndexedCloudPairRVV` | production source-indexed helper，增加 index 合法性和 32-bit gather gate。 | production direct path-hit / fallback evidence。 |
| `pcl::registration::detail::estimateRigidTransformationSVDDualIndicesCloudPairRVV` | production dual-indices helper，增加双 index 合法性和 32-bit gather gate。 | production direct path-hit / fallback evidence。 |
| `pcl::registration::detail::estimateRigidTransformationSVDCorrespondencePairRVV` | production correspondence helper，增加 query/match 合法性和 32-bit gather gate。 | production direct path-hit / fallback evidence。 |
| `pcl::registration::detail::accumulateTransformationEstimationSVDOrderedCloudPairRVV` | production ordered RVV load / reduction helper。 | production asm attribution and board performance。 |
| `pcl::registration::detail::accumulateTransformationEstimationSVDSourceIndexedCloudPairRVV` | production source-indexed RVV load / reduction helper。 | production gather asm attribution and board performance。 |
| `pcl::registration::detail::accumulateTransformationEstimationSVDDualIndicesCloudPairRVV` | production dual-indices RVV load / reduction helper。 | production gather asm attribution and board performance。 |
| `pcl::registration::detail::accumulateTransformationEstimationSVDCorrespondencePairRVV` | production correspondence RVV load / reduction helper。 | production gather asm attribution and board performance。 |

## Correctness Tests

| TEST | 证明什么 | 边界 |
| --- | --- | --- |
| `FusedStdMatchesPublicUmeyamaOrderedCloudPair` | ordered fused scalar reference 与 public Umeyama 语义一致。 | 不证明 RVV。 |
| `FusedCandidateMatchesScalarOrderedCloudPair` | ordered RVV candidate 与 fused scalar reference 一致。 | test-only diagnostic。 |
| `SourceIndexedPublicMatchesFusedReference` | source-indexed public iterator 语义与 fused reference 一致。 | 先证明 row semantics。 |
| `SourceIndexedCandidateMatchesScalar` | source-indexed RVV gather candidate 与标量 reference 一致。 | test-only diagnostic。 |
| `DualIndicesPublicMatchesFusedReference` | dual-indices public iterator 语义与 fused reference 一致。 | 先证明 row semantics。 |
| `DualIndicesCandidateMatchesScalar` | dual-indices RVV candidate 与标量 reference 一致。 | test-only diagnostic。 |
| `CorrespondencePublicMatchesFusedReference` | correspondence public iterator 语义与 fused reference 一致。 | 先证明 row semantics。 |
| `CorrespondenceCandidateMatchesScalar` | correspondence RVV candidate 与标量 reference 一致。 | test-only diagnostic。 |
| `ProductionDirectRVVAcceptsRepresentativeXYZLayouts` | ordered production helper 对 `PointXYZ` / `PointXYZI` / `PointXYZRGB` path-hit 并数值一致。 | correctness，不是逐类型性能。 |
| `ProductionDirectRVVAcceptsSourceIndexedCloudPair` | source-indexed production helper 对 `PointXYZ` / `PointXYZI` / `PointXYZRGB` path-hit 并数值一致。 | correctness，不是逐类型性能。 |
| `ProductionDirectRVVAcceptsDualIndicesCloudPair` | dual-indices production helper 对 `PointXYZ` / `PointXYZI` / `PointXYZRGB` path-hit 并数值一致。 | correctness，不是逐类型性能。 |
| `ProductionDirectRVVAcceptsCorrespondencePair` | correspondence production helper 对 `PointXYZ` / `PointXYZI` / `PointXYZRGB` path-hit 并数值一致。 | correctness，不是逐类型性能。 |
| `ProductionDirectRVVRejectsOutOfScopeGates` | ordered 小规模、非 dense、`use_umeyama_ == false`、`Scalar=double` 不误入 RVV。 | fallback coverage。 |
| `ProductionDirectRVVRejectsSourceIndexedOutOfScopeGates` | source-indexed 小规模、非 dense、非法 index、`use_umeyama_ == false`、`Scalar=double` 不误入 RVV。 | fallback coverage。 |
| `ProductionDirectRVVRejectsDualIndicesOutOfScopeGates` | dual-indices 小规模、非 dense、非法 index、`use_umeyama_ == false`、`Scalar=double` 不误入 RVV。 | fallback coverage。 |
| `ProductionDirectRVVRejectsCorrespondenceOutOfScopeGates` | correspondence 小规模、非 dense、非法 correspondence、`use_umeyama_ == false`、`Scalar=double` 不误入 RVV。 | fallback coverage。 |
| `PointXYZILayoutMatchesScalar` | `PointXYZI` 额外字段不破坏 x/y/z stride load。 | 不证明逐类型性能。 |
| `PublicOrderedCloudPairRepresentativeLayoutsMatchFused` | `PointXYZI` / `PointXYZRGB` 代表性 mixed-field layout 与 public ordered entry 一致。 | 不证明逐类型性能。 |
| `SmallInputFallsBack` | 小规模输入走标量 fallback。 | 不证明大规模性能。 |
| `ReflectionStressMatchesPublicWithinBudget` | determinant sign fix 附近路径保持预算。 | 不证明常规性能。 |

## Bench Harness 与 Case Registry

| 文件 | case-filter | 说明 |
| --- | --- | --- |
| `src/bench_tesvd.cpp` | `public-umeyama` | production direct Std/RVV public ordered/source-indexed/dual/correspondence case。 |
| `src/bench_tesvd.cpp` | `ordered-cloud-pair` | ordered test-only candidate；旧 `fused-full-cloud` 为兼容别名。 |
| `src/bench_tesvd.cpp` | `source-indexed-cloud-pair` | source-indexed public production direct case 和 fused diagnostic case。 |
| `src/bench_tesvd.cpp` | `dual-indices-cloud-pair` | dual-indices public production direct case 和 fused diagnostic case。 |
| `src/bench_tesvd.cpp` | `correspondence-pair` | correspondence public production direct case 和 fused diagnostic case。 |

## Scripts 与 Evidence Output

| 文件 | 作用 | 证据角色 |
| --- | --- | --- |
| `script/generate_tesvd_qemu_evidence_manifest.py` | 把 QEMU bench smoke summary、Std/RVV raw log 和 RVV bench 反汇编翻译成规范 manifest。 | Evidence Doctor 输入；只覆盖 QEMU smoke 合同。 |
| `script/generate_tesvd_board_repeated_summary.py` | 汇总 board repeated raw logs，生成 `summary.md` 和 board `evidence_manifest.json`。 | diagnostic / production_direct 两种模式都支持；按 evidence role 区分 same-boundary、mixed-boundary 和 public direct summary。 |
| `log/board/fused_full_cloud_repeated/summary.md` | Phase 010 ordered diagnostic summary。 | pre-production diagnostic；`full-cloud` 仅历史路径名。 |
| `log/board/production_ordered_cloud_pair_repeated/summary.md` | Phase 020 ordered production direct summary。 | ordered production performance evidence。 |
| `log/board/source_indexed_cloud_pair_repeated/summary.md` | Phase 030 source-indexed diagnostic summary。 | source-indexed PI1 触发证据。 |
| `log/board/production_source_indexed_cloud_pair_repeated/summary.md` | Phase 040 source-indexed production direct summary。 | source-indexed production performance evidence。 |
| `log/board/dual_indices_cloud_pair_repeated/summary.md` | Phase 050 dual-indices diagnostic summary。 | dual-indices PI1 触发证据。 |
| `log/board/correspondence_pair_repeated/summary.md` | Phase 050 correspondence diagnostic summary。 | correspondence PI1 触发证据。 |
| `log/board/production_dual_indices_cloud_pair_repeated/summary.md` | Phase 060 dual-indices production direct summary。 | dual-indices production performance evidence。 |
| `log/board/production_correspondence_pair_repeated/summary.md` | Phase 060 correspondence production direct summary。 | correspondence production performance evidence。 |

Phase 010 / 030 / 050 已调用全局 Evidence Doctor 生成 diagnostic 报告。Phase 020 / 040 / 060 已生成 production direct 报告。`log/evidence_registry.json` 记录了这些 summary / manifest / doctor 的 freshness state。

## Production 与 Test Support 边界

当前 production 文件已新增四条 row source 的 RVV fast path。test-support fused helper 继续作为 reference（参考链路）和诊断候选；production direct 结论只引用真实 public overload、production helper、production gtest、production symbol asm 和 production board summary。`Scalar=double` 明确 fallback。

## 拆分审计

当前采用 `include/`、`include/impl/`、`src/` 布局。`tesvd_support.hpp` 负责 fixtures、indices、correspondences 和共享统计结构，`tesvd_candidates.hpp` 负责 reference、candidate 和 gate 逻辑；这是这轮已经完成的结构拆分。`tesvd_candidates.hpp` 仍低于 helper split hard limit，当前不需要再拆。
