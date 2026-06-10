# filters/fast_bilateral RVV 优化说明

## 背景与范围

`pcl::FastBilateralFilter<PointT>` 是 organized depth cloud 的快速 bilateral smoothing 入口。用户通过 `setSigmaS()` 配置空间窗口尺度，通过 `setSigmaR()` 配置 depth/range 尺度，然后调用 `filter(output)`；基类最终调用：

```text
FastBilateralFilter<PointT>::applyFilter(PointCloud &output)
```

该函数在 PCL 管线中承担“按像素位置和 depth 值构造 bilateral grid，再从 grid 插值回每个点的平滑 depth”的职责：

1. 检查输入必须 organized；
2. 复制输入到 `output`；
3. 扫描 finite `z` 得到 `base_min/base_max`；
4. 把 NaN/Inf `z` 替换为 `base_max`；
5. 将点 splat 到三维 `Array3D` lattice；
6. 沿 x/y/z 三个方向 blur；
7. 对每个点做 trilinear interpolation，并写回 `output(x,y).z`。

本轮 RVV 优化的是直接入口 `applyFilter` 的前置 `z` 预处理主路径：finite `z` min/max 规约，以及 non-finite `z` 替换。三维 grid splat、blur、trilinear interpolation 和最终写回保持标量顺序。

## 与上游差异

公开 API 不变，`fast_bilateral.h` 未新增公开接口。

`impl/fast_bilateral.hpp` 新增内部实体：

| 实体 | 作用 |
| --- | --- |
| `fastBilateralComputeBaseRangeStd` | 常驻标量 helper，保留原 finite `z` min/max 扫描语义 |
| `fastBilateralReplaceNonFiniteZStd` | 常驻标量 helper，保留原 non-finite `z -> base_max` 替换语义 |
| `kFastBilateralZMinPoints` | 小规模 fallback 阈值 |
| `FastBilateralScalar` | 去除 cv/ref 后判断字段类型 |
| `FastBilateralZCompatible` / `kFastBilateralZCompatible` | 判断 `PointT` 是否标准布局且 `z` 为 `float` |
| `fastBilateralComputeBaseRangeRVV` | `__RVV10__` 下的 finite `z` masked min/max 规约 |
| `fastBilateralReplaceNonFiniteZRVV` | `__RVV10__` 下的 non-finite `z` masked stride store |

主路径 helper 位于 `pcl` 命名空间，不额外放入 `pcl::detail`。traits 和阈值使用语义命名，不使用额外 RVV 后缀。

## 分流条件

RVV helper 只在以下条件全部满足时参与：

- `__RVV10__` 编译；
- `PointT` 是标准布局，且存在 `float z`；
- `applyFilter` 已确认输入 organized；
- 点数不少于 `64`。

以下路径保持标量：

- 非 RVV 编译；
- 小规模输入；
- 非 organized 输入；
- 非标准布局或 `z` 不是 `float` 的点类型；
- `Array3D` splat；
- 三维 blur；
- trilinear interpolation；
- `early_division_` 后续路径。

RVV helper 不要求 `input_->is_dense=true`。原标量逻辑只看 `z` 的 `std::isfinite`，即使 cloud 标记为 non-dense，也会忽略 NaN/Inf `z` 并用 `base_max` 替换；RVV 用同一 finite mask 表达该语义。

## 实现设计

公开入口保持短路选择：

```cpp
#if defined(__RVV10__)
  if constexpr (pcl::kFastBilateralZCompatible<PointT>)
  {
    if (!pcl::fastBilateralComputeBaseRangeRVV<PointT> (output, base_min, base_max, found_finite))
      found_finite = pcl::fastBilateralComputeBaseRangeStd<PointT> (output, base_min, base_max);
  }
  else
  {
    found_finite = pcl::fastBilateralComputeBaseRangeStd<PointT> (output, base_min, base_max);
  }
#else
  found_finite = pcl::fastBilateralComputeBaseRangeStd<PointT> (output, base_min, base_max);
#endif
```

RVV min/max helper 的核心组织：

```cpp
while (i < n)
{
  const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
  const auto* z_ptr = reinterpret_cast<const float*> (base + i * sizeof (PointT) + offsetof (PointT, z));
  const vfloat32m2_t vz = __riscv_vlse32_v_f32m2 (z_ptr, stride, vl);

  vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16 (vz, vz, vl);
  finite = __riscv_vmand_mm_b16 (
      finite,
      __riscv_vmflt_vf_f32m2_b16 (
          __riscv_vfabs_v_f32m2 (vz, vl), std::numeric_limits<float>::infinity (), vl),
      vl);

  finite_count += __riscv_vcpop_m_b16 (finite, vl);
  min_z = __riscv_vfmv_f_s_f32m1_f32 (
      __riscv_vfredmin_vs_f32m2_f32m1_m (finite, vz, __riscv_vfmv_s_f_f32m1 (min_z, 1), vl));
  max_z = __riscv_vfmv_f_s_f32m1_f32 (
      __riscv_vfredmax_vs_f32m2_f32m1_m (finite, vz, __riscv_vfmv_s_f_f32m1 (max_z, 1), vl));
}
```

维护重点：

- AoS 点云只能通过 `sizeof(PointT)` stride load 读取 `z`；
- finite mask 使用 `z == z` 排除 NaN，使用 `abs(z) < inf` 排除 `+Inf/-Inf`；
- masked reduction 只让 finite lane 参与；
- 全部 lane 都非 finite 时通过 `finite_count == 0` 返回 `found_finite=false`，对应原标量 warning / return；
- 本主题没有使用 `_rm` `vfcvt` 或修改 FRM/FCSR，不涉及浮点舍入环境保存恢复。

masked replace helper 只写无效 `z`：

```cpp
const vbool16_t replace = __riscv_vmnot_m_b16 (finite, vl);
const vfloat32m2_t vmax = __riscv_vfmv_v_f_f32m2 (base_max, vl);
__riscv_vsse32_v_f32m2_m (replace, z_ptr, stride, vmax, vl);
```

该 store 不触碰 `x/y` 或其它字段，保持 `copyPointCloud` 后的非 z 字段语义。

## 暂缓项

`Array3D` splat 暂缓：每个点根据 `(small_x, small_y, small_z)` 写 lattice cell，多个 lane 可能落到同一 cell，需要冲突累加、原子或分桶策略；直接 scatter 会改变累加顺序和数值语义。

三维 blur 已尝试后回退：内层 z 方向连续，理论上可同时处理 `Eigen::Vector2f` 的 sum/count 两个通道。本轮曾实现按 `sizeof(Eigen::Vector2f)` stride 的双通道 `vlse32/vsse32` helper，并用维度相关 `off` 访问邻居。正确性、checksum 和上游定向测试均通过；生产主路径接入后，Milkv-Jupiter 上 `160x120 finite` 只约 `1.06x`，低于只做 z 预处理时约 `1.09x`。

专项 bench 中保留了一个不接入生产代码的 `LatticeCell {sum,count}` blur microbench，直接观察 `data/buffer` 形态。该实验在 `96x72x64` lattice 上命中 `vlse32.v`、`vsse32.v`、`vfmacc.vf` 和 `vsetvli e32,m2`，但 Milkv-Jupiter 上为 Std `65.8100` ms/iter、RVV `69.4940` ms/iter，即 `0.95x`。结论是真实硬件上当前 stride load/store、双通道访存和三方向通用 helper 开销抵消算术收益，当前实现已回退，生产路径保持标量 blur。后续若继续做，应先单独 micro-bench z 方向专门化、segment load/store、x/y 方向策略或改变 lattice 存储布局。

trilinear interpolation 暂缓：每个输出点读取 8 个 lattice cell，访存为 gather，且 alpha 和除权重数值顺序敏感；本轮保持标量。

`early_division_` 后续路径保持标量：该配置影响 lattice 内除权重时机，本轮 RVV 只改变前置 `z` 预处理，不改变插值公式。

## VL chunk 数值算例

设 `base_max` 尚未计算，某个 VL chunk 从 organized cloud 的线性点 `c` 开始，VL=4：

| lane | `cloud[c+lane].z` | `z == z` | `abs(z) < inf` | finite | min/max 参与 |
| --- | ---: | --- | --- | --- | --- |
| 0 | 0.80 | true | true | true | yes |
| 1 | NaN | false | false | false | no |
| 2 | +Inf | true | false | false | no |
| 3 | 1.10 | true | true | true | yes |

标量公式：

```text
if (std::isfinite(pt.z)) {
  base_min = min(base_min, pt.z)
  base_max = max(base_max, pt.z)
}
```

该 chunk 的 RVV masked reduction 等价于只对 lane 0 和 lane 3 规约：

```text
base_min chunk = min(0.80, 1.10) = 0.80
base_max chunk = max(0.80, 1.10) = 1.10
```

替换阶段：

| lane | 原 `z` | replace mask | 写回 `z` |
| --- | ---: | --- | ---: |
| 0 | 0.80 | false | 0.80 |
| 1 | NaN | true | 1.10 |
| 2 | +Inf | true | 1.10 |
| 3 | 1.10 | false | 1.10 |

图示：

```text
cloud(c, r) linear chunk:
  p[c]      p[c+1]    p[c+2]    p[c+3]
  x y z ... x y z ... x y z ... x y z ...

RVV:
  vlse32(z) -> finite mask -> masked min/max
  vlse32(z) -> !finite mask -> vsse32(base_max)

Scalar continuation:
  splat output(x,y).z into Array3D
  blur lattice
  trilinear_interpolation -> output(x,y).z
```

坐标上，`FastBilateralFilter` 的二维位置来自 organized cloud 的 `output(x,y)`，其中 `x` 是 column，`y` 是 row；RVV chunk 只是按底层线性存储连续处理若干点，不改变 `output(x,y)` 到 lattice 的映射。

## 测试与验证

专项测试：

```text
make -C test-rvv/filters/fast_bilateral run_test_compare
```

结果：std 与 RVV 二进制均通过 4 个用例，覆盖 large finite 主路径、large non-finite 替换路径、小规模 fallback 和非 organized 返回。

上游定向测试：

```text
make -C test-rvv/filters/fast_bilateral run_upstream_test_compare
```

`UPSTREAM_TEST_ARGS` 使用仓库现有数据：

```text
test/milk_cartoon_all_small_clorox.pcd --gtest_filter=FastBilateralFilter.Filters_Bilateral
```

结果：std 与 RVV 两套 `FastBilateralFilter.Filters_Bilateral` 均通过。

QEMU bench 和反汇编：

```text
make -C test-rvv/filters/fast_bilateral run_bench_compare
make -C test-rvv/filters/fast_bilateral dump_bench_rvv
```

结果：

- `output/qemu/analyze_bench_compare.log` 可解析；
- 无 `未解析`、`n/a`、`Total Time 不计算`；
- std/RVV checksum 一致；
- 反汇编确认 `vlse32.v`、`vsse32.v`、`vmfeq.vv`、`vmflt.vf`、`vfredmin.vs`、`vfredmax.vs`、`vsetvli`。

QEMU 只用于构建、正确性补充、日志格式和指令路径证据，不作为性能结论。

## bench case 含义

所有 speedup 均由分析脚本按 `Std avg / RVV avg` 计算。真实性能结论使用下方 Milkv-Jupiter 板卡日志。

| case | 函数入口与数据 | 路径 | 证明点 |
| --- | --- | --- | --- |
| `fast_bilateral organized 64x48 finite` | `filter(output)`，64x48 organized `PointXYZ`，finite z，`sigma_s=4`、`sigma_r=0.05` | RVV 主路径 | 证明中等 organized depth 图的 finite z min/max RVV 路径正确 |
| `fast_bilateral organized 160x120 finite` | `filter(output)`，160x120 organized `PointXYZ`，finite z，`sigma_s=5`、`sigma_r=0.05` | RVV 主路径 | 证明较大图像式输入中前置 z 规约可受益 |
| `fast_bilateral organized 160x120 nonfinite` | 同上，但注入 NaN/+Inf/-Inf z | RVV 主路径 | 证明 finite mask 与 non-finite z mask store 替换语义 |
| `fast_bilateral organized 320x240 finite` | `filter(output)`，320x240 organized `PointXYZ`，finite z，`sigma_s=6`、`sigma_r=0.05` | RVV 主路径 | 放大数据规模后观察整体收益上限，证明未改写 lattice 主成本时 speedup 被稀释 |
| `fast_bilateral organized 320x240 nonfinite` | 同上，但注入 NaN/+Inf/-Inf z | RVV 主路径 | 证明大规模 non-finite 替换路径正确，并观察后续标量成本占比 |
| `fast_bilateral small fallback 7x5` | 7x5 organized 小输入，含 non-finite z | fallback | 证明小规模路径保持标量语义和接近成本，不作为 RVV 主路径性能结论 |
| `fast_bilateral blur lattice 96x72x64` | bench-diagnosis `LatticeCell {sum,count}` 三维 blur，模拟 `Array3D data/buffer` 的两通道 cell | RVV 实验路径，不接入生产 `applyFilter` | 直接测试 data/buffer blur 是否值得 RVV 化；用于决策，不作为生产主路径性能结论 |

## 板卡结果

板卡 bench 已运行并拉回：

```text
make -C test-rvv/filters/fast_bilateral run_board_test run_board_bench_compare fetch_board_logs
```

本地日志：

- `test-rvv/filters/fast_bilateral/output/board/run_test.log`
- `test-rvv/filters/fast_bilateral/output/board/run_bench_std.log`
- `test-rvv/filters/fast_bilateral/output/board/run_bench_rvv.log`
- `test-rvv/filters/fast_bilateral/output/board/analyze_bench_compare.log`

结果环境：

- Device: `Milkv-Jupiter`
- Dataset: synthetic organized PointXYZ depth images; finite and non-finite z cases
- Iterations: 8

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `fast_bilateral organized 64x48 finite` | 0.7972 | 0.7219 | 1.10x | 命中 finite z min/max RVV 主路径；后续 lattice 标量 |
| `fast_bilateral organized 160x120 finite` | 5.6804 | 5.3436 | 1.06x | 较大 organized finite 主路径受益，但主要成本仍在标量 lattice |
| `fast_bilateral organized 160x120 nonfinite` | 5.2061 | 4.8708 | 1.07x | 命中 min/max RVV 与 non-finite z mask store 替换 |
| `fast_bilateral organized 320x240 finite` | 32.1309 | 30.9764 | 1.04x | 放大输入后仍只优化前置 z 预处理，整体收益被 lattice 主成本稀释 |
| `fast_bilateral organized 320x240 nonfinite` | 30.6193 | 29.4964 | 1.04x | 大规模 non-finite 替换正确，整体收益同样受后续标量路径限制 |
| `fast_bilateral small fallback 7x5` | 0.0203 | 0.0305 | 0.67x | fallback 语义 / 成本证据，不作为 RVV 主路径性能结论 |
| `fast_bilateral blur lattice 96x72x64` | 65.8100 | 69.4940 | 0.95x | bench-diagnosis data/buffer blur 实验；命中 RVV stride 双通道路径但未加速，不作为生产主路径结论 |

结论：本轮最终保留的 RVV 覆盖是 `FastBilateralFilter` 的低风险 z 预处理，因此整体 filter speedup 为约 `1.04x` 到 `1.10x`。输入放大到 320x240 后，前置 z 预处理占比下降，speedup 更接近 `1.04x`；主要 lattice splat、blur 和 interpolation 仍是标量主成本，决定了收益上限。blur RVV 已尝试且 bench-diagnosis microbench 已复核，但当前 stride 双通道方案在板卡上没有收益，生产实现保持回退。
