# 函数级评估与 Closeout 文档分工

本文说明 RVV topic（主题）中 S2 evaluation（函数级评估）和 S11 closeout（收尾文档）的职责边界。

## 为什么要拆开

函数级评估用于回答“是否值得继续做 RVV 诊断或实现”。它应该在进入实现前出现，作为人工判断、reviewer（审查者）检查和后续 agent（代理）恢复工作的依据。

Closeout 用于回答“本轮最终证明了什么”。它应该在测试、QEMU、反汇编、板卡或生产接入证据完成后出现，作为长期维护和提交审查依据。

二者可以写在同一个物理文档中，但章节职责必须分开。复杂 topic 建议拆成 evaluation 文档和适用的 production 长期主题文档；no-production topic 不默认创建 `doc-rvv`。

如果一个 topic 同时存在 production 长期主题文档、evaluation、output summary 和 Handoff Packet，先用文档归属矩阵分清长期事实、候选取舍、bench 统计和恢复动作的主归属，再写具体章节。这样 reviewer 才能从主归属一路追到代码、测试和输出。

## S2 Evaluation（函数级评估）应回答什么

S2 阶段创建或更新 evaluation 文档，至少覆盖：

- 函数入口、公开 API、调用链和输入输出。
- 如果 topic 较复杂，evaluation 还应提供或引用 Traceability Map（可追踪性地图），说明关键函数、测试、脚本、输出和文档章节如何互相定位。
- 目标源码中的关键循环、helper（辅助函数）、normal equation（正规方程）、solver（求解器）或状态机边界。
- 可 RVV 化片段、不可 RVV 化片段和成本归因。
- 数据布局，例如 AoS（结构数组）、SoA（数组结构）、indices（索引）或 gather（离散加载）风险。
- 初步判断：production-candidate（生产候选）、diagnostic（诊断）、bench-only（仅性能诊断）、rollback/no-production（不接入生产）或 blocked（阻塞）。
- 需要哪些 test（测试）、benchmark（性能测试）、QEMU、反汇编或 board（板卡）证据才能改变初步判断。

S2 文档可以包含计划和假设，但必须标清哪些内容尚未由证据证明。

## S11 Closeout（收尾文档）应回答什么

S11 阶段更新 evaluation 文档、topic-local closeout 文档、模块状态表和必要工作日志；只有存在 adopted production behavior、production patch 或 PI5 生产证据闭环通过时，才更新 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档。S11 至少覆盖：

- 本轮最终 EvidenceDecision（证据决策）。
- 复杂 topic 的 Traceability Map 是否仍然可用，能否从最终文档跳到 production 入口、diagnostic / candidate helper、bench wrapper、analysis script 和 output summary。
- 实际创建或修改了哪些 production（生产源码）、diagnostic（诊断代码）、test、bench 或文档。
- correctness（正确性）、QEMU path evidence（QEMU 路径证据）、disassembly evidence（反汇编证据）和 board performance（板卡性能证据）分别证明什么。
- production 长期主题文档是否包含“正确性与高效性证据链”小节；未接 production 的诊断结论是否在 evaluation / phase closeout 中包含对应“诊断证据链”。
- 如果不接入生产，说明原因是收益不足、语义风险、证据不足、维护成本过高，还是工具 / 板卡阻塞。
- 如果接入生产，说明真实生产入口、fallback（回退路径）、dispatch（分流逻辑）、`__RVV10__` 关闭行为、生产直连测试和板卡性能结果。
- 未闭合项必须说明是什么、为什么没闭合、完成后能证明什么、当前是否必须完成。
- 如果结论是窄范围 production-ready、partial-production-candidate、bench-only/no-production 或仍保留重要未覆盖范围，
  closeout 必须给用户可选择的后续路径：默认建议、继续当前 topic 的扩展、应另开 topic 的消融 / 扩展、当前不建议做的方向。
- 长期 evaluation / closeout 文档中的时间表述必须稳定。不要写“最新一次”“最新日志”“截至今日”或单独日期来表示证据新鲜度；改写为“本轮”“当前证据”“历史基线”“rerun 结果”，并用 evidence path 或 run label 溯源。work log、manifest、run id、handoff / recovery path 和用户指定目录名里的日期保留，不作为批量清理对象。

## 生产接入后的文档顺序

如果 S10 判定可以接入生产，不要立即把诊断文档当成最终主题文档。应先完成 production integration loop（生产接入闭环）：

1. 生产接入计划。
2. 生产补丁。
3. 生产直连测试。
4. 生产证据重跑。
5. 再次 EvidenceDecision。

之后再做 S11 closeout。最终 production 长期主题文档必须按 `artifact_layout.topic_doc_template` 解析位置，并反映真实生产源码，而不是只反映 diagnostic prototype（诊断原型）或 bench-only 原型。若 S10 没有进入生产接入闭环，则该模板为 `not_applicable`，closeout 写入 evaluation / phase 文档。

如果用户授权进入生产接入闭环，且没有明确要求“只做 PI1 计划”，worker 默认应在同一轮完成 PI1-PI5
和 S11 closeout。PI1 是继续生产补丁前的范围 gate，不是默认交付终点；只有命中生命周期中的暂停条件，
才把 PI1 作为 handoff（交接）边界。

## 生产接入后文档必须新增什么

生产接入后的 S11 文档不是在原诊断结论后追加一句“已接入 production”。production 长期主题文档和 evaluation 文档
至少要同步以下内容：

- 真实生产补丁范围：改了哪些生产文件、helper、dispatch、编译宏和 `__RVV10__` gate，哪些路径没有改。
- 覆盖范围：入口形态、点类型、`Scalar`、数据布局、规模 gate、目标硬件和已证明收益。
- fallback（回退路径）矩阵：非 RVV 构建、非覆盖点类型、`Scalar=double`、indices、correspondences、小规模、布局或字段 gate 不满足时如何回到标量。
- production direct（真实生产入口直连）证据：真实公开入口测试、fallback 测试、反汇编符号归属和板卡 production bench 的命令与路径。
- 证据更新：诊断阶段 speedup、production direct speedup 和最终 EvidenceDecision 是否一致；不一致时以 production direct 证据为准，并写清原因。
- 未闭合项：哪些扩展仍不能接入，为什么当前不做，下一轮要补什么测试、asm、板卡或泛型点类型证据。
- 后续选择：如果当前只接入窄范围，明确是否建议继续扩大当前 topic。例如 exact `PointNormal` 接入后，
  应说明泛型 normal traits（法线字段特征）扩展是否值得继续、需要读取哪些策略文档、哪些测试和板卡证据必须补齐；
  同时说明 indices / correspondences 或 `Scalar=double` 是否应另开消融或保持标量。
- 正确性与高效性证据链：public entry 是否真实命中；row semantics、`accepted_points`、中间态、matrix 和 fallback 的证据；性能结论是否只来自 repeated board 或目标硬件；EvidenceDecision 是否没有超过证据范围；未覆盖范围和扩展条件。

若生产补丁最终回退，文档也要写成 rollback/no-production closeout：说明回退了哪些生产改动、保留了哪些
diagnostic / bench 资产、为什么生产证据不成立。若回退后没有 adopted production behavior，`doc-rvv`
production 长期主题文档应删除、标为历史归档或判为 `not_applicable`；当前结论主归属回到 evaluation / phase closeout。

## 小 Topic 的合并写法

小 topic 可以只维护一个文档，但建议使用清晰章节：

```text
## S2 函数级评估
## 诊断或实现设计
## 验证结果
## S11 Closeout
```

不要把早期假设和最终结论混在同一段里。若 S2 假设被后续证据推翻，应在 closeout 中明确写出“早期判断如何被证据修正”。

## Reviewer 检查点

reviewer 检查文档时应确认：

- 是否有 S2 评估，而不是只在最终 closeout 才解释函数。
- 文档归属矩阵是否清楚区分 production 长期主题文档、evaluation、topic-local phase / diagnostic docs、output summary 和 Handoff Packet 的职责，是否避免把长期事实、实验结果和恢复动作混写。
- 复杂 topic 是否有 Traceability Map，且表格能把 production、RVV test 资产、analysis script、output summary 和文档章节互相定位。
- S2 评估是否足以让人判断为什么继续或停止。
- S11 closeout 是否覆盖最终证据，而不是重复早期计划。
- production closeout 或 production-candidate 文档是否包含“正确性与高效性证据链”；未接 production 的诊断结论是否在 evaluation / phase closeout 中包含“诊断证据链”，且没有默认发布 `doc-rvv`。
- 生产接入 topic 是否在生产证据重跑后才写最终文档。
- 生产接入后的 production 长期主题文档是否以真实生产补丁和 production direct 证据为中心，而不是继续复述诊断原型。
- fallback、未覆盖入口和未闭合项是否能让下一轮 worker 用短 prompt 恢复。
- closeout 是否给出用户可决策的后续路径，而不是只把重要扩展可能性藏在“风险”里。
- 不接入生产 topic 是否明确记录了恢复条件或下一轮证据需求。
