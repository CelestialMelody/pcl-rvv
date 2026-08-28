# Evidence Doctor Report（证据体检报告）

- title：sac_model_circle Phase 020 getDistances diagnostic repeated board summary
- evidence_role：production_shaped_diagnostic
- summary_path：test-rvv/sample_consensus/sac_model_circle/doc/phases/020-circle-getdistances-ablation/getdistances-repeated-evidence-manifest.json
- comparisons：1
- result：Errors=1，Warnings=1，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

### 1. ba_degradation_frequency — sac_model_circle getDistancesToModel public vs test-only RVV candidate PointXYZ 65536

- observed_pattern：B/A values=0.66x, 0.653x, 0.655x, 0.663x, 0.659x；5/5 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. fewer_instructions_but_slower — sac_model_circle getDistancesToModel public vs test-only RVV candidate PointXYZ 65536

- observed_pattern：candidate RVV 指令数 13 少于 baseline 76，但 B/A median=0.659x。
- why_suspicious：指令数更少但更慢说明瓶颈可能不在总指令数，或者归因边界没有闭合。
- possible_non_bug_explanations：可能是寄存器压力、spill/reload、访存、vsetvl、分支、cache 或频率波动。
- possible_bug_or_evidence_issues：也可能 asm attribution 统计到了不同符号、inline/clone 或无关代码。
- recommended_checks：检查 hot symbol、mnemonic histogram、spill/reload、load/store、vsetvli 和 trace；必要时做更窄 component ablation。
- conclusion_policy：不能只用指令数减少支撑接入；必须解释为什么性能没有同步改善。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。
