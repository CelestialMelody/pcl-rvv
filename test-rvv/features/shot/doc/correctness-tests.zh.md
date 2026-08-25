# SHOT correctness tests

## 本文职责

本文解释 `src/test_shot.cpp` 中每个 gtest 的输入、被测路径、断言和证明边界。它不承担性能结论，也不把 helper-level pass（helper 级通过）写成 production dispatch（生产分流）证据。

## 测试文件分工

| 文件 | 职责 |
| --- | --- |
| `src/test_shot.cpp` | public fixed-LRF smoke、component same-chain correctness、production-detail direct 和 fallback / NaN 语义测试。 |
| `include/shot.h` | 聚合头，包含 fixtures、normalization、shape-bin、interpolation 和 color helper。 |
| `include/impl/shot_fixtures.hpp` | 合成点云、reference frame（局部参考系）、descriptor 检查和样本构造。 |
| `include/impl/shot_*` | 各 component 的标量参考链路和 RVV 候选。 |

## 共同断言

公共入口 smoke 主要检查 descriptor（描述子）有限、L2 归一和 invalid LRF（无效局部参考系）输出 NaN。Component tests（组件测试）使用 same-chain（同构链路，标量和 RVV 按同一输入 / 操作顺序对拍）断言，误差阈值按 double / float 中间结果设置；它们只证明 test-only helper 与标量参考一致。

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 | 不证明 |
| --- | --- | --- | --- | --- | --- |
| `Shot352.PublicEntryWithProvidedReferenceFramesProducesUnitDescriptors` | `PointXYZ` surface、fixed LRF、SHOT352 | `SHOTEstimation::computeFeature` | descriptor finite 且 L2 norm 接近 1。 | 公开入口在固定 LRF 下可运行。 | RVV dispatch、目标硬件性能。 |
| `Shot352.InvalidReferenceFrameMarksDescriptorAsNonDenseNaN` | 单个 invalid LRF | `SHOTEstimation::computeFeature` invalid path | 输出 NaN，`is_dense=false`。 | NaN fallback 语义。 | RVV fallback gate。 |
| `Shot1344.ColorPublicEntryWithProvidedReferenceFramesProducesUnitDescriptors` | `PointXYZRGBA` surface、fixed LRF、SHOT1344 | `SHOTColorEstimation::computeFeature` | descriptor finite 且 L2 norm 接近 1。 | color public smoke。 | color RVV production path。 |
| `ShotNormalizeComponent.RvvMatchesScalarReferenceForShot352` | 352 维 descriptor array | `normalizeDescriptorScalar/RVV` | 每维归一化输出一致。 | normalization helper correctness。 | public path speedup。 |
| `ShotNormalizeComponent.RvvMatchesScalarReferenceForShot1344` | 1344 维 descriptor array | `normalizeDescriptorScalar/RVV` | 每维归一化输出一致。 | color descriptor normalization correctness。 | color interpolation。 |
| `ShotShapeBinComponent.RvvMatchesScalarReferenceForFiniteClampAndNaN` | SoA normals、finite / NaN 混合 | `computeShapeBinDistanceScalar/RVV` | bin distance 和 NaN count 一致。 | shape-bin SoA helper correctness。 | AoS / indexed production layout。 |
| `ShotInterpolationGeometryComponent.RvvMatchesScalarReferenceForIndexedSurfaceCloud` | `PointXYZ` AoS + indices | `computeInterpolationGeometryIndexedScalar/RVV` | x/y/z、distance、valid mask 一致。 | geometry staging helper correctness。 | full interpolation scatter。 |
| `ShotColorLabDistanceComponent.RvvMatchesScalarReferenceForNormalizedLabArrays` | normalized LAB arrays | `computeColorBinDistanceScalar/RVV` | color-bin distance 一致。 | LAB arithmetic helper correctness。 | RGB2CIELAB LUT 或 indexed color load。 |
| `ShotColorRgbLutIndexedComponent.RvvMatchesScalarReferenceForIndexedColorCloud` | `PointXYZRGBA` AoS + indices | `computeColorBinDistanceIndexedRGBScalar/RVV` | LAB staging + distance 输出一致。 | indexed color staging correctness。 | production vector push / full color interpolation。 |
| `ShotInterpolationBinSelectionComponent.RvvMatchesScalarReferenceForScalarTailStaging` | geometry staging 后的 double arrays | `computeInterpolationBinSelectionScalar/RVV` | `desc_index`、`step_index`、邻接桶、delta、center weight 一致。 | bin-selection scalar-tail helper correctness。 | `acos` / `atan2`、scatter、production interpolation。 |
| `ShotShapeBinAosComponent.RvvMatchesScalarReferenceForContiguousNormalCloud` | `pcl::Normal` contiguous AoS | `computeShapeBinDistanceAoSScalar/RVV` | bin distance 和 NaN count 一致。 | contiguous AoS layout correctness。 | indexed gather。 |
| `ShotShapeBinIndexedComponent.RvvMatchesScalarReferenceForIndexedNormalCloud` | `pcl::Normal` AoS + indices | `computeShapeBinDistanceIndexedScalar/RVV` | bin distance 和 NaN count 一致。 | indexed gather helper correctness。 | production warning side effect。 |
| `ShotShapeBinProductionDetail.CreateBinDistanceShapeMatchesReferenceForIndexedNormalCloud` | `pcl::Normal` AoS + indices | derived estimator exposing production `createBinDistanceShape` | bin distance、NaN count 和 warning 语义一致。 | PI2 production-detail correctness。 | public entry speedup。 |
| `ShotShapeBinProductionDetail.CreateBinDistanceShapeMatchesReferenceForPointNormalLayout` | `PointNormal` AoS + indices | production `createBinDistanceShape` layout gate | output 与标量参考一致。 | `PointNormal` layout gate correctness。 | 泛型 normal traits 全覆盖。 |
| `ShotShapeBinProductionDetail.SmallNeighborhoodKeepsScalarResult` | 小于 RVV 阈值的 normal 邻域 | production fallback path | 小规模输出与参考一致。 | small-neighborhood fallback。 | 大规模 public performance。 |

## 边界样本策略

当前覆盖 finite / NaN normal、invalid LRF、tail-sized arrays、indexed gather、color RGBA 样本和 PI2 production direct helper。未覆盖项包括完整 generic point type traits、`Scalar=double`、correspondences 和 production patch 采纳后的长期 fallback matrix。PI2 证据不支持采纳，因此这些未覆盖项当前不作为继续扩展理由。

## 验证命令

```bash
make -C test-rvv/features/shot run_test_compare
make -C test-rvv/features/shot run_test_shape_bin
```

手动细分时可在生成的 `test_shot_std` / `test_shot_rvv` 上使用 gtest filter，但当前 Makefile 没有稳定 alias；文档不得把临时 filter 写成正式 target。
