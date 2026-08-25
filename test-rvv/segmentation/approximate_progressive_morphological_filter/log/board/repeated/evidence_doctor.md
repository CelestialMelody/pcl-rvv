# Evidence Doctor Report（证据体检报告）

- title：APMF component repeated board diagnostic
- evidence_role：diagnostic
- summary_path：log/board/repeated/summary.md
- comparisons：4
- result：Errors=0，Warnings=1，Suggestions=1

本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。

## Errors（必须修正，否则不能作为 production evidence 或严格性能结论）

无。

## Warnings（可以继续，但结论必须说明风险和处理方式）

### 1. group_outlier — apmf window open component

- observed_pattern：group=apmf_component_repeated_board 的组内 median=2.29x；当前 case median=1.02x，偏离 55.6%。
- why_suspicious：同组 point type / size / row source 中单个 case 明显偏离，说明结论可能不应按组整体外推。
- possible_non_bug_explanations：可能是点型 layout、AoS stride、字段 offset、缓存局部性或测量噪声不同。
- possible_bug_or_evidence_issues：也可能是该 case 的 gate、mask、fallback、row source 或 asm boundary 与其它 case 不同。
- recommended_checks：按 point type / size / row source 分开报告；检查 metadata、trace 和 asm attribution，必要时收窄 production gate。
- conclusion_policy：未解释前，不能把其它 case 的收益直接继承到该 case。

## Suggestions（不阻塞当前结论，但给出下一步可验证动作）

### 1. near_threshold_ba — apmf window open component

- observed_pattern：median=1.02x，距离 1.0 阈值不足 0.05。
- why_suspicious：接近阈值的收益容易被测量波动、输入分布或二进制差异反转。
- possible_non_bug_explanations：可能仍是可接受的弱收益，尤其当实现简单、fallback 清楚。
- possible_bug_or_evidence_issues：也可能真实收益不足，接入后维护成本高于收益。
- recommended_checks：扩大 runs，补静态实现质量分析；文档写清为什么弱收益仍可接受或为什么暂缓。
- conclusion_policy：不能只因 median 略大于 1 就写成稳定加速。
