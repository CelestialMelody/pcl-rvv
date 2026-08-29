# Correctness Tests

## 测试字典

| TEST / target | 被测路径 | 输入 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `ImplicitShapeModelDiagnostics.DescriptorNearestClusterMatchesReference` | `nearestClusterDistanceStd` vs `nearestClusterDistanceCandidate` | 153 维 descriptor、64 个 cluster center | nearest index 相同，distance 误差 `<=1e-4` | descriptor distance 局部公式 |
| `ImplicitShapeModelDiagnostics.DescriptorBatchAssignmentMatchesReference` | `assignDescriptorBatchStd` vs `assignDescriptorBatchCandidate` | 97 个 synthetic descriptor、64 个 cluster center | 每个 descriptor 的最近 cluster index 一致 | production-shaped descriptor batch assignment |
| `ImplicitShapeModelDiagnostics.ProductionNearestClusterHelperHandlesColumnMajorCenters` | production `findNearestClusterIndexStd` vs `findNearestClusterIndex` | `Eigen::VectorXf` descriptor、column-major `Eigen::MatrixXf` centers | nearest cluster index 一致 | 生产 helper 的 Eigen layout 和 RVV dispatch 正确性 |
| `ImplicitShapeModelDiagnostics.PairwiseSigmaMaxDotMatchesReference` | `maxPairwiseDotSigmaStd` vs candidate | 257 个 synthetic PointXYZ-like 点 | sigma 误差 `<=1e-4` | `calculateSigmas()` 中 pairwise max-dot 局部公式 |
| `ImplicitShapeModelDiagnostics.DensityWeightedSumMatchesReference` | `densityWeightedSumStd` vs candidate | 4097 个 squared distance 和 strength | 相对误差 `<=3e-5` 或绝对误差 `<=1e-3` | radiusSearch 后 Gaussian weighted sum |
| `run_upstream_test_compare` | `test/recognition/test_recognition_ism.cpp` 的 Std/RVV 构建 | `test/ism_train.pcd`、`test/ism_test.pcd` | 上游 `ISM.TrainRecognize` 和 `ISM.TrainWithWrongParameters` 通过 | 真实公开入口回归语义 |

## 当前不覆盖

这些测试不证明 `trainISM()` 的 KMeans、`calculateSigmas()` 的生产入口收益、vote density 的 double
`std::exp` 语义、其它 `FeatureSize`、其它点型、其它 layout 或其它目标硬件性能。QEMU 或 gtest
不支撑性能结论；性能结论只来自 repeated board summary。
