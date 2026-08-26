# Phase 010 Plan: Pair Feature And Output Staging Ablation

## 阶段意图和边界

本阶段实现 test-only RVV candidate（测试专用 RVV 候选），只批量计算 PPF 当前 production 实际使用的
`computePairFeatures` 风格 `f1..f4`。`alpha_m` 仍走 Phase 000 的标量 reference，输出仍按
`index_i * input_size + j` 原顺序写回。目标是判断 pair-feature math（点对特征数学链路）和
output staging（输出暂存 / 写回）是否具备继续做板卡 component ablation（组件消融）的价值。

本阶段不修改 production，不接入 `features/src/ppf.cpp::computePPFPairFeature`，不声明 production-ready。

validated_scope：`PointXYZ` + `Normal`、float、AoS（结构数组）、sequential indices、ordered
`index_i × input_` row source（行来源）。

unvalidated_scope：泛型点类型、非连续/乱序 indices、其它 normal point type、`Scalar=double`、
真实 production dispatch、`PPFRGB`/`CPPF` 和 `alpha_m` RVV。

point_type_expansion_queue：若本阶段 board component bench positive，后续先进入 `PointNormal` 或
`PointXYZ + Normal` production-shaped scope，再审计 traits / fallback。

phase_closeout_boundary：只能关闭 `pair-feature batch RVV` 诊断候选的 correctness、QEMU build/path、
asm 和 component board bench 条目；不能关闭 production integration。

## 当前状态清单

| area | 状态 |
| --- | --- |
| Phase 000 | reference scaffold 已通过 Std/RVV QEMU correctness。 |
| Makefile | 目前只有 test target，bench target 尚未接入。 |
| candidate | 尚不存在 `computePPFPairFeatureBatchRVV`。 |
| board | 用户说明可用；本阶段若 bench 构建完成，应继续跑 5-run repeated board。 |

## 优化矩阵

见 `../optimization-matrix.zh.md`。本阶段推进 `pair-feature batch RVV`，`alpha_m batch RVV`
保持 deferred。

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 写 RED candidate 测试 | `src/test_ppf.cpp` 新增 `PPFCandidate.PairFeatureBatchRVVComputesProductionLikeOutput` | `make -C test-rvv/features/ppf run_test_rvv` 编译失败，缺少 candidate helper。 |
| 实现 RVV candidate | `include/impl/ppf_pair_batch_candidate.hpp`、`include/ppf.h` | RVV 构建下使用 SoA staging（分字段暂存）计算 `f1..f4`；非 RVV 构建回退 reference。direct-AoS gather（直接结构数组离散加载）留给后续独立候选。 |
| 跑 correctness | `make -C test-rvv/features/ppf run_test_compare` | Std/RVV 均通过；误差预算 `2e-3`，来自 RVV `atan2_RVV_f32m2` 近似。 |
| 增加 bench | `src/bench_ppf.cpp`、Makefile `SRCS_BENCH` / libs | 能构建并输出 `Dataset:`、`Iterations:`、`Warmup Iterations:`、case timing 和 checksum。 |
| QEMU bench smoke | 单独运行 `run_bench_rvv`，`BENCH_ARGS` 使用小规模单 case | 只检查日志形状，不写性能结论。 |
| asm attribution | `make -C test-rvv/features/ppf dump_bench_rvv` | `bench_ppf_rvv.asm` 中出现当前 candidate 相关 RVV 指令；归属不足则降级。 |
| board repeated | `REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated` | 若 board target 和 manifest 接上，生成 board summary / Evidence Doctor；若未接上则记录真实 blocker。 |

## Evidence Doctor 和 registry 规则

本阶段如果生成 board repeated summary，必须通过 topic-local manifest wrapper 再调用
`test-rvv/script/evidence_doctor.py`。如果 wrapper 尚未完成，不得把 board 表格写成完整 doctor 通过；
最多写 `metadata_incomplete` warning。

## 板卡复跑预算和决策桶

默认 `REPEATED_BOARD_RUNS=5`。若 summary 显示方向接近阈值、长尾严重或 `B/A < 1` 频率异常，最多
一次同边界确认复跑。decision bucket（决策桶）：median >= 1.20 且 5/5 高于 1 为 positive；
1.05-1.20 为 weak_positive；0.98-1.05 或跨方向为 neutral；低于 0.98 且多数退化为 negative；
复跑跨桶则 unstable。

## Continue / Stop 条件

默认继续到 correctness、bench build、QEMU smoke、asm 和可用板卡 repeated。合法停止条件是：
candidate correctness 不成立、bench / board harness 缺少必要环境、Evidence Doctor Error 无法修复、
或继续需要修改 production。

## 文档更新清单

更新 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、evaluation 和 Handoff。
不创建 `doc-rvv/features/ppf-RVV.zh.md`。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic / component ablation。 |
| A/B boundary | test helper vs test helper；后续 public output 只作为 shape smoke。 |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape。 |
| diagnostic 是否可外推到 production | unknown。只有同边界 board positive 且 PI1 gate 可控时才允许生产探针。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 只 RVV 化 `f1..f4`，`alpha_m` 仍标量；必须按混合边界解释。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 仅 weak_positive 且静态实现极小、fallback 可控时可讨论；neutral/negative 默认不进入生产。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes，若后续还有 `alpha_m` RVV 或 output staging 家族比较。 |
