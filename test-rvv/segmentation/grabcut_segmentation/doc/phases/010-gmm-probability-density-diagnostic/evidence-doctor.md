# Evidence Doctor Report（证据体检报告）

- title：GrabCut component diagnostic board manifest
- evidence_role：diagnostic
- summary_path：test-rvv/segmentation/grabcut_segmentation/doc/phases/010-gmm-probability-density-diagnostic/evidence-manifest.json
- comparisons：1
- result：Errors=0，Warnings=1，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. low_run_count — gmm_probability

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。
