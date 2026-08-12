# Evidence Doctor Report（证据体检报告）

- title：TEPTPLW source-indexed repeated board evidence manifest
- evidence_role：production_direct
- summary_path：test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_staged_gather/summary.md
- comparisons：6
- result：Errors=0，Warnings=7，Suggestions=6

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. asm_boundary_missing — weighted lls production-dispatch source-indices pointnormal 262144

- observed_pattern：strict/performance comparison 缺少 asm_boundary：baseline='std_non_rvv_path_not_attributed', candidate=None。
- why_suspicious：没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。
- possible_non_bug_explanations：可能当前阶段只做 board summary，asm 将在后续补齐。
- possible_bug_or_evidence_issues：也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。
- recommended_checks：补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。
- conclusion_policy：若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。

### 2. long_tail_or_variance — weighted lls production-dispatch source-indices pointnormal 262144

- observed_pattern：min=1.99x, median=2.33x, max=2.39x，max/min=1.20。
- why_suspicious：长尾说明单一均值不足以描述稳定性，异常值可能改变接入判断。
- possible_non_bug_explanations：可能是板卡温度、频率、调度、缓存状态或 run 顺序导致。
- possible_bug_or_evidence_issues：也可能是候选存在数据相关分支、fallback、spill/reload 或内存带宽拐点。
- recommended_checks：查看 per-iteration trace、温度 / governor / freq / taskset；必要时扩大到 20-run 或 50-run。
- conclusion_policy：可以继续分析，但 summary 必须保留 min/median/max 和长尾解释，不能先验剔除异常。

### 3. asm_boundary_missing — weighted lls production-dispatch source-indices pointnormal 65536

- observed_pattern：strict/performance comparison 缺少 asm_boundary：baseline='std_non_rvv_path_not_attributed', candidate=None。
- why_suspicious：没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。
- possible_non_bug_explanations：可能当前阶段只做 board summary，asm 将在后续补齐。
- possible_bug_or_evidence_issues：也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。
- recommended_checks：补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。
- conclusion_policy：若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。

### 4. asm_boundary_missing — weighted lls production-dispatch source-indices pointxyz-to-pointnormal 262144

- observed_pattern：strict/performance comparison 缺少 asm_boundary：baseline='std_non_rvv_path_not_attributed', candidate=None。
- why_suspicious：没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。
- possible_non_bug_explanations：可能当前阶段只做 board summary，asm 将在后续补齐。
- possible_bug_or_evidence_issues：也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。
- recommended_checks：补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。
- conclusion_policy：若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。

### 5. asm_boundary_missing — weighted lls production-dispatch source-indices pointxyz-to-pointnormal 65536

- observed_pattern：strict/performance comparison 缺少 asm_boundary：baseline='std_non_rvv_path_not_attributed', candidate=None。
- why_suspicious：没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。
- possible_non_bug_explanations：可能当前阶段只做 board summary，asm 将在后续补齐。
- possible_bug_or_evidence_issues：也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。
- recommended_checks：补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。
- conclusion_policy：若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。

### 6. asm_boundary_missing — weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 262144

- observed_pattern：strict/performance comparison 缺少 asm_boundary：baseline='std_non_rvv_path_not_attributed', candidate=None。
- why_suspicious：没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。
- possible_non_bug_explanations：可能当前阶段只做 board summary，asm 将在后续补齐。
- possible_bug_or_evidence_issues：也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。
- recommended_checks：补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。
- conclusion_policy：若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。

### 7. asm_boundary_missing — weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 65536

- observed_pattern：strict/performance comparison 缺少 asm_boundary：baseline='std_non_rvv_path_not_attributed', candidate=None。
- why_suspicious：没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。
- possible_non_bug_explanations：可能当前阶段只做 board summary，asm 将在后续补齐。
- possible_bug_or_evidence_issues：也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。
- recommended_checks：补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。
- conclusion_policy：若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

### 1. binary_identity_missing — weighted lls production-dispatch source-indices pointnormal 262144

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 2. binary_identity_missing — weighted lls production-dispatch source-indices pointnormal 65536

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 3. binary_identity_missing — weighted lls production-dispatch source-indices pointxyz-to-pointnormal 262144

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 4. binary_identity_missing — weighted lls production-dispatch source-indices pointxyz-to-pointnormal 65536

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 5. binary_identity_missing — weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 262144

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。

### 6. binary_identity_missing — weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 65536

- observed_pattern：缺少 binary_hash 或等价二进制身份字段。
- why_suspicious：没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。
- possible_non_bug_explanations：可能当前 summary 的命令已经足够复现。
- possible_bug_or_evidence_issues：也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。
- recommended_checks：在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。
- conclusion_policy：不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。
