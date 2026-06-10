# filters/fast_bilateral 函数级 RVV 评估

## 范围

- 主题：`fast_bilateral`
- 主要源码：
  - `filters/include/pcl/filters/fast_bilateral.h`
  - `filters/include/pcl/filters/impl/fast_bilateral.hpp`
- 专项目录：`test-rvv/filters/fast_bilateral/`
- 当前结论：已实现 `FastBilateralFilter<PointT>::applyFilter(PointCloud&)` 中 organized 点云输出副本上的 `z` 预处理 RVV 路径，包括 finite `z` 的 `base_min/base_max` 规约，以及把 NaN/Inf `z` 替换为 `base_max`。三维 bilateral lattice splat、blur、trilinear interpolation 和最终写回保持标量顺序。

## 函数族评估

| 函数族 | 当前状态 | RVV 适配度 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `FastBilateralFilter<PointT>::applyFilter(PointCloud&)` 的 finite `z` min/max 规约 | 已实现 RVV | 中。organized `PointCloud<PointT>` 仍是 AoS，但只读 `z` 字段，可用 `vlse32` + finite mask + `vfredmin/vfredmax` | 覆盖标准布局、`float z`、点数不少于 64 的 organized 输入；小规模、非标准布局、非 float `z` 回退 Std |
| `applyFilter` 的 non-finite `z` 替换 | 已实现 RVV | 中。只写 `z` 字段，mask store 可表达 `!isfinite(z)` | 覆盖同上；回退 Std 时逐点 `std::isfinite` 判断并写 `base_max` |
| splat 到 `Array3D data` | 暂缓 | 低到中。每点通过 `(small_x, small_y, small_z)` 写同一个 3D lattice cell，存在冲突累加 | 保持标量，避免引入 scatter 冲突、原子或改变累加顺序 |
| 三维 blur `data/buffer` | 已尝试后回退 | 中。内层 z 连续，可条带化 Eigen::Vector2f 的两个 float 通道 | RVV helper 正确性通过，但 Milkv-Jupiter 上未带来收益，真实硬件上 stride load/store 与双通道访存开销抵消算术收益；最终保持标量 |
| `Array3D::trilinear_interpolation` 输出写回 | 暂缓 | 低到中。每点读取 8 个 lattice cell，访存为 gather 且 alpha 计算较多 | 保持标量，避免扩大插值数值差异和 gather 验证成本 |
| `early_division_` 路径 | 保持标量后续 | 中。数据全量除法可向量化，但该配置是 protected 内部开关，公开 API 未提供 setter | 本轮只间接受益于 z 预处理；早除法和插值语义不改 |

## 输入输出边界

- 输入：`pcl::PointCloud<PointT>`，必须 organized；非 organized 时原实现报错并返回。
- 输出：`PointCloud &output`，先由 `copyPointCloud(*input_, output)` 复制，随后只平滑 `z`，`x/y` 和其它字段保持输入副本。
- 配置项：
  - `sigma_s_` 控制 x/y 空间降采样尺寸；
  - `sigma_r_` 控制 z/range 方向的 depth bin；
  - `early_division_` 决定 lattice 内是否先除权重，本轮不改变该分支。

## 数据布局

`PointCloud<PointT>` 是 AoS。RVV helper 使用 `sizeof(PointT)` 作为 stride，只从 `offsetof(PointT, z)` 读取或写回 `z`：

```text
chunk base -> p[c]      p[c+1]    p[c+2] ...
             x y z ...  x y z ... x y z ...
RVV load:          vlse32(z)
finite mask:       z == z && abs(z) < inf
range reduce:      masked vfredmin/vfredmax
replace invalid:   masked vsse32(base_max)
```

只处理 `z` 的原因是 `FastBilateralFilter` 本身是 depth smoothing：`x/y` 坐标用于组织图像位置，主算法只对 depth `z` 建立 range lattice 并写回平滑后的 `z`。

## RVV 覆盖条件

- `__RVV10__` 编译；
- `PointT` 为标准布局，且存在 `float z`；
- organized 输入已通过 `applyFilter` 原有检查；
- 点数不少于 `64`。

RVV helper 不要求 `input_->is_dense=true`，因为原标量路径会在非 dense 输入中忽略 NaN/Inf `z` 并用 `base_max` 替换。RVV 使用同一 finite mask 表达该语义。

## 回退条件

- 非 RVV 编译；
- 点数小于 `64`；
- `PointT` 无 `z` 字段，或 `z` 不是 `float`；
- 非标准布局；
- 非 organized 输入；
- 3D lattice splat、blur、interpolation、`early_division_` 后续路径。

## 风险点

- `std::isfinite` 语义要求同时排除 NaN、`+Inf` 和 `-Inf`。RVV finite mask 使用 `z == z` 排除 NaN，并用 `abs(z) < inf` 排除无穷。
- `base_min/base_max` 只从 finite z 产生；若全部 z 非 finite，RVV helper 必须显式返回 `found_finite=false`，不能依赖哨兵值推断。
- mask store 只写非 finite `z`，不能触碰 `x/y` 或其它字段。
- 本轮没有使用 `_rm` `vfcvt` 或修改 FRM/FCSR；不涉及舍入环境保存恢复。
- splat 和插值暂缓不是默认放弃：冲突累加、三方向 offset 和插值 gather 需要单独证据，当前实现先覆盖低风险线性预处理。

## 优化过程问题记录

首轮整体提升只有约 `1.07x` 到 `1.10x`，收益主要受后续 lattice 标量路径限制。本轮补充尝试了 `Array3D` blur RVV：

- 尝试范围：将 `data/buffer` blur 的内层 z loop 按 `Eigen::Vector2f` 两个 float 通道做 stride load/store，三个 blur 方向通过固定 `off` 选择邻居；
- 正确性：专项测试、bench checksum、上游 `FastBilateralFilter.Filters_Bilateral` 均通过；
- QEMU 现象：QEMU bench 显示 `160x120` case 约 `1.9x`，但 QEMU 只能作为路径和格式证据；
- 板卡现象：生产主路径中接入该 helper 后，Milkv-Jupiter 上 `160x120 finite` 仅约 `1.06x`，低于 z 预处理版本约 `1.09x`，说明真实硬件上该 helper 的 stride load/store 和双通道访存开销抵消了算术收益；
- 进一步实验：保留 bench-diagnosis `LatticeCell` blur microbench，尺寸 `96x72x64`，只测 `data/buffer` blur 形态，不接入生产 `applyFilter`；
- microbench 结果：Milkv-Jupiter 上 Std `65.8100` ms/iter，RVV `69.4940` ms/iter，`0.95x`；反汇编确认实验路径命中 `vlse32.v`、`vsse32.v`、`vfmacc.vf`、`vsetvli e32,m2`；
- 最终处理：生产代码回退 blur RVV，保留原标量 blur；bench-diagnosis case 用于证明当前 stride 双通道方案不值得接入主实现。后续若要继续优化 blur，应先对比 segment load/store、按 z 方向专门化、x/y 方向保持标量或改变 lattice 存储布局等方案。

## 专项测试

专项测试文件：`test-rvv/filters/fast_bilateral/test_fast_bilateral.cpp`。

覆盖：

- 大规模 organized finite `PointXYZ`，命中 min/max RVV 主路径；
- 大规模 organized 含 NaN/Inf `z`，命中 RVV 替换路径；
- 小规模 organized fallback；
- 非 organized 输入保持原错误返回语义。

已运行：

```text
make -C test-rvv/filters/fast_bilateral run_test_compare
```

结果：std 与 RVV 二进制均通过 4 个专项用例。

## bench、QEMU 与反汇编

专项 bench 文件：`test-rvv/filters/fast_bilateral/bench_fast_bilateral.cpp`。

bench 输出包含 `Dataset:`、`Iterations:`、`Build:`，每个 case 输出 `<name> : <avg> ms/iter` 和 `Total Time` / checksum / 参数详情。

已运行：

```text
make -C test-rvv/filters/fast_bilateral run_bench_compare
make -C test-rvv/filters/fast_bilateral dump_bench_rvv
```

`output/qemu/analyze_bench_compare.log` 已解析 Dataset / Iterations / Total Time，无 `未解析`、`n/a` 或 `Total Time 不计算`。QEMU 日志中 std/RVV checksum 一致；QEMU 只作为构建、运行、格式和指令路径证据，不作为性能结论。

反汇编文件：

- `test-rvv/filters/fast_bilateral/build/asm/riscv/bench_fast_bilateral_rvv.full.asm`

已确认关键指令路径：

- `vlse32.v`
- `vsse32.v`
- `vmfeq.vv`
- `vmflt.vf`
- `vfredmin.vs`
- `vfredmax.vs`
- `vsetvli`

## 上游测试与板卡结果

上游定向测试入口：

```text
make -C test-rvv/filters/fast_bilateral run_upstream_test_compare
```

`UPSTREAM_TEST_ARGS` 默认复用仓库现有数据：

```text
test/milk_cartoon_all_small_clorox.pcd --gtest_filter=FastBilateralFilter.Filters_Bilateral
```

结果：std 与 RVV 两套 `FastBilateralFilter.Filters_Bilateral` 均通过。

板卡入口已在 Makefile / `board.mk` 中提供：

- `deploy_board`
- `run_board_test`
- `run_board_bench_compare`
- `fetch_board_logs`

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

真实性能结论：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `fast_bilateral organized 64x48 finite` | 0.7972 | 0.7219 | 1.10x | 命中 finite z min/max RVV 主路径；后续 lattice 标量 |
| `fast_bilateral organized 160x120 finite` | 5.6804 | 5.3436 | 1.06x | 较大 organized finite 主路径受益，但主要成本仍在标量 lattice |
| `fast_bilateral organized 160x120 nonfinite` | 5.2061 | 4.8708 | 1.07x | 命中 min/max RVV 与 non-finite z mask store 替换 |
| `fast_bilateral organized 320x240 finite` | 32.1309 | 30.9764 | 1.04x | 放大输入后仍只优化前置 z 预处理，整体收益被 lattice 主成本稀释 |
| `fast_bilateral organized 320x240 nonfinite` | 30.6193 | 29.4964 | 1.04x | 大规模 non-finite 替换正确，整体收益同样受后续标量路径限制 |
| `fast_bilateral small fallback 7x5` | 0.0203 | 0.0305 | 0.67x | 小规模 fallback 语义 / 成本证据，不作为 RVV 主路径性能结论 |
| `fast_bilateral blur lattice 96x72x64` | 65.8100 | 69.4940 | 0.95x | bench-diagnosis data/buffer blur 实验；命中 RVV stride 双通道路径但未加速，不作为生产主路径结论 |

speedup 计算方式为 `Std Avg / RVV Avg`。主路径收益较小，符合本轮只覆盖 `applyFilter` 前置 z 预处理、未改写 lattice 主成本的实现范围。
