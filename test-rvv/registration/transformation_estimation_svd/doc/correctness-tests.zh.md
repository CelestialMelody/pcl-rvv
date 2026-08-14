# 正确性测试说明

## 本文职责

本文记录 `src/test_tesvd.cpp` 中每个 gtest（单元测试）的输入、断言和证据边界。

## 共同输入和断言

测试使用 synthetic dense PointXYZ / PointXYZI / PointXYZRGB ordered-cloud-pair（合成稠密顺序点云对）、source-indexed-cloud-pair（源索引点云对）、dual-indices-cloud-pair（双索引点云对）和 correspondence-pair（对应关系点对）。target 由固定刚体矩阵变换 source 或 `source[indices_src[i]]`、`source[indices_src[i]]` 与 `target[indices_tgt[i]]`、`source[corr.index_query]` 与 `target[corr.index_match]` 得到。断言使用矩阵逐元素误差预算；预算覆盖 fused accumulation（融合累加）、indexed gather（索引离散加载）和 RVV reduction tree（RVV 规约树）带来的合理浮点差异。

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `FusedStdMatchesPublicUmeyamaOrderedCloudPair` | 4096 个 `PointXYZ` | public Umeyama vs fused scalar reference | 4x4 matrix `3e-4` 内一致 | fused formula 没偏离默认 no-scale Umeyama 语义。 | RVV 指令和性能。 |
| `FusedCandidateMatchesScalarOrderedCloudPair` | 8192 个 `PointXYZ` | RVV candidate vs fused scalar reference | stats 一致；RVV 构建应 `used_rvv`；matrix `7e-4` 内一致 | dense ordered-cloud-pair RVV reduction tree 数值可控。 | production dispatch。 |
| `SourceIndexedPublicMatchesFusedReference` | 8193 source / 4096 indices | source-indexed public iterator vs fused scalar reference | matrix `5e-4` 内一致 | `source[indices_src[i]]` 与 `target[i]` 的 public row semantics 与 fused reference 一致。 | RVV 指令和性能。 |
| `SourceIndexedCandidateMatchesScalar` | 131073 source / 65536 indices | source-indexed RVV candidate vs fused scalar reference | stats 一致；RVV 构建应 `used_rvv`；matrix `2e-3` 内一致 | source indexed gather、target strided load 和 reduction tree 数值可控。 | production dispatch。 |
| `DualIndicesPublicMatchesFusedReference` | 524289 source / 262144 source indices / 262144 target indices | dual-indices public iterator vs fused scalar reference | matrix `4e-4` 内一致 | dual gather 的 public row semantics 与 fused reference 一致。 | RVV 指令和性能。 |
| `DualIndicesCandidateMatchesScalar` | 524289 source / 262144 source indices / 262144 target indices | dual-indices RVV candidate vs fused scalar reference | matrix `2e-3` 内一致；RVV 构建应 `used_rvv` | 双 gather reduction tree 数值可控。 | production dispatch。 |
| `CorrespondencePublicMatchesFusedReference` | 524289 source / 262144 correspondences | correspondence public iterator vs fused scalar reference | matrix `4e-4` 内一致 | correspondence query/match row semantics 与 fused reference 一致。 | RVV 指令和性能。 |
| `CorrespondenceCandidateMatchesScalar` | 524289 source / 262144 correspondences | correspondence RVV candidate vs fused scalar reference | matrix `2e-3` 内一致；RVV 构建应 `used_rvv` | correspondence gather reduction tree 数值可控。 | production dispatch。 |
| `DualIndicesCandidateRejectsOutOfScopeGates` | 小规模、非 dense、非法 index、`use_umeyama_ == false`、`Scalar=double` | dual-indices candidate fallback gate | `used_rvv=false`；fallback 分支与标量 reference 一致 | 双索引 RVV 不会在 gate 失败时误命中。 | 这些 fallback 分支的性能。 |
| `CorrespondenceCandidateRejectsOutOfScopeGates` | 小规模、非 dense、非法 correspondence、`use_umeyama_ == false`、`Scalar=double` | correspondence candidate fallback gate | `used_rvv=false`；fallback 分支与标量 reference 一致 | correspondence RVV 不会在 gate 失败时误命中。 | 这些 fallback 分支的性能。 |
| `SmallInputFallsBack` | 8 个 `PointXYZ` | candidate fallback gate | `used_rvv=false`；matrix `3e-4` 内一致 | 小规模输入不会进入 RVV setup cost 更高的路径。 | 大规模性能。 |
| `PointXYZILayoutMatchesScalar` | 4096 个 `PointXYZI` | xyz AoS with extra field | RVV 构建应 `used_rvv`；matrix `8e-4` 内一致 | 额外字段不会破坏 x/y/z stride load。 | 完整泛型点类型 production gate。 |
| `PublicOrderedCloudPairRepresentativeLayoutsMatchFused` | 4096 个 `PointXYZI` 和 `PointXYZRGB` | 真实 public ordered-cloud-pair overload vs fused scalar reference | matrix `2e-3` 内一致 | 代表性 mixed-field（混合字段）xyz AoS 点型语义与 public entry 一致。 | 逐类型板卡性能。 |
| `ProductionDirectRVVAcceptsRepresentativeXYZLayouts` | 4096 个 `PointXYZ`、`PointXYZI`、`PointXYZRGB` | production internal RVV helper（生产内部 RVV helper） | RVV 构建下 `used_rvv=true`，matrix 在误差预算内一致 | ordered production direct path-hit 和 source/target 分别使用当前点型 offset。 | dual indices / correspondences。 |
| `ProductionDirectRVVAcceptsSourceIndexedCloudPair` | 8193 source / 4096 indices，`PointXYZ`、`PointXYZI`、`PointXYZRGB` | source-indexed production internal RVV helper | RVV 构建下 `used_rvv=true`，matrix 在误差预算内一致 | source-indexed production direct path-hit、source/target 分别使用当前点型 offset，source 侧 index 合法时命中 gather RVV。 | 逐点型板卡性能、dual indices / correspondences。 |
| `ProductionDirectRVVRejectsOutOfScopeGates` | 小规模、非 dense、`use_umeyama_ == false`、`Scalar=double` | production internal RVV helper fallback gate | `used_rvv=false` | 小规模、非稠密、非 Umeyama 分支和 double 不会误入当前 RVV path。 | 这些 fallback 分支的性能。 |
| `ProductionDirectRVVRejectsSourceIndexedOutOfScopeGates` | 小规模、非 dense、非法 index、`use_umeyama_ == false`、`Scalar=double` | source-indexed production internal RVV helper fallback gate | `used_rvv=false` | source-indexed RVV gather 不会在 index 非法或其它 gate 失败时误命中。 | fallback 分支性能和非法 index 标量路径错误处理。 |
| `ProductionDirectRVVAcceptsDualIndicesCloudPair` | 524289 source / 262144 source indices / 262144 target indices，`PointXYZ`、`PointXYZI`、`PointXYZRGB` | dual-indices production internal RVV helper | RVV 构建下 `used_rvv=true`，matrix 在误差预算内一致 | dual-indices production direct path-hit、双 gather 和 current point layout offset。 | 逐点型板卡性能。 |
| `ProductionDirectRVVAcceptsCorrespondencePair` | 524289 source / 262144 correspondences，`PointXYZ`、`PointXYZI`、`PointXYZRGB` | correspondence production internal RVV helper | RVV 构建下 `used_rvv=true`，matrix 在误差预算内一致 | correspondence production direct path-hit、query/match gather 和 current point layout offset。 | 逐点型板卡性能。 |
| `ProductionDirectRVVRejectsDualIndicesOutOfScopeGates` | 小规模、非 dense、非法 index、`use_umeyama_ == false`、`Scalar=double` | dual-indices production internal RVV helper fallback gate | `used_rvv=false` | dual-indices production RVV 不会在 gate 失败时误命中。 | fallback 分支性能和非法 index 标量路径错误处理。 |
| `ProductionDirectRVVRejectsCorrespondenceOutOfScopeGates` | 小规模、非 dense、非法 correspondence、`use_umeyama_ == false`、`Scalar=double` | correspondence production internal RVV helper fallback gate | `used_rvv=false` | correspondence production RVV 不会在 gate 失败时误命中。 | fallback 分支性能和非法 correspondence 标量路径错误处理。 |
| `ReflectionStressMatchesPublicWithinBudget` | mirrored target stress | determinant sign fix 附近路径 | matrix `2e-3` 内一致 | SVD 后处理的符号修正语义。 | 常规 dataset 性能和 production 采用。 |

## 边界和随机样本策略

当前输入是确定性 corpus（确定性样本集），便于 reviewer 精确复现。source-indexed、dual-indices 和 correspondence 样本使用固定模运算生成 indices 或 correspondence，覆盖非连续但合法的 source 访问。后续若发现 near-cancellation（近抵消）或 rank-deficient（秩亏）样本风险，再补固定 seed stress。

## 验证命令

```bash
make -C test-rvv/registration/transformation_estimation_svd run_test_compare
```

QEMU / local runner 输出只证明 correctness 和路径，不证明真实性能。当前结果为 Std 22/22、RVV 22/22；board smoke 也是 22/22。
