# Evidence Doctor Report（证据体检报告）

- title：pfhrgb board Std/RVV compare
- evidence_role：mixed_diagnostic_and_public_like
- summary_path：log/board/repeated
- comparisons：5
- result：Errors=1，Warnings=1，Suggestions=6

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

### 1. ba_degradation_frequency — candidate_pfhrgb_pair_batch_rvv

- observed_pattern：B/A values=0.98x, 0.97x, 0.99x, 0.98x, 0.97x；5/5 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. ba_degradation_frequency — component_pfhrgb_signature

- observed_pattern：B/A values=1x, 1x, 1.02x, 1.01x, 0.99x；1/5 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

### 1. environment_metadata_missing — candidate_pfhrgb_pair_batch_rvv

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 2. environment_metadata_missing — component_pfhrgb_signature

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 3. near_threshold_ba — component_pfhrgb_signature

- observed_pattern：median=1x，距离 1.0 阈值不足 0.05。
- why_suspicious：接近阈值的收益容易被测量波动、输入分布或二进制差异反转。
- possible_non_bug_explanations：可能仍是可接受的弱收益，尤其当实现简单、fallback 清楚。
- possible_bug_or_evidence_issues：也可能真实收益不足，接入后维护成本高于收益。
- recommended_checks：扩大 runs，补静态实现质量分析；文档写清为什么弱收益仍可接受或为什么暂缓。
- conclusion_policy：不能只因 median 略大于 1 就写成稳定加速。

### 4. environment_metadata_missing — public_pfhrgb_k

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 5. environment_metadata_missing — public_pfhrgb_k_with_candidate

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 6. environment_metadata_missing — public_pfhrgb_k_with_candidate_reuse

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。
