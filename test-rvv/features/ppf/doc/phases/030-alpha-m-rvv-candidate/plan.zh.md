# Phase 030 Plan: Alpha M RVV Candidate

## 阶段意图和边界

本阶段实现 test-only RVV candidate（测试专用 RVV 候选），只批量计算 `alpha_m` 后段。`f1..f4`
继续调用当前 production 实际使用的 `pcl::computePairFeatures` 标量 helper，以便隔离 `alpha_m`
closed-form formula（闭式公式）和 RVV `atan2_RVV_f32m2` 的收益 / 风险。

本阶段不修改 production，不接入 `PPFEstimation::computeFeature` dispatch（分流逻辑），不声明
production-ready。

validated_scope：`PointXYZ` + `Normal`、float、AoS（结构数组）、sequential indices、ordered
`index_i × input_` row source（行来源）。

unvalidated_scope：泛型点类型、非连续/乱序 indices、其它 normal point type、`Scalar=double`、
真实 production dispatch、`PPFRGB`/`CPPF`、非有限输入、near `-x` normal 和 production fallback。

phase_closeout_boundary：只能关闭 `alpha_m batch RVV` 诊断候选的 correctness、QEMU build/path、
asm 和 component board bench 条目；不能关闭 production integration。

## 当前状态清单

| area | 状态 |
| --- | --- |
| Phase 010 | `pair-feature batch RVV` 5-run board repeated negative，不进入 production。 |
| Phase 020 | `computeAlphaMClosedForm` 通过 Std/RVV correctness，可作为 alpha formula oracle。 |
| RVV math helper | `atan2_RVV_f32m2` 可用于 alpha 角度；`acos_RVV_f32m2` 和 `sincos_finite_domain_RVV_f32m2` 本阶段不需要。 |
| board | 用户说明可用；candidate correctness 和 asm 成立后继续跑 5-run repeated board。 |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED: alpha_m RVV 输出测试 | `src/test_ppf.cpp` 新增 `PPFCandidate.AlphaMBatchRVVComputesProductionLikeOutput` | `make -C test-rvv/features/ppf run_test_rvv` 编译失败，缺少 candidate helper。 |
| GREEN: 实现 RVV candidate | `include/impl/ppf_alpha_candidate.hpp`、`include/ppf.h` | RVV 构建下批量计算 alpha_m；非 RVV 构建回退 reference。 |
| 跑 correctness | `make -C test-rvv/features/ppf run_test_compare` | Std/RVV 均通过；`alpha_m` 误差预算暂用 `2e-3`，与 `atan2_RVV_f32m2` 近似一致。 |
| 增加 bench case | `src/bench_ppf.cpp`、`script/generate_ppf_evidence_manifest.py` | 输出 `candidate_ppf_alpha_m_batch_rvv`，manifest 能识别 case。 |
| QEMU bench smoke | `run_bench_rvv` 小规模单 case | 只检查日志形状，不写性能结论。 |
| asm attribution | `make -C test-rvv/features/ppf dump_bench_rvv` | `bench_ppf_rvv.asm` 中出现 alpha candidate 相关 RVV 指令。 |
| board repeated | `REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated` | 生成 board summary / Evidence Doctor；按 decision bucket 判断是否继续 production probe。 |

## Evidence Doctor 和 registry 规则

本阶段必须用 topic-local wrapper 更新 `log/board/repeated/evidence_manifest.json`，并运行
`test-rvv/script/evidence_doctor.py`。如果新增 case 未被 wrapper 识别，不能使用该 board 表格做
EvidenceDecision。

## 板卡复跑预算和决策桶

沿用 Phase 010 预算：`REPEATED_BOARD_RUNS=5`。median >= 1.20 且 5/5 高于 1 为 positive；
1.05-1.20 为 weak_positive；0.98-1.05 或跨方向为 neutral；低于 0.98 且多数退化为 negative；
复跑跨桶则 unstable。

## Continue / Stop 条件

默认继续到 correctness、bench build、QEMU smoke、asm 和可用板卡 repeated。合法停止条件是：
candidate correctness 不成立、Evidence Doctor Error 无法解释 / 降级、或继续需要修改 production。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic / component ablation。 |
| A/B boundary | test helper vs test helper；public output 仍只是 baseline smoke。 |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape。 |
| diagnostic 是否可外推到 production | unknown。只有同边界 board positive 且 PI1 gate 可控时才允许生产探针。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 只 RVV 化 `alpha_m`，`f1..f4` 仍标量；必须按混合边界解释。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 仅 positive 或可解释 weak_positive 且静态实现可控时讨论；neutral/negative 默认不进入生产。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes，若后续还有 pair-feature RVV 或 direct-AoS 家族比较。 |
