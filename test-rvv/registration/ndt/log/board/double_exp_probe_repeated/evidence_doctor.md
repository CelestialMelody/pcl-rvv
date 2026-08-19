# Evidence Doctor Report（证据体检报告）

- title：Evidence Doctor Report
- evidence_role：pre_production_diagnostic
- summary_path：not_recorded
- comparisons：3
- result：Errors=2，Warnings=3，Suggestions=1

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

### 1. ba_degradation_frequency — derivative-gradient-staged

- observed_pattern：B/A values=0.407x, 0.405x, 0.405x, 0.408x, 0.404x；5/5 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

### 2. ba_degradation_frequency — derivative-hessian-staged

- observed_pattern：B/A values=0.321x, 0.323x, 0.322x, 0.321x, 0.318x；5/5 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. contract_mismatch — derivative-gradient-staged

- observed_pattern：字段 'reduction' 不一致：baseline='scalar order', candidate='RVV reductions for gradient only'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 2. contract_mismatch — derivative-hessian-staged

- observed_pattern：字段 'reduction' 不一致：baseline='scalar order', candidate='RVV reductions for gradient and 6x6 hessian'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 3. contract_mismatch — production-public-align-pointxyz

- observed_pattern：字段 'reduction' 不一致：baseline='scalar order', candidate='production scalar reduction order; no NDT RVV production patch'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

### 1. near_threshold_ba — production-public-align-pointxyz

- observed_pattern：median=1.01x，距离 1.0 阈值不足 0.05。
- why_suspicious：接近阈值的收益容易被测量波动、输入分布或二进制差异反转。
- possible_non_bug_explanations：可能仍是可接受的弱收益，尤其当实现简单、fallback 清楚。
- possible_bug_or_evidence_issues：也可能真实收益不足，接入后维护成本高于收益。
- recommended_checks：扩大 runs，补静态实现质量分析；文档写清为什么弱收益仍可接受或为什么暂缓。
- conclusion_policy：不能只因 median 略大于 1 就写成稳定加速。
