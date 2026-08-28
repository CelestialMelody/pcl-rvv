# Evidence Doctor Report（证据体检报告）

- title：sac_model_cylinder Phase 000 count/select repeated board summary
- evidence_role：production_shaped_diagnostic
- summary_path：test-rvv/sample_consensus/sac_model_cylinder/doc/phases/000-cylinder-count-select-diagnostic/repeated-evidence-manifest.json
- comparisons：4
- result：Errors=0，Warnings=1，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. long_tail_or_variance — sac_model_cylinder diagnostic candidate countWithinDistance PointXYZ+Normal 65536

- observed_pattern：min=6.75x, median=7.22x, max=7.83x，max/min=1.16。
- why_suspicious：长尾说明单一均值不足以描述稳定性，异常值可能改变接入判断。
- possible_non_bug_explanations：可能是板卡温度、频率、调度、缓存状态或 run 顺序导致。
- possible_bug_or_evidence_issues：也可能是候选存在数据相关分支、fallback、spill/reload 或内存带宽拐点。
- recommended_checks：查看 per-iteration trace、温度 / governor / freq / taskset；必要时扩大到 20-run 或 50-run。
- conclusion_policy：可以继续分析，但 summary 必须保留 min/median/max 和长尾解释，不能先验剔除异常。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。
