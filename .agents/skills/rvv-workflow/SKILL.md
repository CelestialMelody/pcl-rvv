---
name: rvv-workflow
description: 调度 C/C++ RVV 优化 agent 的 human-in-the-loop 工作流。适用于选择下一主题、从筛选队列进入函数级评估、协调 project-config/screening/testing/implementation/documentation skills、中断恢复、closeout 同步和交接 prompt。
---

# RVV Agent 工作流

本 skill 是入口和调度层，不承载所有细则。执行具体任务时按需调用：

- 项目配置：`rvv-project-config`
- 候选筛选：`rvv-screening`
- 测试、诊断、benchmark 和证据层级：`rvv-test`
- 生产实现：`rvv-implementation`
- 文档和 closeout：`rvv-documentation`

如果后续发现新规则，只补对应 skill/reference，不把大段历史材料重新塞回本入口。

短 prompt（提示词）入口见 [references/short-prompt-entry.zh.md](references/short-prompt-entry.zh.md)。
当用户只给角色、工作目录和目标时，优先按该文件的默认读取链启动，不要求用户每轮
重复列出“请先读取哪些文件”。
短 prompt 只简化用户输入，不降低 topic 产物质量。worker 选中 topic 后、开始写文件前，
必须按 [references/worker-quality-gates.zh.md](references/worker-quality-gates.zh.md)
检查标量路径、production/diagnostic 数据流映射、文档结构、测试资产注释、bench 边界、
证据模型和 stop condition（停止条件）；命中复杂 RVV 模式时再读取对应细则。测试、
诊断、benchmark、消融和证据日志规则集中在 `rvv-test`。短 prompt 继续已有 topic 或目标是
“继续完善 RVV 优化工作”时，还必须读取 `rvv-test/references/optimization-phase-loop.zh.md`，
恢复或创建 phase plan（阶段计划）和 optimization matrix（优化矩阵）。

S0 恢复和偏好冻结的合同见 [references/s0-preferences-and-recovery.zh.md](references/s0-preferences-and-recovery.zh.md)。
当工作需要从默认偏好、local override 和 prompt override 里恢复当前轮的有效策略、解析已配置的
artifact layout（产物布局）或判断产物发布边界时，先按该 reference 的字段合同恢复，再进入 topic
的后续阶段。

所有 RVV 工作的回复、文档、测试输出、配置解析出的测试资产和 prototype 注释应遵循 [references/reviewability-and-language.zh.md](references/reviewability-and-language.zh.md)：英文术语首次出现时必须解释；中文主导时给中文解释，英文主导时也要给 plain-English explanation（白话解释），必要时再补中文解释。中文说明要自然，避免翻译腔、名词堆叠和模板填空；长测试/诊断文件提供“本文件做什么”这类阅读提示，非平凡函数用自然句说明作用、调用者和证据角色。

单个 RVV topic（主题）的状态机见 [references/topic-lifecycle.zh.md](references/topic-lifecycle.zh.md)。S0-S12 是主干状态，不是线性流水账；S3-S10 可按 phase loop 反复执行设计、实现、测试、证据解释、矩阵更新和 EvidenceDecision。S10 `EvidenceDecision`（证据决策）之后必须按证据进入 no-production closeout（不接入生产收尾）、production integration loop（生产接入闭环）、下一 phase 或 blocked handoff（阻塞交接）。不要把生产接入简单追加成固定 S13；如果进入生产接入，必须完成生产补丁、生产直连测试、生产证据重跑和再次证据决策后，才进入最终文档 closeout。

worker 到达阶段边界、准备进入生产接入闭环或遇到 blocked（阻塞）时，应按 [references/handoff-packet.zh.md](references/handoff-packet.zh.md) 输出 Handoff Packet（交接数据包）。涉及文档 closeout、evaluation、output summary 或复杂 topic 定位时，Handoff 还要包含 document ownership（文档归属）和 Traceability Map（可追踪性地图）状态。reviewer 审查 worker 产物时，应按 [references/reviewer-protocol.zh.md](references/reviewer-protocol.zh.md) 输出 findings（问题清单）、worker prompt patch（给 worker 的提示词补丁）和 agent asset（代理资产）更新建议。

## 开工前偏好冻结

普通主题在 S0 必须先记录本轮工作偏好，之后再进入实现。至少冻结：

- 偏好来源：先读 `.agents/config/defaults.yaml`，再读可选 `.agents/local/user-preferences.yaml`，最后应用当前 prompt 覆盖。
- `preferences_loaded`：报告 defaults、local override（本机私有覆盖）和 prompt override（提示词覆盖）的读取结果。
- 代码注释策略：不注释、简要注释、详细注释。
- 注释语言策略：仅中文、仅英文、中英双写；中英双写时说明先后顺序。
- production 源码注释上限：默认克制，只解释维护边界、fallback、dispatch、数值风险和数据布局。
- 配置解析出的测试资产、diagnostic、prototype 注释下限：默认详细中文注释，除非用户明确选择更轻量策略。
- 文档策略：closeout 当前状态优先，必须有数值算例，长期文档不保留对话流程话术。
- 提交策略：默认不创建 commit；如果用户授权提交，先冻结是否提交 evidence logs、是否使用已脱敏日志、是否拆分 commit。
- 证据策略：默认 `summary-only`，raw logs 不默认提交。
- agent asset 反馈策略：默认 `report-only`（只报告建议），不自动修改 skill、knowledge map 或 prompt。
- 校准模式：如果用户要求单 topic 反复校准，先确认是否需要清理上一轮 worker 产物；未清理前不要在旧产物上继续扩写。

偏好冻结不是长篇计划。它应以几行清单出现在 S0 报告和最终 handoff packet 中，便于 reviewer 判断 worker 是否按本轮约束执行。

## 提交策略

只有用户明确进入 commit phase（提交阶段）时才创建 commit。提交前必须记录 `commit_preferences`：

- `topic-only`：只提交 topic 源码、测试、bench、文档和队列表；不提交 evidence logs。默认选项。
- `topic-plus-sanitized-logs`：提交 topic，并把脱敏后的 evidence logs 作为单独 commit。用户说要提交 log 时默认采用这个策略。
- `topic-plus-raw-logs`：提交 topic，并把 raw evidence logs（原始证据日志）作为单独 commit；只在用户明确要求保留原文、脱敏日志不足以复核、且 reviewer 已确认没有凭据或私有地址风险时使用。
- `split-topic-logs-agent-assets`：topic、evidence logs、agent asset 改动分拆成多个 commit。适合 workflow 校准和证据归档同时发生的任务。

提交 evidence logs 前必须优先运行 topic 目录提供的 `make sanitize_output_logs` 和 `make check_output_logs_sanitized`，或直接运行 `artifact_layout.sanitize_logs_script_template` 解析出的脚本并传入 `--check <logs>`；如果 topic 未接入公共 Makefile，再说明等效检查方式。提交前列出将加入的文件、排除的文件、是否仍包含本机路径 / 远端路径 / 用户名 / 私有地址，以及脱敏是否改变 benchmark（性能测试）数值、checksum（校验和）或命令参数。不要把 `build/` 二进制、临时编译日志、`config.mk`、私有地址或聊天记录混入 topic commit。agent asset 改动应单独提交，不和 topic 内容混在同一 commit，除非用户明确要求。

## 普通主题入口

从模块 second-pass 或 follow-up 状态表按推荐顺序选择第一条未完成主题。不要重新做模块级候选选择，除非文档与当前源码存在明确冲突。

主题入口短模板见 [references/topic-entry-template.md](references/topic-entry-template.md)。

## 状态机

简要状态：

1. S0 恢复上下文和冻结偏好。
2. S1-S2 确认目标并建立函数级评估；需要继续的 topic 应创建或更新 evaluation（评估）文档。
3. S3-S4 形成 RVV / 诊断设计和证据计划；多阶段 topic 每个 phase 都要先有 plan。
4. S5-S9 创建 scaffold（脚手架）、实现诊断或生产候选、执行 QEMU、反汇编和板卡验证；这些步骤可随 phase loop 重复。
5. S10 做 EvidenceDecision，并更新 phase result、optimization matrix 和继续 / 停止决定。
6. S10 后按证据分支：不接入生产则进入 S11 closeout；接入生产则进入 production integration loop；仍有 unblocked next action 则回到下一 phase；阻塞则进入 S12。
7. S11 同步评估文档、主题文档、模块工作日志和状态表。
8. S12 输出 done_or_blocked。

函数级评估是生产接入门禁。建议队列表示优先评估，不表示跳过检验直接改生产路径。

## 资产使用追踪

最终 Handoff Packet 应包含 `agent_asset_trace`，用短清单把关键决策映射到实际读取过的 agent 资产或规则。例如：

- `rvv-workflow/references/reviewability-and-language.zh.md` -> `TEST` 说明、术语解释和注释策略。
- `rvv-test` -> test taxonomy、diagnostic policy、数值一致性、bench / ablation 和 evidence logs。
- `pcl-rvv-knowledge-map.md` -> 读取范围控制。

只写真正影响了本轮行为的资产；不要把未读取或未使用的 skill 机械列入 trace。

## 中断恢复

如果任务被打断，先恢复状态，再继续实施。细则见 [references/recovery-and-handoff.md](references/recovery-and-handoff.md)。

## 背景

PCL 是当前验证项目，长期目标是形成可扩展到不同 C/C++ library 的 RVV 优化 agent。背景摘要见 [references/background-pcl.md](references/background-pcl.md)。

## 提交边界

本 skill 不自动提交。需要提交时按职责拆分：

- 生产实现。
- 专项 test/bench 与函数级评估。
- evidence logs。
- 主题文档与状态表。
- 通用经验文档。
- workflow / skill 规则。

不要默认加入忽略的本地迁移材料，除非用户明确要求。
