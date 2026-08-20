# Evidence Doctor Report（证据体检报告）

- title：marching_cubes_rbf board diagnostic and production direct compare
- evidence_role：mixed_component_and_production_direct
- summary_path：log/board/analyze_bench_compare.log
- comparisons：12
- result：Errors=0，Warnings=12，Suggestions=0

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. low_run_count — mcrbf_full_pipeline_n24_r18

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 2. low_run_count — mcrbf_full_pipeline_n40_r20

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 3. low_run_count — mcrbf_full_pipeline_n56_r18

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 4. low_run_count — mcrbf_matrix_fill_n24

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 5. low_run_count — mcrbf_matrix_fill_n40

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 6. low_run_count — mcrbf_prod_pointnormal_n24_r18

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 7. low_run_count — mcrbf_prod_pointnormal_n40_r20

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 8. low_run_count — mcrbf_prod_pointnormal_n56_r18

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 9. low_run_count — mcrbf_prod_pointxyzinormal_n24_r18

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 10. low_run_count — mcrbf_prod_pointxyzinormal_n40_r20

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 11. low_run_count — mcrbf_prod_pointxyzrgbnormal_n24_r18

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 12. low_run_count — mcrbf_prod_pointxyzrgbnormal_n40_r20

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

无。
