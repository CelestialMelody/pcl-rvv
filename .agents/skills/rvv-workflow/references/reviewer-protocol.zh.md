# Reviewer Protocol（审查者协议）

本文定义 RVV topic（主题）工作中的 reviewer（审查者）协议。它适用于当前 Codex 人工中继工作流，也适用于未来 `rvv-agent` 的内置 review role（审查角色）。

reviewer 的目标不是替 worker（执行者）重做任务，而是发现证据漏洞、行为偏离、文档不可审查、语言规范问题和 workflow（工作流）资产缺口。

## 默认权限

默认使用 Topic review mode（主题审查模式）：

- 只读审查 worker 产物。
- 可以运行 `git diff`、`rg`、`sed`、`find` 等只读命令。
- 必要测试如果会产生 build/log（构建 / 日志）输出，应先说明目的和影响，再判断是否需要执行。
- 不修改 PCL 生产源码、配置解析出的 topic 文档或测试产物。
- 不直接修 worker 产物。
- 不创建 commit（提交）。

只有用户明确授权时，才进入 Workflow improvement mode（工作流改进模式）：

- 可以修改 prompt（提示词）、skill（技能）、knowledge map（知识索引）、`AGENTS.md` 等 agent instructions（代理指令）。
- 只修改与本轮发现直接相关的 workflow / skill / knowledge map / prompt。
- 不修改 PCL 生产源码、配置解析出的 topic 文档或测试产物，除非用户另行明确要求。
- 修改前先说明计划；修改后列出文件、理由和验证方式。
- workflow improvement worker 交给 reviewer 前，应输出 diff 级别摘要和 Handoff Packet，并显式说明是否未触碰 production、配置解析出的 topic 文档 / 测试产物或 evidence logs。

## 必查输入

reviewer 至少应读取：

- `AGENTS.md`
- `.agents/config/defaults.yaml`
- `.agents/local/user-preferences.yaml`，如果存在。只检查覆盖范围和 env var（环境变量）名，不复制私有值。
- `.agents/knowledge/pcl-rvv-knowledge-map.md`
- `.agents/skills/rvv-workflow/SKILL.md`
- `.agents/skills/rvv-workflow/references/topic-lifecycle.zh.md`
- `.agents/skills/rvv-workflow/references/handoff-packet.zh.md`
- `.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md`
- `.agents/skills/rvv-workflow/references/reviewability-and-language.zh.md`
- `.agents/skills/rvv-documentation/references/function-evaluation-and-closeout.zh.md`
- `.agents/skills/rvv-documentation/references/document-ownership-and-traceability.zh.md`
- `.agents/skills/rvv-test/SKILL.md`
- `.agents/skills/rvv-test/references/evidence-doctor.zh.md`，当 worker 输出 benchmark、board summary、checksum、asm attribution 或 EvidenceDecision 时读取并检查 doctor result。
- `.agents/skills/rvv-test/references/optimization-phase-loop.zh.md`，当 worker 用短 prompt 继续 topic、输出 phase plan/result、仍有 unblocked next action，或 reviewer 需要判断是否过早停止时读取。
- registration（配准）topic 若涉及变换估计、对应关系估计、row source 或法方程，读取
  `.agents/skills/rvv-test/references/registration-topic-evidence.zh.md`
- 与当前 topic 相关的 worker Handoff Packet（交接数据包）
- 当前 topic 的 diff（差异）、评估文档、主题文档、测试 / bench / QEMU / 反汇编 / 板卡证据

后续按任务需要读取 `rvv-test`、`rvv-implementation`、`rvv-documentation` 的细则。
旧 diagnostics / benchmarking 职责已经迁移到 `rvv-test`，reviewer 不再读取独立旧 skill。

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
Phase-loop / early-stop review（阶段循环 / 早停检查）
Suggested next worker actions（建议 worker 下一步动作）
Worker prompt patch（可转发给 worker 的提示词补丁）
Suggested skill / knowledge-map updates（建议更新的 skill 或知识索引）
Instruction feedback（指令反馈）
Language/reviewability issues（语言和可审查性问题）
Workflow improvement decision（工作流改进决策）
Files changed in workflow improvement mode（若启用工作流改进模式，本轮改动文件）
```

可以省略空章节，但不能省略 `Evidence reviewed`、`Phase-loop / early-stop review`、`Suggested next worker actions` 和 `Worker prompt patch`。`Instruction feedback` 只在发现可沉淀规则、instruction gap（指令缺口）或冗余规则时输出。

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
- production closeout 或 production-candidate 文档是否包含“正确性与高效性证据链”小节；未接 production 的诊断结论是否在 topic-local evaluation / phase closeout 中包含“诊断证据链”小节，且没有默认新增 `doc-rvv`。
- 证据链是否写清 correctness（正确性）、performance（性能）、boundary（证据边界）和 risk（风险）：public entry 是否真实命中；row semantics 是否清楚；`accepted_points`、中间态、matrix 和 fallback 是否有证据；性能结论是否只来自 repeated board 或目标硬件。
- benchmark、board summary、checksum、asm attribution 或 EvidenceDecision 是否运行或人工填写 Evidence Doctor（证据体检）结果；Error 是否阻塞严格结论，Warning 是否进入 summary / evaluation / Handoff 的风险说明和处理动作。
- 证据链是否把 QEMU timing、diagnostic evidence、representative pointtypes、indexed / correspondences 边界写清。QEMU timing 不能写成性能结论，diagnostic evidence 不能写成 production evidence。
- registration 主题是否按 `registration-topic-evidence.zh.md` 审计 `accepted_points`、`ATA/ATb`、
  matrix、weights、symmetric normals、query/match 输出语义和 production direct 边界。
- 配置解析出的测试资产、diagnostic（诊断代码）、prototype（原型代码）是否有足够中文注释和文件级阅读提示。
- production 长期主题文档是否只在 adopted production behavior、production patch 或 PI5 通过后适用，并区分 S2 evaluation 和 S11 closeout（收尾）。
- 文档归属是否符合 `document-ownership-and-traceability.zh.md`：production 长期主题文档负责 adopted production 事实，evaluation 负责候选取舍和 no-production 诊断证据链，output summary / analysis script 负责 bench 统计，Handoff Packet 负责恢复动作；长段重复、互相矛盾、证据错放，或 no-production 默认发布 `doc-rvv` 应视为可审查性缺口。
- topic-local doc suite 是否按 `rvv-documentation/references/doc-suite-quality-bar.zh.md` 和 `artifact_layout` role path key 审计。成熟 sibling topic 只能作为补充校准，不应成为唯一规范源；如果 worker 只写“参考某成熟 topic”而没有 canonical quality bar 审计表，应视为文档门禁缺口。
- 复杂 topic 是否有 Traceability Map，且能从文档定位到 production 入口、diagnostic / candidate helper、bench wrapper、analysis script、output summary 和 Handoff 恢复字段。
- 文档是否能让读者理解标量实现做了什么、RVV 方案如何实现、bench case 如何构造和证明什么；如果只列公式、helper 名、指令名或 speedup，视为可审查性缺口。
- 对 buffer/staging、scalar tail（标量尾段）、fused multiply-add（融合乘加）、vector reduction（向量规约）、数学函数是否向量化等实现取舍，worker 是否给出理由、替代方案和需要补的证据。
- 如果 worker 声明采用 sibling topic（同模块相邻主题）经验，是否输出 experience-migration audit（经验迁移审计）表，并覆盖 row source、source / weight policy、shared math pipeline、staging / reduction、formula / FMA、evidence model 和 production boundary。缺少 adopted / attempted / deferred / rejected 对照表，或只说“已参考相邻经验”但没有说明未采用的成功 / 负向方案，应视为 workflow/worker 执行缺口。
- 如果单个测试支撑 helper header、根目录测试 / bench 源文件或等价支撑代码超过配置的约 800-1000 行，或混合 reference、row source、RVV math、reduction candidate、bench wrapper、component ablation 中三类以上职责，worker 是否按 `test_support` 配置拆分，或在 Handoff Packet 中写清 deferred reason。没有拆分也没有理由时，应作为可审查性和维护性缺口。reviewer 应检查 worker 是否先做了 test support shape scan；没有字面量 `test_support/` 目录不等于该审计不适用。
- 如果配置解析出的 `test_support.internal_directory` 是 `include/impl`，当前 topic 仍保留旧
  `test_support/` 内部头，而相邻成熟 topic 已采用 `src/` / `include/` / `include/impl` 结构，
  reviewer 应把这视为 structure parity 缺口。worker 可以拒绝复制 sibling 的算法或具体文件集，
  但不能只用“路径重命名 churn”“code map 已经足够”或“reviewer 需要时再做”把目录职责迁移降成
  `turn_stop_deferred`；除非存在真实 blocker，否则应要求默认下一 phase 为
  `internal-helper-layout` 或与 `test-source-split` 合并的窄 phase。
- 如果相邻成熟 topic 已经形成更完整的 source / include / include/impl、topic-local doc suite、evaluation 主路径和 `doc-rvv` 分工，reviewer 应检查 worker 是否把它作为补充校准输出 mature sibling parity status。该审计只迁移结构成熟度，不复制 sibling 算法或文件名；若 worker 没有先按 canonical quality bar 和配置路径审计，或把结构差距标为低优先级 follow-up 却没有真实阻塞，应作为早停或质量门禁缺口。
- reviewer 应检查 legacy pointer、compatibility alias、旧路径 wrapper 或重复正文。默认应更新引用并删除旧入口；若 worker 保留，只写“避免旧引用断开”而没有具体外部依赖、用户要求、dirty isolation 风险和删除阶段，应作为可审查性缺口。
- 负向性能结论是否有受证据约束的归因；不能把未验证猜测写成事实，也不能只写“不接生产”而不解释为什么慢。
- 是否存在不该提交的 build（构建）产物、日志、本机路径、私有地址或 `config.mk`。
- commit review（提交审查）时必须检查 `git diff --cached --name-only`。如果 staged set（暂存集合）包含配置解析出的 evidence output subdir（证据输出子目录）中的文件，reviewer 必须确认用户明确要求提交 evidence summary（证据摘要）或日志，并确认该文件是通过 `git add -f <specific files>` 精确选择的 evidence commit（证据提交），不是共享 ignore 文件自动暴露出的 topic log。
- Handoff Packet 是否字段完整，`instruction_trace` 是否真实反映读取并使用过的 instruction sources（指令来源）。
- Handoff Packet 是否包含 `dirty_isolation`，并明确区分本轮 topic diff、无关 topic diff、instruction diff、raw logs、build 输出和提交边界。缺失时应视为 commit-boundary 风险。
- Handoff Packet 是否包含 `implementation_review`，并能让 reviewer 复核 public entry / `*_Std` / `*_RVV` 或 diagnostic helper 分层、fallback、gate、公共 API 边界、维护风险和本轮是否只限 diagnostic。
- Handoff Packet 是否包含 `candidates_added_or_deferred`，列清新增、尝试、暂缓或拒绝的候选；如果 worker 只给最终方案、没有说明未采用路线，应视为可审查性缺口。
- Handoff Packet 是否包含 `document_ownership_check` 和 `traceability_map_status`，并给出 reviewer 可抽查的文档、代码、测试、脚本和 output 路径；缺失时应视为恢复和审查定位缺口。
- Handoff Packet 是否包含 `ilp_lmul_decision`。对 RVV kernel、reduction、staging 或性能候选，reviewer 应检查 LMUL、VLEN gate、accumulator 数、ILP / unroll、寄存器压力和 spill 风险是否有说明；不适用时理由是否成立。
- Handoff Packet 是否包含 `numerical_budget_result`。对 FMA、reduction、浮点阈值、near-cancellation、`ATA/ATb`、matrix 或 checksum 风险，reviewer 应检查参考链路、误差阈值、关键结果和失败样本状态是否闭合。
- Handoff Packet 是否包含 `evidence_doctor_result`。对 benchmark、board summary、checksum、asm attribution 或 EvidenceDecision，reviewer 应检查 Errors / Warnings / Suggestions、未解决 warning、处理动作，以及是否重跑、降级证据边界或修改结论；缺失时应视为证据复核缺口。
- Handoff Packet 是否包含 `asm_attribution`，并说明关键 RVV 指令归属当前 helper、production 符号、bench harness、Eigen/libm、编译器自动向量化或无关代码；仅说“二进制中出现指令”不够。
- Handoff Packet 是否包含 `board_evidence_paths`，并区分 summary artifact、sanitized log 和 raw log；默认 summary-only 策略下 raw logs 不应进入默认提交边界。
- Handoff Packet 是否显式包含 `evidence_decision` 与 `production_decision`。Reviewer 应检查二者是否一致但不混淆：性能或诊断收益成立不自动等于生产接入成立。
- Handoff Packet 是否包含 `validation` 摘要，列出已运行和未运行的 test、bench、反汇编、板卡或 sanitizer；未运行项是否说明原因。
- 如果 worker 发现可沉淀规则、instruction gap（指令缺口）或冗余规则，Handoff Packet 是否包含 `instruction_feedback`；默认配置下该字段只能报告建议，不能代表已修改 agent instructions。
- Handoff Packet 是否包含 `evidence_decision_summary`，并与 production 长期主题文档的“正确性与高效性证据链”或 topic-local “诊断证据链”一致。
- Handoff Packet 是否包含 `loaded_instruction_sources`、`instruction_trace`、`instruction_feedback`、`preferences_loaded`、`work_preferences`、`commit_preferences`、`resolved_artifacts`、`artifact_publication_decision`、`comment_policy_frozen`、`evidence_policy_frozen` 和 `documentation_policy_frozen`，并与 S0 报告、defaults、local override 和当前 prompt 一致。
- 可提交配置是否只包含默认值、占位符和 env var 名；私有 IP、用户名、个人绝对路径和 raw logs 是否仍留在被忽略的 local override、工作区日志或本机环境中。
- Handoff Packet 是否把重要后续选择暴露给用户。若当前结论是窄范围 production-ready、partial-production-candidate、
  bench-only/no-production 或保留重要未覆盖范围，reviewer 应检查 `followup_options_for_user` 是否列出默认动作、
  继续当前 topic 的扩展动作、应另开 topic 的消融 / 扩展动作和当前不建议做的方向。
- 如果 worker 使用短 prompt 启动，Handoff Packet 是否包含 `worker_quality_gate_check`，且该字段真实覆盖标量路径、production/diagnostic 数据流映射、文档结构、测试资产注释、bench 边界、替代方案审计、证据模型和 stop condition。缺失或虚写时，应视为 workflow/worker 执行缺口。
- 如果 worker 继续已有 topic 或声明处于 phase loop，Handoff Packet 是否包含 `phase_loop_state`，并列出当前 phase、phase plan/result 路径、completion matrix、optimization matrix、unblocked next actions、stop condition、continue/stop decision 和 next phase default。
- 如果 worker 声称 `ready_for_review`、`stop_for_review`、`done` 或 `unblocked_next_actions=none`，reviewer 是否额外检查 `ready_for_review_validity_check`；只要 roadmap、matrix、doc-suite quality bar、mature sibling 补充校准、test support shape scan 或 legacy compatibility 仍有 `phase_deferred + unblocked`，该停止状态就应视为过早停止。
- reviewer 必须读取 topic roadmap 的“默认恢复动作”或等价下一步小节。若其中列出的 test source split、
  internal helper layout、doc suite、registry、legacy 清理或其它当前 topic 授权内动作尚未执行，且 worker
  只是写成“若 reviewer 要求继续”或停在 `ready_for_review_validity_check` 检查动作之后，应视为早停。reviewer
  的修正 prompt 应要求 worker 建立 `roadmap_default_recovery_queue` 并执行第一个未阻塞 phase。
- reviewer 必须检查 worker 是否过早停止：当前 phase plan 是否在修改前存在；plan 的每个动作是否在 result 和矩阵中回填；若仍有 `unblocked_next_actions`，worker 是否错误地停在一个 helper、隔离层、target、bench、summary、roadmap-only、evaluation-only、pointer-only 或表格之后；Evidence Doctor Warning / Error 是否被解释、重跑、降级或阻塞。发现早停时，至少列为 `High` finding，并在 `Worker prompt patch` 要求回到第一个 unblocked next action。
- `worker_quality_gate_check` 是否是证据化表格，而不是只有 `true` / `false`。reviewer 应抽查每项 `evidence` 是否能在当前 topic 产物中定位；若找不到对应文件、章节、日志或代码注释，应把该项判为未闭合。
- `worker_quality_gate_check` 是否在适用时覆盖 `experience_migration_audit_ready`、`test_support_shape_scan_ready`、`test_support_split_decision_ready`、`mature_sibling_parity_action_ready` 和 `legacy_compatibility_decision_ready`。若 worker 声称不适用，reviewer 应抽查当前 topic 是否确实没有 sibling topic 经验、长 helper / 长源文件、多职责支撑代码、legacy 入口或成熟 sibling 结构差距。
- `worker_quality_gate_check` 是否覆盖 `preferences_loaded`、`comment_policy_frozen`、`evidence_policy_frozen` 和 `documentation_policy_frozen`。
- `worker_quality_gate_check` 是否覆盖 `doc_suite_quality_bar_ready`；如果本轮涉及 topic-local 文档套件或 reviewer 点名文档对齐，但该项缺失，应视为文档可审查性缺口。
- `worker_quality_gate_check` 是否覆盖 `ready_for_review_validity_check`；如果 worker 仍有 `phase_deferred + unblocked` 却宣布 `ready_for_review`，应把该项判为 fail 并把问题写进 findings。
- `worker_quality_gate_check` 是否覆盖 `correctness_efficiency_evidence_chain_ready`。
- `worker_quality_gate_check` 是否覆盖 `evidence_doctor_result_ready`，并指向 doctor report 或人工 Errors / Warnings / Suggestions 摘要。若本轮涉及性能、checksum 或 asm 证据但该项缺失，应把 EvidenceDecision 判为未闭合。
- `worker_quality_gate_check` 是否覆盖 `dirty_isolation_ready`、`implementation_review_ready`、`candidates_added_or_deferred_ready`、`document_ownership_matrix_ready`、`traceability_map_ready`、`ilp_lmul_decision_ready`、`numerical_budget_result_ready`、`asm_attribution_ready`、`board_evidence_paths_ready`、`evidence_decision_ready`、`production_decision_ready` 和 `validation_summary_ready`。不适用项必须有理由，不能直接省略。
- `language_check` 是否同样有证据支撑。若 worker 声称通过，但诊断 helper、测试、bench 或主题文档仍有非平凡段落缺少中文说明，应指出具体文件和行号。
- 当前结论若为 `partial-production-candidate`、`production-ready` 或其它强于 no-production 的状态，reviewer 必须检查 worker 是否列出 production direct 缺口。诊断路径上的板卡收益不能自动升级成 production-ready。
- 当前结论若为 `production-ready/narrow`，reviewer 必须检查 worker 是否主动指出“窄在哪里、是否存在常见泛型扩展价值、继续扩展需要哪些证据”。如果 worker 只建议进入下一个 topic，却没有给出当前 topic 的重要扩展选项，应视为 handoff 可决策性缺口。

## Instruction feedback（指令反馈）更新判断

reviewer 可以建议修改 agent instructions，但不要把每个 topic 的一次性偏好都上升为通用规则。
默认 feedback mode（反馈模式）是 `report-only`。reviewer 只能建议进入 Workflow improvement mode；除非用户明确授权，不修改 `.agents`、prompt 或 knowledge map。
reviewer closeout（收尾审查）只有在发现 cross-topic（跨主题）规则、instruction gap（指令缺口）、规则冲突或可删除冗余时才输出 `instruction_feedback`；普通 topic-specific（当前主题特有）观察可放入风险或后续动作，不必升级为 instruction feedback 建议。

适合进入 instruction feedback 的经验：

- 跨多个 topic 复用。
- 影响证据边界、权限边界、提交安全或可审查性。
- 能防止 worker 重复犯错。
- 能明确改变读取策略、状态机、文档结构或测试门禁。

不适合立即进入 instruction feedback 的内容：

- 当前 topic 特有参数。
- 单次实验的临时命令。
- 某个 worker 的表达习惯。
- 尚未被 reviewer 复核的猜测。

如果建议更新 agent instructions，应说明：

- 建议改哪个文件。
- 缺口是什么。
- 为什么它是通用规则。
- 是否需要先在下一轮 topic 中验证。
