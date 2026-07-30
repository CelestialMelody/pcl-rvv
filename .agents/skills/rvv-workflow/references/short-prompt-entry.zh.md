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

worker（执行者）和 reviewer（审查者）启动时先读取：

1. `AGENTS.md`
2. `.agents/config/defaults.yaml`
3. 如果存在，`.agents/local/user-preferences.yaml`
4. `.agents/knowledge/pcl-rvv-knowledge-map.md`
5. `.agents/skills/rvv-workflow/SKILL.md`
6. `.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md`
7. `.agents/skills/rvv-workflow/references/reviewability-and-language.zh.md`

S0 必须在输出中写明 `preferences_loaded`，并冻结注释、文档、证据、日志和 agent asset（代理资产）反馈偏好。若 local override
存在，worker 还要报告读取到的本机覆盖范围；若不存在，写明只使用 defaults。
默认 agent asset feedback mode（代理资产反馈模式）是 `report-only`（只报告建议）。worker 在 S4 测试计划、
S10 EvidenceDecision（证据决策）、S11 closeout（收尾）或 blocked（阻塞）边界发现可复用规则、资产缺口或冗余规则时，
才输出 `agent_asset_feedback`；没有发现时省略。reviewer 在 closeout review（收尾审查）中按同一规则报告建议。

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
或“进入下一阶段”，worker 默认就要从最近 Handoff Packet 读取该字段；若字段存在且与用户新指令不冲突，
它就是本轮下一步动作的主来源。若字段缺失、路径不存在或与用户新指令冲突，worker 先说明恢复风险，
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
4. `.agents/skills/rvv-workflow/references/topic-entry-template.md`
5. `.agents/skills/rvv-documentation/references/function-evaluation-and-closeout.zh.md`
6. 当前模块的筛选状态表。
7. 命中的 topic 源码、文档和测试证据。

worker 选中 topic 后、开始写配置解析出的 topic 测试资产、topic 文档或 production 前，必须按
`worker-quality-gates.zh.md` 做一次轻量自查。若 topic 涉及 staging（分阶段暂存）、
gather（离散加载）、`vcompress`、scalar tail（标量尾段）、vector reduction（向量规约）、
FMA（融合乘加）、板卡性能或 no-production closeout（不接入生产收尾），继续读取该文件指向的
`rvv-documentation`、`rvv-test` 和 `rvv-implementation` 详细 reference。短 prompt
只负责启动变短，不降低 worker 产物质量门槛。

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
或 Handoff Packet，恢复 `phase_reached`、`current_decision`、`next_worker_action_if_review_passes`
和 evidence paths（证据路径）。`next_worker_action_if_review_passes` 是默认续作入口；除非用户新指令覆盖，
不要绕过它自行选择下一个 topic 或重跑旧阶段。当恢复到 `partial-production-candidate` 并进入 PI1 时，还必须读取：

1. `.agents/skills/rvv-implementation/SKILL.md`
2. `.agents/skills/rvv-implementation/references/point-load-store.md`
3. `.agents/skills/rvv-implementation/references/fallback-and-dispatch.md`
4. 配置或 adapter 指定的 generic point type strategy（泛型点类型策略）文档，仅在目标 production 入口是模板点类型、需要 traits / offset / layout gate，或从 `PointNormal` 诊断扩展到泛型入口时读取。

reviewer 再读：

1. `.agents/skills/rvv-workflow/references/reviewer-protocol.zh.md`
2. `.agents/skills/rvv-workflow/references/handoff-packet.zh.md`
3. `.agents/skills/rvv-workflow/references/topic-lifecycle.zh.md`
4. `.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md`
5. `.agents/skills/rvv-documentation/references/function-evaluation-and-closeout.zh.md`
6. `.agents/skills/rvv-test/SKILL.md`，以及当前证据类型需要的窄 reference。
7. worker 输出、Handoff Packet（交接数据包）、当前 diff（差异）和 topic 证据；如果用户没有给 worker 输出路径，就读当前对话中最近一轮 worker 回复或用户贴入的交接内容。

workflow improvement 再读：

1. reviewer 默认读取链。
2. worker 或 reviewer 指出的 agent asset（代理资产）缺口所在 skill（技能）或 reference（参考文件）。
3. 需要更新的 prompt 模板。

不要读取 `tmp/agent-migration-sources/`。只有用户明确要求历史追溯、迁移审计或规则迁移时才读取。

## 默认权限

worker 默认权限：

- 短 prompt 中“处理 topic”视为授权修改该 topic 对应的、由 `artifact_layout` 解析出的测试资产和主题文档产物。
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
- `language_check`。
- `worker_quality_gate_check`，使用 `gate | status | evidence | missing_items` 证据化表格。
- `preferences_loaded`。
- `agent_asset_trace`。
- `agent_asset_feedback`，仅在本轮发现可沉淀规则、资产缺口或冗余规则时输出；默认只报告建议，不自动改 agent asset。
- Handoff Packet。
- 若当前结论是窄范围、局部候选、不接入生产但仍有可复用后续方向，输出给用户的后续路径选项：
  默认建议、继续当前 topic、另开 follow-up topic、当前不建议做的方向。

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
- Changes made in Workflow improvement mode（工作流改进模式的实际改动）。
- `backup_path`，如果本轮创建了备份。
- Validation（验证命令和结果）。
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

如果无法推导模块、topic 或 worker 输出，先提出一个具体问题。其余默认条件从本文读取。
