---
name: rvv-documentation
description: 编写、重排或审查 C/C++ RVV 优化文档。适用于主题 RVV 文档、函数级评估、模块筛选报告、closeout 整理、bench case 解释、staging 表、测试保留策略、生产接入判断和跨文档同步。
---

# RVV 文档工作流

使用本 skill 时，目标是产出离开当前对话后仍可维护的技术文档。读者应能仅凭源码、测试、bench 日志和文档恢复函数职责、标量路径、RVV 边界、证据链、生产接入判断和遗留风险。

术语解释和可审查性规则见 `rvv-workflow/references/reviewability-and-language.zh.md`。文档中英文术语首次出现时必须解释；中文主导文档给中文解释，英文主导文档也要给 plain-English explanation（白话解释），必要时补中文解释。中文文档应使用自然工程说明，避免翻译腔、名词堆叠和模板填空。如果文档引用配置解析出的测试资产、prototype、诊断入口或 bench case，应解释其证据角色和不能覆盖的边界。

## 文档类型

- 主题 RVV 文档：见 [references/topic-doc-structure.md](references/topic-doc-structure.md)。
- 函数级评估文档：见 [references/evaluation-doc-structure.md](references/evaluation-doc-structure.md)。
- 函数级评估与 closeout 分工：见 [references/function-evaluation-and-closeout.zh.md](references/function-evaluation-and-closeout.zh.md)。
- 文档归属矩阵与 Traceability Map（可追踪性地图）：见 [references/document-ownership-and-traceability.zh.md](references/document-ownership-and-traceability.zh.md)。
- 主题本地文档套件质量门槛与模板：见 [references/doc-suite-quality-bar.zh.md](references/doc-suite-quality-bar.zh.md)。
- 筛选文档：见 [references/screening-docs.md](references/screening-docs.md)。
- 诊断和回退文档：见 [references/diagnostic-docs.md](references/diagnostic-docs.md)。
- closeout 重排和写作风格：见 [references/closeout-style.md](references/closeout-style.md)。
- 通用写作风格：见 [references/writing-style.md](references/writing-style.md)。
- 当前 PCL 项目满意样例抽出的质量门槛：见 [references/examples-quality-bar.md](references/examples-quality-bar.md)。

## 通用规则

- 文档使用仓库相对路径和占位符，不写个人路径、私有地址或设备内部绝对路径。
- QEMU 只写成正确性、日志格式和路径证据；性能结论来自板卡或目标硬件。
- 生产接入判断必须连接 local fragment、full diagnostic、production case、fallback 和维护成本。
- 评估文档负责决策审计；`doc-rvv` 主题文档负责 production 长期维护，只有存在 adopted production behavior（已采用生产行为）、production patch（生产补丁）或 PI5 生产证据闭环通过后才适用。
- 文档归属矩阵负责分清长期事实、候选取舍、bench 统计、output summary（输出摘要）和 Handoff Packet（交接数据包）的主归属；其它位置只引用路径、章节、run label（运行标签）或证据角色，不复制长段正文或 raw log（原始日志）。
- 如果文档引用 `artifact_layout.qemu_output_subdir` 或 `artifact_layout.board_output_subdir` 解析目录下的可提交证据，应写明确文件路径、run label 或 summary artifact 路径；这些引用是后续提交日志 / 摘要文件的白名单来源。
- 复杂 topic 必须在 evaluation、topic-local 文档或适用的 production 长期主题文档中维护 Traceability Map，列出关键 production、RVV test 资产、script、output 和文档位置；默认不新建巨型函数文档，除非 map 已经大到影响主文档可读性。
- 多阶段优化 topic 应在配置解析出的 topic test `doc/optimization-roadmap.zh.md` 或等价位置维护主题级优化路线图。路线图只记录候选家族、搜索空间、阶段反思、新增想法、优先级和恢复条件；不要把它写成阶段流水，也不要替代 evaluation 的取舍审计。
- 复杂 topic 的 README、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map、phase index 和 evaluation 应优先按 `doc-suite-quality-bar.zh.md` 审计。成熟 sibling topic（同模块相邻主题）只作为 optional calibration（可选校准样例），不得成为必须点名的规范源，也不得复制其 topic-specific 算法、数值、phase 名或结论。
- S2 函数级评估阶段就应创建或更新 evaluation（评估）文档，用来记录函数功能、可向量化点、RVV 优先级、初步接入判断和需要补齐的证据。不要把这些判断只留到 S11 closeout（收尾）阶段。
- S11 closeout 文档负责记录实验后的最终状态、证据边界、生产接入或不接入理由、遗留风险和队列表同步。如果 topic 进入 production integration loop（生产接入闭环），S11 必须发生在生产补丁、生产直连测试、生产证据重跑和再次 EvidenceDecision（证据决策）之后。
- `diagnostic`、`bench-only`、`rollback/no-production` 或未接 production 的 `partial-production-candidate` 不默认创建 `artifact_layout.topic_doc_template` / `doc-rvv`。这些结论的“诊断证据链”主归属是 topic-local evaluation、phase result、roadmap / matrix 和 Handoff；若已有 `doc-rvv` 仅承载诊断或 no-production 结论，应删除或标为 not_applicable，除非用户明确要求保留历史归档。
- 生产接入后的主题文档必须按 `artifact_layout.topic_doc_template` 解析位置，并以真实 production patch（生产补丁）和 production direct（真实生产入口直连）证据为中心，不能只复述 diagnostic prototype（诊断原型）或早期 bench 结果。必须同步覆盖范围、fallback 矩阵、生产直连测试、反汇编归属、板卡 production bench、最终 EvidenceDecision 和未覆盖路径。
- 筛选文档负责队列和状态，不承担实现事实的长期解释。
- 诊断文档必须区分授权边界：局部实验、production-shaped diagnostic、production direct 或生产回退。
- 技术结论以源码、测试、日志和反汇编为依据，不写成对话来源。
- 如果规则来自一次讨论或复盘，技术文档写技术事实；规则来源写入 workflow 或问题讨论记录。
- 未闭合项、剩余风险和 production gate 不能只列术语；必须用陈述句说明每项是什么、为什么未闭合、完成后能证明什么或降低什么风险、当前阶段是否必须完成。具体写法见 `rvv-workflow/references/reviewability-and-language.zh.md`。
- 长期技术文档和 evaluation / closeout 文档不要把证据写成“最新一次”“最新日志”“截至今日”或孤立日期。优先写“本轮”“当前证据”“历史基线”“rerun 结果”，并用 evidence path（证据路径）或 run label（运行标签）承载溯源。日期只放在 work log、manifest、run id、handoff / recovery path（交接 / 恢复路径）和用户指定目录名里；术语、checksum（校验和）、常量和 run id 不因命中日期样式而改写。

## 术语

推荐使用：

- `local fragment`
- `production-shaped diagnostic`
- `production direct`
- `bench 诊断主题`
- `上游生产分流`
- `回退标量路径`

旧表述只作为迁移清理对象，不作为新文档术语。

## 同步边界

production 长期主题文档修改后，如果影响生产接入结论、bench case 含义、新增诊断失败、workflow 规则或筛选状态，应同步函数级评估、模块工作日志和相关筛选状态表。no-production closeout 修改只同步 topic-local evaluation / phase 文档和状态表，不因此新建 `doc-rvv`。

进入 production integration loop、准备最终 closeout 或准备提交前，若当前 topic 已有 `doc-rvv`，worker 必须先用当前 production diff、phase result 和 Evidence Doctor 做 freshness check；如果长期文档与当前 truth 不一致，先标记 `stale_doc_pending_refresh` 或刷新后再继续后续闭环。普通 diagnostic phase 不要求每轮都做完整 `doc-rvv` 比对。

如果一次主题工作暴露出可复用文档规则，应沉淀到本 skill 或 reference，避免规则只停留在一次主题文档中。

## 质量门槛

新文档至少应回答：

- 入口是什么，调用链如何进入目标函数。
- 复杂 topic 的读者能否通过 Traceability Map 从文档跳到 production 入口、diagnostic / candidate helper、bench wrapper、analysis script 和 output summary。
- 标量路径做了什么，关键公式、循环、状态或输出如何形成；读者不看源码也应能理解被优化函数的作用和原实现流程。
- RVV 方案如何实现，哪一段标量路径被 RVV 接管，数据如何 load/gather、mask、staging、store 或 reduction（规约），哪一段仍是标量以及原因。
- 关键实现取舍为什么成立或暂缓，例如 buffer/staging、scalar tail（标量尾段）、fused multiply-add（融合乘加）、vector reduction（向量规约）、显式舍入或数学函数向量化；不能只写“保持语义”。
- gate 和 fallback 如何保持公开语义。
- 关键 RVV 片段是否覆盖完整阶段，是否只贴公式。
- 数值算例或图示是否能让读者手工对齐一个 VL chunk。
- 每个 bench case 如何构造、测了哪条入口/路径、证明点是什么、不能证明什么，性能结论来自哪里。
- 性能不好时必须给出源码和证据约束下的归因假设，例如 gather、不规则访存、压缩写回、标量 tail、额外 staging 内存流量、solver 或其它主成本；若尚未定位，写清下一轮需要的 profile、asm 或消融 bench。
- 生产接入、bench 诊断或回退判断是否由 full evidence 支撑。
- 生产接入后的文档是否把诊断阶段结论更新为生产证据结论，且没有把未覆盖入口写成已接入。
- 未闭合项是否让读者知道“要做什么”和“做了有什么用”，而不是只看到名词清单。
