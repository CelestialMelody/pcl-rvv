# Optimization Roadmap Role Template

## Metadata

- `role`: optimization_roadmap
- `applies_when`: topic 进入多阶段优化、phase result 产生新想法、当前收益仍可继续推进、或 reviewer 需要知道下一轮搜索空间时。
- `default_path_source`: `artifact_layout.optimization_roadmap_template`。
- `may_omit_or_merge_when`: 只有单轮明确 closeout，且没有 unblocked candidate、阶段反思新增路线或恢复队列。
- `must_not_claim`: roadmap 不是 evidence result；不能把未验证 idea 写成已证实收益；不能替代 evaluation 的 production decision。

## Required Sections

1. 本文职责：跨 phase 搜索空间、恢复条件和优先级。
2. 当前恢复状态：本轮 EvidenceDecision、默认下一 phase、暂停条件。
3. Candidate family 表：idea、来源、预期收益、风险、所需证据、状态和恢复条件。
4. Idea source（想法来源）：源码 gap、bench 负向归因、asm/profile 线索、phase result 反思、reviewer/user 反馈或成熟结构校准。
5. 优先级和早停规则：先做哪些 low-risk / high-signal 验证，哪些必须等 production authorization。
6. 与 optimization matrix 的关系：roadmap 记录长期搜索空间，matrix 记录跨 phase 尝试和证据行。
7. 与 evaluation 的关系：roadmap 给下一步，evaluation 给当前决策。

## Trimming Rules

- 已被证据拒绝且无恢复条件的 idea 可只留一行索引。
- phase 局部实验细节放入 phase result；roadmap 只保留会影响下一轮选择的信息。
- 没有 production 授权时，production integration idea 只能写 prerequisites 和 gate。

## Closeout Checks

- 每个 unblocked idea 都有下一步验证动作和所需证据。
- 每个 deferred / rejected idea 都有原因，不用“以后再看”单独关闭。
- roadmap 与 optimization matrix、phase result、evaluation 的状态一致。
- 新出现的跨 topic 通用做法已进入 `.agents` feedback 或本 skill 更新候选。
