# filters/fast_bilateral_omp 函数级 RVV 评估

## 范围

- 主题：`fast_bilateral_omp`
- 主要源码：
  - `filters/include/pcl/filters/fast_bilateral_omp.h`
  - `filters/include/pcl/filters/impl/fast_bilateral_omp.hpp`
  - 复用 `filters/include/pcl/filters/impl/fast_bilateral.hpp` 中已完成的 z 预处理 helper
- 专项目录：`test-rvv/filters/fast_bilateral_omp/`
- 当前结论：已在 `FastBilateralFilterOMP<PointT>::applyFilter(PointCloud&)` 中接入 finite `z` 的 `base_min/base_max` RVV 规约，以及 NaN/Inf `z` 替换为 `base_max` 的 RVV mask store。后续 lattice splat、三维 blur、trilinear interpolation 和 OMP 分块结构保持原实现。

## 函数族评估

| 函数族 | 当前状态 | RVV 适配度 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `FastBilateralFilterOMP<PointT>::applyFilter(PointCloud&)` 的 finite `z` min/max 规约 | 已实现 RVV | 中。OMP 版本先复制 organized 点云到 `output`，再对 AoS `z` 字段做线性扫描；可复用非 OMP 的 `vlse32` + finite mask + `vfredmin/vfredmax` | 覆盖标准布局、`float z`、点数不少于 64 的 organized 输入；小规模、非标准布局、非 float `z` 回退 Std |
| `applyFilter` 的 non-finite `z` 替换 | 已实现 RVV | 中。替换逻辑与线程无关，RVV mask store 能表达 `!isfinite(z)` | 覆盖同上；RVV helper 内部小规模回退失败时落回原 OMP parallel for |
| OMP lattice splat | 暂缓 | 低。每个小格子会聚合多个 `(x,y)` 输入，存在冲突累加和线程共享 `data` 的语义风险 | 保持原 OpenMP 标量循环，避免引入原子、改变累加顺序或扩大 race 风险 |
| OMP 三维 blur `data/buffer` | 暂缓 | 中但不接入。非 OMP 主题中 bench-only RVV blur 已在 Milkv-Jupiter 上验证为 `0.95x`，说明当前 stride 双通道方案不值得直接复用 | 保持原 OMP 标量 blur；后续若重做，应先改变 lattice 存储或只专门化 z 方向 |
| OMP interpolation 写回 | 暂缓 | 低到中。每点读取 8 个 lattice cell，gather 与插值数值边界复杂 | 保持标量，避免扩大验证范围 |

## 输入输出边界

- 输入：`pcl::PointCloud<PointT>`，必须 organized；非 organized 时原实现报错并返回。
- 输出：`PointCloud &output`，先由 `copyPointCloud(*input_, output)` 复制，然后只平滑 `z`；`x/y` 和其它字段保留输入副本。
- 配置项：
  - `sigma_s_` 控制 x/y 空间 lattice 尺寸；
  - `sigma_r_` 控制 z/range 方向 bin；
  - `threads_` 控制 OpenMP parallel for 的线程数，RVV z 预处理本身不创建线程；
  - `early_division_` 仍按原标量路径处理。

## 数据布局与 RVV 覆盖条件

`PointCloud<PointT>` 是 AoS。RVV helper 只按 `offsetof(PointT, z)` 与 `sizeof(PointT)` stride 访问 depth 字段：

```text
chunk base -> p[c]      p[c+1]    p[c+2] ...
             x y z ...  x y z ... x y z ...
RVV load:          vlse32(z, sizeof(PointT))
finite mask:       z == z && abs(z) < inf
range reduce:      masked vfredmin/vfredmax
replace invalid:   masked vsse32(base_max)
```

覆盖条件：

- `__RVV10__` 编译；
- `PointT` 为标准布局，且存在 `float z`；
- organized 输入已通过原入口检查；
- 点数不少于 `64`。

RVV helper 不要求 `input_->is_dense=true`。原标量路径会忽略 NaN/Inf `z` 并在找到 finite range 后把 non-finite `z` 替换为 `base_max`；RVV finite mask 与 mask store 保持同一语义。

## 回退条件

- 非 RVV 编译；
- 点数小于 `64`；
- `PointT` 无 `z` 字段，或 `z` 不是 `float`；
- 非标准布局；
- 非 organized 输入；
- lattice splat、blur、interpolation、`early_division_` 后续路径。

## 风险点与处理

- OMP 与 RVV 的边界：RVV 只替换 `applyFilter` 中线程并行段之前的 z 预处理。后续 OpenMP 循环继续使用原 `threads_`，避免在同一循环里混用 RVV chunk 与 OpenMP 分块。
- `std::isfinite` 语义：RVV mask 使用 `z == z` 排除 NaN，并用 `abs(z) < inf` 排除正负无穷。
- 全部 z 非 finite：`fastBilateralComputeBaseRangeRVV` 显式返回 `found_finite=false`，入口仍输出原 warning 并返回。
- 舍入环境：本轮 RVV helper 没有使用 `_rm` `vfcvt`，也不修改 FRM/FCSR。反汇编中出现的 `frrm/fsrm/vfcvt` 来自 bench checksum 的 `std::lround`，不在生产 helper 路径；专项测试包含“先命中 RVV 主路径，再运行单线程小规模 fallback”的同进程序列，回归确认没有状态污染现象。
- QEMU bench 下 RVV 版本较慢，但 QEMU 只作为构建、正确性、日志格式和指令路径证据；性能结论以板卡结果为准。

## 专项测试

专项测试文件：`test-rvv/filters/fast_bilateral_omp/test_fast_bilateral_omp.cpp`。

覆盖：

- 大规模 organized finite `PointXYZ`，命中 min/max RVV 主路径；
- 大规模 organized 含 NaN/Inf `z`，命中 RVV 替换路径；
- 小规模 organized fallback；
- 同一进程先运行 RVV 主路径，再运行单线程小规模 fallback；
- 非 organized 输入保持原错误返回语义。

已运行：

```text
make -C test-rvv/filters/fast_bilateral_omp run_test_compare
```

结果：std 与 RVV 二进制均通过 5 个专项用例。

## bench、QEMU 与反汇编

专项 bench 文件：`test-rvv/filters/fast_bilateral_omp/bench_fast_bilateral_omp.cpp`。

bench 输出包含 `Dataset:`、`Iterations:`、`Build:`，每个 case 输出 `<name> : <avg> ms/iter` 和 `Total Time` / checksum / 参数详情。

已运行：

```text
make -C test-rvv/filters/fast_bilateral_omp run_bench_compare
make -C test-rvv/filters/fast_bilateral_omp dump_bench_rvv
```

`output/qemu/analyze_bench_compare.log` 已解析 Dataset / Iterations / Total Time，无 `未解析`、`n/a` 或 `Total Time 不计算`。QEMU 日志中 std/RVV checksum 一致。

反汇编文件：

- `test-rvv/filters/fast_bilateral_omp/build/asm/riscv/bench_fast_bilateral_omp_rvv.full.asm`

已确认关键指令路径：

- `vlse32.v`
- `vsse32.v`
- `vmfeq.vv`
- `vmflt.vf`
- `vfredmin.vs`
- `vfredmax.vs`
- `vsetvli ... e32,m2`

## 上游测试与板卡结果

上游定向测试入口：

```text
make -C test-rvv/filters/fast_bilateral_omp run_upstream_test_compare
```

`UPSTREAM_TEST_ARGS` 默认复用仓库现有数据：

```text
test/milk_cartoon_all_small_clorox.pcd --gtest_filter=FastBilateralFilterOMP.Filters_Bilateral
```

结果：std 与 RVV 两套 `FastBilateralFilterOMP.Filters_Bilateral` 均通过。

板卡入口已在 Makefile / `board.mk` 中提供：

- `deploy_board`
- `run_board_test`
- `run_board_bench_compare`
- `fetch_board_logs`

板卡 bench 已运行并拉回：

```text
make -C test-rvv/filters/fast_bilateral_omp run_board_test run_board_bench_compare fetch_board_logs
```

本地日志：

- `test-rvv/filters/fast_bilateral_omp/output/board/run_test.log`
- `test-rvv/filters/fast_bilateral_omp/output/board/run_bench_std.log`
- `test-rvv/filters/fast_bilateral_omp/output/board/run_bench_rvv.log`
- `test-rvv/filters/fast_bilateral_omp/output/board/analyze_bench_compare.log`

结果环境：

- Device: `Milkv-Jupiter`
- Dataset: synthetic organized PointXYZ depth images; OMP filter with finite and non-finite z cases
- Iterations: 8

真实性能结论：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `fast_bilateral_omp organized 64x48 finite t2` | 0.6381 | 0.5492 | 1.16x | 命中 finite z min/max RVV 主路径；后续 lattice 为 OMP 标量 |
| `fast_bilateral_omp organized 160x120 finite t2` | 3.8876 | 3.5095 | 1.11x | 中等 organized finite 输入受益；z 预处理减少串行前置成本 |
| `fast_bilateral_omp organized 160x120 nonfinite t2` | 3.4481 | 3.1834 | 1.08x | 命中 min/max RVV 与 non-finite z mask store 替换 |
| `fast_bilateral_omp organized 320x240 finite t2` | 16.8088 | 15.9722 | 1.05x | 放大输入后整体成本更多来自 OMP lattice，收益被稀释 |
| `fast_bilateral_omp organized 320x240 nonfinite t2` | 15.2535 | 14.4742 | 1.05x | 大规模 non-finite 替换正确，主路径仍有小幅收益 |
| `fast_bilateral_omp small fallback 7x5 t1` | 0.0441 | 0.0435 | 1.01x | 小规模 fallback 语义 / 成本证据，不作为 RVV 主路径性能结论 |

speedup 计算方式为 `Std Avg / RVV Avg`。主路径收益范围为 `1.05x` 到 `1.16x`；这与实现只覆盖 OMP 入口前置 z 预处理、未改写后续 OpenMP lattice 主成本的范围一致。
