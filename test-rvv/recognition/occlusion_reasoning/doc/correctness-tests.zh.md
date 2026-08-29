# 正确性测试字典

| TEST | 输入 | 被测路径 | 断言 | 不能证明 | 层级 |
| --- | --- | --- | --- | --- | --- |
| `FilterReferenceKeepsExpectedIndicesAndOrder` | 7x7 raw depth buffer，手工构造可见、遮挡、越界和 invalid depth 点 | `filterIndicesScalarReference` | 输出 indices 为手算字面量并保持输入顺序 | 不证明 RVV 命中或 production dispatch | diagnostic reference |
| `RvvBuildHitsCandidatePathAndMatchesScalarReference` | 7x7 raw depth buffer 和 `ProjectionPoint` 样本 | `filterIndicesCandidate` | RVV build 返回 `RvvProjectionFilter`，indices 与标量参考一致 | 这是历史 diagnostic（诊断）守门；不证明最终 production dispatch | production-shaped diagnostic |
| `ProductionDepthMapRectangularResolutionRegression` | 7x5 `ZBuffering` 和一个投影到非零 `u` 的 scene/model 点 | 当前 production `computeDepthMap()` + `filter()` | 期望相同深度点被保留；失败说明矩形 depth map 构建风险仍在 | 不作为 RVV 候选失败；它保护基线语义 | production regression |
| `ProductionZBufferingFilterHitsRvvPathAndKeepsExpectedIndices` | 7x7 `ZBuffering`，真实 `computeDepthMap()` 构造 `depth_`，model 覆盖可见、遮挡、越界和 `z==0` | 真实 `ZBuffering::filter(model, indices, thres)` public member | Std/RVV 输出均为 `{0, 2, 6}`；RVV build 在 test hook 下记录 `Rvv`，Std build 记录 `Scalar` | 只覆盖 `PointXYZ / float / AoS` production direct 路径；不覆盖 `filter(model, filtered)` 的 `copyPointCloud` 成本、smooth depth window 或其它点类型 | production direct |
| `ProductionInlineFilterHitsRvvPathAndKeepsExpectedPoints` | 7x7 organized scene，`scene.at(u, v)` 写入可见、遮挡和 invalid depth，model 覆盖可见、遮挡、越界和 `z==0` | 真实 `pcl::occlusion_reasoning::filter(scene, model, f, threshold)` public inline | Std/RVV 输出均保留 3 个 filtered points；RVV build 在 test hook 下记录 `Rvv`，Std build 记录 `Scalar` | 只覆盖 public inline filtered path；不覆盖 `copyPointCloud` 之外的 wrapper 成本、其它点类型或 `Scalar != float` | production direct |
| `ProductionInlineGetOccludedCloudHitsRvvPathAndKeepsExpectedPoints` | 7x7 organized scene，同上，但断言 occluded 输出 | 真实 `pcl::occlusion_reasoning::getOccludedCloud(scene, model, f, threshold)` public inline | Std/RVV 输出均保留 2 个 occluded points；RVV build 在 test hook 下记录 `Rvv`，Std build 记录 `Scalar` | 只覆盖 public inline occluded path；不覆盖其它点类型或非 dense 场景 | production direct |

`run_test_compare` 会分别构建 Std 和 RVV 版本并在 QEMU 中运行。QEMU 结果只证明
correctness（正确性）、构建和路径命中，不作为性能证据。
