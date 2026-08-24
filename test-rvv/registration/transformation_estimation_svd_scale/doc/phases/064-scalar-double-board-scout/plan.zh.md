# Phase 064: Scalar=double Board Diagnostic Scout Plan

## 阶段意图和边界

本阶段接续 Phase 063，只回答一个窄问题：`Scalar=double` 的 test-only RVV f64 widened accumulation（测试专用 RVV f64 扩宽累加）在目标板卡上是否有继续进入 production probe（生产探针）的性能信号。

本阶段不修改 production 源码，不新增 public dispatch（公开分流），不把 double RVV 写成 adopted behavior（已采纳生产行为）。范围保持为 ordered-cloud-pair（顺序点云对）、`PointXYZ -> PointXYZ`、dense input、64K。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| correctness | Phase 063 已通过 Std/RVV 各 25 tests；double scout 在 `5e-8` 预算内对齐。 | `doc/phases/063-scalar-double-diagnostic-scout/result.zh.md` |
| QEMU smoke | `scalar-double-diagnostic-scout` 已登记为 QEMU smoke-only；checksum 一致，QEMU timing 不作为性能证据。 | `log/evidence_registry.json` |
| board target | 当前没有 scalar-double board repeated target；需要补 Makefile target 和 summary 脚本识别。 | `Makefile`、`script/generate_tesvd_scale_board_repeated_summary.py` |
| production 状态 | `Scalar=double` 仍走父类 / 既有 fallback；本阶段不改变 production gate。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |

## validated_scope / unvalidated_scope

| scope kind | 内容 |
| --- | --- |
| validated_scope | ordered-cloud-pair；`PointXYZ -> PointXYZ`；`Scalar=double`；dense 64K；test-only scalar double fallback vs RVV f64 widened candidate。 |
| unvalidated_scope | source-indexed / dual-indexed / correspondence double；generic / custom layout double；production public double dispatch；ASM attribution；fallback matrix；非法输入语义。 |
| phase_closeout_boundary | 本阶段最多把 double RVV 标成 `board_diagnostic_positive / weak / neutral / negative / unstable`；不能关闭 production adoption。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `rvv-f64-widened-board-scout` | ordered-cloud-pair | `PointXYZ -> PointXYZ` / `double` / dense 64K | Phase 063 gtest + current `run_test_compare` | `scalar-double-diagnostic-scout` repeated board diagnostic | 本阶段新增 5-run board repeated summary | pending/not_applicable for diagnostic | board manifest + Evidence Doctor | pending |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 board target | 新增 `collect_board_bench_scalar_double_diagnostic_scout_repeated`、manifest / Doctor / registry / aggregate target。 | Make target 能按 `--case-filter scalar-double-diagnostic-scout` 采集 5-run repeated，并登记 evidence registry。 |
| A2 summary parser | 让 board summary 脚本识别 `scalar double diagnostic scout ordered-cloud-pair 64K` 和 `max_public_error`。 | 生成 summary / manifest 时不把 double scout 归入普通 fused float diagnostic。 |
| A3 correctness freshness | 运行 `run_test_compare`。 | Std/RVV correctness 当前通过。 |
| A4 board diagnostic | 运行新增 board target；若板卡不可用，记录 `turn_stop_deferred with stop_condition_hit`。 | summary、manifest、Evidence Doctor 和 registry fresh。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | board diagnostic；不是 production public evidence。 |
| A/B boundary | test-only scalar double fallback vs test-only RVV f64 widened accumulation。 |
| 当前决策问题 | f64 widened RVV 是否值得进入 production probe plan。 |
| diagnostic 是否可外推到 production | no。没有 production dispatch、fallback、ASM 或 public overload patch。 |
| comparison-boundary / baseline mismatch 风险 | yes。公开 double fallback 与 test-only fused accumulation 的边界不同；Phase 063 只证明输出误差预算。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | weak/negative/unstable 默认不进入 production probe；positive 也只允许进入 PI1 plan，不自动改 production。 |
| clean adoption 是否需要同一 production boundary 内证据 | yes。必须另开 production integration loop，补 public correctness、fallback、ASM、board repeated、Evidence Doctor 和用户确认。 |

## 板卡复跑预算和决策桶

- runs：5。
- warm-up：5。
- iterations：20。
- decision bucket：沿用 summary 脚本的 `positive / weak_positive / neutral / negative / unstable`。
- 如果 5-run 桶稳定，按该桶关闭本阶段 board diagnostic；如果退化频率高或 Evidence Doctor 报 Error，降级为 `unstable / blocked`，不进入 production probe。

## 继续 / 停止条件

继续条件：board diagnostic 为 stable positive，且 Evidence Doctor 无 Error 时，下一 phase 可以是 `scalar-double-production-probe-plan`。

停止条件：板卡不可用、Evidence Doctor Error 未能解释、correctness 失败、或 board diagnostic 为 weak / neutral / negative / unstable。停止时必须回填 result、matrix、roadmap 和 registry 状态。

## 文档更新清单

- 新增 `064-scalar-double-board-scout/result.zh.md`。
- 更新 `doc/phases/README.zh.md`。
- 更新 `doc/phases/optimization-matrix.zh.md`。
- 更新 `doc/optimization-roadmap.zh.md`。
- 更新 `doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md` 和 `doc/test-support-code-map.zh.md` 中的 target / evidence 边界。
