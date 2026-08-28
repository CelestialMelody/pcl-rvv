# Phase 000 Result: current-state-and-score-accumulation

Status: completed

## EvidenceDecision

Phase 000 的结论是 `attempted-positive / continue-next-phase`。`u8` linearized score maps 到 `u16 score_sums` 的 RVV helper 在 correctness（正确性）、QEMU 路径、反汇编归属和 Milkv-Jupiter 5-run board repeated bench 中都给出正向信号；但证据角色仍是 `production_shaped_diagnostic`（生产形态诊断），因为计时边界只覆盖 test helper 的 score accumulation，不包含 `EnergyMaps`、`LinearizedMaps`、threshold scan、detection 输出顺序或真实 production dispatch（生产分流）。因此本阶段不进入 production integration loop，默认继续 Phase 010 threshold / max scan 诊断。

## 实现和测试结果

| action | result | evidence |
| --- | --- | --- |
| RED 测试 | 已观察到 helper 缺失导致编译失败 | `make run_test_rvv TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"` 历史 RED |
| 标量和 RVV helper | 已实现 `accumulateScoreMapsStd` / `accumulateScoreMapsRVV` | `include/impl/linemod_template_scoring_candidates.hpp` |
| QEMU correctness | Std/RVV 两侧 2 个 gtest 通过 | `make run_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"` |
| bench warm-up | bench CLI 支持 `mem_size nr_maps iterations warmup_iterations`，默认 `4096 96 200 5` | `src/bench_linemod_template_scoring.cpp` |
| asm attribution | RVV bench 反汇编含 `vle8.v`、`vzext.vf2`、`vadd.vv`、`vse16.v` | `build/asm/riscv/bench_linemod_template_scoring_rvv.full.asm` |
| repeated board | Milkv-Jupiter 5-run，checksum 全部一致，0/5 退化 | `log/board/repeated-20260828-phase000-score-accumulation-warmup5/` |
| Evidence Doctor | Errors=0，Warnings=1，Suggestions=0 | `test-rvv/recognition/linemod_template_scoring/doc/phases/000-current-state-and-score-accumulation/score-accumulation-repeated-evidence-doctor.md` |
| Evidence registry | 已登记 summary / manifest / doctor；文档刷新前曾有 doc_ref_missing，刷新后需复查 | `log/evidence_registry.json` |

## Board Summary

当前 summary 主归属为 `test-rvv/recognition/linemod_template_scoring/doc/phases/000-current-state-and-score-accumulation/score-accumulation-repeated-summary.md`，manifest 为 `test-rvv/recognition/linemod_template_scoring/doc/phases/000-current-state-and-score-accumulation/score-accumulation-repeated-evidence-manifest.json`，machine-readable doctor JSON 为 `test-rvv/recognition/linemod_template_scoring/doc/phases/000-current-state-and-score-accumulation/score-accumulation-repeated-evidence-doctor.json`。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum |
| --- | ---: | ---: | ---: | --- |
| run-01 | 389.759000 | 228.749000 | 1.704x | `9840709576022462159` |
| run-02 | 291.746000 | 226.768000 | 1.287x | `9840709576022462159` |
| run-03 | 444.949000 | 227.406000 | 1.957x | `9840709576022462159` |
| run-04 | 316.838000 | 225.652000 | 1.404x | `9840709576022462159` |
| run-05 | 292.119000 | 227.005000 | 1.287x | `9840709576022462159` |

统计口径：Std median `316.838 us/iter`，RVV median `227.005 us/iter`，speedup median `1.404x`，min `1.287x`，max `1.957x`，degrade frequency `0/5`。第一次 no-warmup repeated run 已降级为 historical evidence；当前 EvidenceDecision 只引用 `warmup5` run label。

## Evidence Doctor 处理

Evidence Doctor 报告 `Errors=0，Warnings=1，Suggestions=0`。唯一 warning 是 `long_tail_or_variance`：speedup min/median/max 为 `1.29x / 1.40x / 1.96x`，`max/min=1.52`。本阶段处理方式：

- 不剔除异常值，summary 保留每轮数值。
- 结论保持 `positive` decision bucket，因为 5/5 均正向、checksum 一致、RVV 侧耗时稳定在约 `225.652-228.749 us/iter`。
- 不把该诊断写成 production performance；下一 phase 若进入真实生产接入或 public entry bench，需要重新做同边界 board repeated 和 Evidence Doctor。
- 远端 board 运行出现 `script/rvv-board-run.mk` clock skew warning；命令成功、测试和 bench 输出完整，但该环境信号应在后续生产证据中继续记录。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` |
| 当前决策问题 | `RVV-vs-scalar` for score accumulation helper |
| diagnostic 是否可外推到 production | `unknown`；只证明 already-linearized maps 的累加核 |
| comparison-boundary / baseline mismatch 风险 | `yes`；真实入口还包含 map 构建、feature 遍历、threshold / NMS / averaging 和输出容器写入 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `yes`，条件是后续 phase 或 profile 证明 score scan / threshold 仍是真实入口中的可观成本 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采用 RVV family；如果进入 production，仍需 production direct Std/RVV repeated evidence；若新增多个 RVV family，再补同边界 RVV-vs-RVV A/B |

## Phase Reflection

score accumulation 的 RVV widening add（拓宽加法）本身收益明确，RVV 侧耗时稳定；Std 侧波动说明完整 production 接入前要避免只看单次 speedup。下一步更有价值的是把 threshold / max scan 也拆成同边界 helper：它可复用 `score_sums`，并能直接验证 strict `>` tie-break、raw threshold、NMS 前候选顺序和 detection 输出保序边界。Energy map generation 与 linearized map copy 仍是 unblocked 后续候选，但默认排在 Phase 010 之后。

## Document Ownership / Traceability

| fact | owner | reference |
| --- | --- | --- |
| board repeated 逐轮数值 | phase summary | `test-rvv/recognition/linemod_template_scoring/doc/phases/000-current-state-and-score-accumulation/score-accumulation-repeated-summary.md` |
| Evidence Doctor findings | doctor report | `test-rvv/recognition/linemod_template_scoring/doc/phases/000-current-state-and-score-accumulation/score-accumulation-repeated-evidence-doctor.md` |
| candidate / matrix 状态 | optimization matrix | `doc/phases/optimization-matrix.zh.md` |
| 函数级结论和生产错配审计 | evaluation | `doc/linemod_template_scoring-evaluation.zh.md` |
| 下一阶段恢复入口 | phase index / roadmap | `doc/phases/README.zh.md`、`doc/optimization-roadmap.zh.md` |

## Continue / Stop Decision

未命中停止条件。板卡可用、correctness 通过、doctor 无 Error，且 roadmap 中存在未阻塞的 threshold / max scan、energy map generation、linearized map copy 候选。默认继续 Phase 010：`threshold-scan-and-detection-order`。
