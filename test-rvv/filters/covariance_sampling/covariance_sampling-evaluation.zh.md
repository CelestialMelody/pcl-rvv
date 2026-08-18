# filters/covariance_sampling 函数级 RVV 评估

## 1. 主题与入口

- 主题：`covariance_sampling`
- 主文件：`filters/include/pcl/filters/impl/covariance_sampling.hpp`
- 公开类：`pcl::CovarianceSampling<PointT, PointNT>`
- 专项目录：`test-rvv/filters/covariance_sampling/`
- 模块依据：`doc-rvv/library-screening/filters/filters-second-pass-retained-candidate-rescreen.zh.md` 的 `6.2 暂缓 / 不单独实施（诊断路径记录）` 第 13 项。

保留候选复筛将本主题列为 `保留 / 待诊断`，诊断点是 centroid、scaled point 和 6D vector 构造。保留原因是 `Eigen` 6x6 solver、six-list sort 和 sampling state 可能主导 `applyFilter(Indices&)`，局部 RVV 片段不能直接代表生产入口收益。

本轮只建立 `test-rvv` diagnostic / bench-only 资产，不修改公开 API，不修改 `filters/include/...` 生产源码，不接入生产分流。

## 2. 函数级评估

| 函数 / 片段 | 优先级 | RVV 决策 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `initCompute` 中 centroid 与 scaled point 构造 | bench 诊断 | 已实现专项 RVV 原型 | 覆盖 `PointXYZ`、显式 `indices`、点数 `>=32`；小规模或非 RVV 编译回退 Std helper |
| `applyFilter` 中 6D vector 构造 | bench 诊断 | 已实现专项 RVV 原型 | RVV 只 gather normal，cross product 与 double 写入仍保持 scalar/Eigen 方式 |
| `computeCovarianceMatrix` / `Eigen::SelfAdjointEigenSolver` | 主成本风险 | 保持标量 / Eigen | 作为 full diagnostic 的主成本分量记录，不在本主题中改写 |
| six-list sort 与 sampling state | 主成本风险 | 保持标量 | 该状态机决定 sampled index 语义，是生产接入的主要风险 |
| `applyFilter(Indices&)` 生产入口 | 生产不接入 | 不修改上游源码 | 板卡 full diagnostic 约 `0.99x` 到 `1.00x`，且 RVV/Std checksum 不一致，不能作为生产语义证据 |

结论：局部片段可形成可归因诊断，但没有证明 RVV 覆盖生产入口主成本。`scaled-points` 在板卡上只有 `1.01x` 到 `1.08x` 弱收益；`6D-vector` 部分 case 为 `0.78x` 到 `0.79x`；full diagnostic 基本持平。当前收敛为 bench-only 诊断，生产不接入。

## 3. RVV 设计

专项 header `covariance_sampling_diag.hpp` 提供：

- `computeScaledPointsStd` / `computeScaledPointsRVV`：计算 centroid、scaled points 和 average norm；
- `buildVectorsStd` / `buildVectorsRVV`：构造 6D vector；
- `computeCovarianceFromVectors` / `conditionNumber`：保留 Eigen 6x6 solver 边界；
- `sampleFromVectors`：保留 six-list sort 与 sampling state；
- `runDiagnostic`：把 RVV 片段与标量 solver/sort/sampling 串成 full diagnostic。

RVV 片段使用 `pcl/rvv_point_load.h` 的 AoS gather helper：

- `PointXYZ` 的 x/y/z indexed load；
- `Normal` 的 normal_x/normal_y/normal_z indexed load；
- `vsetvl` 分块；
- 局部 accumulators 和 store-back 后的 scalar normalization / cross product。

该设计有意保留 solver、list sort 和 sampling update，不尝试生产接入。这样可以回答“局部 RVV 片段是否足以抵消真实 full diagnostic 中的主成本”，而不是只测一个脱离入口的数据搬运 microbench。

## 4. 测试与 bench

专项 gtest：

- `ScaledPointsRVVMatchesScalarConditionInput`：RVV scaled-point helper 的 centroid 和 average norm 与 Std helper 在容差内一致；
- `FullDiagnosticIdentityIndicesMatchesScalarWithinTolerance`：identity indices 的 full diagnostic condition number 与 sampled count 对拍；
- `FullDiagnosticShuffledIndicesMatchesScalarWithinTolerance`：shuffled indices 的 full diagnostic condition number 与 sampled count 对拍；
- `SmallInputFallsBackToScalarHelper`：小规模输入不进入 RVV helper。

测试使用 `gtest` / `gtest_main`，`Makefile` 中 `LIBS_TEST` 链接 `-lgtest -lgtest_main -lpthread`。bench 是独立可执行文件，用于输出 Std/RVV 的分段耗时、checksum 和可解析 compare 表。

需要注意：当前 gtest 只证明诊断 helper 的数值条件量在容差内，不证明 sampled index 序列 bitwise 一致。板卡 full diagnostic 的 checksum 在 Std/RVV 间不同，因此不能把本主题作为生产语义接入证据。

专项 bench：

- `covariance_sampling scaled-points identity 4K`
- `covariance_sampling scaled-points shuffled 4K`
- `covariance_sampling 6D-vector identity 4K`
- `covariance_sampling 6D-vector shuffled 4K`
- `covariance_sampling covariance+solver identity 4K`
- `covariance_sampling full apply identity 4K sample 512`
- `covariance_sampling full apply shuffled 4K sample 512`
- `covariance_sampling scaled-points identity 16K`
- `covariance_sampling 6D-vector identity 16K`
- `covariance_sampling covariance+solver identity 16K`
- `covariance_sampling full apply identity 16K sample 1024`
- `covariance_sampling full apply shuffled 16K sample 1024`

## 5. 验证结果

- QEMU 专项测试：`make -C test-rvv/filters/covariance_sampling run_test_compare` 通过，Std/RVV 两套构建均通过 4 个 gtest。
- QEMU bench：`make -C test-rvv/filters/covariance_sampling run_bench_compare` 通过，日志在 `output/qemu/analyze_bench_compare.log`。QEMU 只作为构建、格式和路径证据，不作为性能结论。
- 反汇编：`make -C test-rvv/filters/covariance_sampling dump_bench_rvv` 生成 `build/asm/riscv/bench_covariance_sampling_rvv.full.asm` 和 `.asm` 摘录；摘录中可见 `vsetvli`、`vle/vse`、`vfmul`、`vfmadd`、`vfred*` 等 RVV 指令。该汇编包含 Eigen/RVV 和编译器自动向量化路径，不能把全部 RVV 指令都归因到本主题手写 helper。
- 板卡验证：`make -C test-rvv/filters/covariance_sampling run_board_test run_board_bench_compare fetch_board_logs` 通过，日志在 `output/board/`。

Milkv-Jupiter 结果：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `covariance_sampling scaled-points identity 4K` | 0.3001 | 0.2787 | 1.08x | 局部片段弱收益 |
| `covariance_sampling scaled-points shuffled 4K` | 0.4046 | 0.3857 | 1.05x | 局部片段弱收益 |
| `covariance_sampling 6D-vector identity 4K` | 0.2628 | 0.3340 | 0.79x | RVV helper 变慢 |
| `covariance_sampling 6D-vector shuffled 4K` | 0.4021 | 0.3586 | 1.12x | 局部片段收益不稳定 |
| `covariance_sampling covariance+solver identity 4K` | 0.5279 | 0.5190 | 1.02x | 基本持平 |
| `covariance_sampling full apply identity 4K sample 512` | 23.1350 | 23.2913 | 0.99x | full diagnostic 不成立 |
| `covariance_sampling full apply shuffled 4K sample 512` | 26.8783 | 26.9420 | 1.00x | full diagnostic 持平 |
| `covariance_sampling scaled-points identity 16K` | 1.1653 | 1.1499 | 1.01x | 大规模仍是弱收益 |
| `covariance_sampling 6D-vector identity 16K` | 1.0687 | 1.3626 | 0.78x | RVV helper 变慢 |
| `covariance_sampling covariance+solver identity 16K` | 2.0648 | 2.0622 | 1.00x | 持平 |
| `covariance_sampling full apply identity 16K sample 1024` | 114.8097 | 114.7412 | 1.00x | full diagnostic 持平 |
| `covariance_sampling full apply shuffled 16K sample 1024` | 150.0901 | 150.0058 | 1.00x | full diagnostic 持平 |

QEMU 结果显示 RVV helper 在模拟器上更慢，full diagnostic 约 `0.89x` 到 `0.90x`。该结果不作为板卡性能结论，但与板卡结论方向一致：局部片段不能形成生产收益。

## 6. 结论

`covariance_sampling` 当前完成 bench-only 诊断，生产不接入。板卡证据显示 full diagnostic 基本持平，局部 scaled-point 片段只有弱收益，6D-vector 片段收益不稳定且有明显变慢 case。更关键的是，full diagnostic 的 Std/RVV checksum 不一致，说明 sampled index 序列可能受浮点累加顺序和后续排序 / sampling state 放大影响，不能作为生产语义等价证据。

后续只有在提出新的数据流方案并同时满足以下条件时，才应重新纳入生产候选：

- sampled index 语义可以与标量路径稳定对齐，或明确证明差异符合 PCL API 允许范围；
- full `applyFilter(Indices&)` 或真实生产入口在板卡上有稳定明显收益；
- solver、six-list sort 和 sampling update 的主成本被覆盖，而不是只优化前置片段；
- fallback 和维护边界清晰。
