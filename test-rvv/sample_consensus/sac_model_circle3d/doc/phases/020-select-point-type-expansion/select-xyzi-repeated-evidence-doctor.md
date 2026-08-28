# Evidence Doctor Report（证据体检报告）

- title：sac_model_circle3d Phase 020 PointXYZI select production repeated board summary
- evidence_role：production-public
- summary_path：test-rvv/sample_consensus/sac_model_circle3d/doc/phases/020-select-point-type-expansion/select-xyzi-repeated-evidence-manifest.json
- comparisons：1
- result：Errors=1，Warnings=1，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

### 1. ba_degradation_frequency — sac_model_circle3d selectWithinDistance production public Std/RVV PointXYZI 65536

- observed_pattern：B/A values=1.06x, 0.865x, 0.871x, 0.88x, 0.88x；4/5 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. long_tail_or_variance — sac_model_circle3d selectWithinDistance production public Std/RVV PointXYZI 65536

- observed_pattern：min=0.865x, median=0.88x, max=1.06x，max/min=1.23。
- why_suspicious：长尾说明单一均值不足以描述稳定性，异常值可能改变接入判断。
- possible_non_bug_explanations：可能是板卡温度、频率、调度、缓存状态或 run 顺序导致。
- possible_bug_or_evidence_issues：也可能是候选存在数据相关分支、fallback、spill/reload 或内存带宽拐点。
- recommended_checks：查看 per-iteration trace、温度 / governor / freq / taskset；必要时扩大到 20-run 或 50-run。
- conclusion_policy：可以继续分析，但 summary 必须保留 min/median/max 和长尾解释，不能先验剔除异常。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。
