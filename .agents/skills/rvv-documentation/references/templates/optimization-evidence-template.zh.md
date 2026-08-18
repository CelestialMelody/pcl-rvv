# Optimization Evidence Role Template

## Metadata

- `role`: optimization_evidence
- `applies_when`: topic 有多个 RVV candidate、attempted / rejected / deferred 优化方式、component ablation、RVV-vs-RVV A/B 或 production integration decision 需要索引时。
- `default_path_source`: `artifact_layout.topic_test_dir_template` 和 `artifact_layout.evaluation_doc_subdir` 解析范围；具体文件名由配置或当前 topic role/path index 决定。
- `may_omit_or_merge_when`: 只有一个候选、没有替代方案，且 evaluation 的实现方式审计已足够支撑 closeout。
- `must_not_claim`: 不把尝试过但未采用的 candidate 写成当前 production 行为；不把诊断相对收益写成 production direct speedup。

## Required Sections

1. 本文职责：候选证据索引和取舍定位，不承担完整 phase 流水。
2. 当前结论摘要：adopted、attempted、rejected、deferred、not_applicable 的总体状态。
3. 优化方式总表：candidate family、代码路径、测试路径、bench case、board evidence、asm evidence、decision 和边界。
4. 标量路径与 RVV 路径差异：load/gather、mask、staging、reduction、scalar tail、solver、store 或 fallback 的职责差异。
5. 代码级证据索引：production helper、test support helper、bench wrapper、analysis script 和 output summary。
6. 细粒度 target 字典：每个 target 如何隔离 candidate、row source、layout 或 component ablation。
7. 当前可提交证据：summary、doctor、registry、sanitized log 和不提交的 raw log。
8. 结论边界：哪些 candidate 可继续、哪些需要 production direct、哪些已被证据拒绝。

## Trimming Rules

- 只有一个 candidate 时可以把总表压缩，但仍要说明替代方案是否存在和为什么不展开。
- 负向尝试保留结论、证据和恢复条件即可，不复制完整实现历史。
- 如果 topic 仍是 diagnostic，不写 adopted production behavior，只写 production integration prerequisites。

## Closeout Checks

- 每个 candidate 的状态都有源码、测试、bench、asm、doctor 或 phase result 证据。
- optimization evidence 与 optimization roadmap 分工清楚：本 role 记录已发生证据，roadmap 记录搜索空间和恢复队列。
- adopted / rejected / deferred 的词义在 evaluation 和 phase result 中一致。
- 未闭合 candidate 有明确下一步和 stop condition。
