# Phase 050 结果：generic point type expansion validation

## 实际执行范围

本阶段不再只是静态审计。当前 production 源码已经把 `performReconstruction()` 的 gate 放宽到 `pcl::rvv::RVVXYZAoSFloatLayout<PointNT>::value`，所以本阶段实际做的是把这条 generic（泛型）路径补成可验证的事实链：先修 bench 标签和模板依赖，再跑 QEMU correctness，随后把四个代表点型从 1-run 初筛升级为 5-run board repeated。

## 动作回填

| action | status | evidence | result |
| --- | --- | --- | --- |
| bench label correction | done | `test-rvv/surface/marching_cubes/src/bench_marching_cubes.cpp` | `mc_prod_xyx_64` 改为 `mc_prod_xyz_64`，避免 summary / manifest 解析混淆。 |
| bench template lookup fix | done | `test-rvv/surface/marching_cubes/src/bench_marching_cubes.cpp` | `this->setGridResolution` / `this->setInputCloud` / `this->grid_` 修正后，RVV bench 成功编译并 dump ASM。 |
| QEMU correctness refresh | done | `make -C test-rvv/surface/marching_cubes run_test_compare` | Std/RVV 各 6 tests passed，新增 generic PointXYZ / PointXYZI / PointXYZRGB / PointXYZRGBA 对拍和 fallback 覆盖。 |
| generic board probe | done / 5-run positive | `log/board/generic_xyz_repeated/summary.md`、`generic_xyzi_repeated/summary.md`、`generic_xyzrgb_repeated/summary.md`、`generic_xyzrgba_repeated/summary.md` | `PointXYZ` / `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` 代表点型均为 positive，checksum 对齐；各自 Evidence Doctor 均为 `Errors=0`、`Warnings=0`。 |
| narrow board anchor | historical truth retained | `log/board/production_direct_repeated/summary.md` | 5-run narrow truth 仍保留为历史对照，不再是当前唯一依据。 |
| generic traits audit | done / representative validation complete | `surface/include/pcl/surface/impl/marching_cubes.hpp`、`test-rvv/surface/marching_cubes/doc/phases/050-generic-point-type-expansion-audit/plan.zh.md` | generic gate 已在源码中启用，四个代表点型的 QEMU correctness 和 repeated board 均已闭合；不再只依赖 1-run 初筛。 |

## 当前 board 结果

| case | median | values | bucket | 解释 |
| --- | ---: | --- | --- | --- |
| `mc_prod_xyz_64` | `3.873x` | `3.820x, 3.873x, 3.892x, 3.876x, 3.870x` | positive | `PointXYZ` representative repeated 正向，checksum match。 |
| `mc_prod_xyzi_64` | `3.618x` | `3.618x, 3.600x, 3.618x, 3.618x, 3.633x` | positive | `PointXYZI` representative repeated 正向，checksum match。 |
| `mc_prod_xyzrgb_64` | `3.601x` | `3.597x, 3.566x, 3.623x, 3.601x, 3.626x` | positive | `PointXYZRGB` representative repeated 正向，checksum match。 |
| `mc_prod_xyzrgba_64` | `3.639x` | `3.627x, 3.636x, 3.714x, 3.639x, 3.695x` | positive | `PointXYZRGBA` representative repeated 正向，checksum match。 |
| `mc_prod_sphere_64` | `5.330x` | `5.364x, 5.443x, 5.324x, 5.330x, 5.317x` | positive | narrow historical anchor 仍正向。 |
| `mc_prod_wave_72` | `6.386x` | `6.386x, 6.464x, 6.388x, 6.340x, 6.365x` | positive | narrow historical anchor 仍正向。 |
| `mc_prod_sparse_sphere_80` | `8.824x` | `8.840x, 8.980x, 8.806x, 8.824x, 8.794x` | positive | sparse case 仍为单列解释，不外推到整个 generic family。 |

## Evidence Doctor 处理

- `generic_xyz_repeated` / `generic_xyzi_repeated` / `generic_xyzrgb_repeated` / `generic_xyzrgba_repeated`：每个代表点型都是 `Errors=0`、`Warnings=0`、`Suggestions=0`。
- 处理动作：把这四个 repeated 结果视为 generic family 的代表性 production-public positive 证据；边界仍限于 synthetic voxelized grid，不外推到真实 Hoppe/RBF 输入分布。
- `production_direct_repeated` 历史 truth：`Errors=0`、`Warnings=1`，可继续作为 narrow anchor 的历史对照。

## Generic point type 审计状态

当前 production source 是：

```cpp
if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointNT>::value)
  reconstructSurfaceRVV (intermediate_cloud);
else
  reconstructSurfaceStd (intermediate_cloud);
```

这条 gate 已经把 `PointNT` 的 xyz AoS traits 当成 RVV 输入前提，不再只限定 `PointNormal`。`PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 代表性 board repeated 都已通过，说明 generic gate 在当前 synthetic public boundary 内有稳定收益；它仍不证明真实 Hoppe/RBF voxelization 分布、`Scalar=double` 或 triangle emission RVV。

## Stop / Continue 决策

当前不应该回到只看 narrow `PointNormal` 的状态。默认下一步是：

1. 更新长期 production 文档和筛选队列，把 current production gate 写成 `RVVXYZAoSFloatLayout<PointNT>`，并注明 repeated generic performance 已闭合在四个代表点型 synthetic public boundary 内。
2. 若继续优化当前 topic，优先考虑 active-z tail / table-lookup compression A/B，而不是恢复 edge interpolation RVV。
3. 真实 Hoppe/RBF 输入分布、`Scalar=double`、triangle emission RVV 需要单独 phase plan，不由本阶段关闭。
