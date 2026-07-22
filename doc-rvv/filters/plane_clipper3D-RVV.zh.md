# filters/plane_clipper3D RVV 优化说明

## 背景与范围

`pcl::PlaneClipper3D<PointT>` 是 `Clipper3D` 的平面裁剪实现。用户用齐次平面参数 `(a,b,c,d)` 构造或配置 clipper 后，可以调用 `clipPointCloud3D(cloud, clipped, indices)` 扫描输入点云，把位于平面正侧的点下标追加到 `clipped`。

本轮优化的直接入口是：

```text
PlaneClipper3D<PointT>::clipPointCloud3D(const PointCloud<PointT>& cloud_in,
                                         Indices& clipped,
                                         const Indices& indices)
```

它在 PCL 中承担“几何平面筛选并输出保序 indices”的职责。`clipPoint3D(point)` 是单点判定，`clipLineSegment3D` / `clipPlanarPolygon3D` 是线段和多边形裁剪；本轮 RVV 优化的是 `clipPointCloud3D` 的全云线性主路径。

已覆盖：

- XYZ-compatible 点类型：`pcl::rvv::kRVVXYZPointCompatible<PointT>` 为 true，即
  standard-layout 且有直接 `float x/y/z` 成员；
- `indices.empty()` 的全云扫描；
- 点数不少于 `64`；
- 输出 `clipped` 的追加语义。

未覆盖路径全部回退标量，包括显式 subset indices、非 XYZ-compatible 点类型、小规模输入、单点 / 线段 / 多边形裁剪。

## 实现结构

公开 API 不变。`impl/plane_clipper3D.hpp` 新增常驻标量 helper 和 `__RVV10__` RVV helper：

| 实体 | 作用 |
| --- | --- |
| `clipPointCloud3DStd` | 常驻标量 helper，保留原 `clipPointCloud3D` 主体和追加输出语义 |
| `kPlaneClipper3DMinPoints` | 小规模 fallback 阈值 |
| `clipPointCloud3DRVV` | `__RVV10__` 下的 RVV helper，承载 stride load、plane dot、mask 和压缩写 |

公开入口短路分流：

```cpp
#if defined(__RVV10__)
  if constexpr (pcl::rvv::kRVVXYZPointCompatible<PointT>)
    if (pcl::clipPointCloud3DRVV (cloud_in, clipped, indices, plane_params_))
      return;
#endif
  pcl::clipPointCloud3DStd (cloud_in, clipped, indices, *this);
```

主路径 helper 位于 `pcl` 命名空间，没有额外放入 `pcl::detail`。类型覆盖使用公共
`rvv_point_traits.h` 的 member `x/y/z` layout gate；算法本地仍保留 subset、小规模和
单点 / 线段 / 多边形裁剪的 dispatch 边界。

核心 RVV 片段：

```cpp
const std::size_t old_size = clipped.size ();
clipped.resize (old_size + n);
int* out = clipped.data () + old_size;

while (i < n)
{
  const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
  const auto* chunk = base + i * sizeof (PointT);
  const auto stride = static_cast<ptrdiff_t> (sizeof (PointT));

  const vfloat32m2_t vx = __riscv_vlse32_v_f32m2 (... x ..., stride, vl);
  const vfloat32m2_t vy = __riscv_vlse32_v_f32m2 (... y ..., stride, vl);
  const vfloat32m2_t vz = __riscv_vlse32_v_f32m2 (... z ..., stride, vl);

  vfloat32m2_t distance = __riscv_vfmul_vf_f32m2 (vy, b, vl);
  distance = __riscv_vfmacc_vf_f32m2 (distance, a, vx, vl);
  distance = __riscv_vfmacc_vf_f32m2 (distance, c, vz, vl);
  const vbool16_t keep = __riscv_vmfge_vf_f32m2_b16 (distance, -d, vl);

  const vint32m2_t compact = __riscv_vcompress_vm_i32m2 (source_i32, keep, vl);
  __riscv_vse32_v_i32m2 (out + kept, compact, __riscv_vcpop_m_b16 (keep, vl));
}
```

这段代码展示了维护边界：先记录 `old_size` 保持追加语义；AoS 使用 `sizeof(PointT)` stride 读取 `x/y/z`；RVV 的 `vfmul` / `vfmacc` 求值顺序贴近当前标量编译形状，避免边界点因浮点舍入进入不同分支；`vcompress` 保持扫描顺序。

## RVV 数据组织

XYZ-compatible `PointCloud<PointT>` 是 AoS 布局。一个 VL chunk 从点 `c` 开始：

```text
cloud[c]         cloud[c+1]       cloud[c+2]       cloud[c+3]
x y z padding    x y z padding    x y z padding    x y z padding
```

RVV 分别用 stride load 得到：

```text
vx = [x_c, x_c+1, x_c+2, x_c+3]
vy = [y_c, y_c+1, y_c+2, y_c+3]
vz = [z_c, z_c+1, z_c+2, z_c+3]
```

标量公式：

```text
keep = a*x + b*y + c*z >= -d
```

RVV mask：

```text
distance = a*vx + b*vy + c*vz
keep     = distance >= -d
indices  = vcompress([c, c+1, ...], keep)
```

subset indices 暂缓：需要先 gather 用户 indices，再 gather AoS 点字段，访存更不规整；当前全云主路径已经覆盖 `Clipper3D` 最直接的线性筛选场景。

## 数值算例

平面参数：

```text
(a,b,c,d) = (1.0, -0.5, 0.25, -0.1)
keep = x - 0.5*y + 0.25*z >= 0.1
```

某个 VL chunk 从 `c=20` 开始，VL=4：

| lane | source index | x | y | z | distance | keep |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 0 | 20 | -0.2 | 0.0 | 0.0 | -0.20 | false |
| 1 | 21 | 0.5 | 0.4 | 0.0 | 0.30 | true |
| 2 | 22 | 0.0 | -0.4 | 0.4 | 0.30 | true |
| 3 | 23 | 0.1 | 0.6 | -0.4 | -0.30 | false |

标量扫描会追加 `[21, 22]`。RVV 中：

```text
source_index = [20, 21, 22, 23]
keep mask    = [0, 1, 1, 0]
vcompress(source_index, keep) -> [21, 22]
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

NaN / Inf 语义：原标量 `clipPoint3D` 没有 finite 检查，直接执行浮点比较。本轮 RVV 也不额外加 finite mask；NaN 比较为 false，Inf 按 IEEE 比较参与，与标量条件一致。

FRM/FCSR：本实现不使用 `_rm` intrinsic，不修改浮点舍入环境。

## 测试与验证

专项测试：

```text
make -C test-rvv/filters/plane_clipper3D run_test_compare
```

结果：std 与 RVV 二进制均通过 5 个专项测试，覆盖小规模公式对拍、大规模 `PointXYZ`
主路径、`clipped` 追加语义、subset fallback 和 `PointXYZI` XYZ-compatible 路径与标量公式一致。

上游测试：

```text
make -C test-rvv/filters/plane_clipper3D run_upstream_test_compare
```

结果：std 与 RVV 两套 `test/filters/test_clipper.cpp` 均通过 `BoxClipper3D.Filters` 与 `CropBox.Filters`。该上游文件没有直接 `PlaneClipper3D` case，因此专项测试承担本主题主覆盖。

QEMU bench 和反汇编：

```text
make -C test-rvv/filters/plane_clipper3D run_bench_compare dump_bench_rvv
```

结果：

- `output/qemu/analyze_bench_compare.log` 可解析；
- 无 `未解析`、`n/a`、`Total Time 不计算`；
- std/RVV checksum 对齐；
- 反汇编确认 `vlse32.v`、`vfmul.vf`、`vfmacc.vf`、`vmfge.vf`、`vcompress.vm`、`vcpop.m`、`vse32.v`、`vsetvli ... e32,m2`。

QEMU 下 RVV 主路径慢于标量，这只说明模拟器执行成本，不作为真实性能结论。

## Bench Case 含义

所有 speedup 均由分析脚本按 `Std avg / RVV avg` 计算。QEMU 日志只用于构建、正确性补充、日志格式和指令路径证据；真实性能结论使用下方 Milkv-Jupiter 板卡日志。

| case | 函数入口与数据 | 路径 | 证明点 |
| --- | --- | --- | --- |
| `plane_clipper3D pointxyz balanced 64K` | `PlaneClipper3D<PointXYZ>::clipPointCloud3D`，64K `PointXYZ`，平衡保留率 | RVV 主路径 | 中等规模 plane dot + 保序压缩 |
| `plane_clipper3D pointxyz balanced 1M` | 同入口，1M `PointXYZ` | RVV 主路径 | 大规模主性能 case |
| `plane_clipper3D pointxyz mostly-keep 1M` | 同入口，1M `PointXYZ`，较高保留率 | RVV 主路径 | 输出量增加时 `vcompress` 成本仍可控 |
| `plane_clipper3D subset fallback 1M` | `clipPointCloud3D(..., subset)`，1M 的一半 subset | fallback | 证明 gather/subset 未覆盖路径保持语义和接近成本 |
| `plane_clipper3D pointxyzi xyz-compatible 1M` | `PlaneClipper3D<PointXYZI>::clipPointCloud3D`，1M `PointXYZI` | RVV 主路径 | 证明公共 traits 接入后额外字段点类型保持标量 checksum 一致 |

## 板卡结果

专项 Makefile 提供 `deploy_board`、`run_board_test`、`run_board_bench_compare`、`fetch_board_logs` 和 `board_smoke`。本轮已完成：

```text
make -C test-rvv/filters/plane_clipper3D run_board_test run_board_bench_compare fetch_board_logs
```

日志已拉回：

```text
test-rvv/filters/plane_clipper3D/output/board/run_test.log
test-rvv/filters/plane_clipper3D/output/board/run_bench_std.log
test-rvv/filters/plane_clipper3D/output/board/run_bench_rvv.log
test-rvv/filters/plane_clipper3D/output/board/analyze_bench_compare.log
```

板卡：`Milkv-Jupiter`。Dataset: synthetic `PointXYZ` clouds; full-cloud plane clipping RVV cases and subset/type fallback cases。Iterations: `30`。下表中的 `PointXYZ` 主路径数据仍是本主题性能证据；`PointXYZI fallback` 行来自公共 traits 接入前的历史 bench 命名，本轮类型放宽的正确性和日志格式证据以 QEMU `pointxyzi xyz-compatible` case 为准。

| case | Std ms/iter | RVV ms/iter | speedup | 说明 |
| --- | ---: | ---: | ---: | --- |
| `plane_clipper3D pointxyz balanced 64K` | 1.6313 | 0.5439 | 3.00x | 64K 全云主路径，证明中等规模收益 |
| `plane_clipper3D pointxyz balanced 1M` | 25.8590 | 8.9499 | 2.89x | 1M 主路径，证明大规模 plane dot + compress 收益 |
| `plane_clipper3D pointxyz mostly-keep 1M` | 32.7247 | 13.1726 | 2.48x | 高保留率主路径，写出量较高仍有收益 |
| `plane_clipper3D subset fallback 1M` | 12.6639 | 12.6789 | 1.00x | subset fallback，不作为 RVV 主路径性能结论 |
| `plane_clipper3D pointxyzi fallback 1M` | 26.4751 | 25.7216 | 1.03x | 历史类型 fallback case；不作为本轮 `PointXYZI` RVV 主路径性能结论 |

结论：`PlaneClipper3D<PointXYZ>::clipPointCloud3D` 全云主路径在板卡上约 `2.48x` 到
`3.00x`。当前源码 gate 已扩大为 XYZ-compatible，全云 `PointXYZI` 正确性由 QEMU
compare 覆盖；类型真实性能如需纳入结论，可后续单独重跑板卡。
