# Phase 000 计划：scale-aware fused accumulation 诊断

## 阶段意图和边界

本阶段开启 `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` 的 RVV topic（主题）。目标是判断 `TransformationEstimationSVDScale::getTransformationFromCorrelation` 中的 scale-aware accumulation（带尺度估计的累加）是否值得进入 production integration loop（生产接入闭环）确认点。

本阶段只做未接 production 的 diagnostic（诊断）：新增 `test-rvv/registration/transformation_estimation_svd_scale` 下的测试、bench、脚本和 topic-local 文档，不修改 production（生产源码）。验证范围冻结为 `ordered-cloud-pair`（顺序点云对，source/target 按相同下标一一对应）、dense（稠密点云）、`Scalar=float`、`PointXYZ -> PointXYZ`、规模 `4K/64K/256K`。`Scalar=double`、source-indexed、dual-indexed、correspondence、泛型点型和 production direct 都不在本阶段证明范围内。

## S0 偏好冻结

| 字段 | 冻结值 |
| --- | --- |
| `preferences_loaded` | defaults loaded；local override absent；prompt override 指定 RVV worker、新 topic、板卡可用。 |
| `work_preferences` | 测试资产、diagnostic 和 prototype 用详细中文注释；production 注释克制且本阶段不改 production；文档 current-state-first（当前状态优先）；evidence policy（证据策略）为 summary-only。 |
| `commit_preferences` | 不自动 commit；若后续提交，topic 资产、summary evidence、agent instruction patch 分开。 |
| `artifact_publication_decision` | topic-local test/doc 为 review-required；S0/handoff/local raw logs 为 local-only；`artifact_layout.topic_doc_template` 解析出的 production 长期主题文档在未接 production 时 not_applicable。 |
| `dirty_isolation` | 本阶段允许路径限于 `test-rvv/registration/transformation_estimation_svd_scale/**`，以及后续若用户确认 production integration 才触碰的目标 production 文件；现有 `transformation_estimation_2D` dirty paths 视为无关工作，不回退、不整理。 |

## 当前状态清单

| 对象 | 当前状态 | 证据 |
| --- | --- | --- |
| 目标源码 | `TransformationEstimationSVDScale` 继承普通 SVD，但构造函数强制 `use_umeyama_ == false`。父类公开入口会计算 centroid、demean 矩阵，再虚调用本文件的 `getTransformationFromCorrelation`。 | `registration/include/pcl/registration/transformation_estimation_svd_scale.h`；`registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| scale helper | 先算 `H = src_demean * tgt_demean^T` 和 3x3 SVD，再构造 `R4 * cloud_src_demean` 动态矩阵，最后循环算 `sum_ss` 与 `sum_tt`。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| sibling 经验 | 普通 SVD 已在四类 row source 上证明 fused sum/cross-sum production path 正向；但那条路径只覆盖 `use_umeyama_ == true`，不能外推到 scale 子类。 | `artifact_layout.topic_doc_template` 解析出的普通 SVD production 长期主题文档。 |
| topic 资产 | 本 topic 是新建目录；没有历史 phase、roadmap、matrix、registry 或 handoff。 | `test-rvv/registration/transformation_estimation_svd_scale/` |
| 板卡 | 当前会话确认板卡可用；本阶段需要 repeated board diagnostic 后才能给生产接入建议。 | prompt override |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `matrix-local-scale-simplification` | 在已有 `H` 和 `R` 后，用 `sum_tt = trace(R * H)` 避免 `R4 * cloud_src_demean` 动态矩阵和逐列 dot pass。 | 仍依赖父类 centroid/demean 动态矩阵；收益可能被 Eigen 矩阵乘法和 3x3 SVD 稀释。 |
| `direct-fused-scale-accum` | 直接从 source/target 点对累加 source sum、target sum、source-target cross sum 和 source square sum，最后保留 3x3 Eigen SVD；可避免 centroid/demean 矩阵装填和 scale 后续 pass。 | RVV accumulation（RVV 累加）会改变 reduction tree（规约树）；多 accumulator 可能增加寄存器压力；diagnostic positive 不能直接证明 production dispatch。 |
| `row-source-expansion` | 若 ordered 诊断强正向，可按普通 SVD 的 source-indexed、dual-indexed、correspondence 经验扩展。 | 每个 row source 的 gather 成本独立，不能由 ordered 结论外推。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `direct-fused-scale-accum` | ordered-cloud-pair | `PointXYZ -> PointXYZ` / `float` / dense xyz AoS | `run_test_compare` | `run_bench_compare` 仅 QEMU smoke；board repeated target | planned | `dump_bench_rvv` | planned | planned |
| `matrix-local-scale-simplification` | post-demean helper | Eigen matrices / `float` | planned as scalar A/B | optional component bench | deferred | optional | deferred | phase_deferred |
| `row-source-expansion` | source-indexed / dual-indexed / correspondence | planned | not in this phase | not in this phase | not in this phase | not in this phase | not in this phase | phase_deferred + unblocked after ordered decision |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 建立 topic-local scaffold（脚手架） | `Makefile`、`board.mk`、`include/tesvd_scale.h`、`include/impl/*`、`src/test_tesvd_scale.cpp`、`src/bench_tesvd_scale.cpp` | Std/RVV 都能交叉编译；测试资产包含中文说明和证据边界。 |
| 写同构标量 reference（参考链路） | `estimateScaleStd` | 与 public `TransformationEstimationSVDScale` 在确定性样本和 scale transform 上对拍。 |
| 写 RVV candidate | `estimateScaleCandidate` | RVV 构建命中 `used_rvv=true`，Std 构建自然 fallback，矩阵误差在预算内。 |
| QEMU correctness | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | Std/RVV gtest 全通过；日志仅作为 correctness 和路径形状证据。 |
| QEMU bench smoke | `ALLOW_QEMU_BENCH_COMPARE=1 make ... run_bench_compare BENCH_ARGS="--case-filter ordered-cloud-pair --iterations 3 --warmup-iterations 1"` | 只确认 bench label、checksum 和分析脚本可解析；不写性能结论。 |
| 反汇编归属 | `make ... dump_bench_rvv` | RVV bench 符号或 candidate helper 范围可见 load/FMA/reduction 指令；若归属不清则降级。 |
| 板卡 repeated diagnostic | `make ... run_board_bench_ordered_cloud_pair_repeated` | 5 runs、20 iterations、5 warmup；summary、manifest、Evidence Doctor 和 registry 刷新。 |

## Evidence Doctor 和 registry 规则

本阶段 benchmark、board summary、checksum summary、asm attribution（反汇编归因）和 EvidenceDecision（证据决策）前必须运行 Evidence Doctor（证据体检）。若 topic-local wrapper 尚未完整覆盖所有 metadata，先按 summary-only 或 manifest 结果降级说明，不能写成完整 production evidence。

`log/evidence_registry.json` 为 topic-local evidence registry（证据登记表）。官方 Make target 生成 QEMU / board summary 后应记录 registry；若 registry 不存在或发现未登记文件，Handoff 写 `partial` 并列出人工检查路径。

## 板卡复跑预算和决策桶

默认运行 5 次 repeated board，每次 20 iterations、5 warmup。若 summary 或 Evidence Doctor 显示方向接近阈值、长尾严重或结论跨桶，最多追加 1 次同边界确认复跑；若仍摇摆，decision bucket（决策桶）降级为 `unstable`。

判断口径：median speedup >= 1.20 且各规模方向稳定为 `positive`；1.05-1.20 为 `weak_positive`；接近 1 或方向混杂为 `neutral`；稳定小于 1 为 `negative`；跨复跑反转为 `unstable`。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic`，本阶段使用 test-only helper 和 bench wrapper。 |
| A/B boundary | `test helper`；baseline 是 public scale 标量链路或同构标量 reference，candidate 是 test-only RVV fused scale helper。 |
| 当前决策问题 | `RVV-vs-scalar`，回答是否建议进入生产接入确认点。 |
| diagnostic 是否可外推到 production | unknown。它能说明 scale-aware fused accumulation 有潜在收益，但不能证明真实 public dispatch、fallback 或 production symbol 已闭合。 |
| comparison-boundary / baseline mismatch 风险 | yes。public scale baseline 包含父类 centroid/demean 动态矩阵，candidate 可能直接累加原始点对；若 bench 对比两者，必须说明这是 production-value diagnostic，不是严格同边界 implementation-family A/B。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | weak/neutral 时可允许有界 probe，前提是生产补丁只覆盖 ordered dense `Scalar=float` 且 fallback 容易隔离；negative 或 checksum/doctor Error 时不建议进入 PI。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要 production direct Std/RVV、fallback tests、asm 和 board evidence；本阶段只到“是否建议接入源码”的用户确认点。 |

## Phase scope 与扩展队列

`validated_scope`：ordered-cloud-pair、`PointXYZ -> PointXYZ`、`Scalar=float`、dense、4K/64K/256K diagnostic。

`unvalidated_scope`：source-indexed、dual-indexed、correspondence、mixed-field xyz AoS、泛型 traits gate、`Scalar=double`、非 dense / NaN / Inf、production direct dispatch 和 fallback。

`point_type_expansion_queue`：若 ordered production probe 成立，下一阶段应读取泛型点类型策略，按 `PointXYZI` / `PointXYZRGB` correctness、traits/offset gate、representative board 或明确 fallback 扩展。

`row_source_expansion_queue`：若 ordered production probe 成立，按 source-indexed -> dual-indexed -> correspondence 创建独立 phase；每条 row source 需重新补 correctness、bench、asm、board 和 Evidence Doctor。

## 文档更新清单

本阶段创建或更新：README、evaluation、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map、phase README、optimization matrix、optimization roadmap、phase result、current handoff（如需要）。不创建 `artifact_layout.topic_doc_template` 解析出的 `transformation_estimation_svd_scale` production 长期主题文档，因为没有 adopted production behavior。

## 继续 / 停止条件

若 QEMU correctness、asm 和 board repeated diagnostic 全部闭合，EvidenceDecision 可写成 `partial-production-candidate`，并停在用户确认点：是否建议将 RVV 优化实现接入源码。停止条件是继续会扩大到 production 文件，必须获得用户明确授权。

若 correctness 或 checksum 失败，先修复 test-only helper；若板卡不可达、Evidence Doctor Error 无法处理或 dirty isolation 不安全，则写 blocked handoff。
