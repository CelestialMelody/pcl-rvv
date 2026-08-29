# Correctness Tests

| test | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `ScalarReferenceKeepsExpectedCandidates` | 合成 packed correspondences + fixed consensus set | `pairwiseConsistencyScalarReference` | good/bad 候选的 bool 结果 | 标量参考语义 |
| `RvvBuildHitsCandidatePathAndMatchesReference` | 同上 | `pairwiseConsistencyCandidate` | RVV build 路径和 bool 一致 | RVV 诊断 path-hit |
| `BatchCandidateCountMatchesReference` | 较大 batch 的 packed correspondences | `countConsistentCandidates*` | 候选计数一致 | 组件消融级诊断 |
| `UpstreamGeometricConsistencyGroupingRecognize` | `milk.pcd` + `milk_cartoon_all_small_clorox.pcd` | `GeometricConsistencyGrouping<PointType, PointType>::recognize()` | Std/RVV 两侧 gtest 通过 | production direct correctness |
