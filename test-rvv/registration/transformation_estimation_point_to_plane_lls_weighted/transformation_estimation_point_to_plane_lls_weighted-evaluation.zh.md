# registration/transformation_estimation_point_to_plane_lls_weighted 函数级 RVV 评估

## 范围

- 主题：`transformation_estimation_point_to_plane_lls_weighted`
- 主文件：`registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp`
- 公开入口：`TransformationEstimationPointToPlaneLLSWeighted::estimateRigidTransformation`
- 专项目录：`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/`
- 模块依据：`doc-rvv/library-screening/registration/registration-module-second-pass.zh.md` 建议优化队列第四项。

## S0 偏好冻结

- 本轮从干净 topic 状态重新执行；不继续扩写旧产物。
- `test-rvv`、diagnostic（诊断代码）和 prototype（原型代码）使用详细中文注释，英文术语首次出现带中文解释。
- production（生产源码）注释保持克制；S10 前不修改 production。
- 提交策略：默认不创建 commit（提交）；evidence logs（证据日志）采用 summary-only（只在文档和 handoff 摘要路径）。

## 函数级结论

当前结论是 `bench-only/no-production`：本轮已经建立 weighted LLS 的 production-shaped diagnostic（生产形态诊断，尽量复用真实入口数据形状的测试专用诊断）、QEMU correctness（QEMU 正确性验证，不代表真实性能）、bench（性能测试）输出合同、反汇编路径证据和 board performance（板卡真实性能证据）。板卡显示全云顺序扫描（full-cloud）64K 只有弱收益 `1.18x`，全云 256K 退化到 `0.92x`，对应关系索引路径（correspondences）明显退化到 `0.50x` / `0.45x`。因此不能判定 production-ready（可进入生产接入），也没有修改 production。

QEMU 结果显示 std/RVV 两个构建各 6 个专项测试通过，bench checksum（校验和）基本对齐；反汇编显示当前 RVV helper 命中了 `vlse32.v`、`vluxei32.v`、`vle32.v`、`vcompress.vm` 和 `vfredosum.vs`。其中 `vfredosum.vs` 是压缩 buffer tail（临时缓冲尾段）被编译器自动向量化后的 partial vector accumulation（部分向量累加），不是手写 intrinsic（内建函数），必须在 reviewer 审查中重点确认语义和生产可控性。板卡结果已经证明当前诊断方案的收益不稳定，不能接入生产。

## 标量实现说明

公开 API（应用程序接口）共有四个入口：

| 入口 | 权重来源 | 进入 helper 前的检查 |
| --- | --- | --- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | `weights_`，需先调用 `setCorrespondenceWeights` | source/target 点数一致；`weights_.size() == nr_points` |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | `weights_` | `indices_src.size() == cloud_tgt.size()`；`weights_.size() == nr_points` |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | `weights_` | source/target indices 数量一致；`weights_.size() == nr_points` |
| `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` | `correspondence.weight` 被复制到局部 `weights` vector（向量容器） | 没有 `weights_` 尺寸检查；权重来自 correspondences 字段 |

四个公开入口最终都构造 `ConstCloudIterator`，进入 protected helper（受保护辅助函数）：

```text
estimateRigidTransformation(source_it, target_it, weights_it, transformation_matrix)
```

protected helper 的逐点流程如下：

1. 初始化 `ATA` 为 6x6 double 矩阵、`ATb` 为 6x1 double 向量，并清零。
2. 每个 source/target pair（点对）先做逐点 finite check（有限值检查）：检查 source `x/y/z`、target `x/y/z`、target `normal_x/y/z`。当前 production 不检查 weight 是否有限。
3. 如果任一被检查字段不是 finite，标量循环递增 `source_it`、`target_it` 和 `weights_it` 后 `continue`，该点不贡献 `ATA/ATb`。
4. 对有效点读取 `sx/sy/sz`、`dx/dy/dz`，并先计算 `nx = target.normal[0] * weight`、`ny = target.normal[1] * weight`、`nz = target.normal[2] * weight`。这里权重先乘到 normal（法线），后续所有公式都使用加权 normal。
5. 使用加权 normal 构造 point-to-plane LLS（点到平面线性最小二乘）的一行：

```text
a = nz * sy - ny * sz
b = nx * sz - nz * sx
c = ny * sx - nx * sy
d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz
```

6. `ATA` 只累加上三角 21 项，例如 `a*a`、`a*b`、`a*nx`、`nx*ny`；`ATb` 累加 6 项，例如 `a*d`、`nx*d`。这些累加是主要可批处理阶段，但也是数值规约边界。
7. 循环结束后把 `ATA` 下三角从上三角补齐。
8. 每次公开 estimate 调用只执行一次 `ATA.inverse() * ATb` Eigen solve（Eigen 求解器）。这是 6x6 小矩阵，不是 VL chunk（可变向量长度分块）批处理热点。
9. 每次公开 estimate 调用只执行一次 `constructTransformationMatrix`，把 `x(0..5)` 的欧拉角和平移构造成 4x4 矩阵。该函数内部调用少量 `sin/cos`，调用频率是每次 estimate 一次，不随点数增长。

源码层面的数据流需要和诊断实现区分开看。production 源码的四个公开入口在进入 protected helper 前负责准备不同的 iterator（迭代器）和权重来源：全云入口直接按点云顺序构造 source/target iterator；indices（索引）入口把一个或两个点云访问序列交给 iterator；correspondences（对应关系）入口用 `index_query/index_match` 构造 source/target iterator，并把 `correspondence.weight` 复制到局部权重 vector。进入 protected helper 后，这些差异被统一成 `source_it`、`target_it`、`weights_it` 三个同步前进的逐点流。

RVV 诊断不能直接依赖 iterator 抽象高效取数，所以把源码中的统一逐点流重新显式拆成两条可向量化数据流：全云顺序扫描路径和对应关系索引扫描路径。前者用于验证规则顺序点对，后者用于验证 indexed row（带索引行）如何展开并 gather（离散加载）点字段；这两条诊断路径都对应 production 的真实入口形态，但不是 production 直连分流。

## 函数族评估表

| 函数 / 路径 | 当前决策 | 覆盖与边界 |
| --- | --- | --- |
| 全云顺序扫描 normal-equation 构造 | diagnostic 已建，暂不接 production | 连续 `PointNormal`、`float` 权重 vector、`n >= 64`、`vlmax_e32m2 <= 64`。 |
| 对应关系索引 normal-equation 构造 | diagnostic 已建，暂不接 production | 有效 correspondence 先展开 source index、target index 和 weight，再进行 gather。 |
| Eigen solve / `constructTransformationMatrix` | 保留标量 | 每次 estimate 调用一次，规模固定，当前没有证据说明值得手写 RVV。 |
| 泛型 production 模板 | 未接入 | 未闭合泛型点类型 traits、`Scalar=double`、真实 dispatch（分流逻辑）和目标硬件收益。 |

## RVV 诊断设计

当前 RVV candidate（RVV 候选链路）位于 `transformation_estimation_point_to_plane_lls_weighted_diag.hpp`，只在 `test-rvv` 中使用。

### 两种诊断数据流的差异

全云顺序扫描（full-cloud）和对应关系索引路径（correspondences）最终都在构造同一组 weighted normal-equation row（带权法方程行），也都复用 finite mask（有限值掩码）、`weight * normal`、`a/b/c/d` 公式、`vcompress`（向量压缩）和 buffer tail（缓冲尾段）。区别在于 row 如何进入 RVV chunk（可变向量长度分块）：

- 全云顺序扫描是一一对应扫描。第 `i` 个 source 点、第 `i` 个 target 点和 `weights[i]` 直接形成一行，lane 顺序等于数组下标顺序。`PointNormal` 是 AoS（结构数组）布局，所以点字段用 stride load（跨步加载），权重用 contiguous load（连续加载）。这条路径没有额外 index 展开，主要成本来自 stride load、mask、公式 staging、压缩和 tail。
- 对应关系索引路径先把公开入口中的 `pcl::Correspondence` 列表变成 row 列表。candidate 需要标量扫描每个 correspondence，跳过负 index 或越界 index，把 `index_query`、`index_match` 和 `correspondence.weight` 展开成三个连续 vector，再把 index 转成 `PointNormal` 字节偏移并用 gather（离散加载）读取 source/target 字段。lane 顺序等于 correspondence 列表顺序，而不等于点云物理下标顺序。

因此，全云顺序扫描路径主要验证连续 row 的批处理收益；对应关系索引路径验证 indexed row（带索引行）能否保持公开入口语义，但它天然多了 index/weight 展开、offset 计算和 gather 不规则访存。当前板卡结果只能证明对应关系索引 candidate 整体退化，不能单独判断退化来自 gather、展开、`vcompress`、buffer 写回、自动规约或 bench case 构造。

### 全云顺序扫描数据流

1. fallback gate（回退验收条件）：`n >= 64`、`__riscv_vsetvlmax_e32m2() <= 64`、点数量可转换成 32-bit byte offset（32 位字节偏移）。
2. strip-mined loop（分块循环）按 `vl = vsetvl_e32m2(n - i)` 处理 VL chunk。
3. source/target `PointNormal` 是 AoS（结构数组）布局；用 `vlse32.v` stride load（跨步加载）读取 source `x/y/z`、target `x/y/z`、target normal `x/y/z`。
4. 权重是连续 `std::vector<float>`；用 `vle32.v` contiguous load（连续加载）读取当前 chunk 的 weight。
5. mask（掩码）按生产语义只检查 source/target 坐标和 target normal 有限性，不检查 weight。
6. `staged_weighted_formula` 先做 `weight * normal`，再计算 `a/b/c/d`。手写部分使用 `vfmul.vv`、`vfadd.vv`、`vfsub.vv`，暂不手写 fused multiply-add（融合乘加）intrinsic。
7. `vcpop.m` 计算有效 lane 数；`vcompress.vm` 把 `a/b/c/d/nx/ny/nz` 保序压缩到固定 64 lane buffer。
8. buffer tail（缓冲尾段）在源码上按压缩后顺序消费。反汇编显示 GCC `-O3` 把部分 buffer 累加自动变成 `vfredosum.vs`，这是 partial vector accumulation（部分向量累加），会改变线性累加树；当前只作为诊断观察，不作为 production 语义承诺。

### 对应关系索引数据流

1. candidate 先标量扫描 correspondences，过滤负 index 和越界 index，展开 `src_indices`、`tgt_indices` 和 `weights` 三个连续 vector。这个展开成本包含在 bench case 计时内，因为当前 RVV gather 方案需要它。
2. RVV chunk 用 `vle32.v` 读取展开后的 index 和 weight。
3. source/target 点字段用 `vluxei32.v` indexed gather（离散加载）读取；normal 与 weight 相乘后进入同一套 finite mask、公式 staging、`vcompress` 和 buffer tail。
4. correspondences 的乱序、重复输入通过专项测试覆盖；非法 index 只在 defensive helper 中跳过，本轮没有单独作为测试 case，因为 production 当前公开入口通常假设有效 correspondences。

## 方案取舍审计

| 方案 | 当前处理 | 理由 | 语义风险 | 还需要的证据 |
| --- | --- | --- | --- | --- |
| 直接向量规约 raw lane | 暂缓 | 可减少 buffer 写回和 tail 成本，但会直接改变 21 个 `ATA` 项和 6 个 `ATb` 项的累加树。 | 6x6 solve 对累加误差敏感，可能改变最终矩阵；需要明确误差预算。 | same-chain（同构链路）对抗样本、绝对/相对误差预算、目标硬件 bench、热点 asm。 |
| 部分 vector accumulation | 诊断中被采用但未手写 | 源码写成 buffer tail；反汇编显示 `-O3` 自动把压缩 buffer 累加变成 `vfredosum.vs`。这提供了“压缩后部分规约”的可观察证据。 | 不是显式 intrinsic，编译器版本和优化选项可能改变；累加顺序不同于 production 标量。 | 需要固定 asm gate、更多 adversarial case、板卡结果；若接 production，应考虑手写可控规约或禁用自动规约。 |
| fused multiply-add intrinsic | 暂缓 | 当前手写公式保留 `vfmul` + `vfadd/vfsub`，便于审查 `weight * normal` 和 `a/b/c/d`。全二进制中有 FMA，但热点 helper 的手写 staging 公式主要不是 FMA；部分 FMA 来自 Eigen 或自动生成代码。 | FMA contraction（融合乘加收缩）可能改变 float 中间舍入，特别是 `d` 的六项和。 | 比较标量/RVV 热点 asm、加 FMA same-chain 测试、边界样本、目标硬件 bench。 |
| 数学函数 / 矩阵构造向量化 | 暂缓 | `constructTransformationMatrix` 和 `sin/cos` 每次 estimate 只调用一次，不随点数增长；Eigen solve 也是固定 6x6。 | 手写数学函数会引入精度、特殊值和维护风险，收益上限很低。 | profile 证明 solve/矩阵构造成为热点，且目标硬件上 normal-equation 已不再主导时再评估。 |

这里的 `vcompress + buffer + tail` 不是被性能证据证明过的最佳方案，而是本轮为了保持 finite lane 顺序、降低首轮诊断语义风险而采用的保守方案。它避免在没有误差预算的情况下直接改变所有 `ATA/ATb` 项的跨 lane 累加树，但也引入了额外 store/load 和自动规约风险。下一轮如果要判断“当前实现方式是否拖慢”，应把这套方案与无压缩 masked reduction（带掩码规约）、手写长期向量累加、禁用自动 tail 向量化和 FMA 版本分别对照。

## 测试计划与结果

专项测试：`test_transformation_estimation_point_to_plane_lls_weighted.cpp`

| 测试 | 层级 | 作用 |
| --- | --- | --- |
| `StdDiagnosticMatchesPublicEstimator` | reference path | 证明全云顺序扫描标量诊断与公开 estimator 一致，权重来自 `setCorrespondenceWeights`。 |
| `StdCorrespondencesMatchesPublicEstimator` | reference path / entry shape（入口形态） | 证明对应关系入口直接使用 `correspondence.weight`，不同于 `weights_` 路径。 |
| `FullCloudCandidateMatchesStd` | RVV candidate | 大规模连续 `PointNormal` + 连续权重对拍，RVV 构建要求命中 staging。 |
| `CorrespondenceCandidateMatchesStd` | RVV candidate / gather | 乱序、重复对应关系和 weight 字段对拍，覆盖 index/weight 展开和 gather。 |
| `SmallInputFallsBackForIsolatedSizeGate` | fallback | 单独覆盖 `n < 64` 规模 gate，不混入 invalid 数据。 |
| `InvalidLaneMaskMatchesStd` | finite mask | 单独覆盖 NaN/Inf lane 被剔除；weight 保持有限，因为 production 不检查 weight。 |

QEMU `run_test_compare` 结果：std 构建 6 个测试通过，RVV 构建 6 个测试通过。QEMU 只证明构建、正确性、日志形状和 RVV 路径命中，不证明真实性能。

## Bench 计划与 QEMU 日志形状

专项 bench：`bench_transformation_estimation_point_to_plane_lls_weighted.cpp`

输入构造：

- source 是确定性的解析二次曲面 `PointNormal`，target 由 `pcl::transformPointCloudWithNormals` 使用温和刚体变换生成。
- full-cloud 权重是确定性周期序列 `0.55 + 0.07 * (i % 9)`，覆盖小于、等于和大于 1 的 normal 缩放。
- correspondences 是 deterministic subset（确定性子集），包含非连续、重复 index 和不同 `correspondence.weight`；当前不模拟非法 index，也不模拟任意乱序 target pairing（目标配对）。它更适合暴露 index/weight 展开和 gather 路径成本，不代表所有真实 correspondence 分布。

测量边界：

- 输入 cloud、target、weights 和 correspondences 在计时前构造完成。
- 每次迭代测量 candidate estimate：normal-equation 构造、Eigen solve 和 `constructTransformationMatrix`。
- full-cloud case 不包含权重生成；correspondences case 包含 candidate 内部的 index/weight 展开，因为这是当前 gather 方案进入 RVV 的必要成本。

| case | 入口 | 规模 | 证明点 | 不能证明什么 |
| --- | --- | --- | --- | --- |
| `weighted lls full-cloud pointnormal 65536` | `estimate_candidate_full` | 64K | 全云顺序扫描的中等规模路径：连续 row、stride load、contiguous weight load 和 mask/staging。 | 不能证明泛型点类型、真实 production dispatch 或目标硬件性能。 |
| `weighted lls correspondences pointnormal 65536` | `estimate_candidate_correspondences` | 64K 输入点的 subset | 对应关系索引路径能否通过 index/weight 展开和 gather 正确形成 weighted equations。 | 不能单独归因 gather、展开、压缩或 tail 哪一项最慢。 |
| `weighted lls full-cloud pointnormal 262144` | `estimate_candidate_full` | 256K | 放大全云顺序扫描后检查 setup 外的 staging/tail 成本能否摊薄。 | 不能单独证明 64K 弱收益可以外推到更大规模；板卡结果已经显示该规模退化。 |
| `weighted lls correspondences pointnormal 262144` | `estimate_candidate_correspondences` | 256K 输入点的 subset | 大规模对应关系索引路径的日志形状和 checksum。 | 不能代表所有真实 correspondence 分布。 |

QEMU bench compare 可解析，std/RVV checksum 基本对齐。QEMU timing 显示 RVV 构建更慢，但这只作为日志形状和路径证据，不写成真实性能结论。

板卡 `board_smoke` 已通过专项测试并生成真实性能数据：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| 全云顺序扫描 64K | 7.1234 | 6.0440 | 1.18x | 弱收益，只覆盖中等规模连续输入。 |
| 对应关系索引 64K | 5.2553 | 10.4137 | 0.50x | index/weight 展开、gather、compress/tail 组合明显退化；主因尚未拆分定位。 |
| 全云顺序扫描 256K | 28.0058 | 30.3425 | 0.92x | 放大规模后退化，说明 64K 弱收益不稳定。 |
| 对应关系索引 256K | 20.9287 | 46.2499 | 0.45x | 大规模 indexed row 路径退化更明显；仍不能单独归因 gather 或 buffer。 |

这些板卡结果来自 Milkv-Jupiter，日志在 `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/analyze_bench_compare.log`。由于收益只在一个 full-cloud 中等规模 case 出现，且幅度属于弱收益区间，不能覆盖 correspondences 入口和更大规模退化。

## 反汇编热点归属

反汇编文件：

- `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/build/asm/riscv/bench_transformation_estimation_point_to_plane_lls_weighted_rvv.full.asm`
- `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/build/asm/riscv/bench_transformation_estimation_point_to_plane_lls_weighted_rvv.asm`

热点 helper 归属：

- `accumulate_candidate_full` 符号内出现 `vlse32.v` 读取 `PointNormal` 字段、`vle32.v` 读取 weight、`vfmul.vv/vfadd.vv/vfsub.vv` 计算加权公式、`vcpop.m` 和 `vcompress.vm` 压缩有效 lane。
- 同一 helper 内还出现 `vfredosum.vs`，归因于压缩 buffer tail 的自动向量化 partial accumulation，不是手写 raw-lane direct reduction。
- `accumulate_candidate_correspondences` 符号存在；全二进制和 helper 区域可见 `vluxei32.v` gather。需要 reviewer 进一步用符号范围或更细 asm 脚本确认 gather 指令是否全部来自当前 helper，而不是 STL vector、Eigen 或 bench harness（性能测试外壳）。
- 全二进制中存在大量 `vfmadd/vfmacc` 和其它 `vfred*`，其中相当一部分位于 Eigen kernel、checksum/bench 周边或编译器自动向量化代码。不能把这些写成当前手写 staging 公式采用了 FMA。

当前负向性能归因仍应写成受证据约束的假设：板卡已经证明当前 candidate 在 full-cloud 256K 和 correspondences 上变慢，但还不能单独确认主因。可能来源包括 `vcompress` 后 buffer 读写、自动 vector reduction 的额外 `vsetvli`/宽窄转换、correspondences 的 index/weight 展开、`vluxei32.v` 不规则访存，或 Eigen solve / bench harness 占比稀释。确认主因需要消融 bench 或 profile（性能剖析）。

worker2 复核后应把这条归因边界视为当前文档的关键审查点：反汇编能够证明这些指令形状存在，板卡能够证明整体退化，但二者合起来仍不能证明某个单项成本是主因。若下一轮继续，应优先补四个消融 case：只做 correspondences 展开不 gather、固定连续 index 但保留 gather 形态、去掉 `vcompress`/buffer 写回的 masked reduction 原型、禁用或固定自动 `vfredosum` tail。

## EvidenceDecision

`EvidenceDecision = bench-only/no-production`。

证据支持：

- 专项 diagnostic 已建，且不修改 production。
- QEMU correctness 通过，说明 std/RVV 构建在测试矩阵下语义可对齐。
- QEMU bench 输出合同可解析，checksum 基本对齐。
- 反汇编证明当前 RVV helper 路径存在，并揭示了自动 partial vector accumulation。
- 板卡专项测试通过，证明 RVV candidate 在目标硬件上能正确运行。

证据不支持：

- board/target-hardware benchmark（板卡或目标硬件性能测试）显示收益不稳定：full-cloud 64K 为 `1.18x`，full-cloud 256K 为 `0.92x`，correspondences 为 `0.50x` / `0.45x`。
- 自动 `vfredosum` 规约改变累加树，当前只由专项容差测试覆盖，尚未形成 production 误差预算。
- 泛型 production dispatch、fallback 和点类型 traits 未闭合。

因此本轮不进入 production integration loop（生产接入闭环）。当前保留为 bench-only diagnostic（仅性能诊断主题），下一轮若继续投入，应先做消融诊断，而不是把当前方案接入 production。

## 遗留风险与后续条件

板卡性能已经闭合为负向生产信号。当前 `make board_smoke` 在 Milkv-Jupiter 上通过，但只有 full-cloud 64K 达到 `1.18x`，full-cloud 256K 与 correspondences 均退化。若后续重做，需要新增消融 bench：只测 weight load、只测 stride staging、禁用/固定自动规约、去掉 `vcompress` 或分离 correspondences 展开成本。

自动 partial vector accumulation 尚未语义闭合。反汇编显示 `vfredosum.vs` 位于 helper 内，说明 tail 不再是严格线性标量累加。当前 QEMU 测试只证明选定样本矩阵近似一致；production 前需要 adversarial case、误差预算和明确的 asm gate。

FMA 归属尚需更细脚本确认。全二进制里有大量 `vfmadd/vfmacc`，但不能默认归因到当前手写 staging 公式。下一轮应按符号范围输出 FMA、reduction、vcompress、gather 的归属摘要。

correspondences 负向归因仍是假设。当前设计包含 index/weight 展开、gather、vcompress 和 partial reduction 多个成本源；若板卡结果慢，需要 profile（性能剖析）或消融 bench 分别关闭 gather、关闭压缩、关闭自动规约，才能确认主要原因。

因此当前 closeout 可以接受 bench-only/no-production（仅性能诊断/不接入生产）生产决策；如果目标是回答“当前 RVV 实现方式是否不好导致退化”，则不应直接接 production，也不应只扩写文档，应先做消融 bench。
