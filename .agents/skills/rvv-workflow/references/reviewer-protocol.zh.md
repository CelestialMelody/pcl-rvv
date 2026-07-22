# Reviewer Protocol（审查者协议）

本文定义 RVV topic（主题）工作中的 reviewer（审查者）协议。它适用于当前 Codex 人工中继工作流，也适用于未来 `rvv-agent` 的内置 review role（审查角色）。

reviewer 的目标不是替 worker（执行者）重做任务，而是发现证据漏洞、行为偏离、文档不可审查、语言规范问题和 workflow（工作流）资产缺口。

## 默认权限

默认使用 Topic review mode（主题审查模式）：

- 只读审查 worker 产物。
- 可以运行 `git diff`、`rg`、`sed`、`find` 等只读命令。
- 必要测试如果会产生 build/log（构建 / 日志）输出，应先说明目的和影响，再判断是否需要执行。
- 不修改 PCL 生产源码、`doc-rvv`、`test-rvv`。
- 不直接修 worker 产物。
- 不创建 commit（提交）。

只有用户明确授权时，才进入 Workflow improvement mode（工作流改进模式）：

- 可以修改 prompt（提示词）、skill（技能）、knowledge map（知识索引）、`AGENTS.md` 等 agent asset（代理资产）。
- 只修改与本轮发现直接相关的 workflow / skill / knowledge map / prompt。
- 不修改 PCL 生产源码、`doc-rvv`、`test-rvv`，除非用户另行明确要求。
- 修改前先说明计划；修改后列出文件、理由和验证方式。

## 必查输入

reviewer 至少应读取：

- `AGENTS.md`
- `.agents/knowledge/pcl-rvv-knowledge-map.md`
- `.agents/skills/rvv-workflow/SKILL.md`
- `.agents/skills/rvv-workflow/references/topic-lifecycle.zh.md`
- `.agents/skills/rvv-workflow/references/handoff-packet.zh.md`
- `.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md`
- `.agents/skills/rvv-workflow/references/reviewability-and-language.zh.md`
- `.agents/skills/rvv-documentation/references/function-evaluation-and-closeout.zh.md`
- 与当前 topic 相关的 worker Handoff Packet（交接数据包）
- 当前 topic 的 diff（差异）、评估文档、主题文档、测试 / bench / QEMU / 反汇编 / 板卡证据

后续按任务需要读取 `rvv-diagnostics`、`rvv-implementation`、`rvv-benchmarking`、`rvv-documentation` 的细则。

## Findings First（问题优先）输出格式

reviewer 输出必须 findings first（问题优先）：

```text
Findings
- [Severity] 文件路径:行号 - 问题标题
  说明问题、证据、影响和建议修复方向。
```

如果没有 blocking issue（阻塞问题），必须明确写出“未发现 blocking issue”。不要用“整体不错”替代审查结论。

严重度建议：

- `Blocking`：会导致错误结论、无法复现、错误生产接入、证据缺失或安全 / 隐私风险。
- `High`：会明显影响维护性、证据可信度、性能结论或后续恢复。
- `Medium`：需要修正，但不阻止当前 closeout 或下一阶段。
- `Low`：表达、格式、轻量补充或后续可改进项。

## Then Sections（后续章节）

Findings 之后按顺序输出：

```text
Open questions / assumptions（未解决疑问 / 默认假设）
Evidence reviewed（本轮复核过的证据）
Suggested next worker actions（建议 worker 下一步动作）
Worker prompt patch（可转发给 worker 的提示词补丁）
Suggested skill / knowledge-map updates（建议更新的 skill 或知识索引）
Language/reviewability issues（语言和可审查性问题）
Workflow improvement decision（工作流改进决策）
Files changed in workflow improvement mode（若启用工作流改进模式，本轮改动文件）
```

可以省略空章节，但不能省略 `Evidence reviewed`、`Suggested next worker actions` 和 `Worker prompt patch`。

## Worker Prompt Patch（给 worker 的提示词补丁）要求

`Worker prompt patch` 必须可直接转发给 worker。它应说明：

- 下一步 phase（阶段）或 branch（分支）。
- 必须复核的源码、文档、测试和证据。
- 禁止事项，例如不扩大生产改动、不把 QEMU 写成性能结论、不提交日志。
- 完成条件，例如重新跑哪些命令、更新哪些文档、输出哪些 handoff 字段。

如果 worker 可以继续，patch 应让 worker 聚焦下一阶段，不重新做已完成工作。

如果存在 blocking issue，patch 应先修 blocking issue，不继续扩大实现。

如果当前结果已经可以 closeout，patch 应让 worker 只做收尾或等待用户确认。

如果发现语言或术语问题，patch 必须要求 worker 按 `reviewability-and-language.zh.md` 修正，并点名需要修正的文件或段落；不要只写“润色文档”。

## 审查重点

reviewer 应至少检查：

- worker 是否按队列表选择正确 topic。
- S2 是否创建或更新 evaluation（函数级评估）文档。
- S10 EvidenceDecision（证据决策）后是否按 `topic-lifecycle.zh.md` 进入正确分支。
- production integration loop（生产接入闭环）是否包含生产补丁、生产直连测试、生产证据重跑和再次证据决策。
- QEMU、反汇编和板卡证据是否分层正确。
- `test-rvv`、diagnostic（诊断代码）、prototype（原型代码）是否有足够中文注释和文件级阅读提示。
- doc-rvv 文档是否区分 S2 evaluation 和 S11 closeout（收尾）。
- 文档是否能让读者理解标量实现做了什么、RVV 方案如何实现、bench case 如何构造和证明什么；如果只列公式、helper 名、指令名或 speedup，视为可审查性缺口。
- 对 buffer/staging、scalar tail（标量尾段）、fused multiply-add（融合乘加）、vector reduction（向量规约）、数学函数是否向量化等实现取舍，worker 是否给出理由、替代方案和需要补的证据。
- 负向性能结论是否有受证据约束的归因；不能把未验证猜测写成事实，也不能只写“不接生产”而不解释为什么慢。
- 是否存在不该提交的 build（构建）产物、日志、本机路径、私有地址或 `config.mk`。
- Handoff Packet 是否字段完整，`agent_asset_trace` 是否真实反映读取并使用过的资产。
- Handoff Packet 是否把重要后续选择暴露给用户。若当前结论是窄范围 production-ready、partial-production-candidate、
  bench-only/no-production 或保留重要未覆盖范围，reviewer 应检查 `followup_options_for_user` 是否列出默认动作、
  继续当前 topic 的扩展动作、应另开 topic 的消融 / 扩展动作和当前不建议做的方向。
- 如果 worker 使用短 prompt 启动，Handoff Packet 是否包含 `worker_quality_gate_check`，且该字段真实覆盖标量路径、production/diagnostic 数据流映射、文档结构、test-rvv 注释、bench 边界、替代方案审计、证据模型和 stop condition。缺失或虚写时，应视为 workflow/worker 执行缺口。
- `worker_quality_gate_check` 是否是证据化表格，而不是只有 `true` / `false`。reviewer 应抽查每项 `evidence` 是否能在当前 topic 产物中定位；若找不到对应文件、章节、日志或代码注释，应把该项判为未闭合。
- `language_check` 是否同样有证据支撑。若 worker 声称通过，但诊断 helper、测试、bench 或主题文档仍有非平凡段落缺少中文说明，应指出具体文件和行号。
- 当前结论若为 `partial-production-candidate`、`production-ready` 或其它强于 no-production 的状态，reviewer 必须检查 worker 是否列出 production direct 缺口。诊断路径上的板卡收益不能自动升级成 production-ready。
- 当前结论若为 `production-ready/narrow`，reviewer 必须检查 worker 是否主动指出“窄在哪里、是否存在常见泛型扩展价值、继续扩展需要哪些证据”。如果 worker 只建议进入下一个 topic，却没有给出当前 topic 的重要扩展选项，应视为 handoff 可决策性缺口。

## Agent Asset（代理资产）更新判断

reviewer 可以建议修改 agent asset，但不要把每个 topic 的一次性偏好都上升为通用规则。

适合进入 asset 的经验：

- 跨多个 topic 复用。
- 影响证据边界、权限边界、提交安全或可审查性。
- 能防止 worker 重复犯错。
- 能明确改变读取策略、状态机、文档结构或测试门禁。

不适合立即进入 asset 的内容：

- 当前 topic 特有参数。
- 单次实验的临时命令。
- 某个 worker 的表达习惯。
- 尚未被 reviewer 复核的猜测。

如果建议更新 asset，应说明：

- 建议改哪个文件。
- 缺口是什么。
- 为什么它是通用规则。
- 是否需要先在下一轮 topic 中验证。
