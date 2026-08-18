# Phase Suite Role Template

## Metadata

- `role`: phase_index, phase_plan, phase_result, optimization_matrix
- `applies_when`: topic 使用多阶段优化 loop、存在 early stop、Evidence Doctor 异常、unblocked next action、production integration loop 或跨 phase candidate 搜索时。
- `default_path_source`: `artifact_layout.phase_root_template`、`artifact_layout.phase_plan_template`、`artifact_layout.phase_result_template`、`artifact_layout.optimization_matrix_template`。
- `may_omit_or_merge_when`: 单轮无后续 action 的小 topic，可只在 evaluation 中保留 closeout；若已有 phase 目录，就应保持 index/result 可恢复。
- `must_not_claim`: phase result 不替代 evaluation 的最终 production decision；matrix 不把未验证 idea 写成收益；phase index 不复制每个 result 全文。

## Phase Index Required Sections

1. 当前恢复入口：默认下一 phase、暂停条件和需要先读的 result / roadmap / matrix。
2. 阶段表：phase id、slug、目标、状态、主要证据、下一步。
3. 文档归属：plan、result、matrix、roadmap、evaluation 和 README 的分工。
4. 当前早停规则：用户限定、dirty isolation、工具 / 板卡不可用、生产范围扩大、外部依赖或 evidence blocker。

## Phase Plan Required Sections

1. Phase 目标和授权边界。
2. 当前 evidence baseline：上一 result、matrix、roadmap 和 evaluation 引用。
3. 本轮假设：要验证的 candidate、风险和预期证据。
4. 执行步骤：源码、测试、bench、asm、doctor、registry 和文档更新。
5. 暂停条件和回滚条件。

## Phase Result Required Sections

1. 执行摘要：做了什么、没做什么、为什么。
2. 源码 / 测试 / bench / script 变化和 evidence path。
3. 结果：correctness、QEMU、asm、board、doctor、registry。
4. 解释：收益、负向结果、异常、诊断和 production 边界。
5. 文档同步：topic_navigation、evaluation、roadmap、matrix、production_topic_doc 适用性。
6. 下一步：unblocked action、phase_deferred、turn_stop_deferred 或 closeout。

## Optimization Matrix Required Sections

1. 矩阵职责：跨 phase 优化尝试、想法来源、证据状态和恢复条件。
2. Candidate / idea 行：来源、尝试方式、预期、证据、状态、下一步。
3. Evidence role：每条证据是 correctness、RVV-vs-RVV、production direct、asm、doctor 还是 diagnostic。
4. 新想法入口：phase result 反思、bench 归因、源码 gap、asm/profile 线索、reviewer/user 反馈。
5. Closeout state：采用、拒绝、暂缓、not_applicable 或需要 production authorization。

## Trimming Rules

- 单阶段 topic 可保留短 index 和 result；没有跨 phase 搜索时 matrix 可 `not_applicable with evidence`。
- 计划中的步骤没有执行时，result 必须说明原因和是否仍 unblocked。
- phase 文档保留探索和恢复事实，长期 production 文档只引用最终采用状态。

## Closeout Checks

- phase index 能指向默认恢复位置，不依赖聊天上下文。
- 每个 result 都更新或明确不需要更新 roadmap、matrix、evaluation 和 README。
- matrix 中每个 unblocked idea 都有来源和下一证据动作。
- 早停结论命中合法 stop condition；否则默认继续下一 phase。
