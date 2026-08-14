# Evidence Doctor Report（证据体检报告）

- title：transformation_estimation_svd board repeated diagnostic
- evidence_role：diagnostic
- summary_path：log/board/correspondence_pair_repeated/summary.md
- comparisons：9
- result：Errors=0，Warnings=7，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. long_tail_or_variance — fused same-boundary correspondence-pair 4K

- observed_pattern：min=2.01x, median=2.17x, max=2.39x，max/min=1.19。
- why_suspicious：长尾说明单一均值不足以描述稳定性，异常值可能改变接入判断。
- possible_non_bug_explanations：可能是板卡温度、频率、调度、缓存状态或 run 顺序导致。
- possible_bug_or_evidence_issues：也可能是候选存在数据相关分支、fallback、spill/reload 或内存带宽拐点。
- recommended_checks：查看 per-iteration trace、温度 / governor / freq / taskset；必要时扩大到 20-run 或 50-run。
- conclusion_policy：可以继续分析，但 summary 必须保留 min/median/max 和长尾解释，不能先验剔除异常。

### 2. long_tail_or_variance — mixed-boundary public baseline vs fused RVV correspondence-pair 4K

- observed_pattern：min=7.58x, median=8.66x, max=9.21x，max/min=1.21。
- why_suspicious：长尾说明单一均值不足以描述稳定性，异常值可能改变接入判断。
- possible_non_bug_explanations：可能是板卡温度、频率、调度、缓存状态或 run 顺序导致。
- possible_bug_or_evidence_issues：也可能是候选存在数据相关分支、fallback、spill/reload 或内存带宽拐点。
- recommended_checks：查看 per-iteration trace、温度 / governor / freq / taskset；必要时扩大到 20-run 或 50-run。
- conclusion_policy：可以继续分析，但 summary 必须保留 min/median/max 和长尾解释，不能先验剔除异常。

### 3. production_name_without_role — public build sanity correspondence-pair 4K

- observed_pattern：名称包含 production，但 evidence_role=diagnostic。
- why_suspicious：production-shaped、production direct 和 production-ready 是不同证据角色。名称含 production 但角色不清会扩大结论边界。
- possible_non_bug_explanations：可能只是 case 模拟生产输入形态，并未命中真实 production dispatch。
- possible_bug_or_evidence_issues：也可能是 production direct 证据缺失或 manifest 未记录。
- recommended_checks：明确写成 production-shaped diagnostic、production direct 或 mixed-boundary cross-check，并列出 public entry 是否真实命中。
- conclusion_policy：角色未澄清前，不能把该表单独作为 production evidence。

### 4. long_tail_or_variance — fused same-boundary correspondence-pair 64K

- observed_pattern：min=1.52x, median=1.9x, max=2.14x，max/min=1.41。
- why_suspicious：长尾说明单一均值不足以描述稳定性，异常值可能改变接入判断。
- possible_non_bug_explanations：可能是板卡温度、频率、调度、缓存状态或 run 顺序导致。
- possible_bug_or_evidence_issues：也可能是候选存在数据相关分支、fallback、spill/reload 或内存带宽拐点。
- recommended_checks：查看 per-iteration trace、温度 / governor / freq / taskset；必要时扩大到 20-run 或 50-run。
- conclusion_policy：可以继续分析，但 summary 必须保留 min/median/max 和长尾解释，不能先验剔除异常。

### 5. long_tail_or_variance — mixed-boundary public baseline vs fused RVV correspondence-pair 64K

- observed_pattern：min=6.85x, median=8.18x, max=9.23x，max/min=1.35。
- why_suspicious：长尾说明单一均值不足以描述稳定性，异常值可能改变接入判断。
- possible_non_bug_explanations：可能是板卡温度、频率、调度、缓存状态或 run 顺序导致。
- possible_bug_or_evidence_issues：也可能是候选存在数据相关分支、fallback、spill/reload 或内存带宽拐点。
- recommended_checks：查看 per-iteration trace、温度 / governor / freq / taskset；必要时扩大到 20-run 或 50-run。
- conclusion_policy：可以继续分析，但 summary 必须保留 min/median/max 和长尾解释，不能先验剔除异常。

### 6. production_name_without_role — public build sanity correspondence-pair 64K

- observed_pattern：名称包含 production，但 evidence_role=diagnostic。
- why_suspicious：production-shaped、production direct 和 production-ready 是不同证据角色。名称含 production 但角色不清会扩大结论边界。
- possible_non_bug_explanations：可能只是 case 模拟生产输入形态，并未命中真实 production dispatch。
- possible_bug_or_evidence_issues：也可能是 production direct 证据缺失或 manifest 未记录。
- recommended_checks：明确写成 production-shaped diagnostic、production direct 或 mixed-boundary cross-check，并列出 public entry 是否真实命中。
- conclusion_policy：角色未澄清前，不能把该表单独作为 production evidence。

### 7. production_name_without_role — public build sanity correspondence-pair 256K

- observed_pattern：名称包含 production，但 evidence_role=diagnostic。
- why_suspicious：production-shaped、production direct 和 production-ready 是不同证据角色。名称含 production 但角色不清会扩大结论边界。
- possible_non_bug_explanations：可能只是 case 模拟生产输入形态，并未命中真实 production dispatch。
- possible_bug_or_evidence_issues：也可能是 production direct 证据缺失或 manifest 未记录。
- recommended_checks：明确写成 production-shaped diagnostic、production direct 或 mixed-boundary cross-check，并列出 public entry 是否真实命中。
- conclusion_policy：角色未澄清前，不能把该表单独作为 production evidence。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。

