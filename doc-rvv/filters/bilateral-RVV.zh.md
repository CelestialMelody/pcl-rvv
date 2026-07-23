# filters/bilateral RVV 说明

## 1. PCL 入口作用

`pcl::BilateralFilter<PointT>` 对带 intensity 的点云做 bilateral smoothing。用户调用 `filter(output)` 后，`applyFilter(PointCloud&)` 复制输入点云，遍历 `indices_`，对每个有效 center point 用 `radiusSearch(idx, sigma_s_ * 2)` 找邻域，再计算邻域 intensity 的 bilateral 加权平均并写回 `output[idx].intensity`。

配置项含义：

- `sigma_s_`：空间距离 Gaussian 的尺度，同时决定 radius search 半径 `2 * sigma_s_`；
- `sigma_r_`：intensity 差异 Gaussian 的尺度；
- `indices_`：决定哪些 center point 被写回；
- `input_->is_dense` / `isXYZFinite`：non-dense 点云中 invalid center 保持原输出值。

本主题优化的是 `BilateralFilter<PointT>::computePointWeight` 中满足 XYZ + float intensity
布局条件的生产主路径，公开 API 不变。当前上游预编译实例中覆盖 `PointXYZI` 和
`PointXYZINormal`。

## 2. 标量公式与 RVV 等价式

标量公式：

```text
dist = sqrt(k_distances[n])
spatial = exp(-(dist * dist) / (2 * sigma_s^2))
delta = input[pid].intensity - input[id].intensity
range = exp(-(delta * delta) / (2 * sigma_r^2))
weight = spatial * range
BF += weight * input[id].intensity
W += weight
output[pid].intensity = BF / W
```

`radiusSearch` 返回的 `k_distances` 是 squared distance。标量先 `sqrt(d2)`，`kernel()` 中又平方回 `d2`；RVV 主路径直接构造：

```text
spatial_arg   = -d2 / (2 * sigma_s^2)
intensity_arg = -(center_intensity - neighbor_intensity)^2 / (2 * sigma_r^2)
weight        = expf_RVV_f32m2(spatial_arg) * expf_RVV_f32m2(intensity_arg)
```

这保持同一数学参数，但指数函数从 double/libm `std::exp` 变为 common float 近似 `pcl::expf_RVV_f32m2`。因此 raw checksum 与标量不同，正确性以专项误差预算验证。

## 3. 生产实现

新增 helper 位于 `filters/include/pcl/filters/impl/bilateral.hpp`：

- `computePointWeightStd<PointT>`：常驻标量 helper，保留原公式；
- `computePointWeightRVV<PointT>`：`__RVV10__` 下的 RVV helper；
- `kBilateralXYZIntensityCompatible<PointT>`：本地 production gate，组合公共
  `kRVVXYZPointCompatible<PointT>`、`RVVXYZFloatLayout<PointT>`、
  `RVVFloatFieldLayout<PointT, pcl::fields::intensity>`、可写 `float intensity` 成员和
  traits intensity offset 对齐检查；
- `BilateralFilter<PointT>::computePointWeight`：满足本地 gate 时短路到 RVV，否则回退 Std。

覆盖条件：

```text
__RVV10__ &&
kBilateralXYZIntensityCompatible<PointT> &&
indices.size() >= 16 &&
indices.size() == distances.size()
```

fallback：

- 小邻域 `<16`：避免 vector setup 和 stack staging 成本；
- 邻域与距离长度不一致：保持标量安全路径；
- 不满足 XYZ + writable float intensity layout gate 的点型：保持泛型标量语义；
- 非 RVV 编译：只编译 Std。

`filters/src/bilateral.cpp` 同时预编译 `PointXYZI` 和 `PointXYZINormal`。从源码语义看，
`applyFilter` 先复制整点输出，再只写回 `output[idx].intensity`，normal 字段应保持输入值；
因此该类型可以使用同一个 RVV weight helper。专项测试新增 `PointXYZINormal` 生产对拍，
并检查 `normal_x/y/z` 与 `curvature` 保持输入值；bench 增加 `production normal`
入口。该扩展不是按名字匹配任意 `PointXYZIxxx`，而是按字段语义、成员可写性和 AoS
offset/alignment gate 判定。

生产 helper 使用 `pcl/common/rvv_point_load.h`：

```cpp
const vuint32m2_t v_ids = __riscv_vle32_v_u32m2(raw_indices + offset, vl);
const vuint32m2_t v_point_offsets =
    pcl::rvv_load::byte_offsets_u32m2<PointT> (v_ids, vl);
constexpr std::size_t kIntensityOff = pcl::traits::offset<PointT, pcl::fields::intensity>::value;
const vfloat32m2_t v_intensity =
    pcl::rvv_load::gather_load_f32m2<PointT, kIntensityOff> (
        base_u8, v_point_offsets, vl);
```

邻域 id 来自 `radiusSearch`，不是连续点数组，因此 intensity 必须是 AoS gather。RVV 不做 mask/filter/compress，因为 search 输出已经只包含有效邻居，lane 顺序就是邻域顺序。

权重计算复用 common math helper：

```cpp
const vfloat32m2_t v_spatial_arg = __riscv_vfmul_vf_f32m2 (v_squared, spatial_scale, vl);
const vfloat32m2_t v_delta2 = __riscv_vfmul_vv_f32m2 (v_delta, v_delta, vl);
const vfloat32m2_t v_intensity_arg = __riscv_vfmul_vf_f32m2 (v_delta2, intensity_scale, vl);
const vfloat32m2_t v_weight =
    __riscv_vfmul_vv_f32m2 (pcl::expf_RVV_f32m2 (v_spatial_arg, vl),
                            pcl::expf_RVV_f32m2 (v_intensity_arg, vl),
                            vl);
```

chunk 的 `weight` 和 `weight * intensity` 写回临时数组后按 lane 顺序累加到 double `BF/W`。这里刻意保留标量累加顺序：`radiusSearch` 已经给出邻域顺序，本主题接受的数值差异只来自 common float 近似 `expf_RVV_f32m2`，不再额外引入向量归约顺序差异。

生产 helper 对 GCC 使用局部 `no-tree-vectorize` 作用域：

```cpp
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("no-tree-vectorize")
#endif
template <typename PointT> double
computePointWeightRVV (...)
{
  ...
  for (std::size_t lane = 0; lane < vl; ++lane)
  {
    BF += static_cast<double> (contribs[lane]);
    W += static_cast<double> (weights[lane]);
  }
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
```

该 pragma 的目的不是关闭手写 RVV intrinsic，而是限制 GCC tree vectorizer 不把最后的标量累加循环改成自动向量 reduction。复核过程中曾在反汇编里观察到编译器自动生成归约指令，使 std/RVV 差异同时包含 `expf` 近似和归约顺序变化；加上局部 pragma 后，反汇编仍保留显式 `vle32.v`、`vluxei32.v`、`vfmul.vv`、common exp 路径中的 `vfcvt.*.v` / `vfmacc.vv` 和 `vse32.v`，但不再把生产 helper 的尾段累加作为自动向量归约证据。`!defined(__clang__)` 是为了避免把 GCC 专用优化 pragma 套到 Clang 编译路径。

这种约束属于实现语义边界：如果后续主题为了保持标量累加顺序、tie 选择、状态机更新顺序、浮点舍入环境或 checksum 归因而限制编译器自动优化，主题文档和函数级评估都必须说明限制范围、原因、是否影响手写 RVV 指令，以及反汇编如何确认预期路径。实现不使用显式 `_rm` intrinsic，不修改 FRM/FCSR。

## 4. 数值算例

center intensity `I_c=10`，两个邻居：

| lane | neighbor | `d2` | `I_n` | `delta` |
| ---: | --- | ---: | ---: | ---: |
| 0 | `n0` | 0.25 | 14 | -4 |
| 1 | `n1` | 1.00 | 18 | -8 |

`sigma_s=2`、`sigma_r=8`：

```text
spatial_arg0   = -0.25 / (2*2^2) = -0.03125
intensity_arg0 = -(4^2) / (2*8^2) = -0.125
w0             = exp(spatial_arg0) * exp(intensity_arg0)

spatial_arg1   = -1.00 / 8 = -0.125
intensity_arg1 = -(8^2) / 128 = -0.5
w1             = exp(spatial_arg1) * exp(intensity_arg1)

output = (w0*14 + w1*18) / (w0 + w1)
```

一个 VL chunk：

```text
lane:        0          1          2        ...
id:          n0         n1         n2
d2 load:     d2[n0]     d2[n1]     d2[n2]
gather I:    I[n0]      I[n1]      I[n2]
arg spatial: -d2/2s^2   -d2/2s^2   -d2/2s^2
arg range:   -di^2/2r^2 -di^2/2r^2 -di^2/2r^2
store:       weight/contribution arrays -> scalar BF/W in lane order
```

## 5. 验证

命令：

- `make -C test-rvv/filters/bilateral run_test_compare`
- `make -C test-rvv/filters/bilateral run_bench_compare dump_bench_rvv`
- `make -C test-rvv/filters/bilateral run_upstream_test_compare`
- `make -C test-rvv/filters/bilateral run_board_test run_board_bench_compare fetch_board_logs`

证据路径：

- `test-rvv/filters/bilateral/output/qemu/run_test_std.log`
- `test-rvv/filters/bilateral/output/qemu/run_test_rvv.log`
- `test-rvv/filters/bilateral/output/qemu/analyze_bench_compare.log`
- `test-rvv/filters/bilateral/output/qemu/run_upstream_test_std.log`
- `test-rvv/filters/bilateral/output/qemu/run_upstream_test_rvv.log`
- `test-rvv/filters/bilateral/output/qemu/rvv_asm_check.log`
- `test-rvv/filters/bilateral/output/board/run_test.log`
- `test-rvv/filters/bilateral/output/board/analyze_bench_compare.log`

反汇编确认：

- `BilateralFilter<PointXYZI>::applyFilter` 和 `BilateralFilter<PointXYZINormal>::applyFilter`
  可调用对应 `computePointWeightRVV<PointT>`；
- 生产 helper 中出现 `vsetvli`、`vle32.v`、`vluxei32.v`、`vfsub.vv`、`vfmul.vv`、`vse32.v`；
- common exp helper 路径出现 `vfcvt.x.f.v`、`vfcvt.f.x.v`、`vsll.vi`、`vfmacc.vv`、`vfmul.vv`。

## 6. 性能与误差

Milkv-Jupiter，`Iterations: 3`，speedup = `Std avg / RVV avg`：

| case | 入口 / 参数 | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | --- | ---: | ---: | ---: | --- |
| `bilateral full exp diag 256 radiusSearch` | full diagnostic，256 点 | 19.4169 | 11.2317 | 1.73x | 真实 search + RVV exp 诊断明显收益 |
| `bilateral full exp diag 1K radiusSearch` | full diagnostic，1K 点 | 126.6599 | 76.0978 | 1.66x | 收益覆盖主要权重成本 |
| `bilateral subset full exp diag 1K radiusSearch` | subset center | 63.8591 | 38.6635 | 1.65x | subset 仍成立 |
| `bilateral production filter 256` | 公开 `BilateralFilter<PointXYZI>` | 10.8279 | 3.1566 | 3.43x | 生产主路径收益明显 |
| `bilateral production filter 1K` | 公开 `BilateralFilter<PointXYZI>` | 91.8276 | 44.5187 | 2.06x | 生产主路径收益明显 |

QEMU 最新日志另包含 `PointXYZINormal` production case，用于证明新增覆盖范围可编译、
可运行并维持同一误差预算；QEMU 时间只作为日志/路径证据，不作为板卡性能结论。

误差预算：

- 常规 production case：max abs `3.814697e-06`，max rel 约 `1.19e-07`，RMSE 约 `1.1e-06`；
- 高对比 `ProductionErrorCase`：三组 `sigma_s/sigma_r` 的 max abs `1.525879e-05`，max rel 约 `1.0e-07`，RMSE 最大约 `2.10e-06`；
- 专项预算为 `max_abs <= 8e-4`、`max_rel <= 5e-5`、`rmse <= 2e-4`。

结论：`PointXYZI` 生产主路径收益成立，`PointXYZINormal` 覆盖范围已通过专项 QEMU
生产对拍和 extra-field 保留测试。非覆盖类型、小邻域和非 RVV 构建保持标量 fallback。
