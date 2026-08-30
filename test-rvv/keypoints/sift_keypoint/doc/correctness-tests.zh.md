# SIFT Keypoint Correctness Tests

## gtest 说明

| test | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `SyntheticCaseHasSortedNeighborhoodAndScales` | 11 points / 17 neighbors / 3 octaves | synthetic case builder | scales 严格递增，邻域距离单调递增 | 证明 fixture 形态稳定 |
| `CandidateMatchesScalarReferenceForSyntheticBatch` | 23 points / 29 neighbors / 3 octaves | scalar reference vs RVV candidate | DoG matrix 逐点 near | 证明局部 Gaussian kernel 对拍 |
| `ExtremaScanFindsStableInteriorPoints` | 31 points / 25 neighbors / 3 octaves | scalar extrema scan | extrema 非空且索引合法 | 证明极值扫描边界可复核 |
| `PublicSiftKeypointAcceptsSyntheticOrganizedCloud` | 32x24 synthetic organized cloud | 真实 `SIFTKeypoint::compute()` | 输出 cloud shape 合法 | 只做 public smoke，不证明 RVV 收益 |
| public output trace compare | 320x240 synthetic organized cloud | Std/RVV public `compute()` trace | 116 / 116 keypoints，顺序和 `x/y/z/scale` 完全一致 | 证明当前 production patch 的公开入口输出语义 |

## 当前不覆盖什么

这些测试不证明真正业务输入的 KdTree 邻域搜索收益、`PointNormal` / `PointXYZRGB` /
`PointXYZRGBA` 泛型扩展，也不证明 `findScaleSpaceExtrema()` 已被 RVV 化。板卡性能结论只来自
`log/board/repeated_phase010_public_compute/summary.md`，QEMU timing（QEMU 计时）不参与采纳判断。
