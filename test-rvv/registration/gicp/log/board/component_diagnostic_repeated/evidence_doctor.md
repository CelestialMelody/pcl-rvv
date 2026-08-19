# Evidence Doctor Report（证据体检报告）

- title：Evidence Doctor Report
- evidence_role：diagnostic
- summary_path：not_recorded
- comparisons：3
- result：Errors=0，Warnings=5，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. contract_mismatch — covariance-post-knn-default-k

- observed_pattern：字段 'reduction' 不一致：baseline='scalar order', candidate='RVV chunk reduction'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 2. long_tail_or_variance — covariance-post-knn-default-k

- observed_pattern：min=1.03x, median=1.15x, max=1.26x，max/min=1.23。
- why_suspicious：长尾说明单一均值不足以描述稳定性，异常值可能改变接入判断。
- possible_non_bug_explanations：可能是板卡温度、频率、调度、缓存状态或 run 顺序导致。
- possible_bug_or_evidence_issues：也可能是候选存在数据相关分支、fallback、spill/reload 或内存带宽拐点。
- recommended_checks：查看 per-iteration trace、温度 / governor / freq / taskset；必要时扩大到 20-run 或 50-run。
- conclusion_policy：可以继续分析，但 summary 必须保留 min/median/max 和长尾解释，不能先验剔除异常。

### 3. contract_mismatch — residual-indexed-gather

- observed_pattern：字段 'reduction' 不一致：baseline='scalar order', candidate='RVV chunk reduction'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 4. long_tail_or_variance — residual-indexed-gather

- observed_pattern：min=1.14x, median=1.26x, max=1.32x，max/min=1.16。
- why_suspicious：长尾说明单一均值不足以描述稳定性，异常值可能改变接入判断。
- possible_non_bug_explanations：可能是板卡温度、频率、调度、缓存状态或 run 顺序导致。
- possible_bug_or_evidence_issues：也可能是候选存在数据相关分支、fallback、spill/reload 或内存带宽拐点。
- recommended_checks：查看 per-iteration trace、温度 / governor / freq / taskset；必要时扩大到 20-run 或 50-run。
- conclusion_policy：可以继续分析，但 summary 必须保留 min/median/max 和长尾解释，不能先验剔除异常。

### 5. contract_mismatch — residual-mahalanobis-dense

- observed_pattern：字段 'reduction' 不一致：baseline='scalar order', candidate='RVV chunk reduction'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。
