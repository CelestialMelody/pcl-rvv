# Evidence Doctor Report（证据体检报告）

- title：transformation_estimation_svd production direct ordered-cloud-pair board repeated
- evidence_role：production_direct
- summary_path：log/board/production_ordered_cloud_pair_repeated/summary.md
- comparisons：3
- result：Errors=0，Warnings=1，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. group_outlier — production direct ordered-cloud-pair 4K

- observed_pattern：group=tesvd_production_ordered_cloud_pair 的组内 median=23.8x；当前 case median=14.4x，偏离 39.7%。
- why_suspicious：同组 point type / size / row source 中单个 case 明显偏离，说明结论可能不应按组整体外推。
- possible_non_bug_explanations：可能是点型 layout、AoS stride、字段 offset、缓存局部性或测量噪声不同。
- possible_bug_or_evidence_issues：也可能是该 case 的 gate、mask、fallback、row source 或 asm boundary 与其它 case 不同。
- recommended_checks：按 point type / size / row source 分开报告；检查 metadata、trace 和 asm attribution，必要时收窄 production gate。
- conclusion_policy：未解释前，不能把其它 case 的收益直接继承到该 case。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。

