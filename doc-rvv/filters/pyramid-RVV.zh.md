# filters/pyramid RVV 优化说明

## 1. 被优化入口在 PCL 中的作用

`pcl::filters::Pyramid<PointT>` 构建 organized 点云的多尺度金字塔。公开入口 `compute(output)` 先复制输入到 `output[0]`，再逐层用平滑 kernel 对上一层下采样，生成宽高各减半的下一层。它属于 organized smoothing + subsampling 入口，输入是 organized `PointCloud<PointT>`，输出是多层 `PointCloud<PointT>`。

本主题优化的是 `Pyramid<PointXYZ>::compute` 的 dense small-kernel 主路径：每个输出点只依赖上一层 3x3 邻域的 `x/y/z`，适合按输出行做 VL chunk。RGB/RGBA/RGB 特化、non-dense 权重路径和 5x5 large-kernel 保持标量。

## 2. 上游标量路径与公开入口

源码位置：

- 声明：`filters/include/pcl/filters/pyramid.h`
- 模板实现：`filters/include/pcl/filters/impl/pyramid.hpp`
- RGB/RGBA/RGB 显式特化：`filters/src/pyramid.cpp`

标量 small-kernel 公式：

```text
next(c,r).xyz = sum_m sum_n previous(clamp(2*c+n-1), clamp(2*r+m-1)).xyz * k[2-m] * k[2-n]
k = [1/4, 1/2, 1/4]
```

`large_=true` 时使用 5x5 kernel；`is_dense=false` 时还要做 `isFinite`、中心点距离阈值和 weight 归一化。

## 3. RVV 覆盖范围与 fallback

| 条件 | 路径 |
| --- | --- |
| `__RVV10__`、`PointT=PointXYZ`、`input_->is_dense=true`、`large_=false`、`threads_<=1`、宽高足够 | RVV |
| 非 RVV 编译 | `computeStd` |
| 5x5 large-kernel | `computeStd` |
| non-dense threshold / weight | `computeStd` |
| 显式 `setNumberOfThreads(n>1)` | `computeStd` |
| `PointXYZI`、其它泛型点类型 | `computeStd` |
| `PointXYZRGB` / `PointXYZRGBA` / `RGB` 特化 | 原 `filters/src/pyramid.cpp` 标量特化 |

## 4. 详细设计

新增实体：

- `Pyramid<PointT>::computeStd`：常驻标量 helper，`initCompute()` 后执行原标量流程；
- `pyramidPointXYZDenseLevelRVV`：单层 dense `PointXYZ` small-kernel RVV helper；
- `pyramidPointXYZDenseRVV`：按 level 调用单层 helper，保持 `output[0]` 是输入副本；
- 公开 `compute`：初始化 kernel 后，按覆盖条件短路到 RVV，否则落回 `computeStd`。

VL chunk 组织：

- lane `t` 对应输出列 `c+t`；
- 输入列为 `2*(c+t) + (n-center)`，边界 lane 按标量 clamp；
- 输入是 AoS `PointXYZ`，且 stride-2 / clamp 会让每个 tap 的 lane 地址不完全连续，因此使用公共 `rvv_point_load` indexed load；
- 输出列连续，使用公共 `rvv_point_store` strided store 写回 `x/y/z`；
- 不使用显式舍入模式 intrinsic，不修改 FRM/FCSR。

核心片段：

```cpp
const vuint32m2_t v_index = __riscv_vadd_vx_u32m2 (vjj, ii * previous_width, vl);
const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<PointXYZ> (v_index, vl);
pcl::rvv_load::indexed_load3_f32m2<PointXYZ, kXOff, kYOff, kZOff> (previous_base, v_off, vl, px, py, pz);
vx = __riscv_vfmacc_vf_f32m2 (vx, k, px, vl);
vy = __riscv_vfmacc_vf_f32m2 (vy, k, py, vl);
vz = __riscv_vfmacc_vf_f32m2 (vz, k, pz, vl);
pcl::rvv_store::strided_store3_f32m2<kStride, kXOff, kYOff, kZOff> (next_base + out_offset, vl, vx, vy, vz);
```

## 5. 数值算例与 VL 图示

小 kernel `k=[1/4,1/2,1/4]`。设 `VL=4`，输出行 `r=1`、输出列 `c=0..3`：

```text
lane:        0      1      2      3
output c:    0      1      2      3
tap n=0:  clamp(-1)=0, 1, 3, 5
tap n=1:      0,     2, 4, 6
tap n=2:      1,     3, 5, 7
```

若只看一个 lane `c=1`，输出 `next(1,1).x` 为：

```text
sum_{m=0..2,n=0..2} previous(2+n-1, 2+m-1).x * k[2-m] * k[2-n]
```

这与标量在 `next.at(c,r)` 上的 3x3 flipped-kernel 累加一致；RVV 只是把多个 `c` 同时放入 lane。

## 6. 测试、QEMU、反汇编和板卡证据

专项测试：

- `test-rvv/filters/pyramid/test_pyramid.cpp` 覆盖 dense small-kernel RVV、large-kernel fallback、non-dense fallback、显式多线程 fallback、`PointXYZI` fallback 和 init failure；
- dense `PointXYZ` small/large case 与手写 scalar reference 对比。

验证日志：

- QEMU 测试：`test-rvv/filters/pyramid/output/qemu/run_test_std.log`、`run_test_rvv.log`；
- QEMU bench：`test-rvv/filters/pyramid/output/qemu/analyze_bench_compare.log`；
- 反汇编：`test-rvv/filters/pyramid/build/asm/riscv/bench_pyramid_rvv.full.asm`，确认 `vid.v`、`vfmacc.vf`、`vsetvli`；
- 板卡测试：`test-rvv/filters/pyramid/output/board/run_test.log`；
- 板卡 bench：`test-rvv/filters/pyramid/output/board/analyze_bench_compare.log`。

QEMU 结果只作为构建、checksum、日志格式和指令路径证据，不作为性能结论。QEMU small-kernel RVV 慢于标量，但 Milkv-Jupiter 板卡真实性能成立。

## 7. 性能结果说明

板卡：Milkv-Jupiter。Iterations: `30`。Speedup = `Std avg / RVV avg`。

| bench case | 入口 / 数据 | 路径含义 | 板卡结果 |
| --- | --- | --- | --- |
| `pyramid pointxyz dense 640x480 small-kernel` | `Pyramid<PointXYZ>::compute`，organized dense，3 levels，3x3 kernel | 命中 RVV 主路径，证明常见 VGA organized 下采样收益 | `57.2217 / 26.4954 = 2.16x` |
| `pyramid pointxyz dense 1280x720 small-kernel` | 同入口，HD 规模，3x3 kernel | 命中 RVV 主路径，证明更大行宽仍收益稳定 | `166.8621 / 78.2088 = 2.13x` |
| `pyramid pointxyz dense 640x480 large-kernel` | dense `PointXYZ`，5x5 kernel | fallback case，证明 5x5 未接 RVV 且保持语义，不作为 RVV 主路径性能结论 | `1.01x` |
| `pyramid pointxyz non-dense fallback 640x480` | non-dense `PointXYZ`，距离阈值/weight 语义 | fallback case，证明未覆盖路径保持语义，不作为 RVV 主路径性能结论 | `1.00x` |
| `pyramid pointxyzi fallback 640x480` | `PointXYZI` 泛型路径 | fallback case，证明非 `PointXYZ` 未误入 RVV | `0.99x` |

## 8. 结论

`Pyramid<PointXYZ>::compute` dense small-kernel 单线程主路径已接入生产 RVV，在 Milkv-Jupiter 上约 `2.13x`~`2.16x`。显式多线程、5x5 large-kernel、non-dense、RGB/RGBA/RGB 和非 `PointXYZ` 保持标量 fallback。
