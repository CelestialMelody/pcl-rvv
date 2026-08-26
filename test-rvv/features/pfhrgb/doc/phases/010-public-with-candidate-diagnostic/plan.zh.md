# Phase 010 Public-With-Candidate Diagnostic Plan

## 阶段意图和边界

本阶段继续 Phase 000 的 `partial-production-candidate`。目标是在 topic-local（主题本地）测试资产中新增 public-with-candidate diagnostic（公开入口形态候选探针）：外层复刻 `PFHRGBEstimation::compute` 的 KSearch（K 近邻搜索）逐点邻域流程，内层调用 `computePointPFHRGBSignaturePairBatchRVV`，从而判断 pair-batch RVV 候选收益是否能穿透 search、descriptor output 和 per-neighborhood staging 成本。

本阶段仍不修改 `features/include/pcl/features/impl/pfhrgb.hpp`、`features/src/pfh.cpp` 或公开 API。覆盖范围保持 exact `pcl::PointXYZRGBNormal`、`float`、AoS xyz / normal / rgb、synthetic dense finite cloud、`nr_split=5`、KSearch row source。泛型点型、非 dense / NaN、OMP、真实数据集、production dispatch、indices/correspondence 入口不在本阶段证明范围。

## 当前状态清单

| item | 当前事实 |
| --- | --- |
| Phase 000 result | `000-current-state-and-scaffold/result.zh.md` 已关闭；plan 编写时的 expanded repeated run 中 candidate fixed-neighborhood median `1.23x`，当前已被 Phase 030/040 rerun 覆盖。 |
| Evidence Doctor | `log/board/repeated/evidence_doctor.md` 有 1 个 Error，归属 `component_pfhrgb_signature` 退化频率；candidate case 无 Error。 |
| public-like baseline | `public_pfhrgb_k` median `1.00x`，当前无 production patch。 |
| production 状态 | 未修改 production。进入 PI1 前必须另写 production integration plan 并停在用户检查点。 |
| board availability | 当前会话说明板卡可用；本阶段完成 local correctness / asm 后继续 5-run board repeated。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `pfhrgb-public-with-candidate-diagnostic` | 如果 search/output 成本不是绝对主导，把 candidate helper 放进 public-shaped 外层循环后仍应有可见收益。 | 每个点都重新 SoA staging，分配和拷贝可能吞掉 fixed-neighborhood 收益。 |
| `pfhrgb-staging-reuse` | 若 public-with-candidate 中性，预分配 / 复用 staging buffer 可能降低 per-neighborhood 分配成本。 | 需要改 candidate API，可能远离 production helper shape。 |
| `pfhrgb-production-direct-probe` | 若 public-with-candidate positive，进入 PI1 有价值。 | 需要 production 源码授权、fallback gate、point-type strategy 和 direct evidence。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pfhrgb-public-with-candidate-diagnostic` | public KSearch neighborhood | `PointXYZRGBNormal`, float, AoS, `nr_split=5` | topic-local wrapper using candidate helper per point | planned RED/GREEN gtest | planned `public_pfhrgb_k_with_candidate` | planned 5-run repeated | `dump_bench_rvv` must still show candidate RVV instructions | planned | planned | write failing correctness test first |
| `pfhrgb-staging-reuse` | same | same | candidate helper API shape | not_this_phase unless Phase 010 negative/neutral | not_this_phase | not_this_phase | not_this_phase | not_this_phase | deferred | recover only if allocation/staging appears dominant |
| `pfhrgb-production-direct-probe` | public KSearch / production helper | exact point type first | production patch | not_this_phase | not_this_phase | not_this_phase | not_this_phase | not_this_phase | deferred | PI1 only after Phase 010 positive or explicit user authorization |

## 实现和测试动作

| action | 产物 | 命令 / 验收 |
| --- | --- | --- |
| RED-010 | `src/test_pfhrgb.cpp` 新增 public-with-candidate descriptor 对拍测试。 | `make -B -C test-rvv/features/pfhrgb run_test_rvv` 先因 wrapper 缺失失败。 |
| GREEN-010 | 在 topic-local helper 中实现 public-shaped wrapper：建立 KdTree，逐点 `nearestKSearch`，调用 candidate helper，写入 `PFHRGBSignature250`。 | `run_test_compare` 通过，checksum 与 public reference 在当前输入边界内接近。 |
| BENCH-010 | `src/bench_pfhrgb.cpp` 增加 `public_pfhrgb_k_with_candidate` case。 | Std build 走 scalar fallback，RVV build 走 RVV candidate；输出格式兼容 manifest。 |
| ASM-010 | 刷新 `dump_bench_rvv`。 | 反汇编仍能归属 candidate RVV 指令。 |
| BOARD-010 | 重跑 `REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated`。 | 更新 manifest、Doctor、registry、matrix、roadmap 和 Phase 010 result。 |

## Evidence Doctor 和 registry 规则

本阶段使用 `log/board/repeated/evidence_manifest.json` 与 `log/board/repeated/evidence_doctor.md`。若新增 case 触发 strict A/B、metadata、binary identity、near-threshold 或 degradation frequency，必须在 `result.zh.md` 中解释、降级或转成 blocker。raw logs 默认 local-only；summary-only registry 必须在 rerun 后刷新。

## 板卡复跑预算和决策桶

复跑预算为 5 runs，warmup 2，iterations 8；预算内最多允许 1 次同边界确认复跑。`public_pfhrgb_k_with_candidate` 的 decision bucket：

- `positive`：median >= `1.08x`，且 0/5 低于 1。
- `weak_positive`：median >= `1.03x`，且最多 1/5 低于 1。
- `neutral`：median 在 `0.97x` 到 `1.03x`。
- `negative`：median < `0.97x`。
- `unstable`：跨方向频繁摇摆，预算耗尽后仍不能归入上述桶。

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；不是 production direct。 |
| A/B boundary | topic-local public-shaped wrapper；Std build scalar fallback vs RVV build candidate helper。 |
| 当前决策问题 | RVV-vs-scalar feasibility 和 public-path dilution（公开路径稀释）判断。 |
| diagnostic 是否可外推到 production | 仍不能直接外推；它只比 Phase 000 更接近 public path。 |
| comparison-boundary / baseline mismatch 风险 | 有。wrapper 不是 `pfhrgb.hpp` 的真实 dispatch，candidate helper API 与 production protected method 仍不同。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有当静态审计发现生产接入能显著减少 wrapper 额外成本时才允许；否则应先尝试 staging reuse 或暂停生产接入。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；Phase 010 不能 clean-adopt。 |

## 阶段完成条件

Phase 010 完成需要：RED/GREEN correctness、bench case、asm 归属、5-run board repeated、Evidence Doctor、registry 刷新、Phase 010 result、optimization matrix、roadmap 和 evaluation 全部同步。若 `public_pfhrgb_k_with_candidate` 为 positive / weak-positive，则默认下一 phase 是 PI1 production integration plan；若 neutral / negative / unstable，则默认下一 phase 是 staging allocation / reuse 消融或 no-production mismatch audit，不能直接删除候选证据。

## Continue / Stop 条件

`stop_condition_hit` 仅在以下情况成立：新 wrapper correctness 无法复刻 public descriptor、board/工具链不可用、Evidence Doctor Error 无法解释、dirty isolation 不安全，或继续需要修改 production source。否则本阶段默认持续推进到 repeated board 和 Doctor 完成。
