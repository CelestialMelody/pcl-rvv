# VFH 正确性测试说明

## 测试文件

正确性测试位于 `test-rvv/features/vfh/src/test_vfh.cpp`。测试使用 synthetic dense `PointNormal` cloud
（合成 dense 点云），通过 topic-local scalar reference（测试专用标量参考链路）和真实
`pcl::VFHEstimation<PointNormal, PointNormal, VFHSignature308>` 公开入口对拍。

## GTest 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `VFHReference.MatchesPublicComputeDefaultDescriptor` | `makeFeatureCloud(9)`，默认 VFH options。 | 公开 `compute()` 对 topic-local reference。 | 308-bin descriptor 逐 bin `2e-3f` 内一致，总和一致。 | reference 复刻默认公开标量语义。 |
| `VFHReference.MatchesPublicComputeWithSizeComponent` | `makeFeatureCloud(8)`，`size_component=true`、`normalize_distances=true`。 | 公开 `compute()` 对 topic-local reference。 | descriptor 逐 bin 和总和一致。 | reference 覆盖 size component / distance normalization 的标量语义；当前 RVV 不接管该路径。 |
| `VFHCandidate.CentroidSPFHRVVMatchesReference` | `makeFeatureCloud(12)`，默认 options。 | Phase 000 test-only candidate。 | RVV 构建必须返回 true 并与 reference 近似一致；Std 构建返回 false。 | centroid-to-point pair math candidate 的 same-chain（同构链路）正确性。 |
| `VFHCandidate.SPFHAndViewpointRVVMatchesReference` | 同上。 | Phase 020 test-only combined candidate。 | RVV 构建返回 true 并对拍。 | pair math + viewpoint preparation candidate 正确性。 |
| `VFHCandidate.CentroidsSPFHAndViewpointRVVMatchesReference` | 同上。 | Phase 030 test-only reduction-combined candidate。 | RVV 构建返回 true 并对拍。 | normal centroid reduction + pair/viewpoint candidate 正确性。 |
| `VFHProductionRVV.DefaultPublicBoundaryHelperMatchesPublicCompute` | `makeFeatureCloud(12)`，默认 options。 | `pcl::detail::computeVFHSignatureRVV()` production helper。 | RVV helper 返回 true，输出与强制标量 fallback 的公开 `compute()` 一致。 | 当前 adopted production boundary 的直接正确性。 |
| `VFHProductionRVV.HelperMatchesScalarForOverRangeNormalAngles` | 人工构造 normal dot 超过 `[-1, 1]` 的输入。 | production helper 的 pair-feature swap（点对特征交换）语义。 | RVV helper 输出与标量 helper 保持一致。 | 证明 RVV swap 判断不能因提前 clamp 改变标量 `acos(fabs(...))` 的 NaN 比较行为。 |
| `VFHProductionRVV.HelperRejectsSizeComponentFallbackBoundary` | `makeFeatureCloud(12)`，`size_component=true`。 | production helper gate。 | RVV helper 返回 false。 | `size_component` 保持标量 fallback，不被误接管。 |
| `VFHProductionRVV.HelperRejectsNormalizeBinsFallbackBoundary` | `makeFeatureCloud(12)`，`normalize_bins=false`。 | production helper gate。 | RVV helper 返回 false。 | 关闭 bin normalize 时保持标量 fallback；当前没有把该参数写成已优化路径。 |

## 不能证明的范围

这些测试不证明泛型点类型、`Scalar=double`、非 dense normals、subset indices、CVFH / OUR-CVFH 给定 centroid
/ normal 路径、`normalize_distances` 或其它 VFH 参数已经进入 RVV。扩大这些范围时，需要新增 phase plan、fallback tests、
反汇编归属、board repeated 和 Evidence Doctor。
