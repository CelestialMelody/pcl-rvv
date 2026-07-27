# filters/frustum_culling RVV 优化说明

## 背景与范围

`pcl::FrustumCulling<PointT>` 是 `FilterIndices<PointT>` 派生类，用相机 pose、FOV、near/far plane 和 ROI 定义视锥，然后输出视锥内点的下标。公开调用通常是：

```text
fc.setInputCloud(cloud)
fc.setCameraPose(pose)
fc.setVerticalFOV(...)
fc.setHorizontalFOV(...)
fc.filter(indices or output_cloud)
```

`filter(Indices&)` 经由 `FilterIndices` 调到本主题优化的 `applyFilter(Indices&)`。该入口先根据配置构造 left/right/top/bottom/far/near 6 个平面，再线性扫描 `indices_`，输出 kept indices；当 `extract_removed_indices_` 为 true 时，同时输出 removed indices。它在点云管线中承担“视锥几何筛选 / 保序压缩”的职责。

已覆盖：

- XYZ-compatible 点类型：`pcl::rvv::kRVVXYZPointCompatible<PointT>` 为 true，即
  standard-layout 且有直接 `float x/y/z` 成员；
- 未显式 `setIndices()` 的 dense 全云扫描；
- 点数不少于 `64`；
- `negative_` 和 `extract_removed_indices_`。

未覆盖路径全部回退标量，包括显式 subset indices、non-dense、非 XYZ-compatible 点类型、小规模输入。`keep_organized_` 的 cloud 输出由 `FilterIndices` 基类后处理，本轮 RVV 只优化 `applyFilter(Indices&)` 的 indices 主路径。

## 实现结构

公开 API 不变。`filters/include/pcl/filters/impl/frustum_culling.hpp` 新增：

| 实体 | 作用 |
| --- | --- |
| `frustumCullingApplyFilterStd` | 常驻标量 helper，保留原点循环、`negative_` 与 `removed_indices_` 语义 |
| `kFrustumCullingIndicesMinPoints` | 小规模 fallback 阈值 |
| `kFrustumCullingXYZCompatible` | 本地别名，指向公共 `pcl::rvv::kRVVXYZPointCompatible<PointT>` |
| `frustumCullingApplyFilterRVV` | `__RVV10__` 下的 RVV helper，承载 stride load、6 平面 dot、mask 合并和压缩写 |

公开入口仍负责平面构造；构造完成后短路分流：

```cpp
#if defined(__RVV10__)
  if constexpr (pcl::kFrustumCullingXYZCompatible<PointT>)
  {
    if (pcl::frustumCullingApplyFilterRVV (*input_, indices, removed_indices_,
                                           extract_removed_indices_, negative_, fake_indices_,
                                           pl_l, pl_r, pl_t, pl_b, pl_f, pl_n))
      return;
  }
#endif

  pcl::frustumCullingApplyFilterStd (*input_, *indices_, indices, removed_indices_,
                                     extract_removed_indices_, negative_,
                                     pl_l, pl_r, pl_t, pl_b, pl_f, pl_n);
```

主路径 helper 位于 `pcl` 命名空间，没有额外放入 `pcl::detail`。类型检测是语义小工具，不命名为 `*RVV`。

核心 RVV 片段：

```cpp
vfloat32m2_t vx, vy, vz;
pcl::rvv_load::strided_load3_fields_f32m2<sizeof (PointT),
                                          offsetof (PointT, x),
                                          offsetof (PointT, y),
                                          offsetof (PointT, z)> (
    chunk, vl, vx, vy, vz);

auto plane_leq_zero = [&](const Eigen::Vector4f& plane) {
  vfloat32m2_t distance = __riscv_vfmul_vf_f32m2 (vx, plane[0], vl);
  distance = __riscv_vfmacc_vf_f32m2 (distance, plane[1], vy, vl);
  distance = __riscv_vfmacc_vf_f32m2 (distance, plane[2], vz, vl);
  distance = __riscv_vfadd_vf_f32m2 (distance, plane[3], vl);
  return __riscv_vmfle_vf_f32m2_b16 (distance, 0.0f, vl);
};

vbool16_t inside = plane_leq_zero (pl_l);
inside = __riscv_vmand_mm_b16 (inside, plane_leq_zero (pl_r), vl);
...
const vbool16_t keep = negative ? __riscv_vmnot_m_b16 (inside, vl) : inside;
const vint32m2_t kept_i32 = __riscv_vcompress_vm_i32m2 (source_i32, keep, vl);
```

这段代码展示了主要边界：AoS x/y/z 通过公共字段 load helper 读取，helper 不扩大 FrustumCulling 的 production dispatch；每个平面复用同一 VL chunk；mask 合并对应标量 6 个 `&&`；`negative_` 只反转最终 mask；`vcompress` 保持扫描顺序。

这个 `plane_leq_zero` 组织方式是本轮 filters 里首次使用的“同一 VL chunk 上重复套用多个小型线性谓词”的 RVV 模式，和此前主题有明显区别：

| 已有模式 | 代表主题 | RVV 形态 |
| --- | --- | --- |
| 单公式筛选 | `plane_clipper3D`、`passthrough`、`crop_box` | 一个或少量字段比较直接形成 keep mask |
| 规约 | `voxel_grid` | 对 x/y/z 做 min/max reduction |
| 图像式邻域 | `convolution` | 按 organized 行列组织连续 / stride 访存 |
| 多谓词合取 | `frustum_culling` | 对同一 x/y/z chunk 连续计算 6 个平面谓词，再合并 mask |

这里没有把 6 个平面展开成 6 份几乎相同的代码，而是用局部 `plane_leq_zero` 表达“给定一个平面，复用当前 VL chunk 的 x/y/z，生成 `distance <= 0` mask”。这样做有三个维护目的：

- 避免 6 个平面的 FMA / add / compare 代码手写重复，降低某个平面漏改或参数顺序写错的风险；
- 明确 x/y/z 只按 VL chunk 加载一次，随后在寄存器中被 left/right/top/bottom/far/near 6 个平面复用；
- 把标量语义中的 `pt.dot(plane) <= 0` 保持为一个可命名的小谓词，后面的 `vmand` 链只负责复现 6 个 `&&`。

该 lambda 不是运行时分发层，也不承载额外状态；编译后在 `frustumCullingApplyFilterRVV<PointXYZ>` 中仍展开为 `vlse32.v`、多组 `vfmacc.vf`、`vmfle.vf`、`vcompress.vm` 和 `vcpop.m`。因此它属于“解释多谓词 mask 组织的语义小工具”，不是新的公开 helper，也不应命名为 `*RVV`。后续如果遇到 frustum、model outlier、shadowpoints 这类“多个几何谓词共享同一 VL chunk 输入”的主题，可以复用这种写法，但需要重新确认每个谓词的 NaN/Inf、阈值、反向条件和输出压缩语义是否与标量一致。

## RVV 数据组织

XYZ-compatible `PointCloud<PointT>` 是 AoS：

```text
cloud[c]         cloud[c+1]       cloud[c+2]       cloud[c+3]
x y z padding    x y z padding    x y z padding    x y z padding
```

一个 VL chunk 从点 `c` 开始：

```text
vx = [x_c, x_c+1, x_c+2, x_c+3]
vy = [y_c, y_c+1, y_c+2, y_c+3]
vz = [z_c, z_c+1, z_c+2, z_c+3]
```

对于某个平面 `pl = (a,b,c,d)`：

```text
distance = a*vx + b*vy + c*vz + d
mask_pl  = distance <= 0
```

6 个平面 mask 用 `vmand` 合并：

```text
inside = mask_left & mask_right & mask_top & mask_bottom & mask_far & mask_near
keep   = negative ? !inside : inside
out    = vcompress([c, c+1, ...], keep)
```

显式 subset 暂缓：它需要先加载用户 indices，再按这些 indices gather AoS 点字段；当前板卡证据显示 dense 全云主路径已经有强收益，subset gather 留待单独评估。

## 数值算例

假设某个视锥已经构造成 6 个平面。为便于手算，只列出 lane 是否满足每个平面：

```text
VL chunk source indices = [40, 41, 42, 43]
```

| lane | source index | left | right | top | bottom | far | near | inside | negative=false 输出 |
| --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 40 | T | T | T | T | T | T | T | keep 40 |
| 1 | 41 | T | F | T | T | T | T | F | removed |
| 2 | 42 | T | T | T | T | T | T | T | keep 42 |
| 3 | 43 | T | T | F | T | T | T | F | removed |

标量扫描会输出 `[40, 42]`。RVV 中：

```text
source_index = [40, 41, 42, 43]
inside mask  = [1, 0, 1, 0]
vcompress(source_index, inside) -> [40, 42]
```

若 `negative=true`，最终 mask 变为 `[0,1,0,1]`，输出 `[41,43]`；若 `extract_removed_indices=true`，removed mask 与 keep mask 相反，单独压缩到 `removed_indices_`，顺序仍与标量一致。

## 分流与回退

RVV helper 返回 false 的情况：

- `fake_indices_ == false`；
- `input.is_dense == false`；
- 点数小于 `64`；
- 点数超过 `int` 范围。

公开入口还会在以下情况下落回 Std：

- 非 RVV 编译；
- `PointT` 不满足 `pcl::rvv::kRVVXYZPointCompatible<PointT>`。

NaN / Inf：dense 主路径假定调用者设置的 `is_dense` 可信；non-dense 回退 Std。本实现不额外修改 NaN / Inf 语义。

FRM/FCSR：不使用 `_rm` intrinsic，不修改浮点舍入环境。

## 诊断记录

早期 closeout 曾把 `PointXYZI` 作为非目标类型 fallback，用于证明 exact `PointXYZ`
生产边界。公共 `rvv_point_traits.h` 接入后，当前实现改为
`pcl::rvv::kRVVXYZPointCompatible<PointT>`，`PointXYZI` 这类 standard-layout 且有
直接 `float x/y/z` 成员的点类型会进入全云 dense RVV 路径。当前 QEMU bench 已把
`PointXYZI` case 重命名为 `xyz-compatible`，并用 checksum 证明它与标量路径一致。

QEMU bench 中 RVV 主路径慢于 Std，这是模拟器执行成本现象；QEMU 只作为构建、正确性、日志格式和指令路径证据，不作为真实性能结论。

## 测试与验证

专项测试：

```text
make -C test-rvv/filters/frustum_culling run_test_compare
```

结果：std 与 RVV 二进制均通过 7 个专项测试，覆盖小规模 fallback、大规模 `PointXYZ`
主路径、`negative_`、`extract_removed_indices_`、far plane infinity、non-dense fallback
和 `PointXYZI` XYZ-compatible 路径与显式 all-indices 标量路径一致。

上游测试：

```text
make -C test-rvv/filters/frustum_culling run_upstream_test_compare
```

结果：std 与 RVV 两套 `test/filters/test_filters.cpp` 均通过 `FrustumCulling.Filters`。运行参数使用仓库已有 `test/bun0.pcd` 和 `test/milk_cartoon_all_small_clorox.pcd`。

QEMU bench 和反汇编：

```text
make -C test-rvv/filters/frustum_culling run_bench_compare dump_bench_rvv
```

结果：

- `output/qemu/analyze_bench_compare.log` 可解析；
- 无 `未解析`、`n/a`、`Total Time 不计算`；
- std/RVV checksum 对齐；
- 反汇编在 `frustumCullingApplyFilterRVV<PointXYZ>` 中确认 `vlse32.v`、`vfmacc.vf`、`vmfle.vf`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。

## Bench Case 含义

所有 speedup 均由分析脚本按 `Std avg / RVV avg` 计算。QEMU 日志只用于构建、正确性补充、日志格式和指令路径证据；真实性能结论使用下方 Milkv-Jupiter 板卡日志。

| case | 函数入口与数据 | 路径 | 证明点 |
| --- | --- | --- | --- |
| `frustum_culling pointxyz full-cloud 64K` | `FrustumCulling<PointXYZ>::filter(Indices&)`，64K dense 全云，默认 ROI/FOV 配置 | RVV 主路径 | 中等规模 6 平面判定 + 保序压缩 |
| `frustum_culling pointxyz full-cloud 1M` | 同入口，1M dense 全云 | RVV 主路径 | 大规模主性能 case |
| `frustum_culling pointxyz negative removed 1M` | 同入口，1M dense，`negative=true`，`extract_removed_indices=true` | RVV 主路径 | 同一 VL chunk 同时生成 kept / removed 两路压缩输出 |
| `frustum_culling nondense fallback 1M` | 同入口，1M `PointXYZ`，`is_dense=false` | fallback | 证明未覆盖 non-dense 路径保持标量语义和接近成本 |
| `frustum_culling subset fallback 1M` | 同入口 + `setIndices(subset)`，1M 的一半 subset | fallback | 证明显式 indices/gather 路径未误入 RVV |
| `frustum_culling pointxyzi xyz-compatible 1M` | `FrustumCulling<PointXYZI>::filter(Indices&)`，1M `PointXYZI` | RVV 主路径 | 证明公共 traits 接入后额外字段点类型保持标量 checksum 一致 |

## 板卡结果

专项 Makefile 提供 `deploy_board`、`run_board_test`、`run_board_bench_compare`、`fetch_board_logs` 和 `board_smoke`。本轮已完成：

```text
make -C test-rvv/filters/frustum_culling run_board_test run_board_bench_compare fetch_board_logs
```

日志已拉回：

```text
test-rvv/filters/frustum_culling/output/board/run_test.log
test-rvv/filters/frustum_culling/output/board/run_bench_std.log
test-rvv/filters/frustum_culling/output/board/run_bench_rvv.log
test-rvv/filters/frustum_culling/output/board/analyze_bench_compare.log
```

板卡：`Milkv-Jupiter`。Dataset: synthetic `PointXYZ` clouds; full-cloud six-plane frustum RVV cases and subset/type fallback cases。Iterations: `30`。下表中的 `PointXYZ` 主路径数据仍是本主题性能证据；`PointXYZI fallback` 行来自公共 traits 接入前的历史 bench 命名，本轮类型放宽的正确性和日志格式证据以 QEMU `pointxyzi xyz-compatible` case 为准。

| case | Std ms/iter | RVV ms/iter | speedup | 说明 |
| --- | ---: | ---: | ---: | --- |
| `frustum_culling pointxyz full-cloud 64K` | 3.7764 | 0.6484 | 5.82x | 64K dense 全云主路径，证明中等规模收益 |
| `frustum_culling pointxyz full-cloud 1M` | 60.6842 | 10.2248 | 5.94x | 1M 主路径，证明 6 平面 dot + mask 合并 + 压缩输出收益 |
| `frustum_culling pointxyz negative removed 1M` | 66.1487 | 15.2663 | 4.33x | 双输出主路径，写 kept 和 removed 两路仍有收益 |
| `frustum_culling nondense fallback 1M` | 60.6530 | 61.0681 | 0.99x | non-dense fallback，不作为 RVV 主路径性能结论 |
| `frustum_culling subset fallback 1M` | 30.4931 | 30.6678 | 0.99x | subset fallback，不作为 RVV 主路径性能结论 |
| `frustum_culling pointxyzi fallback 1M` | 76.4250 | 76.3607 | 1.00x | 历史类型 fallback case；不作为本轮 `PointXYZI` RVV 主路径性能结论 |

结论：`FrustumCulling<PointXYZ>::applyFilter(Indices&)` dense 全云主路径在板卡上约
`4.33x` 到 `5.94x`。当前源码 gate 已扩大为 XYZ-compatible，全云 dense `PointXYZI`
正确性由 QEMU compare 覆盖；类型真实性能如需纳入结论，可后续单独重跑板卡。
