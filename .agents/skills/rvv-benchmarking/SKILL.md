---
name: rvv-benchmarking
description: 构建和审查 RVV 专项 test/bench、QEMU、反汇编、板卡验证和证据日志。适用于验证 RVV correctness、bench 输出合同、QEMU 路径、board 性能结论、反汇编证据、evidence logs 分组，以及性能不足时的回退记录。
---

# RVV Benchmarking 与证据工作流

使用本 skill 时，目标是形成可复核的证据链，区分正确性、路径命中和真实性能结论。

回复、bench 文档、Makefile/board.mk 注释和日志说明遵循 `rvv-workflow/references/reviewability-and-language.zh.md`：英文术语首次出现时必须解释；中文主导时给中文解释，英文主导时也要给 plain-English explanation（白话解释）。中文说明要自然，避免翻译腔。输出中区分 gate（可失败验收条件）、diagnostic value（诊断值）、QEMU correctness（QEMU 正确性）和 board evidence（板卡证据）。

## 证据边界

- QEMU：构建、正确性、日志格式、RVV 指令路径辅助证据。
- 反汇编：确认关键 RVV 指令、舍入、mask、压缩、gather、stride、规约和异常路径。
- 板卡或目标硬件：真实性能结论。
- x86：仅用于同平台 baseline vs SIMD 对照，不与 RISC-V 板卡绝对耗时比较。

QEMU、反汇编和板卡细则见 [references/qemu-board-disassembly.md](references/qemu-board-disassembly.md)。

## Bench 输出合同

bench 输出必须可解析，并保留：

- `Dataset:`
- `Iterations:`
- 每个 case 的 avg ms/iter 或分析脚本支持的等价格式。
- `Total Time`。
- checksum。
- case 参数或构建信息。

细则见 [references/bench-output-contract.md](references/bench-output-contract.md)。

上游原始测试和 x86 SIMD 对照见 [references/upstream-and-x86.md](references/upstream-and-x86.md)。有对应上游测试时优先补跑；已有 x86 SIMD 时可做同平台对照，但不与 RISC-V 目标硬件绝对耗时比较。

## Evidence logs

证据日志应单独归档、单独分组，不能混入源码或文档提交。日志中不得写入个人路径、私有板卡地址、用户名或临时本机配置。

细则见 [references/evidence-logs.md](references/evidence-logs.md)。

## 性能判断

性能阈值是判断输入，不是单独开关：

- `>= 2.0x`：强候选。
- `1.2x ~ 2.0x`：中高候选，需要确认覆盖面和维护成本。
- `1.05x ~ 1.2x`：弱收益候选，只在入口常用、实现小、fallback 简单、语义风险低且证据完整时接生产。
- `< 1.05x`：通常不接生产，保留诊断和回退原因。

如果只有 local fragment 快，但 full diagnostic 或生产入口被后续主成本稀释，默认不接生产。
