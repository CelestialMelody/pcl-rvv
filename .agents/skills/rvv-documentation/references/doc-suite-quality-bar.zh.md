# Topic-local Doc Suite Quality Bar

本文定义复杂 RVV topic（主题）的 topic-local doc suite（主题本地文档套件）质量门槛。它是默认规范源，不依赖任何具体 sibling topic（同模块相邻主题）。成熟 sibling 只能补充校准读者路径和结构完整度，不能替代本文、`templates/template-index.zh.md` 和 `.agents/config/defaults.yaml` 中的 role path key。

## 何时读取

- 新建或重排 `artifact_layout.topic_test_dir_template` 解析目录下的 topic_navigation、testing/evidence、phase suite、roadmap 或 evaluation role 文档时读取。
- closeout、production-ready、done、stop-for-review 或 `ready_for_review` 前，当前 topic 命中复杂 topic 条件、存在 production direct（真实生产路径证据）、board summary（板卡摘要）、Evidence Doctor（证据体检）或多阶段 phase loop（阶段循环）时读取。
- 用户或 reviewer 要求“文档对齐”“可审查性”“文档结构是否像成熟 topic”时读取。若用户点名某个成熟 sibling，只把它作为补充校准，不要复制 topic-specific（当前主题特有）的算法、数值、phase 名、文件名或结论。
- 用户已经表达“同意接入 / 可以提交 / 可以保留当前 patch”或 worker 准备停在“是否提交或取消接入”的判断点时，如果当前 topic 有 production patch 或 adopted production behavior，必须同时读取 `topic-doc-structure.md` 的 Production Doc Closeout Gate。此时 `artifact_layout.topic_doc_template` 解析出的长期生产文档质量属于 closeout 本身，不是 `git commit` 前才做的机械检查。

## Doc Suite Role Inventory

恢复 phase loop、新建 `000-current-state-and-gaps`、新建 / 重排 topic-local docs、或准备 closeout / ready-for-review 前，worker 必须写出 `doc_suite_role_inventory`。inventory 逐一覆盖 topic_navigation、testing_overview、correctness_tests、benchmark_and_evidence、optimization_evidence、optimization_roadmap、test_support_code_map、phase_index、evaluation_diagnostic / evaluation_production 和 production_topic_doc。

每个 role 只能使用下列状态：

- `standalone:<path>`：使用 `artifact_layout` 精确 role key 解析出的默认路径，或当前 topic 已有稳定路径；路径必须可从 README、phase index、evaluation 或 Handoff 找到。
- `merged:<path#section>`：role 合并进现有文档的稳定章节；必须说明 closeout checks 如何被覆盖。
- `not_applicable with evidence`：当前 topic 确实没有对应测试、bench、script、row source、candidate、production 行为或证据职责。
- `phase_deferred + unblocked`：仍在当前 topic 授权范围内且没有真实 blocker；下一 phase 默认指向 `structure-parity-doc-suite` 或等价文档补齐 phase。
- `turn_stop_deferred with stop_condition_hit`：本轮合法停止，必须命中用户限定、dirty isolation、工具 / 板卡不可用、扩大到未授权 production / public API / 其它 topic，或真实外部依赖。

复杂 topic 应优先拆出独立 role 文档，而不是继续把职责塞进 evaluation 或 phase result。复杂度触发包括：多个 test / bench target、board summary 或 Evidence Doctor、多个 candidate family、多个 public entry / row source / 点型 / `Scalar` / layout 组合、测试支撑代码多职责、evaluation 已承担 bench 字典、证据白名单或测试支撑代码地图。未拆出时，inventory 必须写 `merged:<path#section>` 或 `phase_deferred + unblocked`，不能只写“topic 较小”或“内容够看”。

## 质量目标

topic-local doc suite 应让下一轮 worker 或 reviewer 不依赖聊天上下文，就能回答：

- 当前 EvidenceDecision（证据决策）是什么，production 是否适用。
- public entry（公开入口）、标量路径、diagnostic candidate（诊断候选）、bench case（性能测试用例）和 output summary（输出摘要）如何互相定位。
- 每个 test、bench、board target 和 Evidence Doctor 报告能证明什么，不能证明什么。
- 若 no-production（不接入生产）或 rollback/no-production（回滚且不接入生产），诊断证据为什么不能替代 production evidence（生产证据）。
- 若还有未阻塞动作，下一 phase 应从哪里恢复。

## Role-based 文档套件

复杂 topic 默认维护一组文档 role（职责），而不是固定文件清单。role-based templates（基于职责的模板）见 [templates/template-index.zh.md](templates/template-index.zh.md)。模板只定义文档职责、内容结构、裁剪规则和 closeout checks；最终文件路径和命名优先服从 `.agents/config/defaults.yaml` 的 `artifact_layout` 精确 role path key。

当前 topic 确实没有对应职责时，可以裁剪，但必须写 `not_applicable with evidence`，列出不存在的测试、bench、script、row source 或 production 行为。若某个 role 需要跨 topic 稳定落到新文件名，先更新 `artifact_layout` 的 role path key；不要在模板正文或某个 sibling topic 中把文件名写成规范。

| role | 主职责 | 模板 |
| --- | --- | --- |
| topic_navigation | 入口导航、当前结论、阅读路径、常用命令、证据白名单和 production_topic_doc 适用性。 | [templates/topic-navigation-template.zh.md](templates/topic-navigation-template.zh.md) |
| testing_overview | 测试类型定义、运行入口分类、target 粒度审计、覆盖矩阵和 QEMU / board / production direct 证据边界。 | [templates/testing-overview-template.zh.md](templates/testing-overview-template.zh.md) |
| correctness_tests | 每个 TEST 或测试族的输入、被测路径、断言、证明范围、不能证明的范围和默认 target。 | [templates/correctness-tests-template.zh.md](templates/correctness-tests-template.zh.md) |
| benchmark_and_evidence | CLI、case-filter、bench label、计时边界、checksum、QEMU / board、summary / manifest / doctor、registry 和提交边界。 | [templates/benchmark-and-evidence-template.zh.md](templates/benchmark-and-evidence-template.zh.md) |
| optimization_evidence | adopted / attempted / rejected / deferred / not_applicable candidate 到代码、target、board、asm、doctor 和 decision 的映射。 | [templates/optimization-evidence-template.zh.md](templates/optimization-evidence-template.zh.md) |
| optimization_roadmap | 跨 phase candidate family、idea source、风险、所需证据、优先级、恢复条件和搜索空间变化。 | [templates/optimization-roadmap-template.zh.md](templates/optimization-roadmap-template.zh.md) |
| test_support_code_map | 测试支撑代码、fixtures、reference、candidate、bench harness、script、production 对照和拆分审计。 | [templates/test-support-code-map-template.zh.md](templates/test-support-code-map-template.zh.md) |
| phase_index / phase_plan / phase_result / optimization_matrix | 阶段恢复入口、计划、结果、跨阶段优化矩阵、Evidence Doctor 异常处理和继续 / 停止判断。 | [templates/phase-suite-template.zh.md](templates/phase-suite-template.zh.md) |
| evaluation_diagnostic | diagnostic / bench-only / partial-production-candidate 的函数级评估、诊断证据链、Traceability Map 和 production 接入前置条件。 | [templates/evaluation-diagnostic-template.zh.md](templates/evaluation-diagnostic-template.zh.md) |
| evaluation_production | production integration loop 后的 production patch scope、fallback matrix、production direct tests、asm、board repeated evidence 和最终 EvidenceDecision。 | [templates/evaluation-production-template.zh.md](templates/evaluation-production-template.zh.md) |
| production_topic_doc | adopted production behavior 的长期生产行为说明。 | 见 [topic-doc-structure.md](topic-doc-structure.md)；路径来自 `artifact_layout.topic_doc_template`。 |

当 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档适用时，topic-local doc suite 仍负责保存 phase 审计、bench 字典、测试支撑代码地图和 evidence registry；production 长期主题文档负责维护当前 production 行为。二者不能互相替代：phase 文档可以解释“为什么尝试 / 为什么撤下”，长期文档必须解释“当前源码实际如何工作、证据支持到哪里、哪些入口仍回退”。如果长期文档仍只是摘要，doc-suite closeout 应标为 `doc_closeout_pending`。

## Target 粒度审计

复杂 topic 或 doc-suite parity（文档套件对齐）阶段必须做 testing target granularity audit（测试 target 粒度审计）。这个审计只从当前 topic 的真实工程入口抽取事实，不能把某个 sibling topic（同模块相邻主题）的 target 名、case 名、日志路径或生产结论当作模板复制。

审计输入至少包含：

- topic `Makefile` 和 `board.mk` 中的 public target、alias target（别名目标）、guarded target（带保护门的目标）和 clean / refresh target。
- `src/test_*.cpp` 中的 gtest、gtest filter（测试过滤条件）、test-only helper（测试专用 helper）和 public-entry-shaped smoke（公开入口形态小型验证）。
- `src/bench_*.cpp` 中的 CLI 参数、case-filter、case label、checksum 输出、计时边界和 historical probe（历史探针）入口。
- topic-local `script/`、summary / manifest / Evidence Doctor 输出、evidence registry，以及 README / evaluation / roadmap / phase result 引用的证据路径。

审计输出应区分下列 target 类别。当前 topic 确实没有某一类时写 `not_applicable with evidence`，并列出缺少的源码、case-filter、board target 或证据职责；不能只写“topic 较小”或“内容够看”。

| target 类别 | 应回答的问题 | 常见文档归属 |
| --- | --- | --- |
| correctness aggregate（正确性汇总入口） | 是否有一条总入口能跑完整 Std / RVV correctness；日志是否稳定可引用。 | testing_overview、correctness_tests、topic_navigation。 |
| correctness aliases（正确性细分入口） | 是否需要按 public semantics（公开入口语义）、input semantics（输入语义）、candidate correctness（候选正确性）、fallback（回退路径）或 production direct（真实生产路径）拆分 gtest target。 | testing_overview 的运行入口分类、correctness_tests 的 TEST 字典。 |
| bench diagnostic aliases（bench 诊断入口） | case-filter 是否能隔离 row source（行来源）、candidate family（候选族）、component ablation（组件消融）或 output contract（输出合同）。 | benchmark_and_evidence、optimization_evidence。 |
| QEMU smoke aliases（QEMU 小型验证入口） | QEMU 是否只用于 build / correctness / log-shape（日志形状）；是否避免完整 bench compare 被写成性能证据。 | testing_overview、benchmark_and_evidence。 |
| board smoke aliases（板卡小型验证入口） | 单次板卡 target 证明什么：可运行、correctness、checksum 或输出形状；是否没有被写成 repeated performance。 | testing_overview、benchmark_and_evidence。 |
| board repeated aliases（板卡重复采集入口） | 哪些 target 生成 repeated summary、manifest、Evidence Doctor；run budget 和 decision bucket 是否明确。 | benchmark_and_evidence、phase_result、Handoff。 |
| doctor / registry aliases（证据体检和登记入口） | 是否有生成 / 检查 manifest、Evidence Doctor 和 evidence registry 的 target 或脚本；未接入时是否有人工检查路径。 | benchmark_and_evidence、phase_result、Handoff。 |
| historical probe guarded aliases（历史探针保护入口） | 历史 production probe、回滚探针或不再默认运行的 target 是否有显式开关、误用保护和证据降级说明。 | topic_navigation、testing_overview、benchmark_and_evidence、phase_result。 |

如果 target 粒度不足已经影响 reviewer 定位、证据边界或后续 candidate 扩展，worker 应在当前 phase 补齐 alias target，或把它写成 `phase_deferred + unblocked` 的下一阶段动作。只有用户限定范围、dirty isolation 风险、工具 / 板卡不可用、需要扩大到 production / public API / 其它 topic，或存在外部脚本依赖时，才能写成 `turn_stop_deferred with stop_condition_hit`。

文档不得虚构不存在的 target。若当前工程只有一个 aggregate target，testing_overview role 可以先把 gtest、case-filter、board summary 和 EvidenceDecision 的映射写清；同时在 doc-suite parity 审计表中说明是否需要补 correctness aliases、bench aliases、board repeated aliases、doctor / registry aliases 或 historical probe guard。

## 模板使用规则

role template 不是可直接复制的固定骨架。使用时先读取 [templates/template-index.zh.md](templates/template-index.zh.md)，再按当前 topic 命中的 role 读取对应模板。

写文档时遵守：

- 先解析 `artifact_layout` 精确 role path key，再决定 role 的实际文件路径。
- 先判断当前 topic 是 diagnostic、partial-production-candidate、production integration 还是 adopted production behavior，再选择 evaluation 模板。
- 当前 topic 已有成熟 role 文档时，可以保留既有文件名，只按模板补职责缺口。
- 当前 topic 没有某个 role 时，必须在 `doc_suite_role_inventory`、phase result 或 Handoff 中用 `merged:<path#section>`、`not_applicable with evidence`、`phase_deferred + unblocked` 或 `turn_stop_deferred with stop_condition_hit` 说明。
- 模板章节可以合并、改名或裁剪，但 closeout checks 覆盖的问题不能消失。

## 审计表

doc-suite quality bar 审计表使用以下列。`quality bar / supplemental calibration` 先写本文规范；只有用户、reviewer 或本地历史明确点名成熟 sibling 时，才在同一格补充 sibling calibration（成熟样例校准）。

```text
| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
```

`decision` 只使用：

- `adopted`：当前 topic 已达到或本 phase 已补齐。
- `rejected with evidence`：当前 topic 不采用该文档形态，并有源码、测试、bench、script 或 production 边界证据。
- `not_applicable with evidence`：当前 topic 确实没有对应职责，并列出不存在的对象。
- `phase_deferred + unblocked`：仍在当前 topic 授权范围内，下一 phase 默认继续。
- `turn_stop_deferred with stop_condition_hit`：本轮合法停止，必须命中用户限定、dirty isolation 风险、工具 / 板卡不可用、扩大到未授权 production / public API / 其它 topic，或真实外部依赖。

不能使用“topic 较小”“等 reviewer 要求”“内容已经够看”单独关闭缺口。它们只能作为裁剪判断的输入，必须配合证据说明当前缺少哪类职责。

## Artifact Tracking

topic_navigation、evaluation、roadmap、phase result 或 production 长期主题文档引用的 topic-local doc-suite 文件必须存在，并在当前 topic 的 tracked / to-be-staged artifact 集合中，或明确标为 local-only / excluded 且不作为提交后入口。

审计时使用包含未跟踪文件的路径限定扫描，例如：

```bash
git status --short --untracked-files=all -- <topic-paths>
git ls-files --others --exclude-standard -- <topic-paths>
```

只看普通 `git status --short` 或本地文件可读性，不足以声明 doc-suite closeout。新增文档若仍是 untracked 且没有提交边界说明，doc-suite quality bar 只能写 `partial` 或 `fail`。

## Sibling Calibration 边界

成熟 sibling 是校准样例，不是规范源。使用时必须遵守：

- 不复制 sibling 的算法、性能数字、phase slug、生产结论或 topic-specific 文件名。
- 只迁移结构成熟度：读者路径、证据白名单、target 字典、代码地图、文档归属和 closeout 门禁。
- 若 sibling 与本文 quality bar 冲突，以本文和当前 topic 的源码 / 证据为准，并在 phase result 或 Handoff 写出 `instruction_feedback`。
- 若某个好做法会跨 topic 复用，应沉淀回本文或其它 `.agents` reference，而不是让未来 worker 继续依赖那个 sibling 路径。
