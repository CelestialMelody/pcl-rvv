# Correctness Tests

## 测试列表

| TEST | 被测路径 | 输入 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `PlaneDValuesMatchHandCheckedDots` | `computePlaneDValuesStd/RVV` | 4 个 `PointXYZ` 和 4 个 `Normal` | 手算点积和 checksum 一致 | 只证明 `plane_d` 局部公式和字段顺序 |
| `BoundaryGatherPreservesIndexOrder` | `gatherBoundaryCloudStd/RVV` | 非单调 boundary indices | 输出点顺序和 checksum 一致 | 只证明有效 index 的 boundary copy |
| `ProjectionMatchesHandCheckedIntersections` | `projectBoundaryFromViewpointStd/RVV` | 平面 `z=2`、视点原点 | 手算交点和 checksum 一致 | 只证明有限值 projection 公式 |
| `RegionBoundaryProjectionMatchesScalarShape` | `assembleRegionBoundariesStd/RVV` | 两个 synthetic region，含 boundary indices / model / centroid | 每个 region 输出 cloud 和 checksum 一致 | 只证明 production-shaped helper，不证明真实 public entry |

## RED/GREEN 记录

Phase 010 新测试先在 `make ... clean_test_rvv run_test_rvv` 中失败，失败原因为 `RegionBoundaryInput` 和 `assembleRegionBoundaries*` 尚不存在。随后补最小 helper 后，`run_test_compare` 中 Std/RVV 4 个测试均通过。

## 未覆盖范围

当前 correctness 不覆盖 invalid indices、非连续 layout、`Scalar=double`、`PointNormal`、`PointXYZINormal`、泛型 traits、真实 `OrganizedMultiPlaneSegmentation` 对象状态、CCL、refine、region fitting 或 production fallback。
