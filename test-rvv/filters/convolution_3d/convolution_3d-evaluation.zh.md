# filters/convolution_3d 函数级 RVV 诊断评估

## 1. 主题与入口

- 主题：`convolution_3d`
- 主文件：`filters/include/pcl/filters/impl/convolution_3d.hpp`
- 公开类：`pcl::filters::Convolution3D<PointInT, PointOutT, KernelT>`
- 关键对象：`GaussianKernel<PointInT, PointOutT>::operator()`
- 专项目录：`test-rvv/filters/convolution_3d/`
- 模块依据：`doc-rvv/library-screening/filters/filters-second-pass-retained-candidate-rescreen.zh.md` 的 `6.2 暂缓 / 不单独实施（诊断路径记录）` 第 12 项。

`Convolution3D::convolve(output)` 用于非 organized 或未知宽高点云的三维邻域卷积。入口先 `initCompute()`，把输入点云或 `surface_` 接到 `search::Search`，再对每个 query point 执行 `radiusSearch(point, search_radius_, nn_indices, nn_distances)`，最后把邻域索引和 squared distances 交给 `kernel_(nn_indices, nn_distances)` 生成输出点。

本轮只做 bench 诊断，不修改公开 API，不修改 `filters/include/pcl/filters/impl/convolution_3d.hpp`，不接入生产分流。

## 2. 函数级评估

| 函数 / 片段 | 优先级 | RVV 决策 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `Convolution3D::convolve` 外层循环 | bench 诊断 | 不改生产 | 真实入口保留标量；诊断中用 full diagnostic 复刻 `finite -> radiusSearch -> kernel -> output write` |
| `GaussianKernel<PointXYZ, PointXYZ>::operator()` | bench 诊断 | 已实现专项 RVV 原型 | 覆盖 `PointXYZ`、邻域数 `>=16`、`indices.size()==distances.size()`；小邻域、非 RVV 编译和异常长度回退 Std |
| distance threshold | 局部可 RVV | 诊断实现 | `dist <= threshold` 直接生成 mask |
| 邻域 gather / finite | 局部可 RVV | 诊断实现 | `radiusSearch` 给出的 neighbor ids 通过公共 `rvv_point_load` gather `x/y/z` |
| kernel weight | 局部可 RVV | 诊断实现 | 复用 common `pcl::expf_RVV_f32m2`；误差预算由专项测试约束 |
| 加权累加 | 语义敏感 | RVV 生成 weight/weighted xyz，标量顺序累加 | 避免同时改变 `GaussianKernel` 的邻域累加顺序，便于归因 |
| output write | 主路径保持标量 | 诊断 full case 保留标量写回 | 输出写回不是主要可向量化对象 |

结论：局部 kernel-only 在 Milkv-Jupiter 上可达到约 `1.85x`，但 full diagnostic 只有 `1.09x` 到 `1.11x`，未改生产入口约 `1.00x`。`radiusSearch` 和邻域不规则访问仍占主要成本；kernel-only microbench 不能代表生产收益。当前主题收敛为 bench 诊断完成，不接生产。

## 3. RVV 诊断设计

专项 helper 位于 `test-rvv/filters/convolution_3d/convolution_3d_diag.hpp`：

- `KernelResult`：保存 kernel 输出 `x/y/z`、`total_weight` 和 accepted neighbor 数；
- `gaussianKernelPointXYZStd`：标量对拍基准，复刻 `GaussianKernel::operator()` 的阈值、finite、`exp`、加权累加和归一化；
- `gaussianKernelPointXYZRVV`：bench-only RVV helper，使用 `vle32` 读取 distances / neighbor ids，公共 `rvv_point_load` gather `PointXYZ::x/y/z`，common `expf_RVV_f32m2` 计算权重；
- `convolveDiagnosticStd/RVV`：复刻 `Convolution3D::convolve` 的 `radiusSearch -> kernel -> output`，仅 kernel 部分可切换 RVV；
- `convolveProductionUnchanged`：调用未修改的上游 `Convolution3D` 入口，证明本轮没有生产分流。

RVV helper 不使用显式 `_rm` intrinsic，也不修改 FRM/FCSR。由于使用 common float `expf_RVV_f32m2` 替代标量 `std::exp`，专项测试采用误差预算：`max_abs < 8e-4`、`max_rel < 5e-5`。当前测试最大误差远低于预算。

## 4. 测试与 bench

专项测试 `test_convolution_3d.cpp` 覆盖：

- 大邻域 `PointXYZ` kernel 命中 RVV；
- 小邻域 fallback；
- 同一进程先运行 RVV 主路径再运行 fallback，确认没有后续标量污染；
- full diagnostic std/RVV 对拍，检查 `max_abs` 和 `max_rel`。

专项 bench `bench_convolution_3d.cpp` 输出包含 `Dataset:`、`Iterations:`、checksum 和误差统计，case 分为：

- `radiusSearch only`：只测真实入口的搜索成本；
- `kernel distance-threshold`：distance mask 局部片段；
- `kernel gather-finite`：邻域 AoS gather 和 finite mask；
- `kernel exp-weight`：Gaussian 权重；
- `kernel-only Gaussian`：完整 kernel microbench，不代表生产收益；
- `full diag`：真实 `radiusSearch` 加 RVV/Std kernel 和 output write；
- `production unchanged`：未修改上游入口。

上游原始测试不强制新增：仓库没有直接面向 `Convolution3D::convolve` 的独立上游测试；当前专项测试和 bench 已覆盖本诊断目标。`convolution` 2D 主题已有独立上游测试，但不覆盖本 3D search-based 入口。

## 5. 验证结果

- QEMU 专项测试：`make -C test-rvv/filters/convolution_3d run_test_compare` 通过，std/RVV 均通过；RVV 日志显示 `rvv_used=1`。
- QEMU bench：`make -C test-rvv/filters/convolution_3d run_bench_compare` 通过，`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`。QEMU 只作为正确性、日志格式和路径证据。
- 反汇编：`make -C test-rvv/filters/convolution_3d dump_bench_rvv` 后，`output/qemu/rvv_asm_check.log` 确认 `vsetvli`、`vle32.v`、`vluxei32.v`、`vmfle.vf`、`vcpop.m`、`vfmul.vf`、`vfmacc.vv`、`vse32.v` 等路径。
- 板卡验证：`make -C test-rvv/filters/convolution_3d board_smoke` 通过，日志已拉回 `test-rvv/filters/convolution_3d/output/board/`。

Milkv-Jupiter 结果：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `radiusSearch only 8K` | 97.6161 | 96.3783 | 1.01x | 搜索自身不受本 RVV helper 影响 |
| `radiusSearch only 32K` | 357.0300 | 353.8309 | 1.01x | 32K 搜索仍是主要成本 |
| `kernel distance-threshold 1024` | 0.0018 | 0.0013 | 1.38x | distance mask 可 RVV 化，但绝对成本很小 |
| `kernel gather-finite 1024` | 0.0295 | 0.0045 | 6.56x | 邻域 gather/finite 局部片段收益明显 |
| `kernel exp-weight 1024` | 0.0455 | 0.0071 | 6.41x | common expf helper 对局部权重有效 |
| `kernel-only Gaussian 1024` | 0.0965 | 0.0522 | 1.85x | kernel-only 成立，但不是生产结论 |
| `full diag 8K` | 130.6141 | 119.8917 | 1.09x | full diagnostic 仅弱收益 |
| `full diag 32K` | 505.0634 | 453.1217 | 1.11x | 搜索和不规则邻域稀释局部收益 |
| `subset full diag 32K` | 279.3580 | 252.8701 | 1.10x | subset full 仍是弱收益 |
| `finite full diag 32K` | 520.9435 | 467.8918 | 1.11x | invalid skip 不改变结论 |
| `production unchanged 8K` | 110.5386 | 110.0236 | 1.00x | 未修改生产入口；不作为新增 RVV 收益 |

## 6. 结论

`GaussianKernel<PointXYZ, PointXYZ>::operator()` 的 distance threshold、邻域 gather/finite、Gaussian weight 和加权求和均可在 bench-only helper 中 RVV 化，并通过 QEMU、反汇编和板卡验证。但真实 `Convolution3D::convolve` 入口的 full diagnostic 只有 `1.09x` 到 `1.11x`，属于弱收益区间；未改生产入口约 `1.00x`。

本主题不接入上游生产分流。原因是生产接入需要为模板 kernel、点类型、误差预算、小邻域 fallback、邻域累加顺序和 `radiusSearch` 后的 gather 维护额外边界，但 full diagnostic 没有证明稳定明显收益。后续只有在真实 profile 证明 kernel 占比显著高于当前数据集，或能同时降低 `radiusSearch` / 邻域组织主成本时，才值得重新纳入生产候选。
