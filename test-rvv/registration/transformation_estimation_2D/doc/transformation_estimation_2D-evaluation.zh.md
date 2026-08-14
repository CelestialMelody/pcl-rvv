# registration/transformation_estimation_2D 函数级 RVV 评估

## 范围和目标源码

目标源码：

```text
registration/include/pcl/registration/transformation_estimation_2D.h
registration/include/pcl/registration/impl/transformation_estimation_2D.hpp
```

专项目录：

```text
test-rvv/registration/transformation_estimation_2D/
```

本文是 S2 evaluation（函数级评估）和当前 production candidate closeout（生产候选收尾）主归属。当前 production patch 只覆盖 exact `PointXYZ -> PointXYZ`、`Scalar=float`、dense finite ordered-cloud-pair；其它公开入口保持标量。

## 函数级结论

`TransformationEstimation2D` 实现 2D rigid transformation（二维刚体变换）估计。公开入口接收 source / target 点云、source indices（源索引）、dual indices（双侧索引）或 correspondences（对应关系），然后统一成两个 `ConstCloudIterator` 进入 protected helper。

当前标量路径的主工作可以分成五段：

1. 分别对 source 和 target 计算 3D centroid（质心），随后把 z 分量置零。
2. 分别生成 source / target 的 demean matrix（去中心化矩阵）。
3. 用 `cloud_src_demean * cloud_tgt_demean.transpose()` 生成 correlation matrix（相关矩阵）`H`。
4. 用 `atan2(H01 - H10, H00 + H11)` 得到 2D 旋转角。
5. 用 `cos/sin` 和 `centroid_tgt - R * centroid_src` 写回 4x4 transform matrix（变换矩阵）。

Phase 020 证明 test-only two-pass centered fused 2D correlation candidate（测试专用两遍中心化融合 2D 相关项候选）在顺序点云对（ordered-cloud-pair，source/target 按相同下标一一对应）、dense finite `PointXYZ -> PointXYZ`、`Scalar=float` 下有诊断价值。Phase 050 的真实板卡 public repeated 最新为 4K `4.222x`、64K `5.310x`、256K `4.947x`，Doctor `0/0/0`，因此保留窄范围 production patch。Phase 030 在不修改 production 的前提下，把 source-indexed、dual-indexed 和 correspondence row source 物化成顺序点云对并复用同一数学 candidate；Std/RVV correctness 各 16/16，QEMU、asm 和板卡 repeated 已闭合，但 Doctor `1/2/6`，因此仍是 diagnostic-only。

## 公开入口和 row source policy

| 公开入口 | row source policy（行来源策略） | source row | target row | 当前 RVV 状态 |
| --- | --- | --- | --- | --- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | ordered-cloud-pair（顺序点云对，source/target 按相同下标一一对应） | `cloud_src[k]` | `cloud_tgt[k]` | production patch retained；不满足 gate 时回退标量路径。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | source-indexed-cloud-pair（源索引点云对） | `cloud_src[indices_src[k]]` | `cloud_tgt[k]` | public scalar boundary 与 materialize-to-ordered candidate 通过；不等于 production gather。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | dual-indexed-cloud-pair（双索引点云对） | `cloud_src[indices_src[k]]` | `cloud_tgt[indices_tgt[k]]` | public scalar boundary 与 materialize-to-ordered candidate 通过；不等于 production 双 gather。 |
| `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` | correspondence-pair（对应关系点对） | `cloud_src[index_query]` | `cloud_tgt[index_match]` | public scalar boundary 与 materialize-to-ordered candidate 通过；不等于 production correspondence gather。 |

RowSourcePolicy 只描述每一行从哪里来。它不等同于完整优化族。ordered-cloud-pair 的诊断结果和 production-public negative result 都不能直接批准 indexed 或 correspondences production。

## 标量流程与 RVV 候选流程对照

| 流程段 | 当前标量 production | RVV 诊断 / probe |
| --- | --- | --- |
| 入口检查 | ordered-cloud-pair / source-indexed / dual-indices 检查规模关系；correspondences overload 不做逐项 index 边界检查。 | 诊断先复刻公开入口规模关系；非法 index / correspondence 只在明确 public semantics 后写成测试合同。 |
| row 读取 | `ConstCloudIterator` 隐藏不同 row source 的访存差异。 | ordered-cloud-pair 用 segment stride load（跨步分段加载）读取 x/y/z；Phase 030 的 indexed / correspondences candidate 先按 index / query-match 物化成顺序点云对，再复用相同数学链路；物化成本计入 bench，未实现 production gather kernel。 |
| centroid | `compute3DCentroid(ConstCloudIterator&)` 逐点检查 `pcl::isFinite`，source 和 target 分别计数。 | 诊断候选只在 dense finite 输入命中 RVV；非有限输入退回 public path。 |
| demean / correlation | `demeanPointCloud(ConstCloudIterator&, ..., Eigen::Matrix&)` 写出 4xN 动态矩阵，再由 Eigen 乘法生成 `H`。 | 两遍中心化：先求 x/y 质心，再直接累加中心化后的 `H00/H01/H10/H11`，避免显式矩阵写出。 |
| angle 和 transform | `atan2`、`cos`、`sin` 每次 estimate 只执行一次。 | 保留标量。热点候选在逐点累加和矩阵写出阶段。 |

需要单独审计的语义边界：当前 centroid 会因为 `pcl::isFinite` 同时检查 x/y/z 而受 z 有限性影响；后续 demean 仍全量写出 iterator 行。测试已记录 x/y 非有限会传播为非有限矩阵，z 非有限会强制 test-only candidate fallback。

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback（分流 / 回退） | narrow production candidate retained | `git diff -- registration/include/pcl/registration/impl/transformation_estimation_2D.hpp`；失败条件回退现有标量 iterator helper。 | 用户审阅后再决定采用；不扩大到其它 row source。 |
| layout / traits gate（布局 / 字段门控） | test-only dense finite gate | `estimateFused2DCandidate` 只覆盖 RVV-compatible `PointXYZ` / `float`。 | 泛型 gate 需要独立 traits / offset / representative board evidence。 |
| row source policy | ordered-cloud-pair attempted; three other policies have test-only candidate | `run_test_compare` 16/16；三类 row-source candidate 与 public scalar overload 对拍通过；QEMU row-source smoke 9 cases，Doctor 0/0/0。 | 仍需要 board repeated；当前不能把 materialize-to-ordered 写成 production gather 或性能收益。 |
| staging / reduction（暂存 / 规约） | retained narrow production shape | `accumulateFused2DRVV` 使用两遍 vector reduction；Phase 020 board diagnostic 为 `weak_positive`。 | Production-public board repeated 为 positive；当前仅保留 exact `PointXYZ -> PointXYZ`、`Scalar=float`、dense finite ordered-cloud-pair gate。 |
| formula / FMA（公式 / 融合乘加） | attempted / not adopted | `log/qemu/production_public/asm_attribution.md` 可见 public boundary 中的 `vfmacc` 和 `vfredosum`。 | 目标硬件收益不成立，FMA 指令存在不等于 production-ready。 |
| production scope（生产范围） | exact ordered-cloud-pair candidate supported | Phase 050 production direct correctness、QEMU、asm、board summary、Doctor。 | 其它点型、Scalar 和 row source 需要独立 PI1。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| ordered-cloud-pair public overload | production public entry | 检查 source / target size 后按 gate 尝试 RVV helper，失败时回退默认 iterator。 | registration 上游调用方。 | RVV helper 或 protected iterator helper。 | public semantics、ordered-cloud-pair row source 和 production dispatch。 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` |
| source-indexed overload | production public entry | source 由 indices 指定，target 顺序扫描。 | registration 上游调用方。 | protected iterator helper。 | source-indexed scalar boundary。 | 同上 |
| dual-indices overload | production public entry | source 和 target 都由 index list 指定。 | registration 上游调用方。 | protected iterator helper。 | dual-indexed scalar boundary。 | 同上 |
| correspondences overload | production public entry | 从 correspondences 抽 query / match 两列。 | registration 上游调用方。 | protected iterator helper。 | correspondence-pair scalar boundary。 | 同上 |
| protected iterator helper | production scalar helper | 计算 centroids、demean matrices、correlation、angle 和 transform。 | 四个公开入口。 | `getTransformationFromCorrelation`。 | scalar baseline 和 candidate 对拍参考。 | 同上 |
| `compute3DCentroid(ConstCloudIterator&)` | common scalar helper | iterator 版本逐点计算 centroid。 | protected iterator helper。 | centroid values。 | finite semantics reference。 | `common/include/pcl/common/impl/centroid.hpp` |
| `demeanPointCloud(ConstCloudIterator&, Eigen::Matrix&)` | common scalar helper | iterator 版本写出 Eigen dynamic matrix。 | protected iterator helper。 | correlation matrix multiply。 | cost source and replacement boundary。 | `common/include/pcl/common/impl/centroid.hpp` |
| `estimateFused2DStd` | test support | 标量两遍中心化 reference。 | gtest / bench。 | `solveTransform2DFromAccumulation`。 | same-chain reference。 | `test-rvv/registration/transformation_estimation_2D/include/impl/te2d_candidates.hpp` |
| `estimateFused2DCandidate` | test support | RVV 构建下尝试 ordered-cloud-pair candidate。 | gtest / bench。 | `accumulateFused2DRVV` 或 fallback。 | diagnostic candidate。 | 同上 |
| `estimateFused2DSourceIndexedCandidate` | test support | 读取 source indices，物化 source row 后复用 fused candidate。 | gtest / bench。 | `accumulateFused2DRVV` 或 fallback。 | source-indexed diagnostic；展开成本计入 bench。 | 同上 |
| `estimateFused2DDualIndexedCandidate` | test support | 读取 source / target 两侧 indices，物化后复用 fused candidate。 | gtest / bench。 | `accumulateFused2DRVV` 或 fallback。 | dual-indexed diagnostic；展开成本计入 bench。 | 同上 |
| `estimateFused2DCorrespondenceCandidate` | test support | 读取 query / match，物化点对后复用 fused candidate。 | gtest / bench。 | `accumulateFused2DRVV` 或 fallback。 | correspondence diagnostic；展开成本计入 bench。 | 同上 |
| `generate_te2d_asm_summary.py` | analysis script | 解析 Std / RVV objdump 并生成 asm attribution summary。 | `generate_asm_attribution_summary`。 | `asm_attribution.md/json`。 | asm attribution。 | `test-rvv/registration/transformation_estimation_2D/script/generate_te2d_asm_summary.py` |
| `generate_te2d_qemu_evidence_manifest.py` | analysis script | 把 QEMU smoke log 和 asm summary 翻译成 manifest。 | `run_qemu*_evidence_doctor`。 | `evidence_doctor.py`。 | QEMU smoke contract。 | `test-rvv/registration/transformation_estimation_2D/script/generate_te2d_qemu_evidence_manifest.py` |
| `generate_te2d_board_repeated_summary.py` | analysis script | 解析 board repeated `run-*` 日志并生成 summary / manifest。 | `run_board_bench_*_repeated`。 | `evidence_doctor.py`。 | board diagnostic 或 production-public probe summary。 | `test-rvv/registration/transformation_estimation_2D/script/generate_te2d_board_repeated_summary.py` |
| row-source QEMU summary | evidence output summary | 记录三类 row-source wrapper、9 个 case、manifest 和 Doctor。 | `run_bench_row_source_smoke`。 | `run_qemu_row_source_evidence_doctor`。 | QEMU log shape / asm path evidence。 | `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/` |
| diagnostic board repeated summary | evidence output summary | 记录 `ordered-cloud-pair-fused` 的 5-run B/A、bucket 和 checksum 边界。 | board run logs。 | evaluation / phase result / Handoff。 | weak-positive diagnostic。 | `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/summary.md` |
| production-public board repeated summary | evidence output summary | 记录 `ordered-cloud-pair-public` 的 5-run B/A、bucket 和 Doctor。 | board run logs。 | Phase 050 / evaluation / Handoff。 | 当前窄范围 production candidate 的板卡证据。 | `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/summary.md` |
| `doc/phases/050-pi2-production-patch-and-direct-evidence/result.zh.md` | phase result | 记录 PI2-PI5 probe、最新 board positive 和当前 review 边界。 | Phase 050 plan。 | README / roadmap / Handoff。 | current closeout truth。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/050-pi2-production-patch-and-direct-evidence/result.zh.md` |

## 测试计划和 bench 计划

| 测试 / 证据 | 层级 | 当前状态 |
| --- | --- | --- |
| scalar public semantics tests | public input semantics（公开入口输入语义） | pass：ordered-cloud-pair、source-indexed、dual-indexed、correspondence valid scalar-boundary，size mismatch 和非有限 x/y/z。 |
| fused accumulator same-chain tests | numerical consistency（数值一致性） | pass：Std / RVV 16/16。 |
| row source adapter tests | production-shaped diagnostic（生产形态诊断） | pass：三类 materialize-to-ordered candidate 与 public scalar overload 对拍；不覆盖非法 index 安全合同。 |
| fallback / gate tests | fallback tests（回退路径测试） | pass for diagnostic dense finite gate、non-finite fallback、小规模 fallback；production patch 保留同一 gate。 |
| QEMU bench smoke | QEMU correctness / log shape | pass：diagnostic smoke 和 production-public smoke 均可生成 manifest / doctor。 |
| asm attribution | disassembly（反汇编） | pass for path hit：production-public probe 可归属 public overload 或 `runPublicCase` 内联边界。 |
| board repeated bench | board performance（板卡性能） | diagnostic summary 为 `weak_positive`；production-public 为 `4.222x / 5.310x / 4.947x`，均 `positive`。 |
| row-source board repeated bench | board performance（板卡性能） | 已完成：source / dual / correspondence 各 4K/64K/256K；64K 有退化或长尾。 |
| Evidence Doctor | evidence validation（证据体检） | production-public QEMU/board 为 0/0/0；row-source board 为 1/2/6。 |

## 生产接入判断

当前判断：`production-candidate-supported / user-review-pending`。理由：

- Production-public repeated board 是性能主证据。它覆盖真实公开入口 probe，三个规模分别为 `4.222x / 5.310x / 4.947x`，每个规模 `B/A<1` 为 `0/5`。
- Evidence Doctor 对 board production-public manifest 给出 Errors=0、Warnings=0、Suggestions=0；这支持保留窄范围 patch，但不扩大 gate。
- QEMU smoke 和 asm attribution 只证明路径命中和指令归属，真实性能仍只引用板卡结果。
- Phase 020 的 diagnostic `weak_positive` 只说明 test-only helper 有探索价值，不能替代 Phase 050 的 production-public evidence。
- 当前 production patch 位于 `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp`；不命中 gate 的输入仍保持标量路径。

### 诊断证据链

Phase 030 的证据链是：三类 row-source 的合法行配对测试 -> materialize-to-ordered candidate correctness -> 9-case QEMU smoke -> `row_source_lambda_boundary` asm attribution -> 5-run `Milkv-Jupiter` board repeated -> Evidence Doctor 1/2/6。该链条能证明候选的输入展开、数学路径和真实板卡表现可复核；它仍不能证明 production dispatch 或 gather kernel 的收益。

## 文档归属

| 信息 | 主归属 |
| --- | --- |
| 当前函数级评估、候选取舍和生产接入判断 | 本文 |
| 测试体系、correctness、bench / evidence、代码地图 | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/test-support-code-map.zh.md` |
| 跨阶段 candidate frontier（候选前沿） | `doc/optimization-roadmap.zh.md` |
| 阶段计划、结果和早停检查 | `doc/phases/` |
| 候选矩阵状态 | `doc/phases/optimization-matrix.zh.md` |
| 未来 production 长期行为 | `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` 只记录当前窄范围 patch，用户采用后再扩展 |

## Remaining Risk 与后续扩展条件

| 风险 | 当前为什么未闭合 | 后续闭合条件 |
| --- | --- | --- |
| source-indexed / dual-indexed / correspondence RVV | correctness、QEMU、asm 和板卡已完成，但 64K 有退化/长尾，Doctor 为 1/2/6。 | 先做布局/稳定性分析；若形成新 gather/staging 族，再新建 PI1。 |
| production ordered-cloud-pair RVV | 窄范围 board summary 已正向，但用户尚未审阅 patch。 | 审阅 diff、fallback 和文档后决定采用/提交。 |
| 泛型点型 | traits、offset、layout 和代表性板卡证据未闭合。 | 读取泛型点类型策略后补 traits/fallback tests 和 representative board evidence。 |
| `Scalar=double` | double reduction、误差预算和目标硬件证据未覆盖。 | double-specific correctness、asm 和 board repeated evidence。 |
| 负向归因 | 当前只知道 public path 收益不成立，不能单因归因为某个 load、reduction 或 wrapper 成本。 | 需要 profile 或 component ablation（组件消融）后再归因。 |
