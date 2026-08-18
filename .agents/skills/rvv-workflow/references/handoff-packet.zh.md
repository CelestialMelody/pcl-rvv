# Handoff Packet（交接数据包）

本文定义 RVV topic（主题）工作中的 Handoff Packet（交接数据包）。它服务 worker（执行者）、reviewer（审查者）和未来 `rvv-agent` 的内部状态恢复。

Handoff Packet 不是普通总结。它必须让 reviewer 不依赖完整对话，也能复核当前阶段、证据、风险和下一步动作。本文只保留交接包的核心合同；详细质量门禁和审查规则通过 reference（参考文件）承载，避免把 Handoff 写成第二份规则书。

## 何时输出

worker 应在这些边界输出 Handoff Packet：

- 完成 S0-S4：目标确认、函数级评估、设计和证据计划已经形成。
- 完成 S5-S8：scaffold（脚手架）、诊断 / 实现、QEMU 和反汇编证据已经形成。
- 完成 S9-S12：板卡验证、EvidenceDecision（证据决策）、文档 closeout（收尾）和 done / blocked 已经形成。
- 进入或准备进入 production integration loop（生产接入闭环）前。
- 遇到 blocked（阻塞）时。

如果 worker 继续向后推进，也必须在最终输出中补齐最近一个 Handoff Packet。不要只在中间阶段输出，最终 closeout 却没有结构化交接。

## 使用原则

- 核心字段默认必须输出；条件字段只在触发条件成立时输出，不适用时写 `not_applicable` 和一句原因。
- 字段内容要可复核：给出路径、命令、阶段名、证据角色或明确的下一步，不用“见上文”替代。
- Handoff 只承载恢复和审查定位，不复制 `worker-quality-gates.zh.md`、`reviewer-protocol.zh.md`、`optimization-phase-loop.zh.md` 中的完整规则。
- 若用户要求保存到当前 Handoff 路径，按 `artifact_layout.current_handoff_template` 和 `artifact_layout.current_handoff_structured_template` 解析；否则可只在最终回复中输出。

## 核心字段

以下字段是多数 Handoff 的最小骨架：

```text
Handoff Packet（交接数据包）
topic:
phase_reached:
current_decision:
files_changed:
dirty_isolation:
artifact_tracking_status:
resolved_artifacts:
commands_run:
validation:
evidence_paths:
evidence_decision_summary:
evidence_decision:
production_decision:
implementation_review:
document_ownership_check:
loaded_instruction_sources:
instruction_trace:
instruction_feedback:
preferences_loaded:
work_preferences:
commit_preferences:
artifact_publication_decision:
language_check:
worker_quality_gate_check:
risks_or_open_questions / remaining_risks:
recommended_reviewer_focus:
followup_options_for_user:
next_worker_action:
```

核心字段要求：

- `phase_reached` 使用状态机阶段或分支，例如 `S4 test_plan_ready`、`S10 EvidenceDecision`、`PI1 production_integration_plan`、`S12 blocked`。
- `current_decision` 写成陈述句，不能只写 `done`、`ok` 或 `needs review`。
- `files_changed` 区分 production（生产源码）、配置解析出的 topic 测试资产、topic 文档、agent instruction patch（agent 指令改动）和本地证据文件。
- `dirty_isolation` 说明当前 worktree 是否有无关 diff（差异），并列出本轮允许 reviewer / commit 关注的路径集合；无关 topic、raw logs、build 输出或用户未授权改动必须标成 `ignore / do not stage / separate commit`。
- `artifact_tracking_status` 列出本轮新增、修改或被 README / evaluation / Handoff 引用的 topic-local docs、phase docs、summary evidence 和长期 `doc-rvv` 是 tracked、to-be-staged、ignored-local 还是 excluded。
- `resolved_artifacts` 列出本轮通过 `artifact_layout` 解析出的关键 worklog、phase、handoff、evaluation、topic doc 或 evidence registry 路径；解析失败时写缺失键，不猜路径。
- `commands_run` 保留关键参数、工作目录和失败命令；没有运行命令时写原因。
- `validation` 用短表或清单列出已运行和未运行的 unit / regression、QEMU correctness、bench build、反汇编、board / target benchmark、sanitizer 或等价检查。
- `evidence_paths` 只列当前结论真正依赖的证据，大型日志只列路径和摘要。
- `evidence_decision_summary` 区分 correctness（正确性）、performance（性能）、boundary（证据边界）和 risk（风险），并说明诊断证据不能替代 production evidence（生产证据）。
- `evidence_decision` 使用明确结论，例如 `production-ready`、`partial-production-candidate`、`bench-only/no-production`、`rollback/no-production`、`blocked`。
- `production_decision` 独立于性能结论写清是否修改 production、是否进入 production integration loop、是否只保留 diagnostic，以及哪些入口 / 点类型 / `Scalar` / row source 保持标量。
- `implementation_review` 说明 public entry / `*_Std` / `*_RVV` 或 diagnostic helper 分层、fallback、gate、公共 API 边界和维护风险；纯文档工作可写 `not_applicable`。
- `document_ownership_check` 说明长期事实、候选取舍、bench 统计、output summary 和恢复动作分别归属到哪些文档或证据路径。
- `loaded_instruction_sources` 只列实际读取并用于本轮决策的 instruction sources（指令来源），不要机械列全量 skill。
- `instruction_trace` 把关键行为映射到 instruction sources；只读过但没有影响决策的来源不放入 trace。
- `instruction_feedback` 只在发现可复用规则、instruction gap（指令缺口）或冗余规则时输出；默认 `report-only`，不代表已经修改 `.agents`。
- `preferences_loaded`、`work_preferences`、`commit_preferences` 和 `artifact_publication_decision` 与 S0 记录一致；涉及板卡、用户名、私有路径时只写 env var（环境变量）名或覆盖范围。
- `language_check` 和 `worker_quality_gate_check` 可以摘要化，但必须有证据路径或 reference，不允许虚写为 `pass` 而不给依据。
- `recommended_reviewer_focus` 指向具体风险；不要写成“请全面审查”。
- `followup_options_for_user` 用于窄范围、局部候选或 no-production 结论后的 2-4 个可选路径。
- `next_worker_action` 只能给一个默认恢复动作；重要替代路径放入 `followup_options_for_user`。

## 条件字段

这些字段按触发条件输出。未触发时可以省略；如果当前结论需要解释为什么不适用，应写 `not_applicable`。

```text
artifacts_created_or_updated:
board_evidence_paths:
asm_attribution:
candidates_added_or_deferred:
traceability_map_status:
optimization_roadmap_status:
mature_sibling_parity_status:
ready_for_review_validity_check:
ilp_lmul_decision:
numerical_budget_result:
evidence_doctor_result:
evidence_freshness_status:
evidence_registry_status:
doc_rvv_freshness_status:
doc_rvv_action:
rerun_budget_decision:
board_availability_continue_status:
phase_loop_state:
experience_migration_audit:
test_support_shape_scan:
target_granularity_audit:
test_support_split_decision:
legacy_compatibility_decision:
```

条件字段触发规则：

- `board_evidence_paths`、`rerun_budget_decision` 和 `board_availability_continue_status`：涉及 board / target hardware（板卡 / 目标硬件）性能、repeated summary、production gate 或需要板卡证据时输出。
- `asm_attribution`：涉及反汇编归属、RVV 指令存在性或自动向量化疑点时输出。
- `evidence_doctor_result`：涉及 benchmark、board summary、checksum summary、asm attribution 或 EvidenceDecision 时输出；没有运行脚本时写人工检查边界。
- `evidence_freshness_status` 和 `evidence_registry_status`：复跑、覆盖 summary、registry 可用或发现未登记证据变化时输出。
- `doc_rvv_freshness_status` 和 `doc_rvv_action`：进入 production integration loop、生产 diff / evidence 变化或提交前输出；说明当前 `doc-rvv` 与 production truth 是否一致，状态使用 `fresh`、`stale_doc_pending_refresh` 或 `not_applicable`，动作使用 `refresh_now`、`keep_as_is`、`remove` 或 `not_applicable`。普通 diagnostic phase 不要求每轮输出该字段。
- `phase_loop_state`：短 prompt 继续已有 topic、复杂 topic 回访或仍有 unblocked next action（未阻塞下一步）时输出。
- `optimization_roadmap_status`：复杂 topic、返工 topic、phase loop 或 roadmap 影响下一步时输出。
- `ready_for_review_validity_check`：worker 声称 `ready_for_review`、`done`、`stop_for_review` 或 `unblocked_next_actions=none` 时输出。
- `mature_sibling_parity_status`、`test_support_shape_scan`、`target_granularity_audit`、`test_support_split_decision` 和 `legacy_compatibility_decision`：恢复旧 topic、长 topic、结构对齐、doc-suite（文档套件）审计、测试工程审计或 legacy 清理时输出。
- `traceability_map_status`：topic 代码、测试、脚本、output 或文档定位复杂，reviewer 需要从文档跳到证据和实现位置时输出。
- `ilp_lmul_decision`：涉及 RVV kernel、reduction（规约）、staging（分阶段暂存）、unroll、LMUL 或寄存器压力时输出。
- `numerical_budget_result`：涉及 FMA（融合乘加）、reduction tree、浮点阈值、near-cancellation（近抵消）、`ATA/ATb`、matrix 或 checksum 风险时输出。
- `candidates_added_or_deferred` 和 `experience_migration_audit`：本轮新增、尝试、暂缓或引用 sibling topic（同模块相邻主题）经验时输出。

## 证据边界

Handoff Packet 中必须区分：

- correctness（正确性）。
- QEMU path evidence（QEMU 路径证据）。
- disassembly evidence（反汇编证据）。
- board performance（板卡性能证据）。
- no-production evidence（不接入生产的负向证据）。

QEMU 和反汇编不能写成真实性能结论。若没有板卡或目标硬件 benchmark（性能测试），`current_decision` 不能写 production performance（生产性能）成立。

## 自查引用

worker 输出 Handoff 前，只在本文做轻量确认；详细检查从对应 reference 读取：

- S0 偏好冻结、`resolved_artifacts` 和 publication class：见 `s0-preferences-and-recovery.zh.md`。
- 写文件前质量门禁和 `worker_quality_gate_check` 表格：见 `worker-quality-gates.zh.md`。
- reviewer 如何判定 blocking issue、早停和证据缺口：见 `reviewer-protocol.zh.md`。
- 多阶段恢复、roadmap、optimization matrix、ready-for-review 合法性和继续 / 停止条件：见 `rvv-test/references/optimization-phase-loop.zh.md`。
- 文档归属、Traceability Map（可追踪性地图）和 doc-suite 质量：见 `rvv-documentation/references/document-ownership-and-traceability.zh.md` 及相关文档 reference。

最终自查只需确认：核心字段完整、触发的条件字段已输出或说明不适用、证据边界没有越界、dirty isolation 清楚、下一步动作可执行。
