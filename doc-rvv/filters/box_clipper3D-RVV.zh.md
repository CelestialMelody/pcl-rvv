# filters/box_clipper3D RVV 优化说明

## 背景与范围

`pcl::BoxClipper3D<PointT>` 是 `Clipper3D` 的 3D 盒裁剪实现。它保存一个 affine transformation，把输入点变换到标准盒空间，再判断齐次坐标的每个分量是否位于 `[-1, 1]`。公开入口 `clipPointCloud3D(cloud, clipped, indices)` 扫描点云并把盒内点下标写入 `clipped`。

这里的 `transformation_` 不是 RVV 新增参数，而是 `BoxClipper3D` 对象本来就保存的裁剪盒定义。构造函数可以直接接收一个 `Eigen::Affine3f`，也可以由 Rodrigues 旋转、平移和盒尺寸生成：

```cpp
transformation_ = (Translation * Rotation * Scaling(0.5 * box_size)).inverse ();
```

含义是“把世界坐标中的点变换到标准盒坐标”。标量 `clipPoint3D` 正是用这个成员矩阵做判定：

```cpp
Eigen::Vector4f point_coordinates (transformation_.matrix ()
  * point.getVector4fMap ());
return (point_coordinates.array ().abs () <= 1).all ();
```

因此 RVV helper 接收 `transformation_.matrix()` 只是把同一成员矩阵预取为 16 个标量系数，避免在每个点上反复走 Eigen 小对象和函数调用路径；bench 中 std 与 RVV 使用同一个 `BoxClipper3D` 构造参数和同一份输入点云。

本轮优化的直接入口是：

```text
BoxClipper3D<PointT>::clipPointCloud3D(const PointCloud<PointT>& cloud_in,
                                       Indices& clipped,
                                       const Indices& indices)
```

它在 PCL 中承担“仿射几何筛选并输出保序 indices”的职责。`clipPoint3D(point)` 是单点判定，线段 / 多边形裁剪入口当前仍是标量或未实现路径；本轮 RVV 优化的是 `clipPointCloud3D` 的全云线性主路径。

已覆盖：

- XYZ-compatible 点类型：`pcl::rvv::kRVVXYZPointCompatible<PointT>` 为 true，即
  standard-layout 且有直接 `float x/y/z` 成员；
- `indices.empty()` 的全云扫描；
- 点数不少于 `64`；
- 输出 `clipped` 的清空后重写语义。

未覆盖路径全部回退标量，包括显式 subset indices、非 XYZ-compatible 点类型、小规模输入、单点 / 线段 / 多边形裁剪。

## 实现结构

公开 API 不变。`impl/box_clipper3D.hpp` 新增常驻标量 helper 和 `__RVV10__` RVV helper：

| 实体 | 作用 |
| --- | --- |
| `clipPointCloud3DStd` | 常驻标量 helper，保留原 `clipPointCloud3D` 主体和清空输出语义 |
| `kBoxClipper3DMinPoints` | 小规模 fallback 阈值 |
| `clipPointCloud3DRVV` | `__RVV10__` 下的 RVV helper，承载 stride load、4 行 affine、mask 和压缩写 |

公开入口短路分流：

```cpp
#if defined(__RVV10__)
  if constexpr (pcl::rvv::kRVVXYZPointCompatible<PointT>)
  {
    if (pcl::clipPointCloud3DRVV (cloud_in, clipped, indices, transformation_.matrix ()))
      return;
  }
#endif

  pcl::clipPointCloud3DStd (cloud_in, clipped, indices, *this);
```

主路径 helper 位于 `pcl` 命名空间，没有额外放入 `pcl::detail`。类型覆盖使用公共
`rvv_point_traits.h` 的 member `x/y/z` layout gate；subset、小规模和其它 clipper
入口仍由算法本地 dispatch 控制。

## 详细设计

XYZ-compatible `PointCloud<PointT>` 是 AoS 布局。RVV path 复用公共 load 封装读取
当前 `PointT` 的 `x/y/z` 字段；齐次第四分量在本 helper 中显式按常量 `1` 参与四行 affine：

```cpp
pcl::rvv_load::strided_load3_f32m2<sizeof (PointT),
                                   offsetof (PointT, x),
                                   offsetof (PointT, y),
                                   offsetof (PointT, z)> (chunk, vl, vx, vy, vz);
```

`strided_load3_f32m2` 在 `x/y/z` 连续时生成 segment stride load，当前反汇编确认命中 `vlsseg3e32.v`。随后在同一个 VL chunk 上计算：

```text
tx = m00*x + m01*y + m02*z + m03
ty = m10*x + m11*y + m12*z + m13
tz = m20*x + m21*y + m22*z + m23
tw = m30*x + m31*y + m32*z + m33
keep = abs(tx)<=1 && abs(ty)<=1 && abs(tz)<=1 && abs(tw)<=1
```

核心 RVV 片段：

```cpp
vfloat32m2_t tx = __riscv_vfmul_vf_f32m2 (vx, m00, vl);
tx = __riscv_vfmacc_vf_f32m2 (tx, m01, vy, vl);
tx = __riscv_vfmacc_vf_f32m2 (tx, m02, vz, vl);
tx = __riscv_vfadd_vf_f32m2 (tx, m03, vl);

vbool16_t keep = __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (tx, vl), 1.0f, vl);
keep = __riscv_vmand_mm_b16 (keep, __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (ty, vl), 1.0f, vl), vl);
keep = __riscv_vmand_mm_b16 (keep, __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (tz, vl), 1.0f, vl), vl);
keep = __riscv_vmand_mm_b16 (keep, __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (tw, vl), 1.0f, vl), vl);

const vint32m2_t compact = __riscv_vcompress_vm_i32m2 (source_i32, keep, vl);
__riscv_vse32_v_i32m2 (out + kept, compact, __riscv_vcpop_m_b16 (keep, vl));
```

`vfabs + vmfle` 是数值语义关键点：标量用 `(point_coordinates.array().abs() <= 1).all()`，因此 NaN lane 必须比较为 false，Inf 超界也为 false。初版“不是小于下界且不是大于上界”的写法会误保留 NaN，已改正并补回归测试。

FRM/FCSR：本实现不使用 `_rm` intrinsic，不修改浮点舍入环境。

## 为什么这个标量循环值得 RVV 化

`clipPointCloud3DStd` 的外层代码看起来只是：

```cpp
for (std::size_t pIdx = 0; pIdx < cloud_in.size (); ++pIdx)
  if (clipper.clipPoint3D (cloud_in[pIdx]))
    clipped.push_back (pIdx);
```

但真正的每点成本藏在 `clipPoint3D` 里：

1. 读取 `PointXYZ::getVector4fMap()` 的齐次 4 元向量；
2. 执行 4x4 affine 矩阵乘法；
3. 构造临时 `Eigen::Vector4f point_coordinates`；
4. 执行 4 个 `abs` 和 `<= 1`；
5. 把四个布尔条件规约成 `all()`；
6. 命中时执行 `push_back`。

RVV 路径不是优化一个单纯的 `if`，而是把上述 per-point 小矩阵 / 小对象 / 函数调用链展开为 VL chunk 上的批量算术：

```text
4 行 affine = 12 次向量 FMA + 4 次向量加常量
box predicate = 4 次 vfabs + 4 次 vmfle + 3 次 mask and
output = source indices + vcompress
```

这类函数容易被低估：标量源码表面是“调用一个单点 predicate 再 push index”，但 predicate 内部是固定小矩阵、固定字段和固定阈值，恰好适合在 RVV 中把输入字段一次性加载、把对象级开销移出循环、把输出保序压缩。`plane_clipper3D`、`frustum_culling` 和本主题都属于这种“单点几何 predicate 批量化”的强模式；`shadowpoints` 则显示当算术密度太低且需要多路 AoS 输入时，类似压缩输出不一定盈利。

本主题的正确性依赖以下等价关系：

```text
标量：q = T * [x, y, z, 1]^T; keep = all(abs(q) <= 1)
RVV ：tx/ty/tz/tw 分别计算 T 的四行；keep = abs(tx)<=1 && abs(ty)<=1 && abs(tz)<=1 && abs(tw)<=1
```

专项测试用同一个 `Eigen::Affine3f transform` 同时喂给标量公式和 `BoxClipper3D` 对象；bench 的 std/RVV 二进制也使用相同的 `makeBalancedBox()` / `makeMostlyKeepBox()` 参数，差异只来自是否定义 `__RVV10__` 后命中 RVV 分流。

## VL Chunk 图示

一个 VL chunk 从点 `c` 开始：

```text
cloud[c]         cloud[c+1]       cloud[c+2]       cloud[c+3]
x y z 1          x y z 1          x y z 1          x y z 1
```

RVV 读取和计算：

```text
vx = [x_c, x_c+1, x_c+2, x_c+3]
vy = [y_c, y_c+1, y_c+2, y_c+3]
vz = [z_c, z_c+1, z_c+2, z_c+3]

tx/ty/tz/tw = T * [x y z 1]^T
keep        = abs(tx)<=1 && abs(ty)<=1 && abs(tz)<=1 && abs(tw)<=1
indices     = vcompress([c, c+1, ...], keep)
```

subset indices 暂缓：需要额外 gather 用户下标和点字段，访存更不规整；当前全云主路径已经覆盖 `BoxClipper3D` 最直接的线性筛选场景。

## 数值算例

取一个简化 affine：

```text
tx = 0.5*x + 0.1
ty = y
tz = z
tw = 1
```

某个 VL chunk 从 `c=40` 开始，VL=4：

| lane | source index | x | y | z | tx | ty | tz | keep |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 0 | 40 | -2.0 | 0.0 | 0.0 | -0.9 | 0.0 | 0.0 | true |
| 1 | 41 | 2.4 | 0.2 | 0.0 | 1.3 | 0.2 | 0.0 | false |
| 2 | 42 | 0.0 | -1.1 | 0.0 | 0.1 | -1.1 | 0.0 | false |
| 3 | 43 | 0.5 | 0.5 | -0.5 | 0.35 | 0.5 | -0.5 | true |

标量扫描会输出 `[40, 43]`。RVV 中：

```text
source_index = [40, 41, 42, 43]
keep mask    = [1, 0, 0, 1]
vcompress(source_index, keep) -> [40, 43]
```

结果与标量公式和顺序一致。

## 分流与回退

RVV helper 返回 false 的情况：

- `indices` 非空；
- 点数小于 `64`；
- 点数超过 `int` 范围。

公开入口还会在以下情况下落回 Std：

- 非 RVV 编译；
- `PointT` 不满足 `pcl::rvv::kRVVXYZPointCompatible<PointT>`；
- 单点、线段、多边形裁剪入口。

## 测试与验证

专项测试：

```text
make -C test-rvv/filters/box_clipper3D run_test_compare
```

结果：std 与 RVV 二进制均通过 6 个专项测试，覆盖小规模公式对拍、大规模 `PointXYZ`
主路径、`clipped` 清空语义、subset fallback、NaN / Inf 语义和 `PointXYZI`
XYZ-compatible 路径与标量公式一致。

上游测试：

```text
make -C test-rvv/filters/box_clipper3D run_upstream_test_compare
```

结果：std 与 RVV 两套 `test/filters/test_clipper.cpp` 均通过 `BoxClipper3D.Filters` 与 `CropBox.Filters`。

QEMU bench 和反汇编：

```text
make -C test-rvv/filters/box_clipper3D run_bench_compare dump_bench_rvv
```

结果：

- `output/qemu/analyze_bench_compare.log` 可解析；
- 无 `未解析`、`n/a`、`Total Time 不计算`；
- std/RVV checksum 对齐；
- 反汇编确认 `vlsseg3e32.v`、`vfmacc.vf`、`vfabs.v`、`vmfle.vf`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。

QEMU 日志只用于构建、正确性补充、日志格式和指令路径证据，不作为真实性能结论。

## Bench Case 含义

所有 speedup 均由分析脚本按 `Std avg / RVV avg` 计算。QEMU 日志只用于构建、正确性补充、日志格式和指令路径证据；真实性能结论使用下方 Milkv-Jupiter 板卡日志。

| case | 函数入口与数据 | 路径 | 证明点 |
| --- | --- | --- | --- |
| `box_clipper3D pointxyz balanced 64K` | `BoxClipper3D<PointXYZ>::clipPointCloud3D`，64K `PointXYZ`，affine box 中等保留率 | RVV 主路径 | 中等规模 4 行 affine + 保序压缩 |
| `box_clipper3D pointxyz balanced 1M` | 同入口，1M `PointXYZ` | RVV 主路径 | 大规模主性能 case |
| `box_clipper3D pointxyz mostly-keep 1M` | 同入口，1M `PointXYZ`，较高保留率 | RVV 主路径 | 输出量增加时 `vcompress` 成本仍可控 |
| `box_clipper3D subset fallback 1M` | `clipPointCloud3D(..., subset)`，1M 的一半 subset | fallback | 证明 gather/subset 未覆盖路径保持语义和接近成本 |
| `box_clipper3D pointxyzi xyz-compatible 1M` | `BoxClipper3D<PointXYZI>::clipPointCloud3D`，1M `PointXYZI` | RVV 主路径 | 证明公共 traits 接入后额外字段点类型保持标量 checksum 一致 |

## 板卡结果

专项 Makefile 提供 `deploy_board`、`run_board_test`、`run_board_bench_compare`、`fetch_board_logs` 和 `board_smoke`。本轮已完成：

```text
make -C test-rvv/filters/box_clipper3D board_smoke
```

日志已拉回：

```text
test-rvv/filters/box_clipper3D/output/board/run_test.log
test-rvv/filters/box_clipper3D/output/board/run_bench_std.log
test-rvv/filters/box_clipper3D/output/board/run_bench_rvv.log
test-rvv/filters/box_clipper3D/output/board/analyze_bench_compare.log
```

板卡：`Milkv-Jupiter`。Dataset: synthetic `PointXYZ` clouds; full-cloud affine box clipping RVV cases and subset/type fallback cases。Iterations: `30`。下表中的 `PointXYZ` 主路径数据仍是本主题性能证据；`PointXYZI fallback` 行来自公共 traits 接入前的历史 bench 命名，本轮类型放宽的正确性和日志格式证据以 QEMU `pointxyzi xyz-compatible` case 为准。

| case | Std ms/iter | RVV ms/iter | speedup | 说明 |
| --- | ---: | ---: | ---: | --- |
| `box_clipper3D pointxyz balanced 64K` | 3.6611 | 0.6245 | 5.86x | 64K 全云主路径，证明中等规模收益 |
| `box_clipper3D pointxyz balanced 1M` | 57.9010 | 10.1840 | 5.69x | 1M 主路径，证明 affine box + compress 收益 |
| `box_clipper3D pointxyz mostly-keep 1M` | 70.5970 | 14.2747 | 4.95x | 高保留率主路径，写出量较高仍有收益 |
| `box_clipper3D subset fallback 1M` | 27.4105 | 27.9268 | 0.98x | subset fallback，不作为 RVV 主路径性能结论 |
| `box_clipper3D pointxyzi fallback 1M` | 61.7393 | 62.3824 | 0.99x | 历史类型 fallback case；不作为本轮 `PointXYZI` RVV 主路径性能结论 |

结论：`BoxClipper3D<PointXYZ>::clipPointCloud3D` 全云主路径在板卡上约 `4.95x` 到
`5.86x`。当前源码 gate 已扩大为 XYZ-compatible，全云 `PointXYZI` 正确性由 QEMU
compare 覆盖；类型真实性能如需纳入结论，可后续单独重跑板卡。
