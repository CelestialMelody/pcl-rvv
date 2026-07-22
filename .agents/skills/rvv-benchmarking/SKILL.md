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

`output/`、`build/`、`log/`、反汇编 dump 和板卡抓回日志默认是工作区证据，不是默认可提交产物。closeout 前必须检查 `git status --short --ignored`：如果日志未被 ignore 或含本机路径，只在 handoff 中列证据路径和摘要，不把这些文件加入源码 / 文档提交。

用户明确要求提交 evidence logs（证据日志）时，先冻结 evidence log policy（证据日志策略）：

- `summary-only`：只在文档和 handoff 写摘要与路径，不提交日志文件。默认策略。
- `sanitized-logs`：提交脱敏日志。用户说要提交 log 时默认采用这个策略，优先运行 `make sanitize_output_logs` 和 `make check_output_logs_sanitized`，或直接运行 `test-rvv/script/sanitize_evidence_logs.py --check <logs>`。
- `raw-logs`：提交原始日志；只在用户明确要求保留原文、脱敏日志不足以复核、且 reviewer 已确认没有凭据或私有地址风险时使用。

日志提交应与 topic 源码 / 文档拆成独立 commit。通常只考虑 `output/qemu/*.log`、`output/board/*.log` 和必要的 bench compare 摘要；不要默认提交 `build/` 二进制、完整反汇编 dump、`log/vec_missed_log/`、本机 `config.mk` 或临时 deploy 脚本。若必须提交反汇编或编译诊断日志，先说明为什么摘要不足以复核结论。提交后在 handoff 中记录脱敏命令、check 命令、日志 commit 和被排除文件。

细则见 [references/evidence-logs.md](references/evidence-logs.md)。

## 性能判断

性能阈值是判断输入，不是单独开关：

- `>= 2.0x`：强候选。
- `1.2x ~ 2.0x`：中高候选，需要确认覆盖面和维护成本。
- `1.05x ~ 1.2x`：弱收益候选，只在入口常用、实现小、fallback 简单、语义风险低且证据完整时接生产。
- `< 1.05x`：通常不接生产，保留诊断和回退原因。

如果只有 local fragment 快，但 full diagnostic 或生产入口被后续主成本稀释，默认不接生产。

性能结果不成立时，closeout 需要写清负向证据的含义。至少说明退化发生在哪些 case、这些 case 覆盖哪条入口或路径、可能的实现原因是什么，以及还缺哪些 profile、反汇编归属、消融 bench 或目标硬件复测才能把假设变成结论。不要只写 speedup 小于阈值。
