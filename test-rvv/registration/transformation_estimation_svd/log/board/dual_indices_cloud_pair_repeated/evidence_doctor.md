# Evidence Doctor Report（证据体检报告）

- title：transformation_estimation_svd board repeated diagnostic
- evidence_role：diagnostic
- summary_path：log/board/dual_indices_cloud_pair_repeated/summary.md
- comparisons：9
- result：Errors=0，Warnings=3，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. production_name_without_role — public build sanity dual-indices-cloud-pair 4K

- observed_pattern：名称包含 production，但 evidence_role=diagnostic。
- why_suspicious：production-shaped、production direct 和 production-ready 是不同证据角色。名称含 production 但角色不清会扩大结论边界。
- possible_non_bug_explanations：可能只是 case 模拟生产输入形态，并未命中真实 production dispatch。
- possible_bug_or_evidence_issues：也可能是 production direct 证据缺失或 manifest 未记录。
- recommended_checks：明确写成 production-shaped diagnostic、production direct 或 mixed-boundary cross-check，并列出 public entry 是否真实命中。
- conclusion_policy：角色未澄清前，不能把该表单独作为 production evidence。

### 2. production_name_without_role — public build sanity dual-indices-cloud-pair 64K

- observed_pattern：名称包含 production，但 evidence_role=diagnostic。
- why_suspicious：production-shaped、production direct 和 production-ready 是不同证据角色。名称含 production 但角色不清会扩大结论边界。
- possible_non_bug_explanations：可能只是 case 模拟生产输入形态，并未命中真实 production dispatch。
- possible_bug_or_evidence_issues：也可能是 production direct 证据缺失或 manifest 未记录。
- recommended_checks：明确写成 production-shaped diagnostic、production direct 或 mixed-boundary cross-check，并列出 public entry 是否真实命中。
- conclusion_policy：角色未澄清前，不能把该表单独作为 production evidence。

### 3. production_name_without_role — public build sanity dual-indices-cloud-pair 256K

- observed_pattern：名称包含 production，但 evidence_role=diagnostic。
- why_suspicious：production-shaped、production direct 和 production-ready 是不同证据角色。名称含 production 但角色不清会扩大结论边界。
- possible_non_bug_explanations：可能只是 case 模拟生产输入形态，并未命中真实 production dispatch。
- possible_bug_or_evidence_issues：也可能是 production direct 证据缺失或 manifest 未记录。
- recommended_checks：明确写成 production-shaped diagnostic、production direct 或 mixed-boundary cross-check，并列出 public entry 是否真实命中。
- conclusion_policy：角色未澄清前，不能把该表单独作为 production evidence。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。

