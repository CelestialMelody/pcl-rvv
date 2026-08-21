# Phase 030: downsample-rvv-probe Result

## 当前结论

Phase 030 完成了 RGB downsample（下采样）测试专用 RVV probe（探针）。候选
`downsample-rgb-stride-vsseg3-u8` 在 test helper 边界下 correctness、QEMU smoke、
asm 和 5-run board evidence 均闭合；Evidence Doctor 有 1 个 long-tail warning（长尾波动警告），
但无 Error。

EvidenceDecision：`partial-production-candidate`。该结论只说明 downsample test helper 候选
值得进入有界 production integration loop（生产接入闭环）；它还不是 adopted production behavior，
也不能替代真实 `ImageYUV422::fillRGB` production direct 证据。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| downsample RVV helper | done | `test-rvv/io/image_yuv422/include/image_yuv422.h` | 新增 `fillRgbDownsampleRVV`，用 `vlse8` 跨步读取 U/Y/V，用 `vsseg3e8` 连续写 RGB。 |
| correctness | done | `make -C test-rvv/io/image_yuv422 run_test_compare` | Std/RVV 各 6 个 gtest 通过；downsample padding 也保持不写。 |
| QEMU smoke | done | `run_bench_rvv --case-filter rgb_downsample_640x480_to_320x240 --iterations 1 --warmup-iterations 1` | bench 可运行并输出 checksum；QEMU timing 不作为性能结论。 |
| asm | done | `dump_bench_rvv` | `fillRgbDownsampleRVV` 符号内可见 `vlse8.v` 和 `vsseg3e8.v`。 |
| board repeated | done | `log/board/rgb_downsample_640x480_to_320x240_repeat_5/summary.md` | mean `1.1821x`、median `1.1477x`、min `1.0949x`、max `1.3051x`。 |
| Evidence Doctor | done | `log/board/evidence_doctor.md` | `Errors=0, Warnings=1, Suggestions=0`；warning 为 long-tail / variance，需要 production direct 阶段保留 min/median/max。 |

## Evidence Doctor 解释

Doctor warning：`long_tail_or_variance`。5-run speedup 为 `1.1065x`、`1.0949x`、
`1.2561x`、`1.3051x`、`1.1477x`，max/min 为 `1.19`。这说明单一均值不足以描述稳定性；
但所有 run 均大于 `1.00x`，median 大于 `1.10x`，方向没有摇摆。本阶段将其判为 positive
production-shaped diagnostic（生产形态诊断正向），但要求下一阶段 production direct repeated board
继续保留 min / median / max，不能只用 mean 采纳。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar，判断 downsample 候选是否值得进入生产探针 |
| diagnostic 是否可外推到 production | unknown；test helper 复刻 downsample 语义，但 production dispatch、line-step 和异常输入 gate 仍需真实入口验证。 |
| comparison-boundary / baseline mismatch 风险 | yes；当前未调用真实 `ImageYUV422::fillRGB` downsample dispatch。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前为 positive，但有 variance warning；允许 bounded production probe，范围仅限 RGB downsample 且必须保留 scalar fallback。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | no；当前 production downsample 没有既有 RVV family，下一阶段决策是 public RVV path 是否快于 public scalar path。 |

## Continue / Stop Decision

本阶段没有命中停止条件。默认下一步是 `040-downsample-production-integration`：
把 `fillRgbDownsampleRVV` 迁入 `io/src/image_yuv422.cpp` 的生产边界，补 production direct correctness、
QEMU smoke、asm、5-run board 和 Evidence Doctor。若 production direct 仍为 positive，再停在 PI5
用户检查点；若 neutral / negative，则等待用户授权回滚或保留为实验路径。
