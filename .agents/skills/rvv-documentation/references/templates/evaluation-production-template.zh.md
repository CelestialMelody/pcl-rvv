# Production Evaluation Role Template

## Metadata

- `role`: evaluation_production
- `applies_when`: 用户授权 production integration loop，或 topic 已有 production patch、adopted production behavior、production direct evidence 或 PI5 closeout。
- `default_path_source`: `artifact_layout.evaluation_doc_template`。
- `may_omit_or_merge_when`: topic 未进入 production integration；此时使用 diagnostic evaluation template。
- `must_not_claim`: 不沿用诊断阶段 speedup 作为最终 production decision；不把未覆盖入口、点类型、Scalar 或 fallback 写成已接入。

## Required Sections

1. 范围和目标源码：production patch scope、public entry、helper、dispatch、`__RVV10__` gate 和 fallback。
2. 函数级结论：最终 EvidenceDecision、adopted / rollback / no-production 状态和理由。
3. Production patch scope：真实改动的 production 文件、helper、dispatch 和 build guard。
4. Covered path：入口、点类型、Scalar、数据布局、规模 gate、目标硬件和默认启用条件。
5. Fallback matrix：非 RVV 构建、非覆盖点类型、`Scalar=double`、indices、correspondences、小规模和布局不满足时的回退证据。
6. Production direct tests：真实公开入口、fallback、upstream test、QEMU 和日志路径。
7. Production asm：关键 RVV 指令、符号或内联范围归属。
8. Production board bench：板卡 repeated evidence、summary、manifest、doctor 和 decision bucket。
9. Decision delta：诊断结论如何被生产证据确认、缩窄、推翻或回退。
10. 与 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档同步：长期文档路径、当前采用方式、证据链和未覆盖边界。
11. 遗留风险：point type expansion、fallback 扩展、性能异常、维护成本和恢复条件。

## Trimming Rules

- 没有 PI5 证据时不能用本模板 closeout production adoption；转为 diagnostic / partial-production-candidate。
- 如果 production patch 被撤回，保留 rollback/no-production 证据，长期 production doc 只在用户要求历史归档时保留。
- 如果只有窄范围生产候选，必须把 covered path 写窄，并列出未覆盖入口。

## Closeout Checks

- 最终 EvidenceDecision 基于 production direct 证据，而不是 diagnostic prototype。
- production 长期主题文档与 evaluation 同步，且没有复制 phase 流水。
- fallback matrix、production direct tests、asm attribution、board repeated evidence 和 Evidence Doctor 都有路径。
- screening / queue 状态、README、Handoff 和 topic-local docs 的 production 状态一致。
