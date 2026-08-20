# Evidence Doctor Report（证据体检报告）

- title：surface/bilateral_upsampling column-stride-depth-direct board diagnostic
- evidence_role：diagnostic
- summary_path：test-rvv/surface/bilateral_upsampling/log/board/analyze_bench_compare.log
- comparisons：3
- result：Errors=0，Warnings=12，Suggestions=1

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. contract_mismatch — bilateral upsampling direct-depth 80x60 w3 dense

- observed_pattern：字段 'wrapper' 不一致：baseline='processScalar', candidate='processDirectDepthCandidate'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 2. contract_mismatch — bilateral upsampling direct-depth 80x60 w3 dense

- observed_pattern：字段 'timer_boundary' 不一致：baseline='window_scan_table_lookup_reduction_unprojection', candidate='window_scan_table_lookup_weight_staging_strided_depth_load_rvv_reduction_unprojection'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 3. contract_mismatch — bilateral upsampling direct-depth 80x60 w3 dense

- observed_pattern：字段 'mask' 不一致：baseline='finite_depth_skip', candidate='finite_depth_skip_with_vmfeq_merge'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 4. contract_mismatch — bilateral upsampling direct-depth 80x60 w3 dense

- observed_pattern：字段 'reduction' 不一致：baseline='scalar_ordered_float_sum', candidate='vfredusum_chunk_sum'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 5. contract_mismatch — bilateral upsampling direct-depth 120x90 w4 holes

- observed_pattern：字段 'wrapper' 不一致：baseline='processScalar', candidate='processDirectDepthCandidate'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 6. contract_mismatch — bilateral upsampling direct-depth 120x90 w4 holes

- observed_pattern：字段 'timer_boundary' 不一致：baseline='window_scan_table_lookup_reduction_unprojection', candidate='window_scan_table_lookup_weight_staging_strided_depth_load_rvv_reduction_unprojection'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 7. contract_mismatch — bilateral upsampling direct-depth 120x90 w4 holes

- observed_pattern：字段 'mask' 不一致：baseline='finite_depth_skip', candidate='finite_depth_skip_with_vmfeq_merge'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 8. contract_mismatch — bilateral upsampling direct-depth 120x90 w4 holes

- observed_pattern：字段 'reduction' 不一致：baseline='scalar_ordered_float_sum', candidate='vfredusum_chunk_sum'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 9. contract_mismatch — bilateral upsampling direct-depth 180x120 w5 dense

- observed_pattern：字段 'wrapper' 不一致：baseline='processScalar', candidate='processDirectDepthCandidate'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 10. contract_mismatch — bilateral upsampling direct-depth 180x120 w5 dense

- observed_pattern：字段 'timer_boundary' 不一致：baseline='window_scan_table_lookup_reduction_unprojection', candidate='window_scan_table_lookup_weight_staging_strided_depth_load_rvv_reduction_unprojection'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 11. contract_mismatch — bilateral upsampling direct-depth 180x120 w5 dense

- observed_pattern：字段 'mask' 不一致：baseline='finite_depth_skip', candidate='finite_depth_skip_with_vmfeq_merge'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 12. contract_mismatch — bilateral upsampling direct-depth 180x120 w5 dense

- observed_pattern：字段 'reduction' 不一致：baseline='scalar_ordered_float_sum', candidate='vfredusum_chunk_sum'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

### 1. near_threshold_ba — bilateral upsampling direct-depth 80x60 w3 dense

- observed_pattern：median=1.01x，距离 1.0 阈值不足 0.05。
- why_suspicious：接近阈值的收益容易被测量波动、输入分布或二进制差异反转。
- possible_non_bug_explanations：可能仍是可接受的弱收益，尤其当实现简单、fallback 清楚。
- possible_bug_or_evidence_issues：也可能真实收益不足，接入后维护成本高于收益。
- recommended_checks：扩大 runs，补静态实现质量分析；文档写清为什么弱收益仍可接受或为什么暂缓。
- conclusion_policy：不能只因 median 略大于 1 就写成稳定加速。
