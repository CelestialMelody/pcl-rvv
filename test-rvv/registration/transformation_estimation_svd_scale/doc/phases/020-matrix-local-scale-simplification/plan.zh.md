# Phase 020 计划：matrix-local scale simplification diagnostic

## 阶段意图和边界

本阶段只验证 `matrix-local-scale-simplification` 这个 implementation-shape diagnostic（实现形态诊断）。候选不替换 ordered direct fused production patch，也不改变 PI5 的 `production_patch_positive_pending_user_confirmation` 状态。

目标问题：在保留父类 centroid / demean 动态矩阵和 Eigen 3x3 SVD 后段的前提下，把 scale 分子从旧路径的 `R4 * cloud_src_demean` 后再逐列点积，改成 `sum_tt = trace(R * H)` 是否数值等价，并是否值得作为更小 production patch 候选继续观察。

本阶段允许修改：

- `test-rvv/registration/transformation_estimation_svd_scale/include/impl/tesvd_scale_candidates.hpp`
- `test-rvv/registration/transformation_estimation_svd_scale/src/test_tesvd_scale.cpp`
- `test-rvv/registration/transformation_estimation_svd_scale/src/bench_tesvd_scale.cpp`
- `test-rvv/registration/transformation_estimation_svd_scale/Makefile`
- topic-local `doc/`、phase 文档、script / evidence registry wrapper

本阶段不允许修改 production 源码，不创建或更新 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档。

## validated_scope

| 维度 | 本阶段验证范围 |
| --- | --- |
| evidence role | diagnostic / implementation-shape，不是 production direct。 |
| A/B boundary | test helper micro path；Std 构建跑旧后段公式，RVV 构建跑 trace 简化公式。 |
| row source | ordered-cloud-pair 的已构造 dense `PointXYZ` 输入。 |
| point type / Scalar | `PointXYZ -> PointXYZ` / `float`。 |
| matrix stage | 已有 source / target demean 矩阵、centroid 和 `H` 后的 scale 后段。 |
| sizes | 4K、64K、256K bench smoke；correctness 使用 8192 点。 |

## unvalidated_scope

- 不证明真实 production dispatch（生产分流）、fallback（回退路径）或 public overload 行为。
- 不证明 source-indexed、dual-indexed、correspondence、泛型点型或 `Scalar=double`。
- 不替代 Phase 010 的 production direct evidence，也不能用于自动采纳当前 production patch。
- QEMU timing 只作为日志形状；若要把该候选作为 production 备选，需要 board repeated 或 production direct probe。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic / implementation-shape。 |
| A/B boundary | test helper micro path，不是 public overload。 |
| 当前决策问题 | implementation-shape：局部代数改写是否正确、是否值得保留为 fallback production patch 候选。 |
| diagnostic 是否可外推到 production | no；它只覆盖后段公式等价，不覆盖父类公开入口、dispatch 或 fallback。 |
| comparison-boundary / baseline mismatch 风险 | yes；Std/RVV 构建差异只作为旧公式 / trace 公式 A/B 的载体，不能写成 RVV 指令收益。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但仅当用户需要较小 production patch 备选，且先补同边界 board repeated / production direct 计划。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；该候选不是 RVV family selection。若接 production，需要 public path correctness、fallback 和 board evidence。 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| helper | 增加旧后段公式和 trace 简化公式的 test helper。 | 两条路径输出 4x4 matrix，且能返回 `max_reference_error`。 |
| correctness | 增加 `MatrixLocalScaleSimplificationMatchesLegacyPath`。 | Std/RVV `run_test_compare` 通过，TEST 说明证明范围。 |
| bench smoke | 增加 `matrix-local-scale` case-filter。 | QEMU smoke 可解析 label、checksum、`max_reference_error`，不写性能结论。 |
| manifest / registry | 让 QEMU manifest 能识别 `matrix-local-scale` 的 implementation-shape 角色。 | Evidence Doctor `Errors=0`，registry fresh。 |
| docs | 更新 benchmark/evidence、correctness-tests、optimization-evidence、matrix、roadmap 和 Phase 020 result。 | 文档写清该候选不是 production adoption。 |

## Evidence Doctor 和 registry 规则

本阶段 QEMU smoke 使用 `log/qemu/evidence_manifest.json` 和 `log/qemu/evidence_doctor.md`，登记为 `qemu_smoke_only` / `implementation_shape_diagnostic`。若脚本不能表达该角色，先修脚本再记录，不把错误 metadata 写成通过。

## 板卡复跑预算

本阶段先只做 QEMU correctness 和 smoke，因为它是 implementation-shape diagnostic，不足以改变当前 PI5 production patch 决策。若 QEMU smoke 和 correctness 正常，再把 board repeated 作为 `phase_deferred + unblocked after user adoption decision` 或用户要求较小 patch 备选时的下一步。

## 阶段完成条件

| 条目 | 完成条件 |
| --- | --- |
| correctness | 新 TEST 通过，旧 / trace 公式在误差预算内一致。 |
| QEMU smoke | `ALLOW_QEMU_BENCH_COMPARE=1 run_bench_compare --case-filter matrix-local-scale` 可运行并生成 manifest / doctor。 |
| decision | 只能写 `attempted`、`deferred` 或 `implementation_shape_positive_qemu_smoke`；不能写 adopted。 |

## 继续 / 停止条件

完成 QEMU correctness、smoke、doctor、registry 和文档同步后，本阶段可停止，因为继续到 board repeated 或 production probe 会影响当前 PI5 采纳判断。默认下一动作仍是等待用户确认当前 production patch；若用户不采纳或要求较小 patch 备选，再恢复 matrix-local board/probe。
