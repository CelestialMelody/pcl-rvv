# Evidence Doctor Report（证据体检报告）

- title：sac_model_line Phase 070 shuffled-index RVV-vs-RVV repeated board summary
- evidence_role：strict_ab
- summary_path：test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-manifest.json
- comparisons：3
- result：Errors=3，Warnings=0，Suggestions=2

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

### 1. ba_degradation_frequency — sac_model_line public countWithinDistance PointXYZ 65536 identity-shuffled gather-only RVV vs identity-strided RVV

- observed_pattern：B/A values=1.01x, 1x, 1x, 1x, 0.993x；2/5 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

### 2. ba_degradation_frequency — sac_model_line public selectWithinDistance PointXYZ 65536 identity-shuffled gather-only RVV vs identity-strided RVV

- observed_pattern：B/A values=0.984x, 0.998x, 1.01x, 1.02x, 1.01x；2/5 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

### 3. ba_degradation_frequency — sac_model_line public getDistancesToModel PointXYZ 65536 identity-shuffled gather-only RVV vs identity-strided RVV

- observed_pattern：B/A values=1.01x, 0.992x, 0.996x, 1x, 0.992x；3/5 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

## Warnings（可以继续，但结论必须说明风险和处理方式）

无。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

### 1. near_threshold_ba — sac_model_line public countWithinDistance PointXYZ 65536 identity-shuffled gather-only RVV vs identity-strided RVV

- observed_pattern：median=1x，距离 1.0 阈值不足 0.05。
- why_suspicious：接近阈值的收益容易被测量波动、输入分布或二进制差异反转。
- possible_non_bug_explanations：可能仍是可接受的弱收益，尤其当实现简单、fallback 清楚。
- possible_bug_or_evidence_issues：也可能真实收益不足，接入后维护成本高于收益。
- recommended_checks：扩大 runs，补静态实现质量分析；文档写清为什么弱收益仍可接受或为什么暂缓。
- conclusion_policy：不能只因 median 略大于 1 就写成稳定加速。

### 2. near_threshold_ba — sac_model_line public selectWithinDistance PointXYZ 65536 identity-shuffled gather-only RVV vs identity-strided RVV

- observed_pattern：median=1.01x，距离 1.0 阈值不足 0.05。
- why_suspicious：接近阈值的收益容易被测量波动、输入分布或二进制差异反转。
- possible_non_bug_explanations：可能仍是可接受的弱收益，尤其当实现简单、fallback 清楚。
- possible_bug_or_evidence_issues：也可能真实收益不足，接入后维护成本高于收益。
- recommended_checks：扩大 runs，补静态实现质量分析；文档写清为什么弱收益仍可接受或为什么暂缓。
- conclusion_policy：不能只因 median 略大于 1 就写成稳定加速。
