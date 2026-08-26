# moment_invariants correctness tests

## 本文职责

本文解释 `src/test_moment_invariants.cpp` 中每个 correctness（正确性）测试证明什么。它不承载 bench 统计，也不把 QEMU（仿真器）运行写成性能结论。

## 共同输入和断言

测试使用合成有限值点云，`Scalar=float`。helper-only tests 使用 `PointXYZ` 和非连续 indices（索引）覆盖 gather（离散加载）和 tail（向量尾段）风险。production direct（真实生产路径）tests 使用 `MomentInvariantsEstimation<PointT, pcl::MomentInvariants>::compute`、真实 KdTree nearestKSearch（近邻搜索）和 public output write（公开输出写回）。

断言使用绝对误差和相对误差组合。production direct case 的 `j3` 容差更宽，因为 RVV vector reduction（向量规约）改变中心矩累加树，而 `j3` 又由六个中心矩三阶组合而成；这不是 raw checksum strict equality（严格相等）测试。

## TEST 字典

| TEST | 被测路径 | 断言 | 证明范围 | 不能证明什么 |
| --- | --- | --- | --- | --- |
| `MomentInvariantsReference.MatchesProductionHelperForSparseIndices` | production `computePointMomentInvariants(cloud, indices, ...)` 与 test-only `computeMomentSummaryStd` | `j1/j2/j3` 接近 | Std reference（标量参考链路）复刻 production indexed helper 的公式和 centroid 后循环。 | 不证明 RVV。 |
| `MomentInvariantsCandidate.RVVMatchesReferenceForIndexedTail` | `computeMomentSummaryRVV(points, indices)` 对拍 Std reference | 六个中心矩和三个输出接近 | 测试专用 RVV indexed gather path 在非连续索引和尾段下数值一致。 | 不证明 public `computeFeature`。 |
| `MomentInvariantsCandidate.RVVMatchesReferenceForFullCloudTail` | `computeMomentSummaryRVV(points, count)` 对拍 Std reference | 六个中心矩和三个输出接近 | 测试专用 RVV stride load（跨步加载）full-cloud path 在非 VLEN 整倍数规模下数值一致。 | full-cloud production overload 当前没有接 RVV。 |
| `MomentInvariantsProductionDirect.ComputeFeatureMatchesReferenceForSparseQueries` | `PointXYZ` public `computeFeature` + KdTree nearestKSearch + output write | 每个 query 的 `j1/j2/j3` 与独立 typed reference 接近 | `PointXYZ` production RVV/fallback 分流后的公开输出语义。 | 不证明其它点型。 |
| `MomentInvariantsProductionDirect.NonDenseInvalidQueryKeepsScalarFallback` | 非 dense `PointXYZ` public `computeFeature` | 非有限查询输出 NaN；有效查询仍与 reference 接近 | public 非 dense 查询语义和保守 fallback。 | 不证明 RVV finite mask；当前 RVV path 要求 `cloud.is_dense`。 |
| `MomentInvariantsProductionDirect.PointXYZILayoutGateMatchesReference` | `PointXYZI` public `computeFeature` | typed output 与独立标量 reference 接近 | `RVVXYZAoSFloatLayout<PointXYZI>` production gate correctness。 | 不证明 intensity 参与输出；本算法只读 xyz。 |
| `MomentInvariantsProductionDirect.PointXYZRGBLayoutGateMatchesReference` | `PointXYZRGB` public `computeFeature` | typed output 与独立标量 reference 接近 | `RVVXYZAoSFloatLayout<PointXYZRGB>` production gate correctness。 | 不证明 RGB 字段语义；本算法只读 xyz。 |
| `MomentInvariantsProductionDirect.PointXYZRGBALayoutGateMatchesReference` | `PointXYZRGBA` public `computeFeature` | typed output 与独立标量 reference 接近 | `RVVXYZAoSFloatLayout<PointXYZRGBA>` production gate correctness。 | 不证明 RGBA 字段语义；本算法只读 xyz。 |

## 当前未覆盖边界

没有覆盖非法 index、`Scalar=double`、`PointXYZRGBNormal`、`PointXYZINormal`、用户自定义点型、其它输出类型、full-cloud production overload 或 RVV finite mask。当前 adopted production path 用 gate 和 fallback 限制这些范围；继续扩展需要新 phase 单独补 correctness、asm、board repeated 和 Evidence Doctor（证据体检）。

## 验证命令

| 命令 | 用途 |
| --- | --- |
| `make run_test_compare` | Std/RVV 构建都运行完整 8 个 gtest。 |
| `make run_test_std` | 只跑标量构建，用于确认 reference 和 production helper 对齐。 |
| `make run_test_rvv` | 只跑 RVV 构建，用于确认 production direct 和测试专用 RVV path。 |
| `make run_board_test` | 在板卡跑 RVV gtest，证明目标硬件 correctness。 |
