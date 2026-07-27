# filters/fast_bilateral_omp RVV 优化说明

## 背景与范围

`pcl::FastBilateralFilterOMP<PointT>` 是 `FastBilateralFilter<PointT>` 的 OpenMP 版本，用于 organized depth point cloud 的快速双边滤波。公开入口是 `filter(output)`，最终调用 `FastBilateralFilterOMP<PointT>::applyFilter(PointCloud&)`：

- 输入是 organized `PointCloud<PointT>`；
- 输出先复制输入点云，再平滑每个点的 `z`；
- `x/y` 作为图像式位置参与 lattice 坐标计算，其它字段保持复制语义；
- `sigma_s_` 控制 x/y 空间窗口，`sigma_r_` 控制 z/range 方向窗口；
- `threads_` 控制 OpenMP parallel for 的线程数。

本轮 RVV 优化的是 `applyFilter` 中线程并行 lattice 之前的 z 预处理：

1. 从 finite `z` 中求 `base_min/base_max`；
2. 把 NaN/Inf `z` 替换为 `base_max`。

三维 lattice splat、blur、trilinear interpolation 和最终写回保持原 OpenMP 标量实现。非 OMP `fast_bilateral` 主题已经证明当前 stride 双通道 blur RVV microbench 在 Milkv-Jupiter 上为 `0.95x`，因此 OMP 版本不把 blur RVV 接入生产路径。

## 与上游实现的差异

公开 API 不变。源码结构为：

- `filters/include/pcl/filters/impl/fast_bilateral.hpp` 常驻 `fastBilateralComputeBaseRangeStd` / `fastBilateralReplaceNonFiniteZStd`，并在 `__RVV10__` 下提供 `fastBilateralComputeBaseRangeRVV` / `fastBilateralReplaceNonFiniteZRVV`；
- `filters/include/pcl/filters/impl/fast_bilateral_omp.hpp` 显式包含非 OMP impl helper；
- OMP 入口在 `__RVV10__` 下对 `PointT` 做 `float z` 和标准布局判断，命中后短路调用 RVV helper；未覆盖时落回 Std 或原 OMP parallel for。

核心入口片段：

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

替换 non-finite `z` 时，RVV helper 覆盖成功则不进入 OpenMP loop；小规模或不兼容类型仍保持原 OMP parallel for。

## 分流条件

RVV 主路径覆盖：

- `__RVV10__` 编译；
- `PointT` 是标准布局；
- `PointT::z` 存在且为 `float`；
- organized 输入；
- 点数不少于 `64`。

回退路径：

- 非 RVV 编译；
- 小规模输入；
- 非标准布局或非 float `z` 点类型；
- 非 organized 输入；
- lattice splat、blur、interpolation、`early_division_`。

该路径不依赖 `input_->is_dense`。dense 和 non-dense 的差异由 finite mask 表达：finite z 参与 min/max，NaN/Inf z 被排除并在后续替换为 `base_max`。

## 数据访问与 VL chunk

PCL 点云是 AoS：

```text
cloud(c, r)      -> x y z ...
cloud(c+1, r)    -> x y z ...
cloud(c+2, r)    -> x y z ...
```

RVV 不能把 `z` 当成连续数组读取，因此通过公共单字段 load/store helper 表达 `sizeof(PointT)` stride 访问：

```text
VL chunk 起点: cloud(c, r)
z_ptr       : &cloud(c, r).z
stride      : sizeof(PointT)

strided_load_f32m2<sizeof(PointT)>(z_ptr)             -> [z(c,r), z(c+1,r), ...]
mask finite                                           -> z == z && abs(z) < inf
vfredmin/vfredmax                                     -> 更新 base_min/base_max
masked_strided_store_f32m2<sizeof(PointT)>(!finite)   -> 只把 invalid z 写成 base_max
```

mask 的作用是保持标量公式：

```text
for point in output:
  if isfinite(point.z):
    base_min = min(base_min, point.z)
    base_max = max(base_max, point.z)

for point in output:
  if !isfinite(point.z):
    point.z = base_max
```

OMP 线程边界保持清晰：RVV z 预处理发生在后续 OpenMP lattice loop 之前，不在同一循环内混用线程分块和 RVV chunk。

## 数值算例

假设一个 organized 点云某行有 6 个点，`PointT=PointXYZ`，一个 VL chunk 覆盖前 4 个 `z`：

| 位置 | `cloud(c,r).z` | finite mask |
| --- | ---: | --- |
| `cloud(0,r)` | 1.2 | true |
| `cloud(1,r)` | NaN | false |
| `cloud(2,r)` | 0.8 | true |
| `cloud(3,r)` | +Inf | false |

标量规约只看 finite 值，得到 `chunk_min=0.8`、`chunk_max=1.2`。RVV 中：

```text
vz       = [1.2, NaN, 0.8, +Inf]
finite   = [T, F, T, F]
vfredmin = 0.8
vfredmax = 1.2
```

若全图最终 `base_max=2.0`，替换阶段同一 chunk 的 mask store 只写 false lane：

```text
before = [1.2, NaN, 0.8, +Inf]
after  = [1.2, 2.0, 0.8, 2.0]
```

这与标量 `if (!std::isfinite(point.z)) point.z = base_max` 完全对应，且不触碰 `x/y`。

## 反汇编与诊断

反汇编文件：

- `test-rvv/filters/fast_bilateral_omp/build/asm/riscv/bench_fast_bilateral_omp_rvv.full.asm`

已确认生产 RVV 路径包含：

- `vlse32.v`
- `vsse32.v`
- `vmfeq.vv`
- `vmflt.vf`
- `vfredmin.vs`
- `vfredmax.vs`
- `vsetvli ... e32,m2`

反汇编中还出现 `frrm/fsrm/vfcvt`，定位为 bench checksum 中 `std::lround` 的转换路径，不属于 RVV helper。RVV helper 没有使用 `_rm` `vfcvt`，也没有直接修改 FRM/FCSR。专项测试覆盖“先命中 RVV 主路径，再运行 fallback/标量 case”的同进程序列，未发现状态污染。

## 测试与验证

专项目录：

- `test-rvv/filters/fast_bilateral_omp/`

专项测试：

```text
make -C test-rvv/filters/fast_bilateral_omp run_test_compare
```

结果：std 与 RVV 二进制均通过 5 个用例，覆盖大规模 finite、non-finite 替换、小规模 fallback、同进程主路径后 fallback、非 organized 返回。

上游定向测试：

```text
make -C test-rvv/filters/fast_bilateral_omp run_upstream_test_compare
```

默认参数：

```text
test/milk_cartoon_all_small_clorox.pcd --gtest_filter=FastBilateralFilterOMP.Filters_Bilateral
```

结果：std 与 RVV 两套 `FastBilateralFilterOMP.Filters_Bilateral` 均通过。

QEMU bench：

```text
make -C test-rvv/filters/fast_bilateral_omp run_bench_compare
```

`output/qemu/analyze_bench_compare.log` 可解析 Dataset / Iterations / Total Time，无 `未解析`、`n/a` 或 `Total Time 不计算`。QEMU 只作为构建、正确性、日志格式和指令路径证据，不作为性能结论。

## 板卡性能结果

板卡验证已完成并拉回：

```text
make -C test-rvv/filters/fast_bilateral_omp run_board_test run_board_bench_compare fetch_board_logs
```

本地日志：

- `test-rvv/filters/fast_bilateral_omp/output/board/run_test.log`
- `test-rvv/filters/fast_bilateral_omp/output/board/run_bench_std.log`
- `test-rvv/filters/fast_bilateral_omp/output/board/run_bench_rvv.log`
- `test-rvv/filters/fast_bilateral_omp/output/board/analyze_bench_compare.log`

环境与数据：

- Device: `Milkv-Jupiter`
- Dataset: synthetic organized PointXYZ depth images; OMP filter with finite and non-finite z cases
- Iterations: 8
- 线程：主路径 case 使用 `threads=2`；小规模 fallback case 使用 `threads=1`

speedup 计算方式为 `Std Avg / RVV Avg`。

| bench case | 入口与参数 | 路径含义 | Std ms/iter | RVV ms/iter | speedup | 证明点 |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `fast_bilateral_omp organized 64x48 finite t2` | `FastBilateralFilterOMP<PointXYZ>`，64x48 finite，`sigma_s=4`，`sigma_r=0.05`，2 线程 | 命中 finite z min/max RVV；后续 lattice 为 OMP 标量 | 0.6381 | 0.5492 | 1.16x | 小 organized 输入中串行 z 预处理占比更高，RVV 有明确收益 |
| `fast_bilateral_omp organized 160x120 finite t2` | 160x120 finite，`sigma_s=5`，2 线程 | 命中 min/max RVV | 3.8876 | 3.5095 | 1.11x | 中等规模主路径收益成立 |
| `fast_bilateral_omp organized 160x120 nonfinite t2` | 160x120，注入 NaN/Inf `z`，2 线程 | 命中 min/max RVV 与 mask store 替换 | 3.4481 | 3.1834 | 1.08x | non-dense 语义与 RVV 替换路径正确，且有收益 |
| `fast_bilateral_omp organized 320x240 finite t2` | 320x240 finite，`sigma_s=6`，2 线程 | 命中 min/max RVV；lattice 成本主导 | 16.8088 | 15.9722 | 1.05x | 放大输入后收益被后续 OMP 标量 lattice 稀释 |
| `fast_bilateral_omp organized 320x240 nonfinite t2` | 320x240，注入 NaN/Inf `z`，2 线程 | 命中 min/max RVV 与 mask store 替换 | 15.2535 | 14.4742 | 1.05x | 大规模 non-finite case 保持正确并有小幅收益 |
| `fast_bilateral_omp small fallback 7x5 t1` | 7x5 nonfinite，1 线程 | 小规模 fallback，不作为 RVV 主路径性能结论 | 0.0441 | 0.0435 | 1.01x | 证明未覆盖路径语义和成本接近 |

真实性能结论：生产主路径在 Milkv-Jupiter 上为 `1.05x` 到 `1.16x`。收益小于改写整个 lattice 的理想上限，但实现边界清晰、复用非 OMP helper、风险低，并保留 OpenMP 主体结构。

## 已知限制

- 当前只优化 `z` 预处理，不优化 splat、blur 和 interpolation。
- `threads_` 只影响后续 OpenMP loop；RVV z 预处理本身是单线程 strip-mined loop。
- 非标准布局、非 float `z` 或小规模输入保持回退。
- 若后续继续优化 OMP blur，需要先重新评估存储布局、线程分块和真实板卡收益，不能直接复用非 OMP 的 bench-diagnosis stride 双通道方案。
