# Topic Navigation Role Template

## Metadata

- `role`: topic_navigation
- `applies_when`: topic 有 README、当前状态入口、常用命令或提交边界需要给下一轮 worker / reviewer 快速恢复时。
- `default_path_source`: `artifact_layout.topic_navigation_doc_template`；当前 topic 已有稳定 README 路径时可保留，并写入 `doc_suite_role_inventory`。
- `may_omit_or_merge_when`: topic 还停在 S0/S1 草稿且没有可复用测试资产、bench、phase 或 evaluation；此时 Handoff 必须说明暂未形成 topic-local suite。
- `must_not_claim`: 不能把 diagnostic speedup 写成 production adoption；不能把不存在的细粒度 target、board repeated evidence 或 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档写成已存在。

## Required Sections

1. 当前结论：EvidenceDecision、production 是否适用、继续 / 停止状态。
2. 先读哪份文档：evaluation、testing overview、phase index、roadmap、optimization matrix 或 production doc 的阅读顺序。
3. 目录分工：每个 role 的主归属和相互引用关系，不写成长篇正文。
4. 常用命令：correctness、QEMU smoke、board smoke / repeated、Evidence Doctor、registry 或 clean target。
5. 当前可提交证据：summary、manifest、doctor、registry 或 sanitized log 的路径和 role。
6. 默认不提交的生成产物：raw log、build output、local config、私有地址和设备路径。
7. production_topic_doc 适用性：adopted production behavior 时引用 `artifact_layout.topic_doc_template`；diagnostic / no-production 时写 `not_applicable with evidence`。

## Trimming Rules

- 命令可以只列当前 topic 已实现的 target；不存在的类别写到 closeout 缺口，不要虚构。
- 细节解释迁到对应 role 文档，README 只保留入口、证据白名单和恢复路线。
- 如果 topic 很小，可以把目录分工写成短表，但仍要说明 evaluation 或 phase result 承担决策主归属。

## Closeout Checks

- README 引用的文档、target 和证据路径存在，或明确标为 local-only / excluded。
- 当前结论与 evaluation、phase result、roadmap 和 Evidence Doctor 不冲突。
- production_topic_doc 状态与 production adoption 状态一致。
- 未闭合项有下一步动作、恢复入口和 stop condition，不只列术语。
