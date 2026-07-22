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
artifacts_created_or_updated (产出物，例如优化代码、benchmark（性能测试）日志、测试报告、性能对比数据):
commands_run (本次执行过的编译、测试、benchmark、git 命令，要求能复现):
evidence_paths (证据文件路径，例如编译日志、测试输出、性能数据、反汇编结果):
agent_assets_used (本次读取或调用的 agent 资产，例如 skills（技能）、knowledge map（知识索引）、PCL adapter（PCL 适配器）、规则集):
agent_asset_trace (资产使用追踪，关键工作行为分别来自哪些实际读取并使用过的资产 / 规则):
work_preferences (S0 冻结的工作偏好，例如注释详细度、注释语言、production（生产源码）注释上限、test-rvv / diagnostic 注释下限、是否处于单 topic 校准重跑):
commit_preferences (S0 冻结的提交偏好，例如是否允许 commit（提交）、topic / log / agent asset 是否拆分、evidence log policy（证据日志策略）是 summary-only / sanitized-logs / raw-logs):
language_check (语言规范校验结果，例如术语解释、文档和代码注释是否达标):
worker_quality_gate_check (worker 写文件前质量门禁执行结果，例如标量路径、数据流映射、文档结构、注释策略、bench 边界、证据模型是否闭合):
risks_or_open_questions (遗留风险、未解决疑问):
recommended_reviewer_focus (给 reviewer 的重点检查清单):
followup_options_for_user (给用户的可选后续路径):
next_worker_action_if_review_passes (评审通过后 worker 应执行的下一步):
```

## 字段要求

- `phase_reached` 必须使用当前状态机中的阶段或分支，例如 `S4 test_plan_ready`、`S10 EvidenceDecision`、`PI1 production_integration_plan`、`S12 blocked`。
- `current_decision` 必须是陈述句，不能只写 `done`、`ok` 或 `needs review`。
- `files_changed` 应区分 production（生产源码）、`test-rvv`、`doc-rvv`、agent asset（代理资产）和本地证据文件。
- `artifacts_created_or_updated` 应说明产物作用，不要只列路径。
- `commands_run` 应保留关键参数、工作目录和失败命令；如果没有运行命令，要写明原因。
- `evidence_paths` 只列当前结论真正依赖的证据。大型日志可以列路径和摘要，不要复制长日志。
- `agent_assets_used` 只列实际读取或调用过的资产，不要机械列全量 skill。
- `agent_asset_trace` 必须把行为映射到资产，例如 `reviewability-and-language.zh.md -> TEST 注释和术语解释`。如果某资产只读过但没有影响决策，不要放入 trace。
- `work_preferences` 和 `commit_preferences` 应与 S0 报告一致；若中途改变，写明用户授权或改变原因。
- `language_check` 不允许虚写。若 `test-rvv`、diagnostic（诊断代码）或 prototype（原型代码）没有详细中文注释，必须写成未达标。通过时应列出覆盖面，例如“诊断 helper 注释、TEST 注释、bench 文件头、主题文档术语解释”，并给出文件或章节证据。
- `worker_quality_gate_check` 不允许虚写。必须使用证据化表格，至少覆盖 `worker-quality-gates.zh.md` 中的标量路径、production/diagnostic 数据流映射、文档结构、test-rvv 注释、bench 边界、替代方案审计、证据模型和 stop condition（停止条件）。表格列建议为 `gate | status | evidence | missing_items`；未完成项要列入 `risks_or_open_questions`。
- `worker_quality_gate_check` 中的 `status` 不应只有 `true` / `false`。使用 `pass`、`partial`、`fail` 或 `not_applicable`，并为每项提供文件 / 章节 / 日志路径证据。
- 如果 `current_decision` 是 `partial-production-candidate` 或任何强于 no-production 的结论，`worker_quality_gate_check` 必须额外列出 production direct 尚未闭合项，例如真实公开入口 direct test、fallback、点类型 traits、`Scalar=double`、indices / correspondences 策略、生产 bench 重跑和人工确认点。
- `recommended_reviewer_focus` 应指向具体风险，例如“检查 fallback gate 是否被单独覆盖”，不要写成“请全面审查”。
- `followup_options_for_user` 用于把 reviewer 和用户需要做的人工选择显式暴露出来。当前结论是窄范围 production-ready、partial-production-candidate、bench-only/no-production 但仍存在可复用扩展方向时，必须列出 2-4 个选项：推荐默认动作、继续当前 topic 的扩展动作、应另开 topic 的动作、明确不建议做的动作。每个选项都要写清收益、风险、需要补的证据和是否会扩大 production 范围。
- `next_worker_action_if_review_passes` 是下一轮短 prompt 继续工作的默认恢复入口，应是可执行动作，例如“进入 PI1 生产接入计划”“按已冻结范围连续推进 PI2-PI5”或“只做 S11 文档收尾”，不要写成泛泛的“继续优化”。如果下一步要限制范围，例如只做 full-cloud、保持 indices / correspondences 标量、只修文档不改 production，必须写在该字段中。
- `next_worker_action_if_review_passes` 只能有一个默认动作；如果存在重要替代路径，不要把它们藏在 `risks_or_open_questions` 里，应放进 `followup_options_for_user`。例如窄范围 `PointNormal` 接入完成后，应主动提示是否继续做泛型 normal traits（法线字段特征）扩展、`Scalar=double` 评估、indexed / correspondences 消融，或进入下一个 topic。
- 如果建议下一轮连续推进 PI2-PI5，`next_worker_action_if_review_passes` 必须同时写清候选范围、不可扩大范围和暂停条件摘要；完整细则可指向主题文档 PI1 计划和 `topic-lifecycle.zh.md`。
- 如果本轮已经完成 PI2-PI5，Handoff Packet 必须写清 S11 文档 closeout 是否已同步 production patch、fallback 矩阵、production direct 测试、反汇编归属、板卡 production bench、PI5 EvidenceDecision 和未覆盖路径；不能只说“文档已更新”。

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
- `agent_asset_trace` 是否是真实使用记录。
- `language_check` 是否覆盖文档、代码注释、测试输出和最终回复。
- `worker_quality_gate_check` 是否真实反映写文件前质量门禁，且每项带 reviewer 可定位的证据；如果短 prompt 启动后产物质量下降，应在这里暴露，而不是只写 agent asset trace。
- 是否明确哪些日志、build（构建）产物和本机配置只作为工作区证据，不进入提交。
- 如果 topic 进入 production integration loop，是否写明生产接入计划和需要人工确认的风险。
- 如果 topic 完成 production integration loop，是否写明最终 doc-rvv 文档已经按生产证据重写，而不是沿用诊断阶段结论。
- 窄范围结论或局部候选是否提供 `followup_options_for_user`，让用户能选择“进入下个 topic”还是“继续扩展当前 topic”。
- `next_worker_action_if_review_passes` 是否足够让下一轮 worker 用一句短 prompt 恢复工作；如果缺少范围、权限或停止条件，应在 Handoff Packet 中补齐。
