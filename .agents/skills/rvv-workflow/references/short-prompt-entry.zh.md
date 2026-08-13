# 短 Prompt 入口

本文定义 PCL RVV 工作的短 prompt（提示词）入口。目标是让用户只给 role（角色）、
工作目录和一个粗目标，agent 也能自动加载默认规则、权限和输出合同。

## 适用条件

当用户请求包含以下信息时，按本文启动：

- PCL RVV 工作。
- worker（执行者）、reviewer（审查者）或 workflow improvement（工作流改进）角色。
- 工作目录，或当前对话已经位于 PCL 仓库。
- 至少一个粗目标，例如“处理下一个 topic”“审查上一轮 worker 结果”“复核 workflow asset（工作流资产）”。

如果用户只说“处理下一个 topic”，worker 先根据当前对话、模块工作日志和
second-pass（第二轮筛选）或 follow-up（复筛）状态表自动选择模块；若模块仍然
不唯一，才先询问模块名。模块一旦确定，worker 默认选择该模块下第一条未完成 topic（主题）。

如果短 prompt 只是“继续完善 <topic> 的 RVV 优化工作”或类似粗目标，worker 默认先恢复 phase loop：
读取最近 Handoff Packet、最近 phase plan/result、`doc/phases/README.zh.md`、topic-level optimization roadmap
和 optimization matrix。若当前没有 phase plan，先创建 current-state phase plan；若没有 roadmap，先按
`artifact_layout.optimization_roadmap_template` 创建 `doc/optimization-roadmap.zh.md` 或配置解析出的等价路径。
随后按当前 phase 继续推进实现、测试、证据解释、阶段反思和计划更新。
`phase_deferred` 只表示当前 phase 暂缓，不表示本轮可以停止；只要 roadmap 或矩阵里仍有当前 topic 授权范围内、
未阻塞且风险可控的下一动作，worker 默认继续创建或修订下一 phase 并推进。
恢复扫描还必须读取 roadmap 中的“默认恢复动作”、`next_phase_default`、`next action`、
`resume condition` 或等价小节。这些不是面向人工的松散建议，而是下一阶段恢复队列的输入。
worker 应把它们归一成 `roadmap_default_recovery_queue`：每项写明 phase 名、范围、是否仍在当前 topic
授权内、是否有 blocker、是否可与其它结构动作合并执行。若队列中存在未阻塞的测试资产、topic-local
文档、evidence registry、legacy 清理或结构成熟度动作，`ready_for_review_validity_checked` 只能表示旧停止位已被检查，
不能作为终点；worker 必须继续到队列中的第一个未阻塞 phase。
如果最近 phase README、result、Handoff 或 worker 输出写着 `ready_for_review`，worker 仍必须重新验证该停止决定：
只要 roadmap、optimization matrix、mature sibling parity audit 或当前 shape scan 暴露未阻塞的结构 / 文档 /
legacy / 测试优化动作，就把旧 `ready_for_review` 标成 stale stop decision，并恢复到第一个未阻塞 phase。
如果选中的 topic 属于 registration（配准）类，row-source family carry-over audit 只是常见的第一阶段，不是整轮默认终点；
它应作为 phase loop 中的一个候选 phase，被放进矩阵后继续判断下一个未阻塞 phase。
不要把一个 policy 的 positive summary 直接外推成其它 policy 的 production 结论。
恢复阶段时还必须做 topic maturity audit：除 production hot path 和数值 / 性能证据外，
同时检查 RVV test support architecture、测试与 bench 文件职责、reference / diagnostic /
production-direct 分层、文档与 evidence registry 一致性以及 closeout hygiene。发现这些结构性
问题影响审计清晰度时，应把它们作为当前 phase 的未完成项纳入计划；用户不需要额外写“重构测试框架”。
其中 test support architecture（测试支撑架构）必须包含布局迁移审计：当前 topic 是否仍使用
根目录超长测试 / bench 源码文件、是否缺少配置解析出的 source subdir（源码子目录）、
aggregator header（聚合头文件）入口和 internal header（内部头文件）职责拆分、是否需要按
`test_support.topic_abbrev_policy` 为长 topic 采用缩写 topic token（主题短标识）、以及是否应把
reference、fixtures、row source、candidate、assertion 和 bench harness 分职责拆分。审计对象从当前 topic
实际文件形态推导：可能是根目录长 `.cpp`、单个大 header、旧 `test_support/` 目录、已有 `include/` /
`include/impl/`、script、bench case registry 或其它等价测试支撑文件；不要假设字面量 `test_support/` 一定存在，也不要因为
没有该目录就跳过结构迁移。具体目录和文件名按
`artifact_layout`、`test_support` 和当前 topic 既有等价结构解析。历史 sibling topic（同类主题）只能作为结构质量 bar
和风险来源，不能机械复制其实现；但 worker 必须给出 `adopt / defer / reject` 决策。若不重构，
phase plan / result / Handoff 必须说明暂缓原因、对 reviewer 可读性和后续候选扩展的影响，
并把仍未阻塞的重构列入 `unblocked_next_actions`，不能因为局部代码清理已通过测试就提前 closeout。
如果配置解析出的内部目录是 `include/impl`，而当前 topic 仍用旧 `test_support/` 目录承载常规测试支撑
内部头，worker 应把 `test_support/ -> include/impl` 作为 structure parity 的默认候选动作。
相邻成熟 topic 的该目录结构可以作为规范级 quality bar，但不能写死某个 sibling 为唯一参考对象。
“只是路径 churn”“已有 code map 可读”或“reviewer 需要时再做”不能单独支撑 `ready_for_review`；
若暂缓且无真实 blocker，默认恢复入口必须是 `internal-helper-layout` 或与 `test-source-split`
合并的下一 phase。
如果 roadmap 的默认恢复队列同时包含 `test-source-split`，而当前 shape scan 又发现旧 `test_support/`
需要迁移到 `include/impl`，worker 不能把二者互相遮蔽。默认做法是先写一个结构 phase，明确选择
“合并执行 test source split + internal helper layout”或“按依赖顺序连续执行两个窄 phase”；只要二者
都仍在当前 topic 测试资产边界内且没有真实 blocker，最终 `ready_for_review` 前必须闭合二者。
若相邻成熟 topic 的 README、topic-local doc suite、evaluation 主路径、`doc-rvv` 长期文档分工或
legacy 清理明显更成熟，worker 必须把这些结构差距合并成当前 topic 的 structure-parity 候选 phase。
除非存在真实外部依赖、dirty isolation 风险或用户限定范围，否则该 phase 是默认下一步，不能把
roadmap-only、evaluation-only 或 pointer-only 小阶段收口成 `ready_for_review`。
该规则不写死任何 sibling 名称：worker 应从同模块、同数据流、同测试复杂度或最近 reviewer 通过的 topic
中选取成熟度标尺；若没有合适 sibling，也要按配置和当前 topic 的真实 shape 自行形成 quality bar。
如果短 prompt 的目标是恢复 S0、冻结偏好或复核产物发布边界，先读 `.agents/skills/rvv-workflow/references/s0-preferences-and-recovery.zh.md`，再决定是否继续 phase loop。

worker（执行者）和 reviewer（审查者）启动时先读取：

1. `AGENTS.md`
2. `.agents/config/defaults.yaml`
3. 如果存在，`.agents/local/user-preferences.yaml`
4. `.agents/knowledge/pcl-rvv-knowledge-map.md`
5. `.agents/skills/rvv-workflow/SKILL.md`
6. `.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md`
7. `.agents/skills/rvv-workflow/references/reviewability-and-language.zh.md`
8. `.agents/skills/rvv-workflow/references/s0-preferences-and-recovery.zh.md`

S0 必须在输出中写明 `preferences_loaded`，并冻结注释、文档、证据、日志和 agent asset（代理资产）反馈偏好。若 local override
存在，worker 还要报告读取到的本机覆盖范围；若不存在，写明只使用 defaults。
默认 agent asset feedback mode（代理资产反馈模式）是 `report-only`（只报告建议）。worker 在 S4 测试计划、
S10 EvidenceDecision（证据决策）、S11 closeout（收尾）、blocked（阻塞）边界、短 prompt 恢复失败、
过早停止复盘或用户 / reviewer 明确反馈工作流程问题时，必须判断是否存在可复用规则、资产缺口或冗余规则。
若存在，输出 `agent_asset_feedback`；没有发现时省略。reviewer 在 closeout review（收尾审查）中按同一规则报告建议。
当用户授权 workflow improvement 时，应先修订对应 `.agents/` skill/reference，再回到 topic 工作；不要只把流程缺口写入 topic follow-up。

## 最短启动写法

worker 可用：

```text
在 <repo> 中，以 RVV worker 身份处理下一个未完成 topic。
```

继续当前 topic 可用：

```text
在 <repo> 中，以 RVV worker 身份继续当前 topic，进入下一阶段。
请保存本轮 work log。
```

也可以显式要求按交接恢复：

```text
在 <repo> 中，以 RVV worker 身份继续当前 topic。
请按当前 Handoff Packet 的 next_worker_action_if_review_passes 恢复，并保存本轮 work log。
```

如果已经完成 PI1，继续生产闭环可用：

```text
在 <repo> 中，以 RVV worker 身份继续当前 topic。
请按当前 Handoff Packet 恢复，并推进 production integration loop，保存本轮 work log。
```

用户不需要知道 `next_worker_action_if_review_passes` 字段名。只要短 prompt 表达“继续当前 topic”
或“进入下一阶段”，worker 默认就要从最近 Handoff Packet 里的 `phase_loop_state` 恢复：
先读 `current_phase`、`phase_plan_paths`、`phase_result_paths`、`unblocked_next_actions`
和 `next_phase_default`。`next_worker_action_if_review_passes` 仍可作为兼容字段，但不再是唯一主来源。
若 phase loop 状态缺失、路径不存在或与用户新指令冲突，worker 先说明恢复风险，
再按 `phase_reached`、`current_decision`、reviewer prompt patch（审查者提示词补丁）和当前源码证据推导下一步。

如果最近 Handoff Packet（交接数据包）的 `current_decision` 是 `partial-production-candidate`
（局部生产候选），且用户说“继续当前 topic”“进入下一阶段”“尝试生产接入”或等价目标，
worker 默认进入 `PI1 production_integration_plan`（生产接入计划），而不是重新从 S0-S12
跑完整诊断，也不是直接修改 production（生产源码）。PI1 必须按 `topic-lifecycle.zh.md`
的 production integration loop 规则先确认候选范围、fallback、dispatch、点类型 / Scalar
边界和证据计划。

如果用户明确说“进入 / 推进 production integration loop（生产接入闭环）”“尝试生产接入”
或等价目标，短 prompt 默认含义不是“只做 PI1 计划”。worker 应把 PI1 当作同轮闭环的第一道
gate（门禁）：PI1 能冻结候选范围且未命中暂停条件时，继续同轮推进 PI2-PI5，并在 PI5 后完成
S11 文档 closeout（收尾文档）。只有用户明确说“只做 PI1 / 只写计划 / 先不要改 production”，
或 PI1 gate 不能闭合，才停在 PI1 并输出 Handoff Packet。

如果最近 Handoff Packet 的 `phase_reached` 已经是 `PI1 production_integration_plan complete`，
且用户要求继续 production integration loop，worker 默认按 `topic-lifecycle.zh.md` 连续推进 PI2-PI5。
连续推进不代表扩大范围；worker 必须遵守 Handoff Packet 中的候选范围和暂停条件。

reviewer 可用：

```text
在 <repo> 中，以 RVV reviewer 身份审查上一轮 worker 结果。
```

workflow improvement 可用：

```text
在 <repo> 中，以 RVV workflow improvement 身份复核 <path>。
允许修改 agent asset，不修改 production 或配置解析出的 topic 文档 / 测试产物。
```

用户补充的规则优先级高于本文默认值。用户没有覆盖时，使用本文默认值。

## 默认保存策略

如果用户要求保存对话输出，默认使用可配置的 work log 根目录：

```text
work_log_root = ${PCL_RVV_WORK_LOG_ROOT:-<repo>/tmp/rvv-work-logs}
```

其中 `<repo>` 表示当前 PCL 仓库根目录。`tmp/rvv-work-logs/` 位于仓库内，
但 `tmp/` 默认不提交。

默认路径结构：

```text
<work_log_root>/<module>/<object>/<run-id>/
```

其中：

- `<module>` 是当前模块名。
- `<object>` 是工作对象名，优先使用 topic 名；如果对象是文件，则使用相对路径
  转成的稳定 slug（路径标识）。
- `<run-id>` 是本轮标识，例如日期加序号。

默认文件划分：

```text
manifest.md
worker1-output.md
worker1-handoff.md
reviewer1-review.md
reviewer1-prompt-patch.md
worker2-output.md
worker2-handoff.md
final-closeout.md
```

如果只有一轮 worker / reviewer，可以只保留对应的 `output`、`handoff` 和 `review`
文件，不必强制补齐后续轮次。

如果用户没有要求保存输出，默认只在对话中返回结果，不落盘。

## 默认读取链

所有角色先读：

1. `AGENTS.md`
2. `.agents/config/defaults.yaml`
3. 如果存在，`.agents/local/user-preferences.yaml`
4. `.agents/knowledge/pcl-rvv-knowledge-map.md`
5. `.agents/skills/rvv-workflow/SKILL.md`
6. `.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md`
7. `.agents/skills/rvv-workflow/references/reviewability-and-language.zh.md`

worker 再读：

1. `.agents/skills/rvv-workflow/references/topic-lifecycle.zh.md`
2. `.agents/skills/rvv-workflow/references/handoff-packet.zh.md`
3. `.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md`
4. `.agents/skills/rvv-test/references/optimization-phase-loop.zh.md`，当短 prompt 继续已有 topic、恢复 phase plan/result 或当前 phase 仍有 unblocked next action 时读取。
5. `.agents/skills/rvv-workflow/references/topic-entry-template.md`
6. `.agents/skills/rvv-documentation/references/function-evaluation-and-closeout.zh.md`
7. `.agents/skills/rvv-documentation/references/document-ownership-and-traceability.zh.md`，当本轮要写 closeout、evaluation、主题文档、Handoff Packet，或 topic 涉及多处代码 / 测试 / 输出定位时读取。
8. 当前模块的筛选状态表。
9. 命中的 topic 源码、文档和测试证据。

worker 选中 topic 后、开始写配置解析出的 topic 测试资产、topic 文档或 production 前，必须按
`worker-quality-gates.zh.md` 做一次轻量自查。若 topic 涉及 staging（分阶段暂存）、
gather（离散加载）、`vcompress`、scalar tail（标量尾段）、vector reduction（向量规约）、
FMA（融合乘加）、板卡性能、benchmark summary、checksum summary、asm attribution 或 no-production closeout（不接入生产收尾），继续读取该文件指向的
`rvv-documentation`、`rvv-test` 和 `rvv-implementation` 详细 reference；其中 benchmark、board summary、checksum、asm attribution 或 EvidenceDecision 必须读取 `rvv-test/references/evidence-doctor.zh.md`。
短 prompt 只负责启动变短，不降低 worker 产物质量门槛。

registration（配准）topic 如果涉及 transformation estimation（变换估计）、correspondence
estimation（对应关系估计）、row source policy（行来源策略）、`accepted_points`、`ATA/ATb`
或 matrix（矩阵）证据，必须读取
`.agents/skills/rvv-test/references/registration-topic-evidence.zh.md`。

短 prompt worker 不要求用户显式写“检索历史经验”。worker 选中 topic 并读取当前源码/文档后，
如果发现多公开入口、indices、correspondences、weights、staging、policy、row source、
common pipeline 或类似数据流分发信号，应按 `.agents/knowledge/pcl-rvv-knowledge-map.md`
的 `Historical Analogy Retrieval Pattern` 自动检索同模块历史 topic 和 dataflows 文档。
检索结果只能作为候选设计、风险提示和证据计划来源；不能因为历史 topic 用过某个 helper 或
production 决策，就跳过当前源码复核、QEMU correctness、反汇编和板卡证据。

如果短 prompt 是“继续当前 topic”或“进入下一阶段”，worker 应先读取最近 work log（工作日志）
或 Handoff Packet，恢复 `phase_loop_state`、`phase_reached`、`current_decision`、
`phase_plan_paths`、`phase_result_paths`、optimization roadmap、optimization matrix 和 evidence paths（证据路径）。
`phase_loop_state.next_phase_default` 是默认续作入口；`next_worker_action_if_review_passes` 只作为兼容别名。
除非用户新指令覆盖，不要绕过 phase loop 自行选择下一个 topic 或重跑旧阶段。
若恢复入口写 `ready_for_review`，必须先执行 `ready_for_review_validity_check`：读取 roadmap、matrix、
最近 phase result 和成熟度审计，确认没有 `phase_deferred + unblocked` 的 structure parity、doc suite、
legacy cleanup、test source split、internal helper layout、row-source family carry-over 或证据 freshness 缺口。
任一项未闭合时，`ready_for_review` 失效，worker 默认创建或修订下一 phase plan。
当恢复到 `partial-production-candidate` 并进入 PI1 时，还必须读取：

1. `.agents/skills/rvv-implementation/SKILL.md`
2. `.agents/skills/rvv-implementation/references/point-load-store.md`
3. `.agents/skills/rvv-implementation/references/fallback-and-dispatch.md`
4. 配置或 adapter 指定的 generic point type strategy（泛型点类型策略）文档，仅在目标 production 入口是模板点类型、需要 traits / offset / layout gate，或从 `PointNormal` 诊断扩展到泛型入口时读取。

reviewer 再读：

1. `.agents/skills/rvv-workflow/references/reviewer-protocol.zh.md`
2. `.agents/skills/rvv-workflow/references/handoff-packet.zh.md`
3. `.agents/skills/rvv-workflow/references/topic-lifecycle.zh.md`
4. `.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md`
5. `.agents/skills/rvv-test/references/optimization-phase-loop.zh.md`，尤其是检查 phase plan/result、optimization matrix 和 early-stop 条件时读取。
6. `.agents/skills/rvv-documentation/references/function-evaluation-and-closeout.zh.md`
7. `.agents/skills/rvv-documentation/references/document-ownership-and-traceability.zh.md`
8. `.agents/skills/rvv-test/SKILL.md`，以及当前证据类型需要的窄 reference。
9. worker 输出、Handoff Packet（交接数据包）、当前 diff（差异）和 topic 证据；如果用户没有给 worker 输出路径，就读当前对话中最近一轮 worker 回复或用户贴入的交接内容。

workflow improvement 再读：

1. reviewer 默认读取链。
2. worker 或 reviewer 指出的 agent asset（代理资产）缺口所在 skill（技能）或 reference（参考文件）。
3. 需要更新的 prompt 模板。

不要读取 `tmp/agent-migration-sources/`。只有用户明确要求历史追溯、迁移审计或规则迁移时才读取。

## 默认权限

worker 默认权限：

- 短 prompt 中“处理 topic”视为授权修改该 topic 对应的、由 `artifact_layout` 解析出的测试资产和主题文档产物。
- 对测试优化和 topic-local 文档成熟度工作，短 prompt 默认授权 worker 在当前 topic 内连续推进多个低风险 phase，例如测试支撑结构迁移、legacy 聚合头 / pointer 清理、evaluation 迁入 `doc/`、README / doc suite 补齐、source-indexed 或其它 row source 的 candidate / correctness / bench / asm / Evidence Doctor 阶段。除非继续会扩大到 production、public API、其它 topic、板卡不可用、证据矛盾或 dirty isolation 不安全，否则不应因为一个小 phase 完成就停止。
- 不把该授权扩展到其它 topic 的测试资产、主题文档或生产源码。
- S10 `EvidenceDecision`（证据决策）前不修改 production（生产源码）。
- 如果证据支持 production-ready（可接入生产），先输出 Handoff Packet，等待用户确认后进入 production integration loop（生产接入闭环）。
- 默认不创建 commit（提交）。
- 默认不要求用户预先指定输出路径；只有用户要求落盘、保存到固定 work-log（工作日志）或跨对话复用时，才写入路径。
- S0 必须记录 `preferences_loaded`，并写明是否读取了 `.agents/config/defaults.yaml` 与 `.agents/local/user-preferences.yaml`。

reviewer 默认权限：

- 只读审查。
- 可以运行 `git diff`、`git status`、`rg`、`sed`、`find` 等只读命令。
- 不修改 production、配置解析出的 topic 文档 / 测试产物或 agent asset。
- 不创建 commit。

workflow improvement 默认权限：

- 只修改 `.agents/skills/`、`.agents/knowledge/`、`.agents/config/`、`AGENTS.md`、必要的 `.gitignore` 和用户指定的 prompt 模板。
- 不修改 production 或配置解析出的 topic 文档 / 测试产物。
- 不创建 commit。
- 批量修改 skill、knowledge map 或入口 prompt 前，先创建 `.agents/backup/` 下的不提交备份目录。

## 默认输出合同

worker 最终输出必须包含：

- 修改文件。
- 执行命令。
- 证据路径。
- EvidenceDecision。
- Evidence Doctor result（证据体检结果），包含 Errors / Warnings / Suggestions、未解决 warning、处理动作和是否降级证据边界；没有运行脚本时说明人工检查边界。
- `language_check`。
- `worker_quality_gate_check`，使用 `gate | status | evidence | missing_items` 证据化表格，并覆盖 `document_ownership_matrix_ready` 与 `traceability_map_ready`。
- `preferences_loaded`。
- `agent_asset_trace`。
- `agent_asset_feedback`，仅在本轮发现可沉淀规则、资产缺口或冗余规则时输出；默认只报告建议，不自动改 agent asset。
- 用户或 reviewer 对工作流程、测试体系、文档结构、恢复方式、停止方式和可读性的反馈，必须先判断是否属于 agent asset 缺口；若是，写入 `agent_asset_feedback`，并在获得 workflow improvement 授权时优先修订对应 skill/reference，避免同类问题重复出现。
- Handoff Packet。
- 若当前结论是窄范围、局部候选、不接入生产但仍有可复用后续方向，输出给用户的后续路径选项：
  默认建议、继续当前 topic、另开 follow-up topic、当前不建议做的方向。
- 如果本轮停止时仍有 `phase_deferred + unblocked` 项，明确列出“本轮没有做但可继续做”的事项、默认下一 phase、停止条件和需要用户 / reviewer 判断的边界；不能只写成泛泛 remaining risks。
- 如果最终输出 `ready_for_review`，必须同时写 `ready_for_review_validity_check` 摘要，说明 structure parity、doc suite、legacy compatibility、roadmap、optimization matrix 和 shape scan 中没有未阻塞缺口；否则不得使用该结论。

reviewer 最终输出必须符合 reviewer protocol（审查协议）：

- Findings（问题清单）。
- Evidence reviewed（已复核证据）。
- Suggested next worker actions（建议 worker 下一步动作）。
- Worker prompt patch（给 worker 的提示词补丁）。
- Suggested skill / knowledge-map updates（建议更新的 skill 或知识索引）。
- `agent_asset_feedback` 或等价小节，仅在发现可沉淀规则、资产缺口或冗余规则时输出。

workflow improvement 最终输出必须包含：

- Findings。
- Asset gaps（资产缺口）。
- Diff-level summary（diff 级别摘要），按文件说明新增、修改和未触碰范围。
- Changes made in Workflow improvement mode（工作流改进模式的实际改动）。
- `backup_path`，如果本轮创建了备份。
- Validation（验证命令和结果）。
- Handoff Packet（交接数据包），至少包含 files_changed、implementation_review、candidates_added_or_deferred、document_ownership_check、traceability_map_status、evidence_doctor_result、dirty_isolation、validation、remaining_risks 和 next_worker_action。
- New short prompt example（新的短 prompt 示例）。

## 缺省工作偏好

用户未覆盖时，worker 在 S0 记录：

- 偏好来源：`.agents/config/defaults.yaml`、可选 `.agents/local/user-preferences.yaml` 和当前 prompt。
- 配置解析出的测试资产、diagnostic（诊断代码）和 prototype（原型代码）默认使用详细中文注释。
- production 注释默认克制，只解释维护边界、fallback（回退路径）、dispatch（分流逻辑）、数值风险和数据布局。
- 英文专有术语首次出现时默认写中文解释。
- QEMU（仿真器）默认只作为 correctness（正确性）、路径和日志形状证据。
- 板卡或目标硬件结果才支撑性能结论。
- evidence logs（证据日志）默认 `summary-only`，不提交 raw logs（原始日志）；用户要求提交时先脱敏并拆分 commit。

## 短 prompt 自查

收到短 prompt 后，agent 先检查：

- role 是否明确。
- 工作目录是否明确。
- 粗目标是否明确。
- 模块、topic、worker 输出或 reviewer 输出是否能从当前上下文、状态表、对话内容或用户贴入内容中推导。
- 是否需要用户授权写 production、进入 workflow improvement 或创建 commit。
- 默认读取链是否足以启动本轮任务。
- 若目标是继续 RVV 优化工作，是否已经恢复或创建 phase plan、optimization roadmap 和 optimization matrix，并判断 `phase_deferred` 是否仍可在本轮继续。
- 若恢复状态声称 `ready_for_review`，是否已证明该状态没有被 roadmap、matrix、mature sibling parity 或当前源码 shape scan 推翻。

如果无法推导模块、topic 或 worker 输出，先提出一个具体问题。其余默认条件从本文读取。
