# Evidence Doctor Report（证据体检报告）

- title：transformation_estimation_svd board repeated diagnostic
- evidence_role：diagnostic
- summary_path：log/board/source_indexed_cloud_pair_repeated/summary.md
- comparisons：9
- result：Errors=0，Warnings=4，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. production_name_without_role — public build sanity source-indexed-cloud-pair 4K

- observed_pattern：名称包含 production，但 evidence_role=diagnostic。
- why_suspicious：production-shaped、production direct 和 production-ready 是不同证据角色。名称含 production 但角色不清会扩大结论边界。
- possible_non_bug_explanations：可能只是 case 模拟生产输入形态，并未命中真实 production dispatch。
- possible_bug_or_evidence_issues：也可能是 production direct 证据缺失或 manifest 未记录。
- recommended_checks：明确写成 production-shaped diagnostic、production direct 或 mixed-boundary cross-check，并列出 public entry 是否真实命中。
- conclusion_policy：角色未澄清前，不能把该表单独作为 production evidence。

### 2. production_name_without_role — public build sanity source-indexed-cloud-pair 64K

- observed_pattern：名称包含 production，但 evidence_role=diagnostic。
- why_suspicious：production-shaped、production direct 和 production-ready 是不同证据角色。名称含 production 但角色不清会扩大结论边界。
- possible_non_bug_explanations：可能只是 case 模拟生产输入形态，并未命中真实 production dispatch。
- possible_bug_or_evidence_issues：也可能是 production direct 证据缺失或 manifest 未记录。
- recommended_checks：明确写成 production-shaped diagnostic、production direct 或 mixed-boundary cross-check，并列出 public entry 是否真实命中。
- conclusion_policy：角色未澄清前，不能把该表单独作为 production evidence。

### 3. production_name_without_role — public build sanity source-indexed-cloud-pair 256K

- observed_pattern：名称包含 production，但 evidence_role=diagnostic。
- why_suspicious：production-shaped、production direct 和 production-ready 是不同证据角色。名称含 production 但角色不清会扩大结论边界。
- possible_non_bug_explanations：可能只是 case 模拟生产输入形态，并未命中真实 production dispatch。
- possible_bug_or_evidence_issues：也可能是 production direct 证据缺失或 manifest 未记录。
- recommended_checks：明确写成 production-shaped diagnostic、production direct 或 mixed-boundary cross-check，并列出 public entry 是否真实命中。
- conclusion_policy：角色未澄清前，不能把该表单独作为 production evidence。

### 4. group_outlier — mixed-boundary public baseline vs fused RVV source-indexed-cloud-pair 4K

- observed_pattern：group=tesvd_public_vs_fused_rvv_source_indexed_cloud_pair 的组内 median=8.83x；当前 case median=7.01x，偏离 20.6%。
- why_suspicious：同组 point type / size / row source 中单个 case 明显偏离，说明结论可能不应按组整体外推。
- possible_non_bug_explanations：可能是点型 layout、AoS stride、字段 offset、缓存局部性或测量噪声不同。
- possible_bug_or_evidence_issues：也可能是该 case 的 gate、mask、fallback、row source 或 asm boundary 与其它 case 不同。
- recommended_checks：按 point type / size / row source 分开报告；检查 metadata、trace 和 asm attribution，必要时收窄 production gate。
- conclusion_policy：未解释前，不能把其它 case 的收益直接继承到该 case。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。

