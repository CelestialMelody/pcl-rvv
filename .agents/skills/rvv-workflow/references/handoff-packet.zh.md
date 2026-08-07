# Handoff Packet（交接数据包）

本文定义 RVV topic（主题）工作中的 Handoff Packet（交接数据包）。它服务 worker（执行者）、reviewer（审查者）和未来 `rvv-agent` 的内部状态恢复。

Handoff Packet 不是普通总结。它必须让 reviewer 不依赖完整对话，也能复核当前阶段、证据、风险和下一步动作。

## 何时输出

worker 应在这些边界输出 Handoff Packet：

- 完成 S0-S4：目标确认、函数级评估、设计和证据计划已经形成。
- 完成 S5-S8：scaffold（脚手架）、诊断 / 实现、QEMU 和反汇编证据已经形成。
- 完成 S9-S12：板卡验证、EvidenceDecision（证据决策）、文档 closeout（收尾）和 done / blocked 已经形成。
- 进入或准备进入 production integration loop（生产接入闭环）前。
- 遇到 blocked（阻塞）时。

如果 worker 继续向后推进，也必须在最终输出中补齐最近一个 Handoff Packet。不要只在中间阶段输出，最终 closeout 却没有结构化交接。

## 字段结构

```text
Handoff Packet（交接数据包）
topic (当前所属任务主题):
phase_reached (当前抵达的工作阶段):
current_decision (Worker 当前结论，例如继续优化、回退、不接入生产、需要人工介入):
files_changed (本次操作改动的源码、配置文件、文档清单):
dirty_isolation (工作区隔离；说明本轮 diff 与其它 topic / agent asset / raw logs 的边界):
artifacts_created_or_updated (产出物，例如优化代码、benchmark（性能测试）日志、测试报告、性能对比数据):
commands_run (本次执行过的编译、测试、benchmark、git 命令，要求能复现):
validation (验证摘要；列出测试、bench、反汇编、板卡和未运行项):
evidence_paths (证据文件路径，例如编译日志、测试输出、性能数据、反汇编结果):
board_evidence_paths (板卡或目标硬件证据路径；区分 summary、sanitized logs 和 raw logs):
asm_attribution (反汇编归属；说明关键指令是否归属当前 helper / production 符号 / bench harness):
evidence_decision_summary (EvidenceDecision 与证据链摘要，说明 correctness、performance、boundary 和 risk):
evidence_decision (本轮 EvidenceDecision，例如 production-ready、partial-production-candidate、bench-only/no-production、blocked):
production_decision (生产接入判断；说明是否修改 production、是否进入 / 暂缓 production integration loop，以及原因):
implementation_review (实现自审；说明入口分层、fallback、helper 边界、维护风险和本轮是否只限 diagnostic):
candidates_added_or_deferred (候选实现或诊断路线；列出新增、尝试、暂缓、拒绝的候选及理由):
document_ownership_check (文档归属检查；说明长期事实、候选取舍、bench 统计、output summary 和恢复动作分别写到哪里):
traceability_map_status (可追踪性地图状态；required / updated / not_required / deferred，并列出 map 位置或暂缓理由):
ilp_lmul_decision (ILP / LMUL 取舍；说明寄存器压力、accumulator 数、VL/LMUL、unroll 或暂不适用原因):
numerical_budget_result (数值预算结果；说明 FMA、reduction tree、误差阈值、near-cancellation 和矩阵 / checksum 结果):
agent_assets_used (本次读取或调用的 agent 资产，例如 skills（技能）、knowledge map（知识索引）、PCL adapter（PCL 适配器）、规则集):
agent_asset_trace (资产使用追踪，关键工作行为分别来自哪些实际读取并使用过的资产 / 规则):
agent_asset_feedback (可选；本轮发现的可沉淀规则、资产缺口或冗余规则，默认 report-only):
preferences_loaded (S0 读取的偏好层级，例如 defaults、local override、prompt override，以及是否只报告 env var 名):
work_preferences (S0 冻结的工作偏好，例如注释详细度、注释语言、production（生产源码）注释上限、测试资产 / diagnostic 注释下限、是否处于单 topic 校准重跑):
commit_preferences (S0 冻结的提交偏好，例如是否允许 commit（提交）、topic / log / agent asset 是否拆分、evidence log policy（证据日志策略）是 summary-only / sanitized-logs / raw-logs):
experience_migration_audit (可选；声明采用 sibling topic 经验时，列出 adopted / attempted / deferred / rejected 对照表):
test_support_split_decision (可选；长 helper 或多职责 helper 是否已按 test_support 配置拆分，或暂缓理由):
language_check (语言规范校验结果，例如术语解释、文档和代码注释是否达标):
worker_quality_gate_check (worker 写文件前质量门禁执行结果，例如标量路径、数据流映射、文档结构、注释策略、bench 边界、证据模型是否闭合):
risks_or_open_questions / remaining_risks (遗留风险、未解决疑问):
recommended_reviewer_focus (给 reviewer 的重点检查清单):
followup_options_for_user (给用户的可选后续路径):
next_worker_action_if_review_passes / next_worker_action (评审通过后 worker 应执行的下一步):
```

## 字段要求

- `phase_reached` 必须使用当前状态机中的阶段或分支，例如 `S4 test_plan_ready`、`S10 EvidenceDecision`、`PI1 production_integration_plan`、`S12 blocked`。
- `current_decision` 必须是陈述句，不能只写 `done`、`ok` 或 `needs review`。
- `files_changed` 应区分 production（生产源码）、配置解析出的 topic 测试资产、topic 文档、agent asset（代理资产）和本地证据文件。
- `dirty_isolation` 必须说明当前 worktree 是否含有与本 topic 无关的 diff（差异），并列出本轮允许 reviewer / commit 关注的路径集合。若存在其它 topic、agent asset、raw logs、build 输出或用户未授权改动，必须写成“ignore / do not stage / separate commit”等明确边界。
- `artifacts_created_or_updated` 应说明产物作用，不要只列路径。
- `commands_run` 应保留关键参数、工作目录和失败命令；如果没有运行命令，要写明原因。
- `validation` 应用短表或清单列出本轮实际运行和未运行的验证：unit / regression、QEMU correctness、bench compare、反汇编、board / target benchmark、sanitizer 或等价检查。未运行项必须写明原因，不能只省略。
- `evidence_paths` 只列当前结论真正依赖的证据。大型日志可以列路径和摘要，不要复制长日志。
- `board_evidence_paths` 只列目标硬件证据，并明确每个路径是 summary artifact（摘要证据）、sanitized log（脱敏日志）还是 raw log（原始日志）。默认 `summary-only` 时，raw log 只能作为本机证据，不进入默认提交边界。
- `asm_attribution` 必须说明关键 RVV 指令或缺失证据归属到当前 helper、production 符号、bench harness、Eigen/libm、编译器自动向量化或无关代码。归属不清时写“指令存在但热点归属未闭合”。如果使用 `generate_vec_report` 辅助解释自动向量化疑点，Handoff 只记录命令、摘要路径和它不能替代 objdump 归因的边界。
- `evidence_decision_summary` 必须对应主题文档的“正确性与高效性证据链”或未接 production 诊断结论的“诊断证据链”。摘要至少写清 public entry 是否真实命中、row semantics、`accepted_points` / 中间态 / matrix / fallback 证据、repeated board 或目标硬件性能来源、EvidenceDecision 边界和未覆盖风险。
- `evidence_decision` 应是 S10 / PI5 的明确枚举或陈述，例如 `production-ready`、`partial-production-candidate`、`bench-only/no-production`、`rollback/no-production`、`blocked`。它可以和 `current_decision` 内容一致，但不能只隐含在长摘要里。
- `production_decision` 必须独立于性能结论写清是否修改 production（生产源码）、是否进入 production integration loop（生产接入闭环）、是否只保留 diagnostic，以及哪些入口 / 点类型 / `Scalar` / row source 仍保持标量。诊断板卡收益不能自动写成 production-ready。
- `implementation_review` 适用于任何实现或诊断 helper 改动。它至少说明 public entry / `*_Std` / `*_RVV` 或 diagnostic helper 分层、fallback 与 gate、是否新增 public API、是否复用公共 load/store / traits / policy、维护风险，以及 reviewer 应重点看哪些实现边界。
- `candidates_added_or_deferred` 应列出本轮新增、尝试、暂缓或拒绝的候选路线。可复用 `adopted`、`attempted`、`deferred`、`rejected`、`not_applicable` 状态；每项必须写理由、证据或下一轮恢复条件。
- `document_ownership_check` 应按文档归属矩阵说明本轮长期事实、候选取舍、bench 统计、output summary、恢复动作和通用 asset feedback 分别写到哪里。若只是引用其它文档，必须给出 path、anchor（章节 / 符号 / run label）或 role（证据角色）。
- `traceability_map_status` 应说明复杂 topic 的 Traceability Map 是 `required`、`updated`、`not_required` 还是 `deferred`。`updated` 时列出章节或独立文档；`not_required` 时说明 topic 为什么简单；`deferred` 时说明缺少哪些代码、测试、脚本或 output 路径。
- `ilp_lmul_decision` 适用于含 RVV kernel、reduction、staging 或性能候选的 topic。必须说明 LMUL（向量寄存器分组）、VLEN gate、accumulator 数、unroll / ILP（指令级并行）、寄存器压力或 spill 风险；若不适用，写清为什么当前工作没有新的 ILP / LMUL 决策。
- `numerical_budget_result` 适用于手写浮点、FMA、reduction、近抵消、阈值谓词、`ATA/ATb`、matrix 或 checksum 证据。它必须写清参考链路、误差阈值、最大 / 关键误差或 checksum 结果、失败样本状态和反汇编 / FMA 归属。若只做文档或整数路径，可写 `not_applicable` 并说明原因。
- `agent_assets_used` 只列实际读取或调用过的资产，不要机械列全量 skill。
- `agent_asset_trace` 必须把行为映射到资产，例如 `reviewability-and-language.zh.md -> TEST 注释和术语解释`。如果某资产只读过但没有影响决策，不要放入 trace。
- `agent_asset_feedback` 只在发现可复用规则、资产缺口或冗余规则时输出。默认模式是 `report-only`：只写建议，不修改 `.agents`、prompt 或 knowledge map。该字段至少说明发现项、建议更新的 skill/reference、适用范围是 topic-specific 还是 cross-topic（跨主题），以及是否建议进入 workflow improvement（工作流改进）。
- `preferences_loaded` 必须写清 `.agents/config/defaults.yaml` 是否读取、`.agents/local/user-preferences.yaml` 是否存在、当前 prompt 是否覆盖配置。涉及板卡、用户名、私有路径时，只写 env var（环境变量）名或 local override 覆盖范围，不写实际值。
- `work_preferences` 和 `commit_preferences` 应与 S0 报告一致；若中途改变，写明用户授权或改变原因。`work_preferences` 至少覆盖 `comment_policy_frozen` 和 `documentation_policy_frozen`；`commit_preferences` 至少覆盖 `evidence_policy_frozen`。
- `experience_migration_audit` 在 worker 声明采用 sibling topic（同模块相邻主题）经验时必须输出。它至少覆盖 row source、source / weight policy、shared math pipeline、staging / reduction、formula / FMA、evidence model 和 production boundary，并用 `adopted`、`attempted`、`deferred` 或 `rejected` 说明每个历史经验维度的处理结果。该字段不要求当前 topic 实现 sibling 的具体算法，但要求未采用的成功或负向方案有理由或下一轮验证计划。
- `test_support_split_decision` 在单个测试支撑 helper header 超过配置阈值，或混合 reference、row source、RVV math、reduction candidate、bench wrapper、component ablation 中三类以上职责时必须输出。若已拆分，说明 aggregator（聚合头文件）和按 `test_support` 配置解析出的内部结构职责；若暂缓，说明 deferred reason 以及对 reviewer 可读性和后续维护的影响。
- `language_check` 不允许虚写。若配置解析出的测试资产、diagnostic（诊断代码）或 prototype（原型代码）没有详细中文注释，必须写成未达标。通过时应列出覆盖面，例如“诊断 helper 注释、TEST 注释、bench 文件头、主题文档术语解释”，并给出文件或章节证据。该字段还必须说明是否执行 `writing_style_trigger_check`，以及它覆盖了文档、Handoff Packet、最终回复、reviewer 报告或 `agent_asset_feedback` 中的哪些文本。
- `worker_quality_gate_check` 不允许虚写。必须使用证据化表格，至少覆盖 `worker-quality-gates.zh.md` 中的 `preferences_loaded`、`comment_policy_frozen`、`evidence_policy_frozen`、`documentation_policy_frozen`、标量路径、production/diagnostic 数据流映射、文档结构、测试资产注释、bench 边界、替代方案审计、证据模型和 stop condition（停止条件）。表格列建议为 `gate | status | evidence | missing_items`；未完成项要列入 `risks_or_open_questions`。
- `worker_quality_gate_check` 中的 `status` 不应只有 `true` / `false`。使用 `pass`、`partial`、`fail` 或 `not_applicable`，并为每项提供文件 / 章节 / 日志路径证据。
- 如果 `current_decision` 是 `partial-production-candidate` 或任何强于 no-production 的结论，`worker_quality_gate_check` 必须额外列出 production direct 尚未闭合项，例如真实公开入口 direct test、fallback、点类型 traits、`Scalar=double`、indices / correspondences 策略、生产 bench 重跑和人工确认点。
- `recommended_reviewer_focus` 应指向具体风险，例如“检查 fallback gate 是否被单独覆盖”，不要写成“请全面审查”。
- `followup_options_for_user` 用于把 reviewer 和用户需要做的人工选择显式暴露出来。当前结论是窄范围 production-ready、partial-production-candidate、bench-only/no-production 但仍存在可复用扩展方向时，必须列出 2-4 个选项：推荐默认动作、继续当前 topic 的扩展动作、应另开 topic 的动作、明确不建议做的动作。每个选项都要写清收益、风险、需要补的证据和是否会扩大 production 范围。
- `next_worker_action_if_review_passes` 是下一轮短 prompt 继续工作的默认恢复入口，应是可执行动作，例如“进入 PI1 生产接入计划”“按已冻结范围连续推进 PI2-PI5”或“只做 S11 文档收尾”，不要写成泛泛的“继续优化”。如果下一步要限制范围，例如只做 full-cloud、保持 indices / correspondences 标量、只修文档不改 production，必须写在该字段中。
- `next_worker_action_if_review_passes` 只能有一个默认动作；如果存在重要替代路径，不要把它们藏在 `risks_or_open_questions` 里，应放进 `followup_options_for_user`。例如窄范围 `PointNormal` 接入完成后，应主动提示是否继续做泛型 normal traits（法线字段特征）扩展、`Scalar=double` 评估、indexed / correspondences 消融，或进入下一个 topic。
- 如果建议下一轮连续推进 PI2-PI5，`next_worker_action_if_review_passes` 必须同时写清候选范围、不可扩大范围和暂停条件摘要；完整细则可指向主题文档 PI1 计划和 `topic-lifecycle.zh.md`。
- 如果本轮已经完成 PI2-PI5，Handoff Packet 必须写清 S11 文档 closeout 是否已同步 production patch、fallback 矩阵、production direct 测试、反汇编归属、板卡 production bench、PI5 EvidenceDecision 和未覆盖路径；不能只说“文档已更新”。
- Handoff Packet 不能只列 `commands_run`。EvidenceDecision 必须有证据链摘要；diagnostic evidence、QEMU timing、representative pointtypes、indexed / correspondences 边界、dirty isolation、implementation review、numerical budget、asm attribution 和 board evidence boundary 不能被省略。

## 证据边界

Handoff Packet 中必须区分：

- correctness（正确性）。
- QEMU path evidence（QEMU 路径证据）。
- disassembly evidence（反汇编证据）。
- board performance（板卡性能证据）。
- no-production evidence（不接入生产的负向证据）。

QEMU 和反汇编不能写成真实性能结论。若没有板卡或目标硬件 benchmark（性能测试），`current_decision` 不能写 production performance（生产性能）成立。

## 交给 Reviewer 前的自查

worker 输出 Handoff Packet 前应检查：

- 字段是否完整，没有用“见上文”替代关键内容。
- 是否列出了能复现当前结论的命令和证据路径。
- 是否输出 `dirty_isolation`，并把本轮可审查 / 可提交路径与其它脏 diff 分开。
- 是否输出 `implementation_review`、`candidates_added_or_deferred`、`document_ownership_check`、`traceability_map_status`、`ilp_lmul_decision`、`numerical_budget_result`、`asm_attribution`、`board_evidence_paths`、`evidence_decision`、`production_decision` 和 `validation`；不适用项是否写明原因。
- `agent_asset_trace` 是否是真实使用记录。
- 如果本轮发现可沉淀规则、资产缺口或冗余规则，是否按 `agent_asset_feedback` 报告；没有发现时可以省略该字段。
- 如果声明采用 sibling topic 经验，是否输出 `experience_migration_audit`，且没有遗漏相邻成功或负向方案中的主要维度。
- 如果长 helper 或多职责 helper 命中拆分阈值，是否输出 `test_support_split_decision`，并说明拆分或暂缓理由。
- `language_check` 是否覆盖文档、代码注释、测试输出、Handoff Packet 和最终回复；是否按 `writing-style.md` 执行触发词检查，并说明命中项、改写结果或保留理由。
- `worker_quality_gate_check` 是否真实反映写文件前质量门禁，且每项带 reviewer 可定位的证据；如果短 prompt 启动后产物质量下降，应在这里暴露，而不是只写 agent asset trace。
- 是否明确哪些日志、build（构建）产物和本机配置只作为工作区证据，不进入提交。
- 如果 topic 进入 production integration loop，是否写明生产接入计划和需要人工确认的风险。
- 如果 topic 完成 production integration loop，是否写明最终主题文档已经按生产证据重写，而不是沿用诊断阶段结论。
- 窄范围结论或局部候选是否提供 `followup_options_for_user`，让用户能选择“进入下个 topic”还是“继续扩展当前 topic”。
- `next_worker_action_if_review_passes` / `next_worker_action` 是否足够让下一轮 worker 用一句短 prompt 恢复工作；如果缺少范围、权限或停止条件，应在 Handoff Packet 中补齐。
- `preferences_loaded` 和三个冻结策略是否能让下一轮 worker 复用同一注释、文档和证据策略。
