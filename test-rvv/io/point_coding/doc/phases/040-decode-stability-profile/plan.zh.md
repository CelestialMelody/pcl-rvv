# Phase 040：decode 稳定性复核计划

## 阶段意图和边界

Phase 020 刷新后，decode contiguous（连续输出片段解码）6 个规模都保持 median 正向，但它仍只是 synthetic component ablation（合成组件消融）证据。本阶段用独立 board output dir 做 decode-only repeated board（仅解码重复板卡测试），判断 decode 是否仍有稳定弱正向，还是需要继续降级。

本阶段不修改 production，不修改默认 encode candidate，不覆盖 `log/board/repeated_phase000` 的当前 12-case summary。

## 当前状态

| area | 当前状态 |
| --- | --- |
| correctness | `make run_test_compare` 已通过。 |
| asm | 默认 bench asm 可见 `vlse8.v` 和 `vsse32.v`。 |
| current 5-run | decode median 1.18x 到 1.29x，当前 Evidence Doctor 没有 decode-specific Error。 |
| production boundary | 仍不是真实 decoder iterator production evidence。 |

## 执行动作

1. 使用 `BENCH_ARGS="--iterations 20 --warmup-iterations 3 --case-filter decode_contiguous_*"` 跑 decode-only board repeated。
2. 将输出目录设置为 `log/board/repeated_phase040_decode`，避免覆盖 default summary。
3. 运行 Evidence Doctor，并将 Errors / Warnings / Suggestions 写入 result。
4. 更新 roadmap、optimization matrix、benchmark/evidence 和 evaluation。

## 板卡复跑预算和决策桶

本阶段预算为 10-run decode-only repeated board。若 Evidence Doctor 无 Error 且各规模 median > 1，结论写 `weak-positive diagnostic`。若任一规模出现高频退化或长尾改变 decision bucket，decode 降级为 `unstable diagnostic`，不进入 production-shaped probe。

## 完成条件

| 条件 | 判定 |
| --- | --- |
| stable weak-positive | 10-run 中 median > 1，退化频率不触发 Error。 |
| unstable | 退化频率、长尾或 doctor Error 触发，或 min/max 让 decision bucket 不稳。 |
| production probe | 本阶段不能直接进入；即使 stable，也只允许作为后续 production-shaped context scout 的输入。 |
