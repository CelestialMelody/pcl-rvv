# Evidence Doctor Report（证据体检报告）

- title：organized_fast_mesh
- evidence_role：production_direct
- summary_path：test-rvv/surface/organized_fast_mesh/log/board/analyze_bench_compare.log
- comparisons：4
- result：Errors=3，Warnings=8，Suggestions=5

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

### 1. ba_degradation_frequency — ofm_public_adaptive_cut

- observed_pattern：B/A values=0.887x；1/1 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

### 2. ba_degradation_frequency — ofm_public_left_cut

- observed_pattern：B/A values=0.995x；1/1 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

### 3. ba_degradation_frequency — ofm_public_right_cut

- observed_pattern：B/A values=0.988x；1/1 低于 1。
- why_suspicious：平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。
- possible_non_bug_explanations：可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。
- possible_bug_or_evidence_issues：也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。
- recommended_checks：报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。
- conclusion_policy：退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. asm_boundary_missing — ofm_public_adaptive_cut

- observed_pattern：strict/performance comparison 缺少 asm_boundary：baseline=None, candidate=None。
- why_suspicious：没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。
- possible_non_bug_explanations：可能当前阶段只做 board summary，asm 将在后续补齐。
- possible_bug_or_evidence_issues：也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。
- recommended_checks：补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。
- conclusion_policy：若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。

### 2. low_run_count — ofm_public_adaptive_cut

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 3. asm_boundary_missing — ofm_public_left_cut

- observed_pattern：strict/performance comparison 缺少 asm_boundary：baseline=None, candidate=None。
- why_suspicious：没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。
- possible_non_bug_explanations：可能当前阶段只做 board summary，asm 将在后续补齐。
- possible_bug_or_evidence_issues：也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。
- recommended_checks：补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。
- conclusion_policy：若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。

### 4. low_run_count — ofm_public_left_cut

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 5. asm_boundary_missing — ofm_public_quad

- observed_pattern：strict/performance comparison 缺少 asm_boundary：baseline=None, candidate=None。
- why_suspicious：没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。
- possible_non_bug_explanations：可能当前阶段只做 board summary，asm 将在后续补齐。
- possible_bug_or_evidence_issues：也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。
- recommended_checks：补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。
- conclusion_policy：若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。

### 6. low_run_count — ofm_public_quad

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

### 7. asm_boundary_missing — ofm_public_right_cut

- observed_pattern：strict/performance comparison 缺少 asm_boundary：baseline=None, candidate=None。
- why_suspicious：没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。
- possible_non_bug_explanations：可能当前阶段只做 board summary，asm 将在后续补齐。
- possible_bug_or_evidence_issues：也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。
- recommended_checks：补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。
- conclusion_policy：若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。

### 8. low_run_count — ofm_public_right_cut

- observed_pattern：只解析到 1 个 repeated value。
- why_suspicious：run 数过低时，无法判断异常频率和稳定性。
- possible_non_bug_explanations：可能当前只是 quick board check。
- possible_bug_or_evidence_issues：也可能 raw logs 缺失或 summary parser 只读到部分行。
- recommended_checks：正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。
- conclusion_policy：低 run count 只能支撑初筛，不能支撑强 production performance 结论。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

### 1. binary_identity_missing — ofm_public_adaptive_cut

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 2. binary_identity_missing — ofm_public_left_cut

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 3. binary_identity_missing — ofm_public_quad

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 4. near_threshold_ba — ofm_public_quad

- observed_pattern：median=1x，距离 1.0 阈值不足 0.05。
- why_suspicious：接近阈值的收益容易被测量波动、输入分布或二进制差异反转。
- possible_non_bug_explanations：可能仍是可接受的弱收益，尤其当实现简单、fallback 清楚。
- possible_bug_or_evidence_issues：也可能真实收益不足，接入后维护成本高于收益。
- recommended_checks：扩大 runs，补静态实现质量分析；文档写清为什么弱收益仍可接受或为什么暂缓。
- conclusion_policy：不能只因 median 略大于 1 就写成稳定加速。

### 5. binary_identity_missing — ofm_public_right_cut

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。
