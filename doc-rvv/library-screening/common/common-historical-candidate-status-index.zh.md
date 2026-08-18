# common 模块 RVV 状态索引

本文记录 `common` 模块早期 RVV 探索成果的状态索引，用于后续从当前 RVV 工作流恢复 common 状态。本文不回填 `common` 的文件候选筛选 high-mid 基线，不伪造标准函数评估队列，也不重新筛选 common 模块。

`doc-rvv/common/module-evaluation.zh.md` 是 legacy exploration input：它反映早期探索阶段的候选判断和已处理方向，可作为历史依据参考，但不是当前可直接执行的状态队列。本文只把已完成主题、待补 closeout 和后续可能候选整理为历史状态索引。后续新模块仍应按当前项目 RVV 工作流正常建立文件候选筛选、函数评估队列或二轮保留候选复筛文档。

## 1. 输入依据与桥接原因

### 1.1 输入依据

- legacy exploration input：`doc-rvv/common/module-evaluation.zh.md`。
- 已完成主题文档：`doc-rvv/common/transforms-RVV.zh.md`、`doc-rvv/common/centroid.zh.md`、`doc-rvv/common/norms-RVV.zh.md`、`doc-rvv/common/gaussian.zh.md`、`doc-rvv/common/distances.zh.md`、`doc-rvv/common/common.zh.md`。
- 数学 helper 文档：`doc-rvv/rvv/math/atan2-RVV.zh.md`、`doc-rvv/rvv/math/expf-RVV.zh.md`、`doc-rvv/rvv/math/logf-RVV.zh.md`、`doc-rvv/rvv/math/getAcuteAngle3DRVV.zh.md`、`doc-rvv/rvv/math/remez-coeffs.zh.md`。
- 函数级评估文档：`test-rvv/common/transforms/transforms-evaluation.zh.md`、`test-rvv/common/centroid/centroid-evaluation.zh.md`、`test-rvv/common/norms/norms-evaluation.zh.md`、`test-rvv/common/gaussian/gaussian-evaluation.zh.md`。
- 证据日志：`test-rvv/common/*/output/qemu/` 与 `test-rvv/common/*/output/board/` 下现有日志。

### 1.2 为什么需要桥接文档

`common` 是早期 RVV 探索阶段的模块，已有多个主题落地，但缺少当前工作流下的 `library-screening/common/` 恢复入口。如果后续 agent 直接从 `doc-rvv/common/module-evaluation.zh.md` 继续，很容易把历史探索表误读为当前执行队列，或者忽略已完成主题中已经暴露出的 gate、fallback、bench 合同和反汇编归档缺口。

因此本文值得创建：它只做状态桥接，帮助恢复 common 历史状态，明确哪些主题已完成、哪些需要 closeout 证据补齐、哪些后续候选只有在重新出现 workload/profile 需求时才值得继续。

## 2. common 已完成主题总览

| 主题                          | 主文件                                            | 实际覆盖范围                                                                                                                                                                                                      | 生产接入状态                                       | 主要证据                                                                                                                                                                       | 当前定位                                            |
| ----------------------------- | ------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------- |
| `transforms`                | `common/include/pcl/common/impl/transforms.hpp` | dense、非 indexed、`Scalar=float` 的整云 xyz / xyz+normal 变换；小点云、double、non-dense、indices、PointXY 保持标量                                                                                            | 已生产接入                                         | QEMU 专项/上游测试通过；板卡`1,000,000` 点、20 iterations，xyz `2.50x-3.54x`，xyz+normal `1.58x-1.86x`；主题文档记录反汇编入口                                           | 已完成，后续只保留 indexed / PointXY 复筛可能       |
| `centroid`                  | `common/include/pcl/common/impl/centroid.hpp`   | `compute3DCentroid`、`computeMeanAndCovarianceMatrix`、`computeCovarianceMatrix`、`demeanPointCloud` 的 dense / indices 主路径；`n < 16`、non-dense、`ConstCloudIterator`、`computeNDCentroid` 回退 | 已生产接入                                         | QEMU 23 tests 通过；板卡`1,000,000` 点、20 iterations，统计/协方差类 `2.08x-6.24x`，`demeanPointCloud` `1.16x-1.52x`                                                   | 已完成，弱收益子项不扩大解释                        |
| `norms`                     | `common/include/pcl/common/impl/norms.hpp`      | 连续`float*` / `std::vector<float>`、`dim >= 16` 的范数和 `selectNorm` 被调路径；非连续/generic 容器与小维回退                                                                                            | 已生产接入                                         | QEMU 单测和同进程 Std/RVV 对拍通过；板卡`dim={8,36,128,512,4096}`，大维度多项收益明显，小维和个别维度有弱收益或倒退                                                          | 已完成，但需避免把局部强项泛化为所有 dim            |
| `gaussian`                  | `common/src/gaussian.cpp`                       | `GaussianKernel::convolveRows` / `convolveCols` 的 `PointCloud<float>` shared-library 路径；`compute` 和 `impl/gaussian.hpp` 保持标量                                                                   | 已生产接入，路径证据待补                           | QEMU 公开入口 5 tests 通过；local fragment 对拍 3 tests 通过；板卡双库`960x540`、10 iterations、`sigma=5`，rows `8.46x`，cols `2.49x`，rows+cols `3.29x`             | 已完成但反汇编 closeout 未闭合                      |
| `distances`                 | `common/include/pcl/common/distances.h`         | `getMaxSegment` cloud / indices；`n < 512` 回退，indices 先打包后复用 cloud RVV 内核                                                                                                                          | 已生产接入，函数级评估待补                         | QEMU 5 tests 通过；板卡`maxseg_points=2500`、20 iterations，cloud `5.03x`，indices `7.06x`；load microbench 仅作访存策略证据                                             | 已完成但 evaluation、bench 输出合同和反汇编归档待补 |
| `common.hpp` / math helpers | `common/include/pcl/common/impl/common.hpp`     | `getMeanStd`、`getPointsInBox`、`getMaxDistance`、`getMinMax3D`、`calculatePolygonArea`，以及 `acos_RVV`、`getAcuteAngle3DRVV`、`atan2_RVV`、`expf_RVV`、`logf_RVV` 等 helper                 | 已生产接入或作为生产 helper 被复用，函数级评估待补 | QEMU common 66 tests 通过；板卡 common bench 中`getMeanStd` `7.92x`、`getMinMax3D` 最高 `11.18x`、`getAngle3D` `24.53x`；math helper 有专项文档和部分板卡/QEMU记录 | 已完成但 evidence closeout 需要统一                 |

## 3. 已完成主题证据包

### 3.1 transforms

- 主文件：`common/include/pcl/common/impl/transforms.hpp`；公开入口在 `common/include/pcl/common/transforms.h`。
- 实际覆盖范围：主题文档明确第一轮只覆盖 dense、非 indexed、`Scalar=float` 的整云入口，并覆盖 `transformPointCloud` 与 `transformPointCloudWithNormals`；indices、`PointXY`、single-point 和 `getPrincipalTransformation` 暂缓。
- 生产接入状态：已生产接入，公开 API 不变，运行期 gate 包括 dense、点数阈值、点类型字段兼容和矩阵标量类型。
- 正确性证据：`test-rvv/common/transforms/output/qemu/run_test_rvv.log` 记录 RVV 专项 6 tests 通过；`run_upstream_test_rvv.log` 记录上游原始测试 23 tests 通过。
- QEMU / 反汇编 / 板卡证据：QEMU 仅用于正确性、构建和路径材料；主题文档记录 `dump_bench_rvv` 与 `bench_transforms_rvv.asm` 可匹配 RVV 指令。板卡 `test-rvv/common/transforms/output/board/bench_compare.log` 为性能结论来源。
- 文档状态：主题文档与函数级评估均存在。
- 待补证据或 closeout：若恢复该主题，建议复核当前仓库是否仍保留实际 `.asm` 产物；状态表层可接受主题文档中的反汇编说明，但归档证据需要重新确认。
- 对后续候选的影响：dense AoS stride + fused normal 条带是有效模式；indexed gather 和 `PointXY` 应重新从函数级评估或 bench 诊断开始，不能直接沿用 dense 结论。

### 3.2 centroid

- 主文件：`common/include/pcl/common/impl/centroid.hpp`。
- 实际覆盖范围：`compute3DCentroid`、`computeMeanAndCovarianceMatrix`、`computeCovarianceMatrix` 及 `demeanPointCloud` 的 dense / indices 主路径；`n < 16`、non-dense、`ConstCloudIterator`、`computeNDCentroid`、`CentroidPoint` 等保持标量或暂缓。
- 生产接入状态：已生产接入，使用 `__RVV10__` 与点类型兼容 gate，保留 `*Standard` 回退。
- 正确性证据：`test-rvv/common/centroid/output/qemu/run_test.log` 显示 23 tests 通过。
- QEMU / 反汇编 / 板卡证据：QEMU 作为正确性和构建证据；板卡 `test-rvv/common/centroid/output/board/bench_compare.log` 记录 `points=1000000 seed=42`、20 iterations。协方差和均值类收益高，`demeanPointCloud` 类收益较弱。反汇编归档未在主题文档中形成与 transforms 同等明确的保存路径。
- 文档状态：主题文档与函数级评估均存在。
- 待补证据或 closeout：补齐或复核反汇编归档；`demeanPointCloud` 的弱收益应保留为已知边界，不作为新增同类写回密集路径的强证据。
- 对后续候选的影响：统计规约类 dense AoS 路径值得优先考虑；带 Eigen solver 或泛型字段聚合的路径不应仅因有前置循环就进入生产。

### 3.3 norms

- 主文件：`common/include/pcl/common/impl/norms.hpp`。
- 实际覆盖范围：连续 `float` 的 L1、L2²、L2、Linf、JM、B、Sublinear、CS、Div、PF、K、KL、HIK 与 `selectNorm` 被调路径；`dim < kNormRvvMinDim`、非连续或 generic `FloatVectorT` 走标量。
- 生产接入状态：已生产接入，采用窄分发和小维回退。
- 正确性证据：`test-rvv/common/norms/output/qemu/run_test.log` 显示专项测试通过；`run_test_std_vs_rvv_compare.log` 显示同进程 Std/RVV 对拍通过。
- QEMU / 反汇编 / 板卡证据：QEMU 只作为正确性、构建和日志格式材料；板卡 `test-rvv/common/norms/output/board/bench_compare.log` 覆盖 `dim={8,36,128,512,4096}`、30 iterations。大维度和复杂范数有明显收益，小维度以及个别 `L2_Norm_SQR` case 有弱收益或倒退。反汇编归档没有统一 closeout。
- 文档状态：主题文档与函数级评估均存在。
- 待补证据或 closeout：复核反汇编归档；如果工具链或容差策略变化，需要重审 `logf_RVV`、`Div`、`KL` 和有序规约误差预算。
- 对后续候选的影响：连续 float reduction 是可复用模式；含 log/div/sqrt 或短维度的收益必须按 case 衡量。

### 3.4 gaussian

- 主文件：`common/src/gaussian.cpp`。
- 实际覆盖范围：仅 `GaussianKernel::convolveRows` / `convolveCols` 的 `PointCloud<float>` shared-library 路径；`GaussianKernel::compute`、`impl/gaussian.hpp` 泛型模板和别名拷贝结构保持标量或暂缓。
- 生产接入状态：已生产接入，但路径命中反汇编证据待补。
- 正确性证据：`test-rvv/common/gaussian/output/qemu/run_test.log` 与 `run_test_rvv.log` 均显示 5 tests 通过；`run_test_convolve_compare.log` 显示 local fragment 的 rows、cols、rows+cols 对拍 3 tests 通过。
- QEMU / 反汇编 / 板卡证据：QEMU 只证明正确性、可运行性和 local fragment 对拍，不作为性能结论。板卡 `test-rvv/common/gaussian/output/board/banch_compare.log` 记录 `960x540`、10 iterations、`sigma=5` 的双库对比，rows `8.46x`、cols `2.49x`、rows+cols `3.29x`。当前仓库未发现已保存的 RVV 构建共享库或对象文件反汇编日志。
- 文档状态：主题文档和函数级评估均存在；主题文档已明确 evidence 层级。
- 待补证据或 closeout：补 `libpcl_common` RVV 构建产物或 `gaussian.cpp` 对象文件的反汇编证据，确认 rows 中的连续 load / slide / FMA / store 和 cols 中的 strided load-store / FMA 指令。
- 对后续候选的影响：shared-library TU 的宏边界必须单独说明；local fragment 只能证明算法副本一致，不能替代真实库入口路径命中。

### 3.5 distances

- 主文件：`common/include/pcl/common/distances.h`。
- 实际覆盖范围：`getMaxSegment` cloud / indices 两个入口；`n < 512` 回退标量；indices 路径先打包为连续点云，再复用 cloud RVV 内核。
- 生产接入状态：已生产接入，但函数级评估文档缺失。
- 正确性证据：`test-rvv/common/distances/output/qemu/run_test.log` 显示 5 tests 通过。
- QEMU / 反汇编 / 板卡证据：QEMU 可作为正确性与构建材料；板卡 `test-rvv/common/distances/output/board/bench_compare.log` 记录 `maxseg_points=2500`、20 iterations，cloud `5.03x`、indices `7.06x`。`bench_load_compare.log` 中的 segment-load microbench 只证明访存策略差异，不等同于端到端收益。反汇编归档待复核。
- 文档状态：主题文档存在；`test-rvv/common/distances/*-evaluation.zh.md` 不存在。
- 待补证据或 closeout：补函数级 evaluation；修正或补充 bench 输出合同，使 Iterations 能被汇总脚本解析；补反汇编归档。
- 对后续候选的影响：O(n^2) 内层 SIMD 可以强收益，但索引打包、端点平局和浮点归约顺序都需要明确语义边界。

### 3.6 common.hpp / math helpers

- 主文件：`common/include/pcl/common/impl/common.hpp`。
- 实际覆盖范围：`getMeanStd`、`getPointsInBox`、`getMaxDistance`、`getMinMax3D`、`calculatePolygonArea`，以及 `acos_RVV`、`getAcuteAngle3DRVV`、`atan2_RVV_f32m2`、`expf_RVV_f32m2`、`logf_RVV_f32m2` 等 helper。
- 生产接入状态：多个 common 入口已生产接入；math helpers 作为 common 内部或其他模块复用 helper 使用。
- 正确性证据：`test-rvv/common/common/output/qemu/run_test.log` 显示 66 tests 通过；`atan2`、`expf`、`acos` 等有数学专项文档和日志说明。
- QEMU / 反汇编 / 板卡证据：QEMU 只作为正确性和构建材料；板卡 `test-rvv/common/common/output/board/bench_compare.log` 记录 cloud `200000` points、vector `500000` elements、20 iterations，覆盖 `getMeanStd` `7.92x`、`getPointsInBox` `3.79x`、`getMaxDistance` `4.83x-5.26x`、`getMinMax3D` `8.76x-11.18x`、`getAngle3D` `24.53x`、`calculatePolygonArea` `2.87x`。`expf_test.log`、`atan2_test.log` 和 `doc-rvv/rvv/math/getAcuteAngle3DRVV.zh.md` 提供部分 helper 的专项板卡或 QEMU 数值证据。反汇编归档待复核。
- 文档状态：主题文档和多个 helper 文档存在；`test-rvv/common/common/*-evaluation.zh.md` 不存在。
- 待补证据或 closeout：补 common.hpp / math helpers 的函数级 evaluation 或 historical closeout；统一 helper 的反汇编归档；对 `getMeanStd` 的 float 向量部分和上游 double 累加差异保持显式说明。
- 对后续候选的影响：common math helper 可优先作为其他模块的复用点，但必须携带数值误差预算，不能把 helper microbench 直接升级为完整生产入口结论。

## 4. 可复用模式与失败/回退边界

| 模式                    | 来自主题                                                    | 成立条件                                                                  | 失败 / 回退边界                                          | 对后续候选的影响                                                    |
| ----------------------- | ----------------------------------------------------------- | ------------------------------------------------------------------------- | -------------------------------------------------------- | ------------------------------------------------------------------- |
| dense AoS stride 主路径 | `transforms`、`centroid`、`common.hpp`、`distances` | 点类型标准布局、`x/y/z` 为 `float`、主成本是顺序或可控 stride 访问    | non-dense、字段不兼容、泛型字段、短循环或写回/拷贝占主导 | 可作为后续 direct-main-path 的优先模式                              |
| 算术密集规约            | `centroid`、`norms`、`common.hpp`                     | 入口主成本是长向量规约、FMA 或 min/max，容差可接受                        | 有序标量 bit 等价、短维度、复杂特殊值语义                | 新候选必须先说明累加顺序和误差预算                                  |
| shared-library TU 分流  | `gaussian`、部分 `norms` bench                          | Std/RVV 的对比产物确实来自不同编译宏下的库或对象文件                      | 只切换 test/bench 可执行文件宏，不能改变已链接库机器码   | 恢复时必须同时记录库构建条件和路径命中证据                          |
| local fragment 诊断     | `gaussian` local convolve compare、helper microbench      | 用于证明局部算法或 helper 数值一致、定位向量内核                          | 不能证明公开入口真实命中 RVV，也不能作为性能结论         | 状态表中必须区分 local fragment、full diagnostic 和 production case |
| 数学近似 helper         | `expf`、`logf`、`atan2`、`acos`                     | 调用方接受单精度吞吐优先和明确误差预算                                    | 要求 libm 全特殊值语义、bit exact 或 double 累加语义     | 其他模块复用时必须在主题文档中写明误差来源和 fallback               |
| bench 输出合同          | 各主题 board compare                                        | 日志包含 Dataset、Iterations、Avg、Total、Speedup，且 QEMU 与板卡结论分开 | Iterations 不能解析、Total 为 n/a、QEMU 被写成性能结论   | closeout 前先修日志合同或在状态表标为待补                           |

## 5. 待补 closeout / 待补证据

| 主题                          | 待补项                                                                          | 当前影响                                                               |
| ----------------------------- | ------------------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| `gaussian`                  | 缺 RVV 构建共享库或`gaussian.cpp` 对象文件反汇编证据                          | 生产性能和正确性证据已有，但路径命中 gate 未完全闭合                   |
| `distances`                 | 缺函数级 evaluation；bench compare 的 Iterations 解析合同待修；反汇编归档待复核 | 不能作为完全闭合的当前 workflow 主题，只能标为历史完成 / closeout 待补 |
| `common.hpp` / math helpers | 缺函数级 evaluation；helper 反汇编归档不统一                                    | 需要补 historical closeout，尤其是被其他模块复用时                     |
| `centroid`                  | 反汇编归档待复核；`demeanPointCloud` 弱收益边界需保持                         | 生产接入可保留，但后续不得把弱收益写回路径泛化                         |
| `norms`                     | 反汇编归档待复核；个别维度倒退和`logf_RVV` 误差预算需随工具链复核             | 后续恢复时先验证 dim 分布和数值容差                                    |
| `transforms`                | 反汇编产物归档位置待复核                                                        | 主题文档已记录路径命中说明，但恢复时建议重新确认产物                   |

## 6. 后续候选逐项复筛

本文不是重新筛选 common 模块。下表只说明如果未来恢复 common 工作，哪些历史候选可以重新进入评估，以及进入条件。

### 6.1 建议启动函数级评估

| 主题                                          | 关键入口                                                                  | 依据                                                    | 主要风险                                              | 下一步条件                                                               |
| --------------------------------------------- | ------------------------------------------------------------------------- | ------------------------------------------------------- | ----------------------------------------------------- | ------------------------------------------------------------------------ |
| `transforms` indexed gather                 | `transformPointCloud` / `transformPointCloudWithNormals` indices 重载 | 函数级评估中列为中优先级暂缓，已有 dense 生产模式可参考 | gather 成本、`copy_all_fields` 行为、non-dense 语义 | 有真实 workload 证明 indexed transform 是热点，并先建立函数级 evaluation |
| `transforms` `PointXY` 2D affine          | `transformPointCloud(PointXY, Affine2f)`                                | 函数级评估中列为中优先级轻量扩展                        | 覆盖面和点数规模不明，收益可能小                      | 有 PointXY 大规模 workload 或明确调用方后再评估                          |
| `distances` closeout repair                 | `getMaxSegment` cloud / indices                                         | 已有生产和板卡收益，但缺 evaluation 和路径证据          | 不是新优化，而是历史主题补证据                        | 恢复 common 状态时优先补文档 closeout，而非先改代码                      |
| `common.hpp` / math helpers closeout repair | `getMeanStd`、`getMinMax3D`、math helpers                             | 已有主题文档和日志，但缺统一 evaluation                 | helper 与完整入口证据容易混淆                         | 其他模块复用 helper 前，先补 helper 误差预算和路径证据索引               |

### 6.2 暂缓 / 不单独实施（诊断路径记录）

| 主题                                   | 关键入口                                        | 诊断目标                                   | 当前结论 / 重新考虑条件                                                       |
| -------------------------------------- | ----------------------------------------------- | ------------------------------------------ | ----------------------------------------------------------------------------- |
| `centroid` `computeCentroidAndOBB` | `computeCentroidAndOBB` cloud / indices       | 分离前置统计/min-max 与 Eigen solver 成本  | 只有 profile 显示 OBB 是热点，且 Eigen solver 不主导总耗时，才建立 diagnostic / bench-only 路径 |
| common FFT                             | `common/src/fft/kiss_fft.c`、`kiss_fftr.c`  | 判断蝶形计算是否能形成可维护 RVV 路径      | 早期文档认为适配成本高；只有目标 workload 中 FFT 占比显著时再诊断             |
| polynomial fitting                     | `impl/polynomial_calculations.hpp` 的拟合路径 | 判断样本循环、矩阵求解和算法替代的成本边界 | 先评估算法替代或 Eigen 求解成本，再考虑 RVV 片段                              |

### 6.3 暂缓 / 不单独实施

| 主题                                             | 暂缓原因                                                                      | 重新考虑条件                                        |
| ------------------------------------------------ | ----------------------------------------------------------------------------- | --------------------------------------------------- |
| `GaussianKernel::compute`                      | 短循环、分支和`exp` 主导，主题文档已明确保持标量                            | 大规模 profile 证明 kernel 生成本身成为热点         |
| `impl/gaussian.hpp` 泛型模板                   | 通过`std::function` 和泛型 `PointT` 逐点取值，无法稳定假设连续 float 布局 | 有具体点类型专版和生产入口需求                      |
| `computeNDCentroid` / `CentroidPoint`        | MPL / 泛型字段聚合，可维护性和收益边界差                                      | 有明确固定字段批量入口和可测 workload               |
| `impl/eigen.hpp` / small geometry helpers      | 固定小维公式或 Eigen 委托主导，不是长向量主成本                               | 目标平台上 Eigen / libm 链路复核后仍有 PCL 自写热点 |
| `impl/pca.hpp`、`impl/vector_average.hpp` 等 | 特征分解或增量状态主导                                                        | 有 profile 证明 PCL 自写长循环主导而非 solver 主导  |

## 7. 恢复入口 / 状态表

| 顺序 | 主题                                    | 主文件                                                         | 类型                                                       | 当前状态               | 当前结论                                                        | 下一步条件                                                       |
| ---- | --------------------------------------- | -------------------------------------------------------------- | ---------------------------------------------------------- | ---------------------- | --------------------------------------------------------------- | ---------------------------------------------------------------- |
| 1    | `transforms`                          | `common/include/pcl/common/impl/transforms.hpp`              | historical completed / production direct                   | 已完成                 | dense 非 indexed xyz / xyz+normal 生产收益成立                  | 只在需要 indexed 或 PointXY 时重开函数级评估；先复核反汇编归档   |
| 2    | `centroid`                            | `common/include/pcl/common/impl/centroid.hpp`                | historical completed / production direct                   | 已完成                 | 统计与协方差类收益成立，去均值为弱收益边界                      | 未来只在 OBB 或泛型路径有 profile 证据时继续                     |
| 3    | `norms`                               | `common/include/pcl/common/impl/norms.hpp`                   | historical completed / production direct                   | 已完成                 | 连续 float 大维度路径成立，小维和个别 case 不能泛化             | 恢复时先按目标 dim 分布复核 bench 与容差                         |
| 4    | `gaussian`                            | `common/src/gaussian.cpp`                                    | historical completed / production direct with evidence gap | 已完成 / closeout 待补 | rows/cols 板卡收益成立，QEMU 正确性和 local fragment 对拍已记录 | 补 RVV 构建库或对象文件反汇编证据后再视为完全闭合                |
| 5    | `distances`                           | `common/include/pcl/common/distances.h`                      | historical completed / closeout repair                     | 已完成 / closeout 待补 | `getMaxSegment` 板卡收益成立                                  | 补函数级 evaluation、bench 输出合同和反汇编归档                  |
| 6    | `common.hpp` / math helpers           | `common/include/pcl/common/impl/common.hpp`                  | historical completed / shared helper package               | 已完成 / closeout 待补 | common 入口和数学 helper 已有多项板卡/QEMU证据                  | 补函数级 evaluation 或 historical closeout，明确 helper 误差预算 |
| 7    | `transforms` indexed / PointXY        | `common/include/pcl/common/impl/transforms.hpp`              | possible retained-candidate rescreen item                  | 未启动                 | 有已完成 dense 模式可参考，但不能直接升级                       | 有真实 workload 和函数级评估后再考虑                             |
| 8    | `centroid` OBB                        | `common/include/pcl/common/impl/centroid.hpp`                | bench diagnostic candidate                                 | 未启动                 | 可能被 Eigen solver 稀释                                        | profile 证明主成本可由 RVV 覆盖时才建诊断                        |
| 9    | FFT / polynomial / Eigen-adjacent paths | `common/src/fft/*`、`common/include/pcl/common/impl/*.hpp` | deferred / diagnostic only                                 | 暂缓                   | 历史文档已说明适配成本或主成本不适合直接生产 RVV                | 只有目标 workload 证明热点时才重新评估                           |

## 8. 使用说明

后续恢复 common 工作时，优先按第 7 节状态表选择第一条仍需处理的 closeout 或候选。若目标是补齐历史状态，建议先处理 `gaussian`、`distances`、`common.hpp / math helpers` 的证据缺口；若目标是新优化，必须从候选的函数级评估开始，重新证明入口主成本、fallback、反汇编路径和目标硬件收益。

QEMU 日志只用于正确性、构建、日志格式和路径材料；性能结论只引用板卡或目标硬件日志。local fragment、full diagnostic 与 production case 必须分开记录，不能用局部 helper 或算法副本的 speedup 代替公开生产入口结论。
