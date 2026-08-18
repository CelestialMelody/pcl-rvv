# Evidence Doctor Report（证据体检报告）

- title：transformation_estimation_dual_quaternion QEMU smoke diagnostic
- evidence_role：diagnostic
- summary_path：not_recorded
- comparisons：9
- result：Errors=0，Warnings=9，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. zero_warmup_iterations — public dual quaternion dual-indexed-cloud-pair 256K

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 2. zero_warmup_iterations — public dual quaternion dual-indexed-cloud-pair 4K

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 3. zero_warmup_iterations — public dual quaternion dual-indexed-cloud-pair 64K

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 4. zero_warmup_iterations — public dual quaternion ordered-cloud-pair 256K

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 5. zero_warmup_iterations — public dual quaternion ordered-cloud-pair 4K

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 6. zero_warmup_iterations — public dual quaternion ordered-cloud-pair 64K

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 7. zero_warmup_iterations — public dual quaternion source-indexed-cloud-pair 256K

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 8. zero_warmup_iterations — public dual quaternion source-indexed-cloud-pair 4K

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 9. zero_warmup_iterations — public dual quaternion source-indexed-cloud-pair 64K

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。
