# sac_model_circle3d Correctness Tests

| TEST | 输入 | 被测路径 | 断言 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `CountAndSelectCandidateMatchesPublicPath` | 10 个确定性 3D 圆附近点，乱序 indices | public count/select vs test-only projection candidate | count、inliers 顺序和 `error_sqr_dists_` 在 `1e-5` 内一致 | candidate 保持公开入口筛选语义 | production dispatch、性能和 `getDistancesToModel` |
| `DegenerateProjectionFallsBackToScalarPath` | 包含投影点落在圆心附近的样本 | public count/select vs candidate fallback | 输出一致 | 退化 normalize 边界不会被 RVV candidate 误处理 | 泛型点型和生产 fallback |
| `PointXYZILayoutMatchesStandardSelectPath` | `PointXYZI` 点云，乱序 indices | public select vs test-only reference（测试专用参考实现） | inliers 和误差一致 | RVV 构建中的 public entry 对未采纳点型保持当前标量语义 | 不证明 `PointXYZI` RVV 有收益 |
| `PointXYZRGBAndRGBALayoutsMatchStandardSelectPath` | `PointXYZRGB` / `PointXYZRGBA` 点云 | public select vs test-only reference | inliers 和误差一致 | 额外颜色字段不改变公开输出；生产 helper 回滚后两种构建都走标量公开入口 | 不证明 RGB/RGBA RVV 有收益 |
