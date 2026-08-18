# registration/transformation_estimation_symmetric_point_to_plane_lls 函数级 RVV 评估

## 范围

- 主题：`transformation_estimation_symmetric_point_to_plane_lls`
- 主文件：`registration/include/pcl/registration/impl/transformation_estimation_symmetric_point_to_plane_lls.hpp`
- 公开入口：`TransformationEstimationSymmetricPointToPlaneLLS::estimateRigidTransformation`
- 专项目录：`test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls/`
- 模块依据：`doc-rvv/library-screening/registration/registration-function-evaluation-queue.zh.md` 建议优化队列第五项。

## S0 偏好冻结

- `test-rvv`、diagnostic（诊断代码）和 prototype（原型代码）使用详细中文注释，英文术语首次出现带中文解释。
- production（生产源码）注释保持克制；S10 前不修改 production。
- 提交策略：默认不创建 commit（提交）；evidence logs（证据日志）采用 summary-only（只在文档和 handoff 摘要路径）。
- QEMU（仿真器）只作为 correctness（正确性）、路径和日志形状证据；性能结论只来自板卡。

## 函数级结论

当前结论是 `production-ready/generic-normal-full-cloud-and-source-indexed`（泛型 normal 全云和 source 单侧索引入口可生产接入）。本轮 production integration loop（生产接入闭环）用 row-source policy（行来源策略）统一两条已获证据支持的数据流：full-cloud（全云顺序扫描）和 source indices + target full-cloud（source 索引 + target 全云）。后续 dispatch structure cleanup（分流结构收口）把公开 overload 整理为“语义检查 -> RVV 短路 -> Std fallback”：原标量路径集中到 `estimateSymmetricPointNormal*Std` helper，RVV 路径集中到 `estimateSymmetricPointNormal*RVV` helper。`PointSource` 和 `PointTarget` 必须分别满足 `x/y/z/normal_x/normal_y/normal_z` 都是单个 `float` 字段、POD（普通数据布局）/ standard-layout（标准布局）和 offset alignment（字段偏移对齐）条件，且 `Scalar=float`。source+target indices、correspondences 和 `Scalar=double` 全部保持标量 fallback（回退路径）。

公共 trait 重构后，上述点类型 gate 改由 `pcl::rvv::RVVXYZNormalFloatLayout<PointT>` 提供，本地 `SymmetricXYZNormalFloatLayout` 已删除；32-bit byte offset 边界改用 `pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()`。本轮 source-indexed production 复用这些 gate，并保持 valid-index-only 合同：RVV 不做负数或越界 index 过滤；source+target indices、correspondences 和 `Scalar=double` 不纳入 RVV production。

QEMU correctness（QEMU 正确性验证）通过，bench（性能测试）输出合同可解析，反汇编证明真实 public estimator（公开估计器）调用 production helper 模板 `estimateSymmetricPointNormalRowsRVV<RowSourcePolicy>`。Milkv-Jupiter 板卡 `board_smoke` 25 个专项测试通过；production-direct full-cloud `PointNormal` 64K / 256K 为 `2.71x` / `2.70x`，`PointXYZINormal` 为 `2.71x` / `2.48x`；production-direct source-indexed `PointNormal` 为 `2.11x` / `1.87x`，`PointXYZINormal` 为 `2.10x` / `1.90x`。dual-indices diagnostic 使用独立 target index stream 后为 `0.80x` / `0.63x`；correspondences diagnostic 仍退化为 `0.77x` / `0.86x`，不接 production，退化主因保持为待消融假设。

## 标量实现说明

公开 API（应用程序接口）共有四个入口：

| 入口 | 数据流 | 进入 helper 前的检查 |
| --- | --- | --- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | 全云顺序扫描，第 `i` 个 source 对第 `i` 个 target。 | source/target 点数一致。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | source 使用 indices，target 顺序扫描。 | `indices_src.size() == cloud_tgt.size()`。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | source/target 都使用 indices。 | 两个 indices 数量一致。 |
| `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` | 使用 `index_query/index_match` 指定点对。 | 当前入口依赖调用方提供有效 correspondences。 |

四个入口的 Std helper 最终构造 `ConstCloudIterator`，进入共同标量 helper：

```text
estimateRigidTransformation(source_it, target_it, transformation_matrix)
```

protected helper 的逐点流程如下：

1. 初始化 6x6 `ATA` 和 6x1 `ATb`，并把 `ATA` 当作 selfadjoint upper（自伴随上三角视图）累加。
2. 每个 source/target pair（点对）读取 `p = source.xyz`、`q = target.xyz`、`n1 = source.normal`、`n2 = target.normal`。
3. 如果 `enforce_same_direction_normals_` 为真，先计算 `n1.dot(n2)`：点积非负时 `n = n1 + n2`，点积为负时 `n = n1 - n2`；如果关闭该开关，直接使用 `n = n1 + n2`。
4. finite check（有限值检查）发生在 `n` 合成之后：`p`、`q` 和 `n` 任一分量不是 finite 时跳过该点。
5. 对有效点构造 `v = [(p + q).cross(n), n]`。展开为：

```text
a = (py + qy) * nz - (pz + qz) * ny
b = (pz + qz) * nx - (px + qx) * nz
c = (px + qx) * ny - (py + qy) * nx
d = (qx - px) * nx + (qy - py) * ny + (qz - pz) * nz
```

6. `ATA += v * v.transpose()`，`ATb += v * d`。这是随点数增长的主循环，也是 RVV 可覆盖阶段。
7. 循环结束后用 `M.ldlt().solve(ATb)` 解 6x6 小系统；`constructTransformationMatrix` 把 6 个参数转成 4x4 变换矩阵。solve 和矩阵构造每次 estimate 只执行一次，当前保留标量。

源码层面的数据流需要和诊断实现区分开看。production helper 只看到两个同步前进的 iterator；RVV 诊断为了暴露取数成本，显式拆成全云顺序扫描、source indices + target full-cloud、source indices + target indices 和对应关系索引扫描。全云路径使用 stride load（跨步加载）读取 AoS（结构数组）字段；source-indices 现在通过 test-rvv-only `SourceIndexedRowSource` policy/template 进入共用 row pipeline，只让 source 侧 gather；dual-indices 使用两条不同的 source/target index stream，让两侧 gather 和 row 配对语义一起被验证；correspondences 还包含 query/match 展开后的 gather。

## RVV 诊断设计

当前 RVV candidate（RVV 候选链路）位于 `transformation_estimation_symmetric_point_to_plane_lls_diag.hpp`，只在 `test-rvv` 中使用。

| 路径 | 当前处理 | 证据边界 |
| --- | --- | --- |
| 全云顺序扫描 | `n >= 64`、`vlmax_e32m2 <= 64` 且 32-bit byte offset 可表达时走 RVV。production 使用 traits offset 分别读取 source/target 坐标和 normal。 | production direct 已接入 generic normal / `float` full-cloud，板卡 `PointNormal` 为 `2.71x` / `2.70x`，`PointXYZINormal` 为 `2.71x` / `2.48x`。 |
| source indices + target full-cloud | source 侧加载 index stream 后 gather 点字段，target 侧保持顺序 stride load。 | production direct 已接入 valid-index-only generic normal / `float` source-indexed，板卡 `PointNormal` 为 `2.11x` / `1.87x`，`PointXYZINormal` 为 `2.10x` / `1.90x`。 |
| source indices + target indices | source/target 两侧各自加载独立 index stream 后 gather 点字段，不解析 `pcl::Correspondence`，不展开 `correspondence.weight`。 | test-rvv diagnostic 层板卡 `0.80x` / `0.63x`，说明双侧 gather 成本可与 correspondence 展开分离，但当前已经负向；不接 production。 |
| 对应关系索引 | 先展开 `src_indices/tgt_indices`，再用 `vluxei32.v` gather 点字段。 | QEMU correctness 通过，板卡 `0.77x` / `0.86x` 退化，不接 production；退化仍是多因素待消融假设。 |
| 法线同向选择 | RVV 中用 dot mask（点积掩码）在 `n1 + n2` 和 `n1 - n2` 之间 merge（合并）。 | 测试覆盖默认开启和显式关闭；未覆盖所有法线极端边界。 |
| `ATA/ATb` 累加 | RVV 只 staging `a/b/c/d/n`，压缩后交给 scalar tail。 | 反汇编显示 tail 被编译器自动形成 `vfredosum.vs` partial vector accumulation（部分向量规约），不是 production 语义承诺。 |
| Eigen solve / 矩阵构造 | 保留标量。 | 每次 estimate 一次，当前没有证据说明值得向量化。 |

## 方案取舍审计

| 方案 | 当前处理 | 理由 | 风险与后续证据 |
| --- | --- | --- | --- |
| 直接 vector reduction（向量规约）累加 27 项 | 暂缓 | 会改变 `ATA/ATb` 的累加树，需先定义误差预算。 | 需要 adversarial case（对抗样本）、误差预算和目标硬件 bench。 |
| `vcompress + buffer + scalar tail` | 采用为诊断方案 | 保持有效 lane 顺序，便于与 production 逐点扫描对齐。 | 额外 store/load 成本高；GCC 还可能自动生成 `vfredosum.vs`，生产接入前必须显式控制。 |
| 全云 production direct | 已接入 generic normal production | 板卡 `PointNormal` production-direct `2.71x` / `2.70x`，`PointXYZINormal` production-direct `2.71x` / `2.48x`，真实公开入口证据闭合。 | 仅覆盖 `Scalar=float` / full-cloud，且点类型必须满足 normal traits gate。 |
| source-indices production direct | 已接入 generic normal production | 板卡 `PointNormal` production-direct `2.11x` / `1.87x`，`PointXYZINormal` production-direct `2.10x` / `1.90x`，真实公开入口证据闭合。 | 仅覆盖 valid-index-only、`Scalar=float`、source indices + target full-cloud；不证明非法 index、dual-indices 或 correspondences。 |
| dual-indices diagnostic | 保留在 test-rvv | 使用独立 target index stream 后，板卡 `0.80x` / `0.63x`，说明双侧 gather 成本可与 correspondence 展开分离，但当前不足以作为 production 候选。 | 没有 production direct 证据；非法 index 行为不覆盖。 |
| correspondences production | 不接入 | 板卡 `0.77x` / `0.86x` 明确负向。 | 退化仍是多因素假设，不能归因为 gather 单一原因。 |
| fused multiply-add（融合乘加）intrinsic | 暂缓 | 当前手写公式保留 `vfmul + vfadd/vfsub`，可读性和语义审查更直接。 | 若后续全云 production 候选需要更高收益，再补 FMA same-chain（同构链路）测试和反汇编归属。 |

## 测试计划与结果

专项测试：`test_transformation_estimation_symmetric_point_to_plane_lls.cpp`

| 测试 | 层级 | 作用 |
| --- | --- | --- |
| `StdDiagnosticMatchesPublicEstimatorWithNormalDirectionGate` | production direct / reference path | RVV 构建中覆盖真实 public estimator 的 production 分流；std 构建中覆盖标量公开入口。 |
| `StdDiagnosticMatchesPublicEstimatorWithoutNormalDirectionGate` | production direct / branch | 覆盖 `setEnforceSameDirectionNormals(false)` 入口语义。 |
| `ProductionDirectFullCloudMatchesDiagnosticCandidate` | production direct | 真实 public full-cloud estimator 与同构 diagnostic candidate 对拍。 |
| `ProductionDirectFullCloudWithoutNormalDirectionGateMatchesCandidate` | production direct / branch | 真实 public full-cloud estimator 在关闭 normal gate 后与同构 candidate 对拍。 |
| `ProductionDirectSmallInputFallsBackToScalar` | production fallback | 真实 public full-cloud 小输入回到标量。 |
| `ProductionDirectInvalidLaneMatchesDiagnosticCandidate` | production direct / finite mask | 真实 public full-cloud invalid lane 与同构 candidate 对拍。 |
| `ProductionDirectPointXYZINormalFullCloudMatchesPointNormalReference` | production direct / generic normal | `PointXYZINormal` full-cloud 公开入口与 `PointNormal` reference 对拍，覆盖 traits offset 和 normal 字段映射。 |
| `ProductionDirectMixedNormalLayoutsMatchPointNormalReference` | production direct / generic normal | source/target 使用不同 normal layout，证明两侧 traits gate 和 offset 分别计算。 |
| `ScalarDoubleFallbackStillMatchesGroundTruth` | production fallback | `Scalar=double` 模板实例保持标量并保持几何精度。 |
| `ScalarDoublePointXYZINormalFallbackStillMatchesGroundTruth` | production fallback / generic normal | `PointXYZINormal, double` 保持标量，防止泛型 normal 扩展越过 `Scalar=float` gate。 |
| `ProductionDirectSourceIndicesMatchesCandidate` | production direct | source indices + target full-cloud 真实公开入口命中 production policy，并与 diagnostic candidate 对拍。 |
| `ProductionDirectPointXYZINormalSourceIndicesMatchesPointNormalReference` | production direct / generic normal | source-indexed 入口中 source gather 与 target stride 分别使用 source/target normal layout。 |
| `ScalarDoubleSourceIndicesFallbackStillMatchesReference` | production fallback | source-indexed `Scalar=double` 仍保持标量并与 reference 对拍。 |
| `ProductionDirectMixedNormalLayoutsSourceIndicesMatchPointNormalReference` | production direct / generic normal | source-indexed mixed layout 证明 source gather 和 target stride 分别使用两侧 offset。 |
| `ProductionDirectSourceIndicesInvalidLaneMatchesReference` | production direct / finite mask | source-indexed valid-index-only 公共入口在 NaN/Inf lane 下与 reference 一致。 |
| `SourceAndTargetIndicesOverloadRemainsScalarFallback` | production fallback | source+target indices overload 保持标量。 |
| `SourceIndicesCandidateMatchesStd` | indexed diagnostic | source gather + target stride 的 test-rvv 对拍。 |
| `DualIndicesCandidateMatchesStd` | indexed diagnostic | 双侧 gather 的 test-rvv 对拍，不包含 correspondence parsing。 |
| `StdCorrespondencesMatchesPublicEstimator` | reference path / correspondences | 证明对应关系入口的标量诊断匹配公开 estimator。 |
| `FullCloudCandidateMatchesStd` | RVV candidate | 大规模连续 `PointNormal` 对拍，RVV 构建要求命中 staging。 |
| `FullCloudCandidateWithoutNormalDirectionGateMatchesStd` | RVV candidate / branch | 覆盖关闭同向法线 gate 的 RVV 路径。 |
| `CorrespondenceCandidateMatchesStd` | RVV candidate / gather | 乱序、重复对应关系对拍，覆盖 index 展开和 gather。 |
| `SmallInputFallsBackForIsolatedSizeGate` | fallback | 单独覆盖 `n < 64` 规模 gate。 |
| `InvalidLaneMaskMatchesStd` | finite mask | 覆盖 NaN/Inf lane 被剔除，含 source normal 出错后通过合成 `n` 剔除。 |

QEMU `run_test_compare` 结果：std 构建 25 个测试通过，RVV 构建 25 个测试通过。板卡 `board_smoke` 中 RVV test 25 个测试通过。

## Bench 计划与结果

专项 bench：`bench_transformation_estimation_symmetric_point_to_plane_lls.cpp`

输入构造：

- source 是确定性的解析二次曲面 `PointNormal`，target 由 `pcl::transformPointCloudWithNormals` 使用温和刚体变换生成。
- `PointXYZINormal` production-direct case 由同一 source/target 几何点云转换得到，只改变 AoS layout（结构数组布局）和字段 offset。
- target 每隔固定间隔翻转 normal，用于暴露 `enforce_same_direction_normals` 分支成本。
- indexed 消融使用有效、非连续且含重复的 `pcl::Indices`。source-indices case 在计时前把 target 压成紧凑全云，使计时内只有 source gather 和 target stride load；dual-indices case 直接传 source/target 两条不同的 index vector，不包含 correspondence parsing 或 weight 展开。
- correspondences 是 deterministic subset（确定性子集），包含非连续和重复 index；它用于暴露 index 展开和 gather 成本，不代表所有真实 correspondence 分布。

测量边界：

- 输入 cloud、target、indices 和 correspondences 在计时前构造完成。
- 每次迭代测量 candidate estimate：normal-equation 构造、Eigen LDLT solve 和矩阵构造。
- source-indices / dual-indices case 只计入各自 index/gather/stride 形态，不包含 correspondence parsing；它们是 valid-index-only 诊断，不覆盖负数或越界 index 行为。
- correspondences case 包含 candidate 内部 index 展开成本，因为这是当前 gather 方案的必要入口成本。

QEMU bench compare 可解析，std/RVV checksum 基本对齐。QEMU timing 显示 RVV 更慢，但这只作为日志形状和路径证据，不作为真实性能结论。

板卡结果来自 Milkv-Jupiter：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| production-direct `PointNormal` full-cloud 64K | 16.0400 | 5.9216 | 2.71x | 真实公开入口收益成立，接 production。 |
| production-direct `PointXYZINormal` full-cloud 64K | 16.0146 | 5.9035 | 2.71x | 泛型 normal traits-gated 入口收益成立，接 production。 |
| production-direct `PointNormal` source-indices 64K | 12.0123 | 5.6970 | 2.11x | 有效 source index stream 下真实公开入口收益成立，接 production。 |
| production-direct `PointXYZINormal` source-indices 64K | 11.9900 | 5.7087 | 2.10x | 泛型 normal source-indexed 入口收益成立，接 production。 |
| diagnostic full-cloud 64K | 9.5015 | 5.5691 | 1.71x | 诊断链路仍有收益。 |
| diagnostic source-indices 64K | 7.9827 | 6.2866 | 1.27x | 单侧 gather 诊断仍有正向信号。 |
| diagnostic dual-indices 64K | 8.7335 | 10.9079 | 0.80x | 独立 target stream 后双侧 gather 转负。 |
| diagnostic correspondences 64K | 7.8728 | 10.2090 | 0.77x | 完整对应关系入口仍退化。 |
| production-direct `PointNormal` full-cloud 256K | 63.9674 | 23.7303 | 2.70x | 放大规模后生产收益仍稳定。 |
| production-direct `PointXYZINormal` full-cloud 256K | 64.0034 | 25.7566 | 2.48x | 泛型 normal layout 放大规模后仍有收益。 |
| production-direct `PointNormal` source-indices 256K | 47.7063 | 25.4518 | 1.87x | 放大规模后有效 source index stream 仍有 production direct 收益。 |
| production-direct `PointXYZINormal` source-indices 256K | 47.8044 | 25.2074 | 1.90x | 泛型 normal source-indexed 放大规模后仍有收益。 |
| diagnostic full-cloud 256K | 37.9238 | 28.5084 | 1.33x | 诊断链路仍有收益。 |
| diagnostic source-indices 256K | 31.9641 | 25.3826 | 1.26x | 单侧 gather 诊断仍有正向信号。 |
| diagnostic dual-indices 256K | 35.2729 | 55.5724 | 0.63x | 独立 target stream 后双侧 gather 明确退化。 |
| diagnostic correspondences 256K | 31.6106 | 36.5483 | 0.86x | 大规模完整对应关系入口仍退化。 |

## 反汇编热点归属

反汇编文件：

- `test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls/build/asm/riscv/bench_transformation_estimation_symmetric_point_to_plane_lls_rvv.full.asm`
- `test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls/build/asm/riscv/bench_transformation_estimation_symmetric_point_to_plane_lls_rvv.asm`
- 摘要日志：`test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls/output/qemu/rvv_asm_check.log`

热点 helper 归属：

- production helper `estimateSymmetricPointNormalRowsRVV<RowSourcePolicy>` 模板符号存在，public full-cloud estimator 对 `SymmetricFullCloudRowSource<PointNormal, PointNormal>` 和 `SymmetricFullCloudRowSource<PointXYZINormal, PointXYZINormal>` 的调用点可见。
- public source-indices estimator 对 `SymmetricSourceIndexedRowSource<PointNormal, PointNormal>` 和 `SymmetricSourceIndexedRowSource<PointXYZINormal, PointXYZINormal>` 的调用点可见；指令摘录确认 source index `vle32.v`、`vmul.vx`、source `vluxei32.v` 和 target `vlse32.v`。
- production helper 指令摘录确认 `vlse32.v`、`vcpop.m`、`vcompress.vm`、`vfmul.vv`、`vfadd.vv`、`vfsub.vv` 和 `vfredosum.vs`。
- diagnostic `accumulate_candidate_full` 和 `accumulate_candidate_correspondences` 符号仍存在，bench 调用点可见。
- source-indices 和 dual-indices diagnostic helper 被内联到 bench lambda；反汇编摘要按 lambda 范围标出 source gather + target stride、双侧 gather。dual-indices 的输入来自两条不同 index stream；具体 index load 指令数量以 `rvv_asm_check.log` 的实际摘录为准。
- 全二进制摘录可见 `vluxei32.v` gather，需 reviewer 用符号范围复核具体归属。
- `vfredosum.vs` 出现在压缩 buffer tail 附近，说明编译器把部分 tail 累加自动向量化；这解释了为什么当前诊断不能直接成为 production 语义承诺。

## EvidenceDecision

`EvidenceDecision = production-ready/generic-normal-full-cloud-and-source-indexed`。

证据支持：

- 专项 diagnostic 已建，production 已接入 traits-gated generic normal full-cloud 和 source-indexed 分流。
- QEMU std/RVV 专项测试各 25 个通过，板卡 RVV test 25 个通过。
- QEMU bench 输出合同可解析，checksum 基本对齐。
- 反汇编证明真实 public full-cloud 和 source-indices estimator 分别调用 `SymmetricFullCloudRowSource` 与 `SymmetricSourceIndexedRowSource` production helper 模板实例，并揭示自动 partial vector accumulation。
- 板卡 production-direct `PointNormal` full-cloud 64K / 256K 为 `2.71x` / `2.70x`；`PointXYZINormal` full-cloud 64K / 256K 为 `2.71x` / `2.48x`。
- 板卡 production-direct `PointNormal` source-indices 64K / 256K 为 `2.11x` / `1.87x`；`PointXYZINormal` 为 `2.10x` / `1.90x`。
- test-rvv dual-indices 使用独立 target stream 后为 `0.80x` / `0.63x`，correspondences 为 `0.77x` / `0.86x`。

证据不支持：

- correspondences 路径板卡 `0.77x` / `0.86x`，不能接入生产。
- 不支持扩大到 `Scalar=double`、dual-indices 或 correspondences；source-indexed 只在有效 index stream、`Scalar=float` 和 generic normal layout gate 下接入。
- 自动 `vfredosum.vs` 规约改变累加树；当前由 production direct 测试和板卡 bench 覆盖，但仍需作为数值边界记录。

本轮按用户要求完成泛型 normal full-cloud + source-indexed 扩展的 PI1-PI5。最终接入范围限定为 full-cloud 和 source indices + target full-cloud 的 generic normal / `float` 入口；correspondences 和 dual-indices 入口保持标量。

## PI1-PI5 生产接入记录

PI1 冻结真实 public estimator（公开估计器）的 full-cloud overload：

```text
cloud_src + cloud_tgt
  -> size check
  -> RVV full-cloud dispatch if __RVV10__ && Scalar=float && generic normal traits/runtime gates pass
  -> fallback to existing ConstCloudIterator scalar helper
```

不覆盖这些入口：

- `cloud_src + indices_src + cloud_tgt`。
- `cloud_src + indices_src + cloud_tgt + indices_tgt`。
- `cloud_src + cloud_tgt + correspondences`。

PI2 已采用窄范围 production patch（生产补丁）：

| gate（门禁） | 当前行为 | fallback（回退路径） |
| --- | --- | --- |
| 编译宏 | `#if defined(__RVV10__)` 下才编译 RVV helper | 非 RVV 构建保留当前标量路径。 |
| 点类型 | source/target 分别满足 `has_xyz`、`has_normal`、单个 float 字段、POD / standard-layout、`sizeof(PointT) == sizeof(POD)` 和 offset alignment | traits 或 layout gate 不满足时走标量。 |
| Scalar | 只允许 `Scalar=float` | `Scalar=double` 走标量。 |
| work item count | `n >= 64` | 小输入走标量。 |
| VLEN / buffer | `vlmax_e32m2 <= 64` | 固定 buffer 无法容纳时走标量。 |
| finite | RVV keep mask 检查 source/target xyz 和合成 normal | 语义等价于 production 标量 `continue`。 |
| source-indexed row | 接入 `SourceIndexedRowSource` | 只接受 PCL valid-index precondition；RVV 不做负数/越界过滤，`Scalar=double`、layout 不满足或 gate 不满足时回退标量。 |
| dual-indexed / correspondence row | 不接入 | 双侧 gather 与 correspondence 展开仍是诊断层证据，不能共用 source-indexed production 结论。 |

第二轮 PI3 已新增 production direct（真实生产入口命中 RVV 分流后的证据）测试：`PointXYZINormal` full-cloud、mixed normal layout、generic normal 的 `Scalar=double` fallback，并保留 full-cloud 默认同向法线、关闭同向法线、小规模 fallback、invalid lane、source indices fallback、source+target indices fallback 和 correspondences fallback。PI4 已重跑 QEMU correctness、bench 输出、生产符号反汇编和板卡 `board_smoke`。

PI5 结论：generic normal / `float` / full-cloud + source-indexed 已升级为 `production-ready/generic-normal-full-cloud-and-source-indexed`。它不是整个模板入口 production-ready；`Scalar=double`、dual-indices 和 correspondences 仍保持标量。

公共 trait 迁移后，专项测试源增加 compile-time static_assert（编译期静态断言）：`PointNormal` 和 `PointXYZINormal` 满足 `RVVXYZNormalFloatLayout`，`PointXYZ` 不满足 normal gate，防止后续把 symmetric production gate 意外放宽。

## 遗留风险与后续条件

全云生产接入已在 generic normal / `float` 范围内闭合。剩余扩展风险是 `Scalar=double`、indices 和 correspondences 入口都没有 production RVV 正向证据，因此不能扩大分流范围。

correspondences 负向归因仍是假设。当前设计同时包含 index 展开、offset 计算、gather、压缩和 tail，多项成本混合；如果后续想重新评估 indexed row，需要消融 bench 分离这些成本。

自动 partial vector accumulation 仍是数值边界。当前 production direct 测试、QEMU 对拍和板卡 bench 说明选定样本下结果可接受；若未来改编译器、flags、FMA 结构或规约结构，需要重新做反汇编和容差审计。
