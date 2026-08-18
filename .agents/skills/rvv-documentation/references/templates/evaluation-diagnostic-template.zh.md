# Diagnostic Evaluation Role Template

## Metadata

- `role`: evaluation_diagnostic
- `applies_when`: topic 仍是 diagnostic、bench-only、production-shaped diagnostic、partial-production-candidate 或 no-production closeout，且没有 adopted production behavior。
- `default_path_source`: `artifact_layout.evaluation_doc_template`。
- `may_omit_or_merge_when`: 函数级评估尚未启动；S2 之后默认不省略。
- `must_not_claim`: 不创建或要求 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档承载 diagnostic 结论；不把局部 helper speedup 写成 production adoption；不把 QEMU timing 写成性能收益。

## Required Sections

1. 范围和目标源码：public entry、生产源码、test support、bench 和脚本边界。
2. 函数 / 函数族作用速览：关键函数、输入输出状态、与主流程关系和 RVV 判断。
3. 函数级结论：EvidenceDecision、production 是否适用、继续优化或停止条件。
4. 标量流程与诊断 RVV 流程对照：源码真实数据流、诊断拆分的数据流和计时边界。
5. Traceability Map：production、diagnostic helper、bench wrapper、script、output、文档 role 的定位。
6. 实现方式审计：candidate、状态、证据、边界 / 恢复条件。
7. 测试计划和 bench 计划：correctness、QEMU、board、doctor、registry 和 asm。
8. 当前证据：本轮、历史基线、rerun、doctor warning / error 和 evidence path。
9. 诊断证据链：local fragment、full diagnostic、RVV-vs-RVV、negative evidence 和不能覆盖的 production 边界。
10. 生产接入判断：production integration prerequisites、缺失证据、第一步验证和为什么本轮不直接接入。
11. 遗留风险和下一步：unblocked optimization、phase_deferred、stop condition 或 closeout。

## Trimming Rules

- 若 topic 足够小，函数族速览可缩短，但仍要说明 public entry 和目标 helper 的关系。
- 没有 production patch 时，把 production doc 写为 `not_applicable with evidence`，不要省略这条判断。
- 若只有负向诊断结果，保留失败证据、归因假设和是否值得继续的条件。

## Closeout Checks

- evaluation 是 diagnostic decision 的主归属，README、roadmap、phase result 只引用它。
- 每个收益或负向结论都有 correctness、board、asm、doctor 或 phase result 证据角色。
- partial-production-candidate 明确区分“有收益诊断路径”和“可进入 production integration loop 的范围”。
- production_topic_doc 状态、screening 状态和 Handoff 下一步一致。
