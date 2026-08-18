# 文档归属矩阵与 Traceability Map

本文定义 RVV topic（主题）工作中各类事实的主归属，以及复杂 topic 的 traceability map（可追踪性地图）规则。目标是让 reviewer（审查者）和下一轮 worker（执行者）能从文档定位到代码、测试、脚本和 output（输出证据），同时避免把同一段事实复制到多个长期文档。

## 何时读取

- 新建、重排或 closeout（收尾）`artifact_layout.topic_doc_template` 解析出的 production 长期主题文档时读取。
- 新建或更新 `artifact_layout.evaluation_doc_template` 解析出的 evaluation（函数级评估）文档时读取。
- Handoff Packet（交接数据包）需要说明文档、测试、输出和代码位置如何互相定位时读取。
- reviewer 审查文档重复、证据错放、恢复路径不清或函数关系看不懂时读取。

## 文档归属矩阵

每类事实只设一个主归属。其它文档可以引用主归属的路径、章节、表格、run label（运行标签）或 evidence path（证据路径），但不要复制长段正文、raw log（原始日志）或完整实验流水。
`artifact_layout.qemu_output_subdir` 和 `artifact_layout.board_output_subdir` 解析目录下的生成证据，只有被 `paths.doc_root` 或 `paths.test_root` 解析目录下的文档明确引用时才进入提交候选；因此长期文档和 evaluation 引用证据时要写具体文件、run label 或 summary artifact 路径，而不是只写输出目录。

阶段探索归属在 `artifact_layout.phase_root_template` 解析目录：计划、负向尝试、异常解释、optimization matrix 和 unblocked next action 都先放这里。每一轮 production 接入尝试、具体点型 production candidate、代表性点型验证、row source 扩展和 point-type expansion（点类型扩展）都必须保留对应 phase plan/result；这些阶段记录保存测试事实和范围边界。`artifact_layout.topic_doc_template` 解析出的 production 长期主题文档只保存用户确认采纳后的最终 production 行为、当前采用实现、证据链和长期维护边界；它可以引用阶段文档作为审计来源，但不要把阶段流水或临时计划复制进去。没有 adopted production behavior、用户确认保留的 production patch 或 PI5 生产证据闭环通过且用户确认采纳时，该模板为 `not_applicable`，不得为了 no-production closeout 新建 production 长期主题文档。跨阶段的 candidate 搜索空间、阶段反思新增路线和恢复条件归到 `artifact_layout.optimization_roadmap_template` 解析出的 roadmap，不要塞进 phase result 或 evaluation。

| 信息类型 | 主归属 | 允许引用 | 不应复制 |
| --- | --- | --- | --- |
| 当前采用的生产优化方式、覆盖范围、fallback（回退路径）和生产边界 | `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档，仅在 production 行为已采用后适用 | evaluation 的实现方式审计表、Handoff 摘要、模块状态表 | output summary 的 raw 表、每轮 bench 全量日志、对话过程；no-production 诊断结论 |
| S2 evaluation、候选路线、采用 / 尝试 / 暂缓 / 拒绝理由、no-production 诊断证据链 | `artifact_layout.evaluation_doc_template` 解析出的 evaluation 文档 | production 长期主题文档只在适用时引用最终采用状态和证据路径；Handoff 引用下一步动作 | production 长期主题文档复制完整候选流水账；Handoff 写成完整实验报告 |
| 阶段计划、阶段结果、优化矩阵、unblocked next action、early-stop 证据 | `artifact_layout.phase_root_template` 解析目录 | Handoff 的 `phase_loop_state`、evaluation 的阶段审计、适用时的 production 长期主题文档最终结论 | production 长期主题文档的最终生产行为说明、长期结论和跨阶段通用规则 |
| 具体点型 / 代表性点型 production 接入记录、未覆盖点类型、`point_type_expansion_queue` 和每轮扩展证据 | `artifact_layout.phase_root_template` 解析目录与 optimization matrix | production 长期主题文档只引用用户确认采纳的当前范围和下一扩展状态；Handoff 引用恢复队列 | 把第一阶段窄范围 gate 写成整个模板入口最终实现；在 production 长期主题文档复制每轮探索流水 |
| 跨阶段候选搜索空间、阶段反思新增路线、恢复条件和优先级 | `artifact_layout.optimization_roadmap_template` 解析出的 roadmap | phase result 的反思摘要、Handoff 的 `optimization_roadmap_status`、evaluation 的候选取舍索引 | 单阶段流水、board 统计明细和 production 最终结论 |
| test、diagnostic、bench case 的输入构造、计时边界和证明点 | evaluation 文档和对应测试 / bench 源码注释 | 适用的 production 长期主题文档只引用能支撑结论的 case；Handoff 列命令和路径 | production 长期主题文档复制每个 TEST 的长注释；output summary 承担测试设计说明 |
| bench 统计、A/B 公式、异常值口径、run label 和复现命令 | `artifact_layout.board_output_subdir` 解析目录下的 summary 或 analysis script（分析脚本） | evaluation / 适用的 production 长期主题文档引用 summary 路径、脚本路径和关键结论 | production 长期主题文档或 Handoff 复制 raw log；把 QEMU timing 写成性能结论 |
| 当前数值结论、复跑和过期状态 | 最近一次 run-labelled summary、phase result 和 evaluation | Handoff / 适用的 production 长期主题文档引用 current run label；旧 run 仅作 historical evidence | 把旧 summary 继续写成 current truth，或让 phase result 与最新复跑数值冲突 |
| Evidence Doctor（证据体检）结果、异常信号、处理动作和结论降级 | `evidence_doctor.md` / `evidence_doctor.json` 或 output summary 内的 Evidence Doctor 小节 | evaluation / 适用的 production 长期主题文档引用 doctor 路径和关键 finding；Handoff 记录处理动作 | 长期文档复制完整 doctor 报告；把 Warning 隐藏在 raw log 或只写“异常可接受” |
| Evidence registry（证据登记表）、人工复跑发现和未登记覆盖状态 | `artifact_layout.evidence_registry_template` 解析出的 registry 或等价 output summary 状态小节 | Handoff 的 `evidence_registry_status`、phase result 的 freshness 检查、提交前检查输出 | 把 registry 当 raw log 长篇复制；只看 git status 就假设 ignored 日志没变 |
| QEMU、反汇编、board（板卡）和 production direct（真实生产路径证据）的证据边界 | 证据 summary、evaluation 证据表和适用的 production 长期主题文档证据链共同引用同一批路径 | Handoff 列 evidence paths；reviewer 抽查路径 | 多处写互相矛盾的“最新结果”或无路径结论 |
| 真实 production（生产源码）补丁、dispatch（分流逻辑）、public API（公开接口）和维护解释 | production 源码 + production 长期主题文档 | evaluation 记录 production decision（生产接入判断）；Handoff 列 production diff | evaluation 复述生产实现长文；output summary 解释生产维护边界 |
| reviewer 恢复动作、dirty isolation（脏工作区隔离）、提交边界和下一轮动作 | Handoff Packet、work log（工作日志）或 CURRENT_STATUS（当前状态入口） | evaluation / 适用的 production 长期主题文档只保留稳定后续方向 | production 长期主题文档写成当前待办清单；长期文档依赖聊天上下文 |
| screening（筛选）队列、模块级优先级和 topic 状态 | `artifact_layout.screening_root_template` 解析目录或配置解析出的状态表 | Handoff 和 closeout 引用状态同步结果 | production 长期主题文档复制模块队列表 |
| 通用 workflow、reviewer 或文档规则 | `.agents/skills/`、`.agents/knowledge/` 和 `agent_asset_feedback` | Handoff 说明建议更新位置 | topic 文档写成通用 agent 规则 |

用户或 reviewer 对工作流程、停止条件、文档拆分、测试支撑结构、恢复方式或 reviewer 可读性的反馈，
默认先进入 agent asset audit（代理资产审计）。若反馈暴露的是可跨 topic 复用的规则缺口，worker / reviewer
应在 Handoff 的 `agent_asset_feedback` 中写明建议更新的 skill/reference；获得 workflow improvement
授权后，先更新 `.agents/`，不要只把它记成当前 topic follow-up。

复杂 topic 可以把测试和证据说明拆成多份 topic-local 文档。推荐分工：

这些分工是 role（职责），不是固定文件名。具体路径先从 `.agents/config/defaults.yaml` 的 `artifact_layout`
解析；如果某个 role 还没有精确路径 key，则使用当前 topic 已确认的 role/path index 或既有链接，并在需要跨
topic 稳定复用时先补配置，不在模板正文里写死命名。role-based templates 见
`templates/template-index.zh.md`。

- `topic_navigation`：入口导航、当前结论、阅读路径、常用命令、证据白名单和 production_topic_doc 适用性。
- `testing_overview`：测试类型定义、运行入口分类、覆盖矩阵和证据白名单。
- `correctness_tests`：每个 gtest 的中文含义、输入、被测路径、断言、证明范围和代码位置。
- `benchmark_and_evidence`：bench label 语法、case-filter 字典、QEMU target、board smoke target、repeated board collect target、checksum 来源、trace、asm attribution、复现命令和提交边界。
- `optimization_evidence`：每种 RVV 优化方式、候选或暂缓路径对应的 production / test_support 代码路径、test target、bench target、board evidence、结论和边界。
- `test_support_code_map`：`artifact_layout` 与 `test_support` 解析出的源码、聚合入口、内部头文件、script 和 production helper 的函数族、调用关系和边界。
- `phase_index` / `phase_plan` / `phase_result` / `optimization_matrix`：阶段探索、阶段计划、阶段结果、optimization matrix、Evidence Doctor 异常处理、continue / stop decision 和早停检查。
- `optimization_roadmap`：跨阶段 candidate family、idea source、阶段反思新增路线、优先级、恢复条件和搜索空间变化。
- `evaluation_diagnostic` / `evaluation_production`：EvidenceDecision、当前证据、历史候选取舍、accepted risk、Traceability Map、production 接入判断和最终证据更新。

README 只作为导航、常用命令和证据白名单入口。它不承担每个测试、每个 bench case 或每个 helper 的长解释。

topic-local doc suite 的 canonical quality bar（规范质量门槛）见 `doc-suite-quality-bar.zh.md`。当 topic
命中复杂 topic 条件，或已有 production direct、board summary、Evidence Doctor、多阶段 phase loop
等恢复 / 审查负担时，doc suite parity（文档套件对齐）是 structure maturity（结构成熟度）的一部分。
worker 可以根据当前 topic 的真实复杂度合并或裁剪文档，但必须在 phase plan/result 和 Handoff 中逐项说明
`adopted / rejected with evidence / not_applicable with evidence / turn_stop_deferred with stop_condition_hit`。
若只是暂缓且不存在用户限定、dirty isolation 风险、工具阻塞、生产范围扩大或真实外部依赖，默认继续到下一
phase；不要把“只有 reviewer 需要才补”写成合法 closeout。

production closeout、production-ready、done 或 stop-for-review 声明前，若当前 topic 命中上述 doc-suite
quality bar 条件，worker 必须产出一个 doc-suite parity 审计结果。该结果可以写在当前 phase `result.zh.md`，
也可以新建 `structure-parity-doc-suite` phase，但不能只出现在 roadmap、最终回复或 Handoff。成熟 sibling topic
只能作为 optional calibration（可选校准样例）：用户 / reviewer 点名时可用来补充 quality bar，但不能成为唯一规范源。
审计表必须使用以下列，并覆盖 topic_navigation、testing_overview、correctness_tests、benchmark_and_evidence、
optimization_evidence、optimization_roadmap、test_support_code_map、evaluation、production_topic_doc、phase suite 和 artifact tracking。
审计表可以在 `area` 中同时写 role 和当前 topic 实际路径，但不能把模板名当成路径规范：

```text
| area | current shape scan | quality bar / optional calibration | decision | blocker / evidence | next action |
```

审计完成后还必须检查文档引用的 artifact tracking（产物跟踪状态）：README、evaluation、roadmap、
phase result 或 production 长期主题文档中引用的 topic-local doc-suite 文件必须存在，并在当前 topic 的
tracked / to-be-staged artifact 集合中，或明确标成 local-only / excluded 且不作为提交后文档入口。
worker 应使用包含未跟踪文件的路径限定扫描，而不是只看普通 `git status --short`。如果新增文档仍是
untracked 且没有提交边界说明，doc-suite parity 只能写 `partial`，不能支撑 `ready_for_review`。

裁剪规则：

- 当前 topic 没有某类 row source、test family、bench family、script 或 evidence output 时，可以写
  `not_applicable with evidence`，并在 evidence 中列出不存在的对象。
- 当前 topic 规模较小但仍有 production dispatch / fallback、board evidence、Evidence Doctor 或
  Traceability Map 需求时，不能把 topic-local docs 全部合并进 README；至少要让测试语义、bench 证据、
  优化证据和代码地图各有稳定主归属。
- `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档不承担 test support 全量说明；若它开始复制测试工程细节，worker 应把内容迁回
  topic-local docs，并在长期文档只保留 production 行为与证据链摘要。
- README 是入口和证据白名单，不是每个 gtest、bench case、helper 或 phase 的正文归属。

## 写入顺序

1. 先判断本轮事实类型和主归属。
2. 在主归属文档写完整解释、表格或证据摘要。
3. 在其它文档只写短引用：仓库相对路径、章节名、符号名、run label 或 output summary 路径。
4. 如果两个文档都需要同一事实，拆成“production 长期事实”和“决策审计”。production 长期主题文档写当前 adopted production 状态；evaluation 文档写候选取舍和证据如何改变判断。no-production 没有 adopted production 状态时，不新建 production 长期主题文档。
5. Handoff 只写 reviewer 恢复需要的定位信息、验证结果和下一步动作，不替代 production 长期主题文档或 evaluation。

如果一次复跑改变了数值结论、decision bucket 或证据角色，旧 summary 立刻转为 historical evidence，不能继续作为当前 truth。worker 必须同步刷新 phase result、optimization roadmap、evaluation、Handoff 和适用的 production 长期主题文档；若还没刷新，文档状态应显式标成 stale / refresh pending，而不是继续沿用旧 run label。若 registry 或扫描发现 `unregistered_change` / `manual_run_detected`，先把当前数值结论降级为待刷新状态，再决定是否重建 summary / Evidence Doctor。

legacy pointer（旧路径指针）和 compatibility alias（兼容别名）不是默认的文档归属策略。若长期文档、
evaluation 或 topic-local doc suite 已迁移到新主路径，worker 应优先更新引用并删除旧入口；只有存在
明确外部依赖、同轮无法同步更新的脚本、dirty isolation 风险或用户明确要求保留时，才临时保留，并在
Handoff / phase result 写出删除条件和下一阶段清理动作。

## Traceability Map 触发条件

复杂 topic 必须在 evaluation、topic-local 文档或适用的 production 长期主题文档中加入 `Traceability Map（可追踪性地图）` 章节。确实需要时可以拆成独立 `*-traceability.zh.md`，但默认不新建大型长期函数文档。

命中任一条件即可视为复杂 topic：

- 存在多个 public entry（公开入口）、dispatch / fallback、row source policy（行来源策略）或数据布局路径。
- 同时存在 production helper、Std helper、RVV helper、diagnostic reference、candidate helper、bench wrapper 或 analysis script 中的三类以上角色。
- 存在多个 candidate（候选实现）、component ablation（组件消融）、RVV-vs-RVV A/B 或 repeated board（重复板卡测试）结果。
- 证据分布在 QEMU、反汇编、board summary、analysis script 和 raw output 多个位置。
- reviewer 或用户难以从文档回答“这个函数 / helper / 脚本在候选链路中是哪一层”。

## Traceability Map 最小表格

推荐表格如下。`位置` 使用仓库相对路径；长期文档优先写文件 + 符号 / target / 章节。行号可以出现在 Handoff 或 reviewer 当前 diff 中，长期文档不要依赖易漂移的行号。

```text
| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
```

`层级` 使用通用标签，不写 topic 专用算法名称：

- production public entry
- production dispatch / fallback
- production Std helper
- production RVV helper
- diagnostic reference
- row source / input policy
- candidate formula / reduction / staging
- bench wrapper
- analysis script
- evidence output summary
- evidence raw log（只列本机路径边界或不提交说明）
- documentation section

## Traceability Map 覆盖要求

复杂 topic 的 map 至少覆盖本轮结论依赖的对象，不要求枚举每个小函数。

- production 侧：public entry、dispatch / fallback gate、`*_Std` / `*_RVV` helper、traits / layout gate、保持标量的入口。
- RVV test 侧：reference path（参考链路）、row source、candidate helper、reduction / staging helper、production-shaped diagnostic、production direct test、bench wrapper。
- script / output 侧：分析脚本、summary artifact（摘要证据）、QEMU / board output、反汇编或 profiling 证据入口。
- 文档侧：适用的 production 长期主题文档的“当前采用的优化方式”或“正确性与高效性证据链”、evaluation 的实现方式审计表、no-production phase closeout 的“诊断证据链”、Handoff Packet 的恢复字段。

每一行的 `证据角色` 必须说明该对象能证明什么，不能只写“测试”或“bench”。示例：

- `correctness gate（正确性验收）`
- `RVV-vs-RVV candidate B/A summary（候选相对基线性能摘要）`
- `fallback coverage（回退路径覆盖）`
- `asm attribution（反汇编归属）`
- `production boundary（生产边界）`
- `recovery pointer（恢复入口）`

## 交叉引用格式

跨文档引用至少包含以下三件信息中的两件；复杂结论尽量三件都给出。

```text
path: <repo-relative-path>
anchor: <section title / symbol / make target / run label>
role: <该对象在证据链中的角色>
```

例如：

```text
path: artifact_layout.evaluation_doc_template
anchor: 实现方式审计
role: candidate 取舍主归属；production 长期主题文档只在适用时引用最终 adopted 状态
```

```text
path: {artifact_layout.board_output_subdir}/<summary>.md
anchor: RVV-vs-RVV B/A summary
role: 板卡性能摘要；该路径可进入提交候选，raw log 不进入默认提交边界
```

## Reviewer 检查点

reviewer 审查文档和 Handoff 时应确认：

- 是否能用文档归属矩阵指出每类事实的主归属。
- production 长期主题文档、evaluation、topic-local phase / diagnostic docs、output summary 和 Handoff 是否存在长段重复、互相矛盾或证据错放。
- 是否存在复跑后数值变化但文档仍引用旧 run label 的情况；如果有，是否已显式标成 historical / stale 并刷新主归属。
- 是否存在 registry 显示 evidence 文件被覆盖、扫描到未登记文件或 Handoff 写 `evidence_registry_status=not_available` 但没有人工检查路径的情况。
- 复杂 topic 是否有 Traceability Map；如果没有，Handoff 是否给出 `not_applicable` 理由。
- 复杂 topic 是否有 topic-level optimization roadmap；如果没有，Handoff 是否给出 `not_applicable` 或 `deferred` 理由，并说明下一轮为什么需要创建。
- Traceability Map 是否能从关键文档跳到代码、测试、脚本和 output，而不是只写自然语言说明。
- Handoff 是否列出 `document_ownership_check` 和 `traceability_map_status`，并给出 reviewer 可抽查的路径。

## Handoff 字段要求

worker 的 Handoff Packet 应补充：

```text
document_ownership_check: 本轮长期事实、候选取舍、bench 统计、output summary、恢复动作分别写到哪里；是否存在重复或错放。
traceability_map_status: required / updated / not_required / deferred；列出 map 所在文档章节，或说明暂缓原因和下一轮补齐条件。
optimization_roadmap_status: required / updated / not_required / deferred；列出 roadmap 路径、当前候选搜索空间、阶段反思新增路线和恢复条件，或说明暂缓原因。
```

这些字段不要求复制 map 或 roadmap 全文。它们只给 reviewer 一个入口，用来抽查文档、测试、输出、代码位置和候选搜索空间是否能互相定位。
