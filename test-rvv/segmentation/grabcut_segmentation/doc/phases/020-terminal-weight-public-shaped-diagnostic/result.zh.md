# Phase 020: terminal weight public-shaped diagnostic result

## 阶段结论

Phase 020 完成了 terminal weight（端点权重）的 public-shaped diagnostic（公开入口形态诊断）。本阶段复刻
`GrabCut<PointT>::initGraph()` 在 unknown trimap（未知三分图标记）分支里的核心公式：两个 K=5
GMM（高斯混合模型）分别执行 `probabilityDensity(c)` 的 `pi` 加权求和，再转换成
`fore=-log(background_probability)` 与 `back=-log(foreground_probability)`。

当前结果支持继续做 PI1 前置计划或更接近 production（生产源码）的 `initGraph` no-solve（不含求解）
probe；仍不支持直接把 test-only helper 接入 production。真实 `setTerminalWeights`、n-link graph edge
mutation（图边写入）和 max-flow（最大流）求解尚未进入计时边界。

## 执行范围回填

| 计划动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED terminal correctness | done | `make run_test_compare` 曾因 `grabcut_diag::MixtureGMM`、`computeTerminalWeightsReference`、`computeTerminalWeightsCandidate` 缺失失败。 | 测试能捕获 public-shaped helper 缺失。 |
| GREEN helper | done | `include/impl/grabcut_diagnostic_reference.hpp` | 增加 `Gaussian::pi`、`MixtureGMM`、mixture probability 和 terminal weight wrapper；Std build 走标量同构链路，RVV build 复用 Phase 010 的 RVV per-Gaussian batch。 |
| correctness 对拍 | done | `make run_test_compare` | Std 3 tests passed；RVV 4 tests passed。terminal weight 最大误差受 `abs=5e-5` 与 `rel=3e-4` 组合预算约束。 |
| bench extension | done | `src/bench_grabcut.cpp` 支持 `--case terminal_weights` | bench 输出 `checksum_policy=input_fingerprint_error_budget`、`max_abs_error`、`max_rel_error` 和 `max_budget_ratio`。 |
| QEMU smoke | done | `make run_test_compare run_bench_std run_bench_rvv analyze_bench_compare BENCH_ARGS="--width 128 --height 96 --iterations 2 --warmup 1 --case terminal_weights" ALLOW_QEMU_BENCH_COMPARE=1` | QEMU 只证明 build、correctness、路径和日志形状；QEMU timing 不作为性能结论。 |
| 单次板卡 smoke | done | `make board_smoke BENCH_ARGS="--width 640 --height 480 --iterations 8 --warmup 2 --case terminal_weights" REMOTE_TEST_ARGS=""` | Milkv-Jupiter 上单次为 Std `254.974807 ms`、RVV `167.334151 ms`，约 `1.52x`。 |
| 5-run repeated board | done | `doc/phases/020-terminal-weight-public-shaped-diagnostic/repeated-evidence-manifest.json` | 5 个 B/A 值为 `1.5433, 1.5398, 1.5348, 1.5250, 1.5485`；median（中位数）约 `1.5398x`。 |
| Evidence Doctor | done | `doc/phases/020-terminal-weight-public-shaped-diagnostic/repeated-evidence-doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 |
| 反汇编归属 | done | `build/asm/riscv/bench_grabcut_rvv.full.asm` | `computeTerminalWeightsCandidate` 符号内调用 `computeGMMProbabilityCandidate`，并在 mixture accumulation / store 边界出现 `vsetvli`、`vle32.v`、`vse32.v`；per-Gaussian helper 内出现 RVV FMA 和 `pcl::expf_RVV_f32m2` 多项式链路。 |

## 诊断证据链

| 证据层 | 当前事实 | 不能证明的范围 |
| --- | --- | --- |
| correctness | `GrabCutDiagnosticReference.TerminalWeightsMatchScalarReference` 覆盖两个 K=5 GMM 和 6 个 BGR 样本。 | 不覆盖 production GMM fitting、trimap 分支、graph node 映射或 max-flow。 |
| 数值预算 | repeated board 最大 `max_abs_error=2.328306e-07`，`max_budget_ratio=0.004656613`。 | `max_rel_error=0.003508725` 出现在极小 expected 值附近；结论需结合绝对 / 相对组合预算解释。 |
| QEMU | QEMU correctness 和 terminal bench log-shape 通过。 | QEMU timing 不进入性能排序。 |
| 反汇编 | Terminal wrapper 符号和 per-Gaussian RVV helper 都有可归属的 RVV 指令。 | 仍不是 production binary 的 public dispatch 证据。 |
| 板卡 repeated | Milkv-Jupiter `640x480`、`iterations=8`、`warmup=2` 下 5-run median 约 `1.5398x`，且所有 B/A 均为正向。 | 只是 test-support helper 的 production-shaped diagnostic，不证明完整 `extract/refineOnce`。 |
| Evidence Doctor | repeated manifest 报告 `Errors=0, Warnings=0, Suggestions=0`。 | Doctor 只覆盖 manifest 中表达的诊断边界；不能替代 reviewer 对源码、生产边界和日志新鲜度的审查。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic / component_ablation`。 |
| A/B boundary | `test_support_helper`。baseline 是 topic-local scalar reference，candidate 是 topic-local RVV helper。 |
| 当前决策问题 | Phase 010 单分量收益是否能穿过 K=5 mixture 和 terminal `-log`，是否值得进入 PI1 前置计划。 |
| diagnostic 是否可外推到 production | 只能部分外推。公式和 `initGraph` unknown 分支一致，但本阶段不包含 `setTerminalWeights`、n-link edge mutation、graph clear/addNodes、trimap background/foreground 分支或 max-flow。 |
| comparison-boundary / baseline mismatch 风险 | 存在。当前是不同 build 的 helper-level Std/RVV 对比，不是同一 production boundary 内的 RVV-vs-RVV family selection。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前不是弱 / 负 / 中性 / 不稳定；若后续 public-shaped no-solve probe 反转，仍可保留 bounded production probe 作为人工检查点，而不能直接写 no-production。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。任何 production 接入必须另走 PI1-PI5，且 PI5 等待用户确认是否采纳或回滚。 |

## Evidence Doctor 和证据新鲜度

主证据是 `doc/phases/020-terminal-weight-public-shaped-diagnostic/repeated-evidence-manifest.json` 和
`doc/phases/020-terminal-weight-public-shaped-diagnostic/repeated-evidence-doctor.md`。原始 5-run 日志保存在
`doc/phases/020-terminal-weight-public-shaped-diagnostic/repeated-board-20260827-141925/`，默认不提交 raw logs
（原始日志），除非用户明确要求保留已脱敏 evidence logs。

`log/board/run_bench_std.log` 和 `log/board/run_bench_rvv.log` 会被 `board_smoke` 覆盖，只能代表最后一次
板卡 run。需要审查 repeated 结果时以 phase-local repeated manifest 为准。

## 阶段反思和矩阵更新

| candidate family | 更新 | 下一步 |
| --- | --- | --- |
| terminal-weight public-shaped component | `attempted / positive diagnostic`。K=5 mixture + terminal `-log` 后仍保持 5-run 正向。 | 下一步可创建 PI1 pre-production plan，或先做 `initGraph` no-solve probe，把 `setTerminalWeights` 和 graph node 写入成本纳入边界。 |
| GMM probability density | Phase 010 单分量正向信号得到 Phase 020 支持。 | 若进入 production integration loop，复用此 helper 的公式和数值预算，但必须重建 production direct correctness / fallback。 |
| organized n-link RVV exp candidate | 仍不作为生产接入触发项。 | 只在需要解释 n-link 退化时 reopen。 |

## Continue / stop decision

`continue_stop_decision=continue`。没有命中停止条件：板卡可用，correctness、asm、5-run board repeated 和
Evidence Doctor 均已闭合到 diagnostic 边界。默认下一动作是 Phase 030 / PI1 前置计划：

- 若保持不修改 production：构造 `initGraph` no-solve diagnostic，把 graph node 和 terminal weight write 计入。
- 若用户明确授权进入 production integration loop：先写 PI1 计划，冻结 fallback、公开入口、数值预算、point type / layout、board repeated 和 PI5 用户检查点。
