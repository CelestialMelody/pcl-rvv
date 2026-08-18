# filters/box_clipper3D 函数级 RVV 评估

## 范围

- 主文件：`filters/include/pcl/filters/impl/box_clipper3D.hpp`
- 公开入口：`pcl::BoxClipper3D<PointT>::clipPointCloud3D(const PointCloud<PointT>&, Indices&, const Indices&)`
- 专项目录：`test-rvv/filters/box_clipper3D/`
- 模块依据：`doc-rvv/library-screening/filters/filters-retained-candidate-rescreen.zh.md` 的后续执行清单第四项。

## 函数级结论

`BoxClipper3D` 是 `Clipper3D` 的仿射盒裁剪实现。输入点先被 `transformation_` 映射到以原点为中心、每个坐标范围 `[-1, 1]` 的标准盒空间，`clipPointCloud3D` 扫描点云并输出盒内点的保序下标。

`transformation_` 是 `BoxClipper3D` 对象的原有成员，不是 RVV 新增语义。标量 `clipPoint3D` 使用：

```cpp
Eigen::Vector4f point_coordinates (transformation_.matrix ()
  * point.getVector4fMap ());
return (point_coordinates.array ().abs () <= 1).all ();
```

RVV helper 接收 `transformation_.matrix()` 后拆成 16 个标量系数，逐 VL chunk 计算同一 4 行 affine 公式。bench 中 std 与 RVV 使用相同的 `makeBalancedBox()` / `makeMostlyKeepBox()` 参数、相同输入点云和相同 case 名；差异仅由 `USE_PCL_RVV10` 控制是否命中 RVV 分流。

标量单点判定是：

```text
q = transformation_ * point.getVector4fMap()
keep(p) = all(abs(q) <= 1)
```

本轮实现覆盖：

- XYZ-compatible 点类型：`pcl::rvv::kRVVXYZPointCompatible<PointT>` 为 true，即
  standard-layout 且有直接 `float x/y/z` 成员；
- `indices.empty()` 的全云扫描；
- 点数 `>= 64`；
- 输出 `clipped` 的清空后重写语义。

以下路径保持标量：

- 显式 subset indices：需要先读取用户 indices 再 gather 点字段，访存形态更复杂；
- 非 XYZ-compatible 点类型：不能证明直接 `float x/y/z` member AoS load；
- 小规模输入：`vsetvl` / `vcompress` 固定成本不稳定；
- `clipPoint3D`、`clipLineSegment3D`、`clipPlanarPolygon3D`：不是本轮全云线性主路径。

## RVV 实现计划

- 常驻 `clipPointCloud3DStd`，保留原标量主体；
- `__RVV10__` 下新增 `clipPointCloud3DRVV`；
- 公开入口用 `if constexpr (pcl::rvv::kRVVXYZPointCompatible<PointT>)` 短路调用 RVV，否则落回 Std；
- RVV 路径复用 `pcl/rvv_point_load.h` 的 `strided_load3_f32m2` 读取 AoS `x/y/z`；
- 一个 VL chunk 内计算 4 行 affine 结果 `tx/ty/tz/tw`，用 `vfabs + vmfle` 形成与标量 `abs() <= 1` 一致的 mask，再用 `vcompress` 保序写出源 indices。

## 收益判断补充

外层标量循环表面只是 `if (clipper.clipPoint3D (...)) clipped.push_back (...)`，但 per-point 成本集中在 `clipPoint3D` 内部：`getVector4fMap()`、4x4 affine 矩阵乘法、临时 `Eigen::Vector4f`、4 个 `abs/compare` 和 `all()`。RVV path 把这个单点 predicate 批量化为 VL chunk 上的 `x/y/z` load、四行 affine、`vfabs/vmfle` mask 和 `vcompress` 输出，避免逐点小对象和函数调用开销。

该模式与 `plane_clipper3D`、`frustum_culling` 同属“单点几何 predicate 批量化”：标量源码看起来短，但 predicate 内部有固定字段、固定矩阵 / 平面系数和保序 indices 输出，适合 RVV。是否接生产仍需板卡验证；`shadowpoints` 已证明算术密度过低或多路 AoS 输入时类似形态可能退化。

## 风险和处理

| 风险 | 处理 |
| --- | --- |
| `clipPointCloud3D` 原语义是先清空输出 | Std helper 和 RVV helper 都先 `clipped.clear()`；RVV 预分配 `n` 后按 keep 数 resize |
| NaN / Inf 语义 | RVV 使用 `vfabs` 后执行 `<= 1`，NaN lane 比较为 false，Inf 超界为 false，与标量 `abs(q) <= 1` 一致；专项 test 覆盖 non-finite |
| 第四齐次分量 | RVV helper 显式使用常量 `1` 计算四行 affine，不读取点类型的第四存储槽；只要求 xyz member layout |
| subset indices gather | 保持标量，专项 test 和 bench 覆盖 fallback |
| 与 `crop_box` 入口重叠 | `BoxClipper3D` 是独立 `Clipper3D` 实现，上游 `test_clipper.cpp` 直接覆盖；板卡主路径收益证明独立入口价值成立 |
| 浮点舍入环境 | 不使用 `_rm` intrinsic，不修改 FRM/FCSR |

## 测试计划

专项测试：`test-rvv/filters/box_clipper3D/test_box_clipper3D.cpp`

- 小规模点云按 affine box 公式对拍；
- 大规模 `PointXYZ` 全云保序输出；
- `clipped` 清空重写语义；
- 显式 subset fallback；
- NaN / Inf 与标量比较一致；
- `PointXYZI` XYZ-compatible 路径与标量公式一致。

上游测试：`test/filters/test_clipper.cpp` 直接覆盖 `BoxClipper3D.Filters`，专项 Makefile 保留 `run_upstream_test_compare` 并补齐 `pcl_filters`、`pcl_io`、`pcl_sample_consensus`、`pcl_search`、`pcl_kdtree`、`pcl_octree`、`flann`、`lz4`、`hdf5`、`zlib`、`libpng` 和 Boost 链接依赖。

## Bench 计划

专项 bench：`test-rvv/filters/box_clipper3D/bench_box_clipper3D.cpp`

输出保持可解析：

- `Dataset: synthetic PointXYZ/PointXYZI clouds; full-cloud affine box clipping RVV cases and subset fallback case`
- `Iterations: 30`
- 每个 case 一行 `<name> : <avg> ms/iter`，详情行包含 `Total Time` 和 checksum。

| case | 入口 | 规模 / 参数 | 路径含义 |
| --- | --- | --- | --- |
| `box_clipper3D pointxyz balanced 64K` | `clipPointCloud3D` | 64K `PointXYZ`，affine box 中等保留率 | RVV 主路径，中等规模 affine + compress |
| `box_clipper3D pointxyz balanced 1M` | `clipPointCloud3D` | 1M `PointXYZ`，同一 affine box | RVV 主路径性能主 case |
| `box_clipper3D pointxyz mostly-keep 1M` | `clipPointCloud3D` | 1M，较高保留率 | 验证输出量增加时压缩写成本 |
| `box_clipper3D subset fallback 1M` | `clipPointCloud3D(..., subset)` | 1M 的一半 subset | fallback 语义 / 成本，不作为 RVV 性能结论 |
| `box_clipper3D pointxyzi xyz-compatible 1M` | `BoxClipper3D<PointXYZI>` | 1M `PointXYZI` | XYZ-compatible RVV 主路径；证明额外字段点类型 checksum 对齐 |

## 当前状态

- 函数级评估：完成。
- RVV 实现：已接入 XYZ-compatible 全云主路径；`PointXYZ` 是已量化板卡性能主 case。
- 专项 test / bench / Makefile / board.mk：已建立。
- QEMU 对拍：`make -C test-rvv/filters/box_clipper3D run_test_compare` 通过，std/RVV 均通过 6 个专项测试。
- QEMU bench：`make -C test-rvv/filters/box_clipper3D run_bench_compare` 通过，std/RVV checksum 对齐；`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`。QEMU 只作为构建、正确性和日志格式证据，不作为真实性能结论。
- 反汇编：`make -C test-rvv/filters/box_clipper3D dump_bench_rvv` 生成 `build/asm/riscv/bench_box_clipper3D_rvv.full.asm`，确认 `vlsseg3e32.v`、`vfmacc.vf`、`vfabs.v`、`vmfle.vf`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。
- 上游测试：`make -C test-rvv/filters/box_clipper3D run_upstream_test_compare` 通过，std/RVV 两套 `test/filters/test_clipper.cpp` 均通过 `BoxClipper3D.Filters` 与 `CropBox.Filters`。
- 板卡验证：`make -C test-rvv/filters/box_clipper3D board_smoke` 通过，日志已拉回 `test-rvv/filters/box_clipper3D/output/board/`。

## 调试记录

初版 RVV mask 用“不是 `< -1` 且不是 `> 1`”表达区间，这会让 NaN lane 被误保留。复核标量 `clipPoint3D` 后确认语义必须是 `abs(q) <= 1`，即 NaN 比较为 false。本轮将 mask 改为 `vfabs + vmfle`，并补充 `NonFiniteValuesMatchScalarComparison` 回归用例。修正后专项 std/RVV 对拍、上游对拍、QEMU bench checksum、反汇编和板卡验证均重新通过。

## 板卡结果

设备：`Milkv-Jupiter`。数据集：synthetic `PointXYZ` clouds，full-cloud affine box clipping RVV cases and subset/type fallback cases。Iterations: `30`。该板卡结果来自原始 `PointXYZ` closeout；本轮类型放宽的正确性和日志格式证据以 QEMU `pointxyzi xyz-compatible` case 为准。

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `box_clipper3D pointxyz balanced 64K` | 3.6611 | 0.6245 | 5.86x | `PointXYZ` 全云主路径，64K 规模 affine 判定收益成立 |
| `box_clipper3D pointxyz balanced 1M` | 57.9010 | 10.1840 | 5.69x | 1M 主路径，4 行 affine + `vcompress` 是本主题真实性能结论核心 |
| `box_clipper3D pointxyz mostly-keep 1M` | 70.5970 | 14.2747 | 4.95x | 高保留率下写出量增加，RVV 仍保持收益 |
| `box_clipper3D subset fallback 1M` | 27.4105 | 27.9268 | 0.98x | 显式 subset fallback，证明未覆盖 gather 路径语义和成本接近 |
| `box_clipper3D pointxyzi fallback 1M` | 61.7393 | 62.3824 | 0.99x | 历史类型 fallback case；不作为本轮 `PointXYZI` RVV 主路径性能结论 |

结论：`BoxClipper3D<PointXYZ>::clipPointCloud3D` 全云主路径在板卡上约 `4.95x` 到
`5.86x`。当前源码 gate 已扩大为 XYZ-compatible；`PointXYZI` 正确性由 QEMU compare
覆盖，类型真实性能如需纳入结论，可后续单独重跑板卡。
