# Evidence Doctor Report（证据体检报告）

- title：summary-md:log/qemu/run_bench_all_rvv.log
- evidence_role：summary_only_unknown
- summary_path：log/qemu/run_bench_all_rvv.log
- comparisons：0
- result：Errors=1，Warnings=0，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

### 1. missing_comparisons — <manifest>

- observed_pattern：manifest 没有非空 comparisons 列表。
- why_suspicious：没有 comparison 时，脚本无法检查 A/B 边界、数值分布或异常频率。
- possible_non_bug_explanations：可能是当前工作只生成了 raw log，还没有生成 summary manifest。
- possible_bug_or_evidence_issues：也可能是 analysis script 输出路径错误或旧日志未被解析。
- recommended_checks：先生成 summary manifest，至少列出 case name、证据角色、run count、baseline/candidate 或 ba_values。
- conclusion_policy：不能把空 doctor report 当作证据通过。

## Warnings（可以继续，但结论必须说明风险和处理方式）

无。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。
