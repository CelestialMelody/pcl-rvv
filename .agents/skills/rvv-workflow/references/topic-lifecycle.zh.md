# RVV Topic 生命周期

本文定义单个 RVV topic（主题）的推荐状态机。它用于 worker（执行者）规划、reviewer（审查者）检查和未来 `rvv-agent` 的内部任务状态，不要求每个 topic 都机械产出同样数量的文件。

原则：

- S0-S12 是主干状态，不是线性流水账。
- S10 `EvidenceDecision`（证据决策）是分支点。
- 如果证据不支持生产接入，进入诊断 / bench closeout（收尾）并在 S12 结束。
- 如果证据支持生产接入，进入 production integration loop（生产接入闭环），完成生产实现后重新跑必要的测试、反汇编、板卡和证据决策，再做最终文档 closeout。
- Commit phase（提交阶段）是可选独立阶段，不属于 S0-S12 的固定后缀。
- S3-S10 不是一次性瀑布。它们是可重复的优化循环：设计、实现、测试、证据解释、矩阵更新和决策可以在同一 topic 内重复多轮，只要还有授权且未阻塞的下一步动作。

## Phase Groups（阶段分组）

### S0 恢复和偏好冻结

- 读取 `AGENTS.md`、`rvv-workflow` 和 knowledge map（知识索引）。
- 读取 `.agents/config/defaults.yaml`，如果存在 `.agents/local/user-preferences.yaml` 也读取。
- 在 S0 输出 `preferences_loaded`，并记录 defaults、local override（本机私有覆盖）和 prompt override（提示词覆盖）的来源。
- 检查 git status（工作区差异）。
- 冻结本轮工作偏好：注释详细度、注释语言、production（生产源码）注释上限、配置解析出的测试资产 / diagnostic（诊断代码）注释下限。
- 冻结文档偏好：closeout（收尾文档）当前状态优先、数值算例要求、长期文档不保留对话流程话术。
- 冻结提交偏好：默认不提交；如果用户授权提交，再确认 topic、日志和 agent asset（代理资产）是否拆分。
- 冻结 evidence logs（证据日志）策略：默认 `summary-only`，raw logs（原始日志）不默认提交。
- 检查同 topic 是否残留上一轮 worker 产物；若存在且用户未确认复用，先停止。

S0 的字段级合同、artifact layout（产物布局）恢复和 artifact publication（产物发布）判断，见 `rvv-workflow/references/s0-preferences-and-recovery.zh.md`；S0 记录与 Handoff Packet（交接数据包）应使用同一组恢复术语，而不是重新发明另一套状态名。

### S1-S2 目标确认和函数级评估

S1 确认目标源码、公开入口、已有测试、已有文档和同模块 RVV 经验。

S2 建立函数级评估。评估不是只在对话里口头完成；如果 topic 会进入诊断、bench 或实现，应创建或更新函数级评估文档，至少说明：

- 函数入口、调用链和输入输出职责。
- 关键循环、helper（辅助函数）或 solver（求解器）边界。
- 可向量化点、不可向量化点和 RVV 优先级。
- production-candidate（生产候选）、diagnostic（诊断）、bench-only（仅性能诊断）或 no-go（不继续）的初步判断。
- 需要哪些证据才能改变当前判断。

如果目标循环可能被 compiler auto-vectorization（编译器自动向量化）覆盖，或需要判断手写 RVV 是否有必要，S2 可以把 `generate_vec_report` 列为可选诊断。该诊断默认不开启，报告只作为可向量化潜力和编译器限制的辅助输入。

小 topic 可以把 S2 评估写在主题文档的“函数级评估”章节；复杂 topic 建议单独写 evaluation（评估）文档。

### S3-S4 设计和证据计划

S3 形成 RVVPlan（RVV 计划）或 DiagnosticDesign（诊断设计），说明：

- load/store（加载 / 存储）、AoS/SoA（数组结构 / 结构数组）、VL chunk（可变向量长度分块）、mask（掩码）、reduction（规约）。
- fallback（回退路径）、dispatch（分流逻辑）和 `__RVV10__` 关闭时的行为。
- 数值语义、对象状态、Eigen solver 或其它标量尾段边界。

S4 形成测试和证据计划，区分：

- correctness（正确性）证据。
- QEMU path evidence（QEMU 路径证据）。
- disassembly evidence（反汇编证据）。
- board performance（板卡性能证据）。
- negative evidence（负向证据，例如证据显示不值得接入生产）。

如果计划中的结论依赖“编译器不会自动向量化”或“自动向量化不足”，S4 应记录 missed-vectorization report（未自动向量化报告）的生成命令、摘要路径或未使用原因。该报告不替代 correctness、反汇编归因或板卡性能证据。

S3-S4 的输出不是一次性草稿，而是后续每一轮 phase loop 的输入。worker 可以在新的 candidate family、row source policy、点类型、`Scalar`、布局或 Evidence Doctor 结果出现时回到 S3-S4，修订计划和证据矩阵，再继续 S5-S9。

测试类别、row source policy（行来源策略）、production-shaped diagnostic（生产形态诊断）、
production direct（真实生产路径证据）、component ablation（组件消融）和 evidence logs 策略按
`rvv-test` 执行。

S4 如果暴露出可跨 topic 复用的测试矩阵、证据缺口或冗余规则，应在 Handoff Packet（交接数据包）的
`agent_asset_feedback` 中按 `report-only`（只报告建议）记录；没有发现时省略，避免短 prompt（短提示词）输出膨胀。

### S5-S9 产物和验证（可重复执行）

S5 创建或复查 topic scaffold（脚手架），包括配置解析出的测试资产、Makefile、board 配置、评估文档或诊断原型。

S6 根据 S3 决策实现 production RVV、production-shaped diagnostic（生产形态诊断）或 bench-only 原型。证据不足时不要强行修改生产源码。

S7 运行必要测试和 QEMU 验证。QEMU 只能支持正确性、路径命中和日志形状，不支持真实性能结论。

S8 检查反汇编或等效指令路径，确认关键 RVV 指令是否出现，并尽量归属到当前 helper、production 符号、bench harness、库代码或编译器自动向量化区域。若归因存在自动向量化疑点，可以补充 missed-vectorization 摘要；默认 S8 不要求生成该报告。

S9 在板卡或目标硬件上运行必要 smoke（小型验证）、test 和 benchmark（性能测试）。只有板卡或目标硬件性能数据能支撑真实性能结论。

S5-S9 可以随着 phase loop 重复多轮：新增 candidate、补 test、扩 bench、更新板卡证据或重跑反汇编都不意味着 topic 已结束。worker 不应把一个 isolated helper、一个 target、一次 bench 或一张 summary 当作整轮完成。

### S10 EvidenceDecision

S10 汇总 evidence bundle（证据包）并给出明确决策：

- `production-ready`：证据支持进入生产接入闭环。
- `diagnostic`：需要继续诊断，暂不接入 production（生产源码）。
- `bench-only`：作为性能诊断或局部证据保留，不接入生产。
- `rollback/no-production`：现有证据反对生产接入，应回收或避免生产改动。
- `blocked`：缺少工具、板卡、用户判断或必要源码条件。

S10 必须写清“证据证明了什么”和“不能证明什么”。QEMU 或反汇编不能被写成生产性能结论。
进入 closeout 或 production-candidate 后，文档必须包含证据链。`artifact_layout.topic_doc_template`
解析出的 `doc-rvv` 长期主题文档只适用于 adopted production behavior（已采用生产行为）、production patch
（生产补丁）或 PI5 生产证据闭环通过后的主题，并使用“正确性与高效性证据链”。未接 production 的诊断结论
写入 topic-local evaluation / phase closeout 的“诊断证据链”，并说明 diagnostic evidence（诊断证据）不能替代
production evidence（生产证据）。
S10 如果发现 EvidenceDecision（证据决策）依赖了尚未写入 `rvv-test`、`rvv-implementation`
或 `rvv-documentation` 的通用规则，应输出 `agent_asset_feedback`，但默认不修改 agent asset（代理资产）。

S10 是当前 phase 的决策点，不是 topic 的天然终点。若 `current_decision` 之外仍存在授权且未阻塞的下一动作，worker 应把 S10 结果回填到 Handoff Packet，再回到 S3-S9 继续下一 phase，而不是把一次局部 positive / negative 当作最终完成。

## S10 后分支

### Branch A: no-production closeout（不接入生产收尾）

适用于 `diagnostic`、`bench-only`、`rollback/no-production` 或非性能 blocked。

进入 S11 文档 closeout：

- 更新函数级评估文档，记录为什么不接入生产。
- 更新 topic-local phase result、diagnostic 文档或 roadmap / matrix，解释测试、bench、QEMU、反汇编、板卡证据和遗留风险。
- 在 evaluation 或 phase closeout 中新增或更新“诊断证据链”，说明未接 production 的诊断证据边界。
- 不新建 `artifact_layout.topic_doc_template` / `doc-rvv` 长期主题文档；若已有 `doc-rvv` 只是诊断或 no-production 遗留产物，应删除或标为不适用，除非用户明确要求保留历史归档。
- 更新模块队列表和状态表。
- 写清下一轮如果要重新评估，需要补什么证据。

然后进入 S12 done_or_blocked。

### Branch B: production integration loop（生产接入闭环）

适用于 S10 判定 `production-ready`，且用户或工作规则允许生产接入。

如果 S10 / Handoff Packet（交接数据包）判定为 `partial-production-candidate`（局部生产候选），
且用户要求“继续当前 topic”“进入下一阶段”“尝试 production direct（真实生产入口分流）”
或等价目标，worker 可以进入 `PI1 production_integration_plan`。此时只授权做窄范围生产接入计划；
是否进入 `PI2 production_patch` 必须由 PI1 明确证明候选范围、fallback、dispatch、点类型 / Scalar
边界和证据计划可控。不能把 partial candidate 自动升级成 production-ready。

当用户明确授权“进入 / 推进 production integration loop（生产接入闭环）”时，默认目标是同一轮完成
PI1-PI5 加 S11 closeout，而不是每个 PI 阶段结束都等待人工确认。PI1 是生产补丁前的 gate：
如果 PI1 能冻结候选范围、不可扩大范围、fallback 矩阵、生产直连测试和暂停条件，worker 应继续
PI2-PI5；只有命中下方暂停条件，或用户明确要求“只做 PI1 / 只写计划 / 暂不改 production”，才停在 PI1。

生产接入闭环不是简单追加新的 S11/S12/S13，而是一次受控子循环：

1. `PI1 production_integration_plan`（生产接入计划）：把 S3 的设计落实到生产源码边界，确认公开 API、fallback、dispatch、编译宏和维护成本。
2. `PI2 production_patch`（生产补丁）：最小修改生产源码，保留 `__RVV10__` 未启用时的原路径。
3. `PI3 production_direct_tests`（生产直连测试）：补真实入口、真实 fallback 和必要上游测试。
4. `PI4 production_evidence_rerun`（生产证据重跑）：重新执行 S7-S9 中与生产路径相关的测试、QEMU、反汇编和板卡验证。
5. `PI5 production_evidence_decision`（生产证据决策）：再次执行 S10。若证据仍成立，进入 S11；若证据不成立，回退或转入 Branch A。

如果用户授权“继续推进 production integration loop”“连续推进 PI2-PI5”或等价目标，worker 可以在同一轮连续完成 PI2-PI5，
不需要每个 PI 阶段都暂停等待用户确认。连续推进仍必须按顺序产出证据，且不得扩大 PI1 已冻结的候选范围。

连续推进的默认暂停条件：

- 实现需要扩大到 PI1 未授权的入口、点类型、`Scalar`、indices、correspondences 或泛型 normal traits。
- 需要修改 public API（公开接口）、大规模重构 production helper，或引入跨 topic 公共 API 变更。
- fallback gate 无法隔离，导致非覆盖路径可能误命中 RVV。
- production direct test、`Scalar=double` fallback、indices / correspondences fallback 或非 RVV 构建边界无法闭合。
- 反汇编不能证明生产 helper 命中预期 RVV 指令，或关键指令无法归属到 production 符号范围。
- 板卡或目标硬件证据不可用，或 production direct 性能不成立。
- 发现诊断证据与 production direct 证据矛盾，需要 reviewer 或用户判断是否回退。

PI1 若涉及模板点类型、PCL traits（点类型字段特征）、字段 offset、泛型 source / target 组合、`Scalar=double`
或从诊断 `PointNormal` 扩展到 production 模板入口，必须读取并应用：

- `rvv-implementation/SKILL.md`
- `rvv-implementation/references/point-load-store.md`
- `rvv-implementation/references/fallback-and-dispatch.md`
- 配置或 adapter 指定的 generic point type strategy（泛型点类型策略）文档

如果这些 gate 不能闭合，PI1 应停止在计划或窄范围候选，不进入 PI2。

生产接入后，S11 文档 closeout 必须重新检查 `artifact_layout.topic_doc_template` 解析出的主题文档。早期 S2 评估或诊断文档不能直接当作最终生产文档；它们只能作为输入。最终文档必须反映真实生产源码、生产直连测试、fallback、板卡结果和未闭合项。生产接入后的最终文档至少同步：

- 生产补丁范围：新增 / 修改的生产 helper、dispatch、编译宏和未触碰路径。
- 覆盖范围：入口、点类型、`Scalar`、数据布局、规模 gate、目标硬件。
- fallback 矩阵：非 RVV 构建、非覆盖点类型、`Scalar=double`、indices、correspondences、小规模或布局不满足时如何回退。
- 生产直连证据：真实公开入口测试、fallback 测试、反汇编符号归属和板卡 production bench。
- 结论修正：诊断阶段 speedup 与 production direct speedup 是否一致；若不一致，最终文档以生产证据为准。
- 未闭合项：哪些扩展仍不能接入，以及下一轮必须补什么证据。
- 正确性与高效性证据链：public entry（公开入口）真实命中、row semantics（行语义）、`accepted_points`、中间态、matrix（矩阵）、fallback、repeated board（重复板卡测试）、EvidenceDecision 边界和未覆盖范围。

### Branch C: blocked handoff（阻塞交接）

适用于缺少板卡、缺少依赖、队列表冲突、源码事实不清、权限不足或需要用户判断。

进入 S12 时必须写清：

- 已完成到哪个阶段。
- 阻塞条件是什么。
- 缺少的命令、工具、证据或用户判断是什么。
- 下一轮从哪个文件、命令和文档恢复。
- 如果 blocked（阻塞）来自 agent asset 缺口、规则冲突或短 prompt 恢复信息不足，写入 `agent_asset_feedback`。

## S11 文档 Closeout

S11 是最终文档收口，不是所有文档的首次出现。

- S2 文档回答“为什么值得或不值得继续”。
- S11 文档回答“本轮实际证明了什么、接入了什么、没有接入什么、下一步该做什么”。
- closeout 或 production-candidate 文档必须包含证据链：production 长期主题文档使用“正确性与高效性证据链”；未接 production 的诊断结论在 evaluation / phase closeout 中使用“诊断证据链”并标清 production direct 缺口。`doc-rvv` 不适用于无 adopted production behavior 的 no-production closeout。
- S11 closeout 如果沉淀出新的跨 topic 规则、发现旧规则冗余，或发现后续回访文档需要统一整改，按 `agent_asset_feedback` 报告建议；只有用户授权 workflow improvement（工作流改进）时才修改 `.agents`。

如果 topic 进入生产接入闭环，S11 必须发生在 PI5 之后。若 topic 不接入生产，S11 可以直接发生在第一次 S10 之后。

## S12 Done Or Blocked

`done` 可以表示：

- 生产接入成立并完成生产证据闭环。
- bench-only 或 diagnostic 结论成立，且文档和队列表同步完成。
- rollback/no-production 结论成立，且生产改动已回收或未发生。

`blocked` 必须带恢复条件，不能只写“等待板卡”或“需要更多测试”。

## Commit Phase

提交不是生命周期的必经阶段。只有用户明确授权时才进入 Commit phase。

提交前必须确认：

- topic 产物、evidence logs（证据日志）和 agent asset 是否拆分。
- 日志是否需要 sanitize（脱敏）和 check（检查）。
- 是否排除了 build（构建）产物、二进制、私有路径、私有地址、本机 `config.mk` 和聊天记录。

agent asset 改动应单独提交，除非用户明确要求合并。
