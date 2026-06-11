# filters/convolution_3d RVV 诊断说明

## 1. 函数入口作用

`pcl::filters::Convolution3D<PointInT, PointOutT, KernelT>` 是 filters 模块的三维邻域卷积入口。用户设置输入点云、可选 search surface、search method、radius 和 kernel 后调用 `convolve(output)`。入口对每个 surface 点做：

```text
finite(query point)
-> radiusSearch(query, search_radius_, nn_indices, nn_distances)
-> kernel_(nn_indices, nn_distances)
-> output[point_idx] = kernel result
```

`GaussianKernel<PointInT, PointOutT>::operator()` 是默认参考 kernel。它读取 `radiusSearch` 返回的 neighbor ids 和 squared distances，对每个邻居执行：

```text
if dist2 <= threshold && isFinite(input[id]):
  weight = exp(-0.5 * dist2 / sigma_sqr)
  sum += weight * input[id]
  total_weight += weight
output = sum / total_weight
```

本主题是 bench 诊断，不改变公开 API，不修改上游生产源码，不添加默认生产 RVV 分流。

## 2. 覆盖范围与 fallback

| 项目 | 结论 |
| --- | --- |
| 诊断点类型 | `pcl::PointXYZ` |
| 诊断 kernel | `GaussianKernel<PointXYZ, PointXYZ>` 等价公式 |
| RVV 覆盖 | distance threshold、neighbor id load、AoS gather `x/y/z`、finite mask、common `expf_RVV_f32m2` 权重、weighted xyz 生成 |
| 标量保留 | `radiusSearch`、外层 query 循环、输出写回、最终累加顺序 |
| fallback | 非 RVV 编译、小邻域 `<16`、`indices.size()!=distances.size()`、非诊断生产入口 |
| FRM/FCSR | 不使用 `_rm` intrinsic，不修改 FRM/FCSR |

## 3. 详细设计

诊断实现位于 `test-rvv/filters/convolution_3d/convolution_3d_diag.hpp`。特殊实体如下：

| 实体 | 类型 | 输入 / 输出 | 标量语义与边界 |
| --- | --- | --- | --- |
| `KernelResult` | staging 结构 | `x/y/z/total_weight/accepted` | 对应 `GaussianKernel` 的加权和、总权重和 accepted 邻居数；不是公开 API |
| `gaussianKernelPointXYZStd` | 标量 helper | cloud + neighbor ids + distances -> `KernelResult` | 对拍基准，复刻 threshold、finite、`exp`、累加、归一化 |
| `gaussianKernelPointXYZRVV` | RVV 诊断 helper | 同上 | 只在 `__RVV10__ && PCL_CONVOLUTION_3D_RVV_DIAGNOSTIC` 下编译；失败返回 false |
| `convolveDiagnosticStd/RVV` | full diagnostic helper | cloud + query ids + radius + kernel 参数 -> output cloud | 复刻 `Convolution3D::convolve`，只替换 kernel 部分 |
| `convolveProductionUnchanged` | 生产观察 helper | cloud + radius + sigma -> output cloud | 调用未修改上游入口，用于证明本轮不改变生产分流 |

核心 RVV 片段：

```cpp
const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
const vuint32m2_t v_ids = __riscv_vle32_v_u32m2(raw_indices + offset, vl);
const vuint32m2_t v_point_offsets =
    pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZ>(v_ids, vl);
const vfloat32m2_t v_dist = __riscv_vle32_v_f32m2(distances.data() + offset, vl);
const vbool16_t m_threshold = __riscv_vmfle_vf_f32m2_b16(v_dist, threshold, vl);

// radiusSearch already produced neighbor ids; x/y/z therefore require AoS
// gather rather than contiguous loads.  Public point-load wrappers keep the
// byte-offset convention consistent with other PCL RVV topics.
const vfloat32m2_t vx =
    pcl::rvv_load::gather_load_f32m2<pcl::PointXYZ, offsetof(pcl::PointXYZ, x)>(
        base_u8, v_point_offsets, vl);
const vfloat32m2_t vy =
    pcl::rvv_load::gather_load_f32m2<pcl::PointXYZ, offsetof(pcl::PointXYZ, y)>(
        base_u8, v_point_offsets, vl);
const vfloat32m2_t vz =
    pcl::rvv_load::gather_load_f32m2<pcl::PointXYZ, offsetof(pcl::PointXYZ, z)>(
        base_u8, v_point_offsets, vl);
const vbool16_t m_finite = finite_xyz(vx, vy, vz);
const vbool16_t mask = __riscv_vmand_mm_b16(m_threshold, m_finite, vl);
const vfloat32m2_t v_weight =
    pcl::expf_RVV_f32m2(__riscv_vfmul_vf_f32m2(v_dist, -0.5f / sigma_sqr, vl), vl);

// Weighted values are staged from RVV lanes, then consumed in neighbor order.
// This isolates RVV gather/exp effects from a separate vector reduction order.
store(weights, v_weight);
store(wx, v_weight * vx);
store(wy, v_weight * vy);
store(wz, v_weight * vz);
for lane in 0..vl:
  if scalar condition for the same neighbor is true:
    sum += staged weighted xyz in lane order
```

没有使用 `vcompress`：`GaussianKernel` 不输出 neighbor ids，也不需要保序压缩列表。mask 只决定哪些 lane 参与后续标量顺序累加。

## 4. 数值算例与 VL 图示

设 `sigma_sqr=0.01`，`threshold=0.04`，一个 VL chunk 有 4 个邻居：

| lane | id | dist2 | point `(x,y,z)` | predicate | weight `exp(-0.5*dist2/sigma_sqr)` |
| ---: | ---: | ---: | --- | --- | ---: |
| 0 | 10 | 0.00 | `(1, 0, 0)` | keep | 1.0000 |
| 1 | 11 | 0.01 | `(0, 2, 0)` | keep | 0.6065 |
| 2 | 12 | 0.05 | `(9, 9, 9)` | skip: dist2 > threshold | - |
| 3 | 13 | 0.02 | `(0, 0, 3)` | keep | 0.3679 |

标量与 RVV chunk 都得到：

```text
sum_x = 1.0000*1 + 0.6065*0 + 0.3679*0 = 1.0000
sum_y = 1.0000*0 + 0.6065*2 + 0.3679*0 = 1.2130
sum_z = 1.0000*0 + 0.6065*0 + 0.3679*3 = 1.1037
total = 1.9744
output ~= (0.5065, 0.6144, 0.5590)
```

VL chunk 图示：

```text
neighbors:  [10] [11] [12] [13]
dist mask:   1    1    0    1
finite mask: 1    1    1    1
final mask:  1    1    0    1
staging:    wx0  wx1  wx2  wx3
consume:    lane0 -> lane1 -> skip lane2 -> lane3
```

## 5. 测试、反汇编与板卡证据

验证命令：

- `make -C test-rvv/filters/convolution_3d run_test_compare`
- `make -C test-rvv/filters/convolution_3d run_bench_compare`
- `make -C test-rvv/filters/convolution_3d dump_bench_rvv`
- `make -C test-rvv/filters/convolution_3d board_smoke`

证据日志：

- QEMU test：`test-rvv/filters/convolution_3d/output/qemu/run_test_std.log`、`run_test_rvv.log`
- QEMU bench compare：`test-rvv/filters/convolution_3d/output/qemu/analyze_bench_compare.log`
- 反汇编摘录：`test-rvv/filters/convolution_3d/output/qemu/rvv_asm_check.log`
- 板卡 test / bench：`test-rvv/filters/convolution_3d/output/board/`

反汇编摘录确认 `vsetvli`、`vle32.v`、`vluxei32.v`、`vmfle.vf`、`vcpop.m`、`vfmul.vf`、`vfmacc.vv`、`vse32.v` 等路径。

## 6. 性能结果与结论

Milkv-Jupiter 结果：

| case | 对应入口 / 证明点 | Std | RVV | speedup |
| --- | --- | ---: | ---: | ---: |
| `radiusSearch only 8K` | 只测搜索；不命中本 RVV kernel | 97.6161 | 96.3783 | 1.01x |
| `radiusSearch only 32K` | 大规模搜索成本 | 357.0300 | 353.8309 | 1.01x |
| `kernel distance-threshold 1024` | distance mask 局部片段 | 0.0018 | 0.0013 | 1.38x |
| `kernel gather-finite 1024` | neighbor gather + finite mask | 0.0295 | 0.0045 | 6.56x |
| `kernel exp-weight 1024` | Gaussian weight，复用 common expf | 0.0455 | 0.0071 | 6.41x |
| `kernel-only Gaussian 1024` | 完整 kernel microbench，不代表生产收益 | 0.0965 | 0.0522 | 1.85x |
| `full diag 8K` | `radiusSearch -> kernel -> output` | 130.6141 | 119.8917 | 1.09x |
| `full diag 32K` | 大规模 full diagnostic | 505.0634 | 453.1217 | 1.11x |
| `subset full diag 32K` | subset query full diagnostic | 279.3580 | 252.8701 | 1.10x |
| `finite full diag 32K` | invalid skip full diagnostic | 520.9435 | 467.8918 | 1.11x |
| `production unchanged 8K` | 未修改生产入口 | 110.5386 | 110.0236 | 1.00x |

speedup 按 `Std avg / RVV avg` 计算。full diagnostic 的 `1.09x` 到 `1.11x` 是弱收益；production unchanged 约 `1.00x` 说明本轮没有改变生产入口。结论是：`convolution_3d` 的 kernel 局部 RVV 可行，但真实 `convolve` 入口被 `radiusSearch` 和不规则邻域访问稀释，不接入生产分流，保留为 bench 诊断证据。
