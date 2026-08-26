# Phase 030 Staging Reuse Ablation Plan

## 阶段意图和边界

本阶段在 topic-local（主题本地）测试资产中做 staging reuse ablation（暂存复用消融）。Phase 010 的 `public_pfhrgb_k_with_candidate` 为 positive，但 wrapper 每个点都会重新构造 SoA staging（结构数组到分字段暂存）和 tuple buffers（中间特征缓冲）。本阶段验证复用这些缓冲是否能减少 public-shaped diagnostic（公开入口形态诊断）的额外成本。

本阶段不修改 production source（生产源码）和 public API。覆盖范围仍是 exact `pcl::PointXYZRGBNormal`、`float`、AoS xyz / normal / rgb、synthetic dense finite cloud、`nr_split=5`、KSearch row source。计划入口时板卡 SSH 不可达；若本阶段完成 QEMU correctness（正确性）和 ASM（反汇编）后板卡仍不可达，board repeated 标为 `turn_stop_deferred with stop_condition_hit`。Phase 030 result 已在板卡恢复后刷新当前证据。

## 当前状态清单

| item | 当前事实 |
| --- | --- |
| Phase 010 | plan 入口时 `public_pfhrgb_k_with_candidate` repeated board median `1.22x`，0/5 below 1，Doctor 0 Errors / 0 Warnings；当前 truth 已由 Phase 030/040 rerun 覆盖。 |
| Board | plan 入口时 `check_board_ssh` 返回 `No route to host`；Phase 030/040 已恢复可达并完成 repeated。 |
| Production | `features/include/pcl/features/impl/pfhrgb.hpp` 尚未修改；PI1 仍等待用户确认和板卡恢复。 |

## 候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `pfhrgb-staging-reuse` | 在 public-shaped wrapper 中复用 staging 和 tuple buffers，可降低 per-neighborhood allocation（逐邻域分配）成本。 | 如果 KSearch 或 histogram scatter 是主成本，复用收益可能很小。 |
| `pfhrgb-production-direct-probe` | Phase 010 positive 支持 PI1，但需要 production 授权和板卡。 | 当前不执行。 |

## 实现和测试动作

| action | 产物 | 验收 |
| --- | --- | --- |
| RED-030 | 新增 gtest，调用 `computePublicPFHRGBWithReusablePairBatchCandidate` 并与 public estimator descriptor 对拍。 | `run_test_rvv` 先因 helper 缺失失败。 |
| GREEN-030 | 新增 `PairBatchWorkspace`、reusable pair-batch helper 和 public-shaped reusable wrapper。 | `run_test_compare` 通过。 |
| BENCH-030 | 新增 `public_pfhrgb_k_with_candidate_reuse` case。 | bench build 和 QEMU log-shape 可运行；性能结论等待板卡。 |
| ASM-030 | 刷新 `dump_bench_rvv`。 | 仍能看到 candidate RVV 指令。 |
| BOARD-030 | 若 board 恢复，跑 5-run repeated + Doctor。 | 若仍不可达，记录 stop condition，不把 QEMU timing 写性能结论。 |

## 决策桶

若 board 可达，`public_pfhrgb_k_with_candidate_reuse` 与 `public_pfhrgb_k_with_candidate` 同边界比较：median >= `1.03x` 且最多 1/5 below 1 标为 positive for reuse；`0.97x..1.03x` 为 neutral；低于 `0.97x` 为 negative；摇摆为 unstable。若 board 不可达，本阶段只能关闭 correctness / build / asm，不能关闭性能 decision。

## Continue / Stop 条件

本阶段默认推进到 RED/GREEN、bench build、ASM 和 board recheck。停止条件：board 仍不可达、继续需要 production source、或 helper correctness 无法复刻 public descriptor。
