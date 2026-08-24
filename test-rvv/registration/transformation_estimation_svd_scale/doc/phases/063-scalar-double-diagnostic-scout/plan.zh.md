# Phase 063: Scalar=double Diagnostic Scout Plan

## 阶段意图和边界

本阶段只把 `Scalar=double` 从“未验证 / not_applicable”推进到 diagnostic scout（诊断侦察）状态。目标是回答：在 ordered-cloud-pair（顺序点云对，source / target 按相同下标一一对应）且 `PointXYZ -> PointXYZ` 的窄范围内，test-only double accumulation（测试专用双精度累加）是否能与公开 `TransformationEstimationSVDScale<..., double>` 语义对齐，并给后续 RVV f64 或生产 gate 讨论提供数值预算输入。

本阶段不修改 production（生产源码），不扩大当前 `Scalar=float` 的 production RVV gate，不覆盖 source-indexed、dual-indexed、correspondence、custom layout、非法 index / correspondence 或全部泛型点型。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| production 状态 | `Scalar=float` 已有 ordered、row-source、correspondence sorted-copy、matrix-local helper simplification 和 affine contiguous fast path 等 adopted production behavior；`Scalar=double` 仍保持父类 / 既有路径。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| 测试支撑 | 现有 `ScaleAccumulation`、`accumulateScaleStd`、`accumulateScaleRVV` 和 `solveScaleFromAccumulation` 都以 `float` 为主。 | `test-rvv/registration/transformation_estimation_svd_scale/include/impl/tesvd_scale_candidates.hpp` |
| 当前 correctness | Phase 062 后 current QEMU correctness 为 Std/RVV 各 23 tests passed。 | `doc/phases/062-affine-index-fast-path-adoption-closeout/result.zh.md` |
| roadmap / matrix | `Scalar=double` 当前写为需要独立数值预算和性能计划。 | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` |

## validated_scope / unvalidated_scope

| scope kind | 内容 |
| --- | --- |
| validated_scope | ordered-cloud-pair；`PointXYZ -> PointXYZ`；`Scalar=double`；dense input；8192 点确定性样本；公开 double fallback 作为 reference。 |
| unvalidated_scope | source-indexed / dual-indexed / correspondence；`Scalar=float` production fast path 的 double 版本；RVV f64 board performance；custom layout；全部 xyz AoS 泛型点型；非法输入语义。 |
| phase_closeout_boundary | 本阶段最多把 `Scalar=double / ordered / PointXYZ` 标成 diagnostic correctness scout；不能标成 production-ready 或 adopted。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `scalar-double-accumulation-scout` | 把 source sum、target sum、cross sum 和 source square sum 全部以 `double` 累加，再用 `Eigen::JacobiSVD<Eigen::Matrix3d>` 求解，可与公开 double fallback 对齐。 | 累加顺序与父类 double path 不完全一致；SVD 符号翻转和近退化样本可能放大差异。 |
| `rvv-f64-widened-accumulation-scout` | 未来可参考 dual-quaternion topic 的 f32 load widened-to-f64 reduction（float 加载后扩宽到 double 规约），先做 correctness scout 再谈性能。 | RVV f64 寄存器压力、规约树和目标硬件 f64 吞吐未知；本阶段可先暂缓，不作为生产前置证据。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `scalar-double-accumulation-scout` | ordered-cloud-pair | `PointXYZ -> PointXYZ` / `double` / dense | 新增 gtest：public double fallback vs test-only double accumulation | QEMU smoke only：`scalar-double-diagnostic-scout` 只检查 log-shape / checksum / path | not_required_for_correctness_scout | not_required_for_scalar_scout | manual diagnostic boundary check | complete_diagnostic_correctness_scout |
| `rvv-f64-widened-accumulation-scout` | ordered-cloud-pair | `PointXYZ -> PointXYZ` / `double` / dense | RVV f64 candidate vs scalar double accumulation | QEMU smoke only；board repeated 未跑 | pending_board_if_performance_claim | planned_if_production_probe | manual diagnostic boundary check | complete_diagnostic_correctness_scout |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| TDD red | 在 gtest 中先引用 `estimateScaleStdDouble` / double scout helper。 | 构建或测试先因 helper 缺失失败，证明测试约束新实现。 |
| double accumulation helper | 新增 `ScaleAccumulationD64`、`accumulateScaleStdDouble`、`solveScaleFromAccumulationDouble` 和 `estimateScaleStdDouble`。 | 新测试通过；输出矩阵与公开 double fallback 在 `5e-8` 预算内一致。 |
| production fallback guard | 保留已有 `UncoveredPublicEntriesStayCorrect`，新增测试不要求 production RVV 命中。 | `Scalar=double` 仍不进入 production RVV gate。 |
| docs update | 回填 result、matrix、roadmap 和必要 topic-local 文档。 | `Scalar=double` 状态从 `not_applicable` 更新为本阶段实际结果。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic；若只实现标量 double helper，则是 numeric correctness scout（数值正确性侦察）。 |
| A/B boundary | test helper vs public fallback；不是 production public RVV-vs-scalar。 |
| 当前决策问题 | numeric budget / implementation-shape；不做 production adoption。 |
| diagnostic 是否可外推到 production | no。它只能证明 double 累加公式与公开 double fallback 对齐，不能证明 RVV f64 性能或生产分流。 |
| comparison-boundary / baseline mismatch 风险 | yes。公开 double fallback 经过父类动态矩阵路径，test helper 走 fused accumulation 公式；通过同输出矩阵对拍降低语义风险，但不消除性能边界差异。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no，本阶段没有 production probe 授权；负向或不稳定只说明 double 数值预算未闭合。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；未来若要采纳 double RVV，必须另开 PI1-PI5，补 production direct、fallback、asm 和 board repeated。 |

## Evidence Doctor 和 registry 规则

本阶段若只跑 correctness，不生成 benchmark summary，也不需要脚本化 Evidence Doctor。Handoff 里做人工 Evidence Doctor 边界检查：Errors / Warnings / Suggestions 均围绕“是否把 diagnostic correctness 写成 production evidence”。若后续补 QEMU smoke 或 board repeated，必须生成 manifest、Doctor 和 registry 记录。

## 板卡复跑预算和决策桶

本阶段默认不跑 board。若实现 RVV f64 scout 并需要性能信号，再另写 board repeated plan：至少 5 runs、warm-up 5、iterations 20，decision bucket 仍按 positive / weak-positive / neutral / negative / unstable 写入 summary 和 Doctor。

## 继续 / 停止条件

继续条件：double scalar scout correctness 通过后，如果实现成本低且 QEMU RVV f64 编译可行，可在同 phase 继续补 RVV f64 test-only candidate；否则将其写为 `phase_deferred + unblocked` 或 `turn_stop_deferred`，取决于工具链 / 时间 / dirty isolation。

停止条件：helper 编译受当前 toolchain / intrinsic 支持阻塞、double correctness 与公开 fallback 差异超过预算且无法解释、继续需要修改 production gate 或引入板卡性能决策。

## 文档更新清单

- 新增 `063-scalar-double-diagnostic-scout/result.zh.md`。
- 更新 `doc/phases/README.zh.md`。
- 更新 `doc/phases/optimization-matrix.zh.md`。
- 更新 `doc/optimization-roadmap.zh.md`。
- 如新增 gtest，更新 `doc/correctness-tests.zh.md` 和 `doc/test-support-code-map.zh.md`。
- 不更新 `doc-rvv` production 长期主题文档，除非未来 double 进入真实 production adoption closeout。

## roadmap 同步动作

本阶段结束后把 `Scalar=double` 拆成至少两条 roadmap 状态：`scalar-double-accumulation-scout` 的实际结果，以及 `rvv-f64-widened-accumulation-scout` 的恢复条件。阶段反思必须说明是否出现新的数值预算、RVV f64、row-source double 或 production fallback 测试方向。
