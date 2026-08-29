# Optimization Evidence

| candidate family | 状态 | 代码入口 | 当前证据 | 生产边界 |
| --- | --- | --- | --- | --- |
| descriptor cluster distance | attempted / positive diagnostic | `nearestClusterDistanceCandidate` | Phase 000 board median `1.980x`，作为局部公式证据 | 不单独接 production；由 Phase 010/020 接力验证 |
| descriptor batch assignment | attempted / positive production-shaped diagnostic | `assignDescriptorBatchCandidate` | Phase 010 board median `2.120x`，Doctor 无 Error / Warning | 诊断证据；已被 Phase 020 production direct 取代为最终依据 |
| public `findObjects()` descriptor assignment | adopted production behavior | `pcl::ism::detail::findNearestClusterIndexStd/RVV` | Phase 020 board median `1.060x`，0/5 退化，Doctor `Errors=0 / Warnings=0` | 只覆盖 `findObjects()` 中 nearest cluster assignment |
| sigma pairwise max-dot | deferred | `maxPairwiseDotSigmaCandidate` | Phase 000 diagnostic positive | 需要 trainISM-shaped profile / bench 后才能进入生产接入计划 |
| vote density Gaussian sum | deferred | `densityWeightedSumCandidate` | Phase 000 diagnostic positive | 需要 double `std::exp` 语义、tree boundary 和 production-shaped diagnostic |
| full `trainISM()` path | not_now | 无 production patch | 无入口级 production direct 收益证据 | KMeans、feature estimator、对象状态和随机训练路径保持未接 |

## 当前采用的优化方式摘要

生产侧采用的是 per-cluster vector reduction（逐 cluster 向量规约）：RVV helper 每次按 VL chunk
连续加载 descriptor，按 `Eigen::MatrixXf::outerStride()` 跨步加载 column-major center row，计算
`(descriptor - center)^2` 并用 RVV reduction（向量规约）得到当前 cluster 距离；cluster 间的最小值选择
仍保持标量，以保留原 tie 顺序和输出语义。

未采用的方向包括：为 `calculateSigmas()` 直接接生产、替换 vote density 的 double `std::exp`、把
`trainISM()` 训练流程整体向量化，以及把 Phase 020 的代表性点型证据外推到所有 `FeatureSize` / 点型组合。
这些方向需要新的 phase plan 和同边界 production direct 证据。
