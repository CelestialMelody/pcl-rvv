# ISS 3D Correctness Tests

## 测试入口

默认命令是：

```bash
make -C test-rvv/keypoints/iss_3d run_test_compare
```

该 target 分别构建 Std binary 和 RVV binary，在 QEMU 上运行同一组 gtest。QEMU 只用于 correctness（正确性）和路径可运行性，不用于性能结论。

## GTest 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `ISS3DScatterDiagnostic.CandidateMatchesScalarForIndexedNeighbors` | 257 个合成 `PointXYZ`，193 个伪随机邻域索引。 | `computeScatterMatrixStd` vs `computeScatterMatrixCandidate`。 | 3x3 matrix 在 `1e-3` 内一致。 | indexed gather 和 f64 reduction 的主路径正确性。 | production `searchForNeighbors`、EVD 和 NMS。 |
| `ISS3DScatterDiagnostic.CandidateHandlesTailNeighborCount` | 131 个合成 `PointXYZ`，73 个邻域索引。 | 同上。 | 3x3 matrix 在 `1e-3` 内一致。 | VL tail（向量长度尾段）能正确处理非整块邻域。 | 小邻域 fallback 的性能收益。 |
| `ISS3DScatterDiagnostic.EmptyNeighborSetKeepsZeroMatrix` | 空邻域，candidate matrix 初值为 42。 | `computeScatterMatrixCandidate` fallback / 空输入路径。 | 输出为全 0。 | 空邻域不会留下旧 matrix 内容。 | production min-neighbor early return。 |
| `ISS3DProductionScatter.GetScatterMatrixMatchesIndependentRadiusReference` | 257 个 `PointXYZ` 点云，KdTree 半径搜索。 | protected `getScatterMatrix` 真实 production path vs 独立 radius reference。 | 3x3 matrix 在 `1e-8` 内一致。 | production dispatch 命中后保持 scatter 数值语义。 | public `compute()` 输出非空、其它点型、其它 layout。 |

## 数值预算

RVV path（RVV 路径）从 float `x/y/z` 离散加载后 widen（拓宽）到 double，使用 6 个 double covariance accumulator（协方差累加器）。由于 vector reduction（向量规约）的规约树与标量循环顺序不同，diagnostic gtest 使用 `1e-3` 容差；protected production probe 使用更紧的 `1e-8`，因为该输入下 QEMU 结果与独立 reference 对齐。

## Fallback 覆盖

非 RVV build 由 Std binary 覆盖。`neighbor_count < 16` 当前只在 test-only candidate wrapper（测试专用候选包装）中保留 RVV fallback 证据；production 源码已回到原标量路径，因此非 xyz AoS 点型、超大 cloud 的 32-bit byte offset gate 和其它模板实例不再是当前提交的 production fallback 义务。
