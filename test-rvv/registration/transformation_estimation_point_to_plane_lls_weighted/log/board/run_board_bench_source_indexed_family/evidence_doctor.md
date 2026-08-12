# Evidence Doctor Report（证据体检报告）

- title：TEPTPLW source-indexed implementation-family diagnostic manifest
- evidence_role：diagnostic
- summary_path：test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/run_board_bench_source_indexed_family/analyze_bench_compare.log
- comparisons：10
- result：Errors=2，Warnings=30，Suggestions=26

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

### 1. ba_degradation_frequency — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 65536

- observed_pattern：B/A values=0.822x；1/1 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

### 2. ba_degradation_frequency — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 262144

- observed_pattern：B/A values=0.67x；1/1 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. zero_warmup_iterations — weighted lls source-indexed-family staged-gather pointnormal 65536

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 2. low_run_count — weighted lls source-indexed-family staged-gather pointnormal 65536

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 3. zero_warmup_iterations — weighted lls source-indexed-family block-baseline pointnormal 65536

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 4. low_run_count — weighted lls source-indexed-family block-baseline pointnormal 65536

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 5. zero_warmup_iterations — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 65536

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 6. low_run_count — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 65536

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 7. zero_warmup_iterations — weighted lls component source-indexed-family block-baseline pointnormal no-solve 65536

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 8. low_run_count — weighted lls component source-indexed-family block-baseline pointnormal no-solve 65536

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 9. zero_warmup_iterations — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 65536

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 10. low_run_count — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 65536

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 11. zero_warmup_iterations — weighted lls source-indexed-family staged-gather pointnormal 262144

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 12. low_run_count — weighted lls source-indexed-family staged-gather pointnormal 262144

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 13. zero_warmup_iterations — weighted lls source-indexed-family block-baseline pointnormal 262144

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 14. low_run_count — weighted lls source-indexed-family block-baseline pointnormal 262144

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 15. zero_warmup_iterations — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 262144

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 16. low_run_count — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 262144

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 17. zero_warmup_iterations — weighted lls component source-indexed-family block-baseline pointnormal no-solve 262144

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 18. low_run_count — weighted lls component source-indexed-family block-baseline pointnormal no-solve 262144

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 19. zero_warmup_iterations — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 262144

- observed_pattern：manifest 记录 warmup_iterations=0。
- why_suspicious：带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。
- possible_non_bug_explanations：可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。
- possible_bug_or_evidence_issues：也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。
- recommended_checks：正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。
- conclusion_policy：该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。

### 20. low_run_count — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 262144

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 21. group_outlier — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 65536

- observed_pattern：group=source_indexed_family_block_fused_abcd_ilp 的组内 median=1.01x；当前 case median=0.822x，偏离 18.9%。
- why_suspicious：同组 point type / size / row source 中单个 case 明显偏离，说明结论可能不应按组整体外推。
- possible_non_bug_explanations：可能是点型 layout、AoS stride、字段 offset、缓存局部性或测量噪声不同。
- possible_bug_or_evidence_issues：也可能是该 case 的 gate、mask、fallback、row source 或 asm boundary 与其它 case 不同。
- recommended_checks：按 point type / size / row source 分开报告；检查 metadata、trace 和 asm attribution，必要时收窄 production gate。
- conclusion_policy：未解释前，不能把其它 case 的收益直接继承到该 case。

### 22. group_outlier — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 262144

- observed_pattern：group=source_indexed_family_block_fused_abcd_ilp 的组内 median=1.01x；当前 case median=0.67x，偏离 33.9%。
- why_suspicious：同组 point type / size / row source 中单个 case 明显偏离，说明结论可能不应按组整体外推。
- possible_non_bug_explanations：可能是点型 layout、AoS stride、字段 offset、缓存局部性或测量噪声不同。
- possible_bug_or_evidence_issues：也可能是该 case 的 gate、mask、fallback、row source 或 asm boundary 与其它 case 不同。
- recommended_checks：按 point type / size / row source 分开报告；检查 metadata、trace 和 asm attribution，必要时收窄 production gate。
- conclusion_policy：未解释前，不能把其它 case 的收益直接继承到该 case。

### 23. group_outlier — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 262144

- observed_pattern：group=source_indexed_family_block_fused_abcd_ilp 的组内 median=1.01x；当前 case median=1.4x，偏离 37.8%。
- why_suspicious：同组 point type / size / row source 中单个 case 明显偏离，说明结论可能不应按组整体外推。
- possible_non_bug_explanations：可能是点型 layout、AoS stride、字段 offset、缓存局部性或测量噪声不同。
- possible_bug_or_evidence_issues：也可能是该 case 的 gate、mask、fallback、row source 或 asm boundary 与其它 case 不同。
- recommended_checks：按 point type / size / row source 分开报告；检查 metadata、trace 和 asm attribution，必要时收窄 production gate。
- conclusion_policy：未解释前，不能把其它 case 的收益直接继承到该 case。

### 24. component_no_solve_slower_than_full_estimate — weighted lls component source-indexed-family block-baseline pointnormal no-solve 65536 [Std]

- observed_pattern：component no-solve 10.46 ms > full estimate 8.723 ms，ratio=1.2x。
- why_suspicious：component no-solve 理论上少了 solve / matrix 构造；明显更慢说明这个 pair 不能被直接解释为“只少做 solve”。
- possible_non_bug_explanations：可能是 cold cache、频率 / 中断、branch predictor、日志顺序、编译差异、checksum sink 或计时边界造成。
- possible_bug_or_evidence_issues：也可能是 component wrapper、accumulate helper、fallback gate、sink policy 或 manifest 解析有误。
- recommended_checks：用带 warm-up 的板卡 repeated run 复核；必要时增加 sink-aligned pair、per-iteration trace、asm attribution 或 profile。
- conclusion_policy：该 warning 未解释前，component/full 对照只能作为异常诊断，不能支撑 solve 成本或候选取舍结论。

### 25. component_no_solve_slower_than_full_estimate — weighted lls component source-indexed-family block-baseline pointnormal no-solve 65536 [RVV]

- observed_pattern：component no-solve 7.732 ms > full estimate 7.177 ms，ratio=1.08x。
- why_suspicious：component no-solve 理论上少了 solve / matrix 构造；明显更慢说明这个 pair 不能被直接解释为“只少做 solve”。
- possible_non_bug_explanations：可能是 cold cache、频率 / 中断、branch predictor、日志顺序、编译差异、checksum sink 或计时边界造成。
- possible_bug_or_evidence_issues：也可能是 component wrapper、accumulate helper、fallback gate、sink policy 或 manifest 解析有误。
- recommended_checks：用带 warm-up 的板卡 repeated run 复核；必要时增加 sink-aligned pair、per-iteration trace、asm attribution 或 profile。
- conclusion_policy：该 warning 未解释前，component/full 对照只能作为异常诊断，不能支撑 solve 成本或候选取舍结论。

### 26. component_no_solve_slower_than_full_estimate — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 65536 [Std]

- observed_pattern：component no-solve 10.5 ms > full estimate 8.697 ms，ratio=1.21x。
- why_suspicious：component no-solve 理论上少了 solve / matrix 构造；明显更慢说明这个 pair 不能被直接解释为“只少做 solve”。
- possible_non_bug_explanations：可能是 cold cache、频率 / 中断、branch predictor、日志顺序、编译差异、checksum sink 或计时边界造成。
- possible_bug_or_evidence_issues：也可能是 component wrapper、accumulate helper、fallback gate、sink policy 或 manifest 解析有误。
- recommended_checks：用带 warm-up 的板卡 repeated run 复核；必要时增加 sink-aligned pair、per-iteration trace、asm attribution 或 profile。
- conclusion_policy：该 warning 未解释前，component/full 对照只能作为异常诊断，不能支撑 solve 成本或候选取舍结论。

### 27. component_no_solve_slower_than_full_estimate — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 65536 [RVV]

- observed_pattern：component no-solve 12.78 ms > full estimate 7.213 ms，ratio=1.77x。
- why_suspicious：component no-solve 理论上少了 solve / matrix 构造；明显更慢说明这个 pair 不能被直接解释为“只少做 solve”。
- possible_non_bug_explanations：可能是 cold cache、频率 / 中断、branch predictor、日志顺序、编译差异、checksum sink 或计时边界造成。
- possible_bug_or_evidence_issues：也可能是 component wrapper、accumulate helper、fallback gate、sink policy 或 manifest 解析有误。
- recommended_checks：用带 warm-up 的板卡 repeated run 复核；必要时增加 sink-aligned pair、per-iteration trace、asm attribution 或 profile。
- conclusion_policy：该 warning 未解释前，component/full 对照只能作为异常诊断，不能支撑 solve 成本或候选取舍结论。

### 28. component_no_solve_slower_than_full_estimate — weighted lls component source-indexed-family block-baseline pointnormal no-solve 262144 [Std]

- observed_pattern：component no-solve 42.52 ms > full estimate 35.87 ms，ratio=1.19x。
- why_suspicious：component no-solve 理论上少了 solve / matrix 构造；明显更慢说明这个 pair 不能被直接解释为“只少做 solve”。
- possible_non_bug_explanations：可能是 cold cache、频率 / 中断、branch predictor、日志顺序、编译差异、checksum sink 或计时边界造成。
- possible_bug_or_evidence_issues：也可能是 component wrapper、accumulate helper、fallback gate、sink policy 或 manifest 解析有误。
- recommended_checks：用带 warm-up 的板卡 repeated run 复核；必要时增加 sink-aligned pair、per-iteration trace、asm attribution 或 profile。
- conclusion_policy：该 warning 未解释前，component/full 对照只能作为异常诊断，不能支撑 solve 成本或候选取舍结论。

### 29. component_no_solve_slower_than_full_estimate — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 262144 [Std]

- observed_pattern：component no-solve 42.89 ms > full estimate 35.83 ms，ratio=1.2x。
- why_suspicious：component no-solve 理论上少了 solve / matrix 构造；明显更慢说明这个 pair 不能被直接解释为“只少做 solve”。
- possible_non_bug_explanations：可能是 cold cache、频率 / 中断、branch predictor、日志顺序、编译差异、checksum sink 或计时边界造成。
- possible_bug_or_evidence_issues：也可能是 component wrapper、accumulate helper、fallback gate、sink policy 或 manifest 解析有误。
- recommended_checks：用带 warm-up 的板卡 repeated run 复核；必要时增加 sink-aligned pair、per-iteration trace、asm attribution 或 profile。
- conclusion_policy：该 warning 未解释前，component/full 对照只能作为异常诊断，不能支撑 solve 成本或候选取舍结论。

### 30. full_component_solve_delta_outlier — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 262144 [RVV]

- observed_pattern：full-component delta=22.78 ms，同组 median delta=12.76 ms，ratio=1.78x。
- why_suspicious：同一 row source / 点型 / size 下，某个 full estimate 比对应 component 多出的时间明显偏离其它候选，说明异常可能在 solve 之外。
- possible_non_bug_explanations：可能是测量波动、cache / 频率状态、case 顺序、matrix checksum、branch 或 wrapper 差异。
- possible_bug_or_evidence_issues：也可能是 full estimate helper、solve 输入、fallback gate、寄存器压力、spill/reload 或 asm hot path 归属存在真实问题。
- recommended_checks：对 `weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 262144` 和 `weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 262144` 做带 warm-up repeated board；补 per-iteration trace、asm attribution，并确认 full 和 component sink 口径。
- conclusion_policy：未解释前，不能把该 full estimate 退化直接归因到 fused formula 本身，也不能永久拒绝生产候选。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

### 1. environment_metadata_missing — weighted lls source-indexed-family staged-gather pointnormal 65536

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 2. binary_identity_missing — weighted lls source-indexed-family staged-gather pointnormal 65536

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 3. environment_metadata_missing — weighted lls source-indexed-family block-baseline pointnormal 65536

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 4. binary_identity_missing — weighted lls source-indexed-family block-baseline pointnormal 65536

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 5. environment_metadata_missing — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 65536

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 6. binary_identity_missing — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 65536

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 7. environment_metadata_missing — weighted lls component source-indexed-family block-baseline pointnormal no-solve 65536

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 8. binary_identity_missing — weighted lls component source-indexed-family block-baseline pointnormal no-solve 65536

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 9. environment_metadata_missing — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 65536

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 10. binary_identity_missing — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 65536

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 11. environment_metadata_missing — weighted lls source-indexed-family staged-gather pointnormal 262144

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 12. binary_identity_missing — weighted lls source-indexed-family staged-gather pointnormal 262144

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 13. near_threshold_ba — weighted lls source-indexed-family staged-gather pointnormal 262144

- observed_pattern：median=1.04x，距离 1.0 阈值不足 0.05。
- why_suspicious：接近阈值的收益容易被测量波动、输入分布或二进制差异反转。
- possible_non_bug_explanations：可能仍是可接受的弱收益，尤其当实现简单、fallback 清楚。
- possible_bug_or_evidence_issues：也可能真实收益不足，接入后维护成本高于收益。
- recommended_checks：扩大 runs，补静态实现质量分析；文档写清为什么弱收益仍可接受或为什么暂缓。
- conclusion_policy：不能只因 median 略大于 1 就写成稳定加速。

### 14. environment_metadata_missing — weighted lls source-indexed-family block-baseline pointnormal 262144

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 15. binary_identity_missing — weighted lls source-indexed-family block-baseline pointnormal 262144

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 16. near_threshold_ba — weighted lls source-indexed-family block-baseline pointnormal 262144

- observed_pattern：median=1x，距离 1.0 阈值不足 0.05。
- why_suspicious：接近阈值的收益容易被测量波动、输入分布或二进制差异反转。
- possible_non_bug_explanations：可能仍是可接受的弱收益，尤其当实现简单、fallback 清楚。
- possible_bug_or_evidence_issues：也可能真实收益不足，接入后维护成本高于收益。
- recommended_checks：扩大 runs，补静态实现质量分析；文档写清为什么弱收益仍可接受或为什么暂缓。
- conclusion_policy：不能只因 median 略大于 1 就写成稳定加速。

### 17. environment_metadata_missing — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 262144

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 18. binary_identity_missing — weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 262144

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 19. environment_metadata_missing — weighted lls component source-indexed-family block-baseline pointnormal no-solve 262144

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 20. binary_identity_missing — weighted lls component source-indexed-family block-baseline pointnormal no-solve 262144

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 21. environment_metadata_missing — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 262144

- observed_pattern：缺少环境字段：taskset, governor, freq, temperature
- why_suspicious：环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。
- possible_non_bug_explanations：可能当前板卡环境固定，worker 没有重复记录。
- possible_bug_or_evidence_issues：也可能温度、governor、频率或绑核变化影响了异常点。
- recommended_checks：下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。
- conclusion_policy：若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。

### 22. binary_identity_missing — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 262144

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 23. component_full_checksum_sink_mixed — weighted lls component source-indexed-family block-baseline pointnormal no-solve 65536 vs weighted lls source-indexed-family block-baseline pointnormal 65536

- observed_pattern：full checksum_policy='matrix_checksum_with_accepted_points'，component checksum_policy='normal_equation_checksum'。
- why_suspicious：full estimate 和 component no-solve 的输出对象不同；这不是 correctness 错误，但会削弱二者耗时差的归因。
- possible_non_bug_explanations：可能当前设计只是为了防止编译器消除两条路径，而不是为了严格测 solve overhead。
- possible_bug_or_evidence_issues：也可能文档把 component no-solve 误写成与 full estimate 只差 solve / matrix 构造。
- recommended_checks：后续若要比较二者，应让 full 路径也保留 normal-equation sink，或把该 pair 明确标为 mixed-sink component diagnostic。
- conclusion_policy：checksum sink 未对齐时，不要把 full-component 差值写成单独的 solve / matrix 构造成本。

### 24. component_full_checksum_sink_mixed — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 65536 vs weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 65536

- observed_pattern：full checksum_policy='matrix_checksum_with_accepted_points'，component checksum_policy='normal_equation_checksum'。
- why_suspicious：full estimate 和 component no-solve 的输出对象不同；这不是 correctness 错误，但会削弱二者耗时差的归因。
- possible_non_bug_explanations：可能当前设计只是为了防止编译器消除两条路径，而不是为了严格测 solve overhead。
- possible_bug_or_evidence_issues：也可能文档把 component no-solve 误写成与 full estimate 只差 solve / matrix 构造。
- recommended_checks：后续若要比较二者，应让 full 路径也保留 normal-equation sink，或把该 pair 明确标为 mixed-sink component diagnostic。
- conclusion_policy：checksum sink 未对齐时，不要把 full-component 差值写成单独的 solve / matrix 构造成本。

### 25. component_full_checksum_sink_mixed — weighted lls component source-indexed-family block-baseline pointnormal no-solve 262144 vs weighted lls source-indexed-family block-baseline pointnormal 262144

- observed_pattern：full checksum_policy='matrix_checksum_with_accepted_points'，component checksum_policy='normal_equation_checksum'。
- why_suspicious：full estimate 和 component no-solve 的输出对象不同；这不是 correctness 错误，但会削弱二者耗时差的归因。
- possible_non_bug_explanations：可能当前设计只是为了防止编译器消除两条路径，而不是为了严格测 solve overhead。
- possible_bug_or_evidence_issues：也可能文档把 component no-solve 误写成与 full estimate 只差 solve / matrix 构造。
- recommended_checks：后续若要比较二者，应让 full 路径也保留 normal-equation sink，或把该 pair 明确标为 mixed-sink component diagnostic。
- conclusion_policy：checksum sink 未对齐时，不要把 full-component 差值写成单独的 solve / matrix 构造成本。

### 26. component_full_checksum_sink_mixed — weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 262144 vs weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 262144

- observed_pattern：full checksum_policy='matrix_checksum_with_accepted_points'，component checksum_policy='normal_equation_checksum'。
- why_suspicious：full estimate 和 component no-solve 的输出对象不同；这不是 correctness 错误，但会削弱二者耗时差的归因。
- possible_non_bug_explanations：可能当前设计只是为了防止编译器消除两条路径，而不是为了严格测 solve overhead。
- possible_bug_or_evidence_issues：也可能文档把 component no-solve 误写成与 full estimate 只差 solve / matrix 构造。
- recommended_checks：后续若要比较二者，应让 full 路径也保留 normal-equation sink，或把该 pair 明确标为 mixed-sink component diagnostic。
- conclusion_policy：checksum sink 未对齐时，不要把 full-component 差值写成单独的 solve / matrix 构造成本。
