# Evidence Doctor Report（证据体检报告）

- title：Evidence Doctor Report
- evidence_role：production_public
- summary_path：not_recorded
- comparisons：1
- result：Errors=0，Warnings=2，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. contract_mismatch — production-public-align-pointxyz

- observed_pattern：字段 'reduction' 不一致：baseline='scalar order', candidate='RVV chunk reduction'。
- why_suspicious：A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。
- possible_non_bug_explanations：可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。
- possible_bug_or_evidence_issues：也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。
- recommended_checks：若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。
- conclusion_policy：strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。

### 2. low_run_count — production-public-align-pointxyz

- observed_pattern：只解析到 3 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。
