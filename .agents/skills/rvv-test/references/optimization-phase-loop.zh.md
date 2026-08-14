# RVV 多阶段优化循环

本文是 `rvv-test` 中 optimization phase loop（优化阶段循环）的唯一细则来源。它把一个 RVV topic 的持续优化定义为可恢复的循环：

```text
恢复状态 -> 写阶段计划 -> 实现 / 测试 -> 解释证据 -> 更新结果和矩阵 -> 继续或停止决策
```

它不把历史实现、某个局部正向结果或一个阶段的完成当作整个 topic 的终点。历史 topic、开源实现和本地知识库只能提供候选、风险和验证方向；新的 code shape（代码组织形态）仍可被提出，但必须通过当前 topic 的同边界正确性、性能、反汇编归属和 Evidence Doctor（证据体检）检查。

复杂 topic 还必须维护 topic-level optimization roadmap（主题级优化路线图）。roadmap 保存跨 phase 的搜索空间、候选家族、历史经验、阶段反思后新增的路线、优先级和恢复条件；它不替代阶段 `plan/result`，也不替代 EvidenceDecision。phase 解决“本阶段做什么、做成什么”，matrix 解决“证据状态是什么”，roadmap 解决“后面还能尝试什么、为什么排这个顺序”。roadmap 是持续搜索的工作面，不是收尾附录：每个阶段结束后的反思都要更新 candidate frontier（候选前沿），把被证据支持、被拒绝、暂缓或新生成的路线转成下一阶段可恢复状态。

## 适用条件

以下情况默认进入 phase loop：

- 用户用短 prompt 表达“继续完善 `<topic>` 的 RVV 优化工作”或等价含义。
- topic 已有 RVV 优化，但出现新的实现族、row source、点类型、`Scalar`、布局、入口或证据缺口。
- 一个阶段结束后，计划矩阵仍有 `unblocked`（未阻塞）动作。
- topic 需要在接入 production（生产源码）前后分别补测试、bench、fallback、反汇编或板卡证据。
- phase plan、optimization matrix 或 EvidenceDecision 需要板卡 / 目标硬件证据，且配置或当前会话显示板卡可用。

用户明确限定“只写计划”“只做一个指定 target”“只修一个文件”时，限定范围覆盖默认继续规则；worker 仍要记录未完成的 phase loop 状态。

短 prompt 中的“自行判断未完成项 / 下一步”必须按 topic maturity audit（主题成熟度审计）执行，而不是只寻找一个局部代码问题。若同时存在局部修补点和结构性测试 / 证据问题，先判断结构性问题是否影响后续审计清晰度；不能因为局部修复容易完成就提前 closeout。

## 阶段文档布局

阶段文档属于配置解析出的 topic 测试目录，不属于通用 `.agents/knowledge/`，也不属于 `artifact_layout.topic_doc_template` 解析出的最终主题文档。默认布局为：

```text
<topic-test-dir>/doc/phases/
  README.zh.md
  <phase-id>-<stable-slug>/
    plan.zh.md
    result.zh.md
    evidence-doctor.md       # 本阶段使用 Evidence Doctor 时可提交的摘要
    evidence-doctor.json     # 只有需要机器读取且已脱敏时保留
```

跨阶段路线图默认放在：

```text
<topic-test-dir>/doc/optimization-roadmap.zh.md
```

若 `.agents/config/defaults.yaml` 中 `artifact_layout.optimization_roadmap_template` 被覆盖，使用配置解析出的路径。没有 roadmap 时，恢复已有复杂 topic 或新建 `000-current-state-and-gaps` 阶段时必须创建；已有 roadmap 与当前源码、phase result 或 evidence 冲突时先标记 stale / refresh pending，再修订。

`<phase-id>` 使用单调、可排序的标识，例如 `000`、`010`、`020`；slug 描述阶段目标，不写易漂移的“最新”。worker 恢复时优先读取 `README.zh.md`、最近已完成阶段的 `result.zh.md`、当前未完成阶段的 `plan.zh.md` 和最近 Handoff Packet。没有阶段目录时，先创建 `000-current-state-and-gaps/plan.zh.md`，不能先写 candidate、bench 或 production。

`README.zh.md` 只维护阶段索引、状态、当前默认恢复入口和文档归属；不要复制各阶段实验流水。阶段 `plan.zh.md` 是修改前的意图和范围合同，`result.zh.md` 是完成后对计划逐项回填的事实记录。阶段结果即使被阻塞或拒绝也要保留，不能删除失败尝试来制造“已完成”假象。

## Phase Plan 最小合同

开始任何代码、测试支撑、bench、生产补丁或阶段性长文档修改前，当前阶段必须有 `plan.zh.md`。计划至少包含：

1. **阶段意图和边界**：本阶段要证明什么、不证明什么；入口、row source、点类型、`Scalar`、布局、规模、production / diagnostic 层级和不可触碰路径。
2. **当前状态清单**：已有实现、测试 target、bench target、板卡证据、反汇编、Evidence Doctor 报告、生产状态和上一阶段未完成项，并附路径或章节。
3. **假设与候选族**：允许提出新的 RVV code shape；列出历史经验作为参考而非约束，并说明要验证的 load/store、staging、reduction、FMA、ILP、LMUL 或 scalar tail 假设。
4. **优化矩阵**：至少覆盖本阶段相关的 `candidate family × row source × point type / Scalar / layout × test × bench × board × asm × doctor × decision`。
5. **实现和测试动作**：每个动作有明确产物、命令或 target、预期证据、负责人角色和完成判据。动作可以依赖其它动作，依赖必须显式写出。
6. **Evidence Doctor 和 registry 规则**：输入 manifest / summary / evidence registry 路径、预期 Errors / Warnings / Suggestions、异常时的重跑、降级、拒绝或暂停动作。
7. **阶段完成条件**：矩阵条目如何进入 `adopted`、`attempted`、`rejected`、`deferred`、`blocked` 或 `not_applicable`，哪些条目必须有同边界证据才能关闭。
8. **板卡复跑预算和决策桶**：run count、warm-up、最大复跑次数、统计口径、positive / weak-positive / neutral / negative / unstable 的判断口径，以及复跑预算耗尽后的降级或人工判断规则。
9. **继续 / 停止条件**：下一阶段默认入口、unblocked next actions、扩大权限或需要人工判断的边界。
10. **文档更新清单**：phase result、topic test 文档、evaluation、Handoff；production 行为只有在真实接入后才同步到 `artifact_layout.topic_doc_template` 解析出的主题文档。
11. **roadmap 同步动作**：本阶段会新增、尝试、拒绝、暂缓或重排哪些 roadmap candidate；哪些新想法来自阶段反思、同模块成熟 sibling、开源/论文启发或当前源码证据。

若阶段计划需要板卡证据，计划还必须写明 board availability check（板卡可用性检查）和继续策略：
配置解析出的 board target / rsync / ssh 入口是否存在、当前会话是否已确认板卡可用、可用时本轮要跑到哪一级
correctness / benchmark / repeated summary / Evidence Doctor / registry 刷新，以及何时因为真实 blocker
转为 `turn_stop_deferred`。不要把“下一步需要板卡验证”作为可停止动作；板卡可用时它是同轮 phase loop
的下一个执行动作。

计划不是愿望清单。每个动作都必须能在 `result.zh.md` 中回填为事实、证据路径、结论和下一步。

## Topic Maturity Audit

恢复已有 topic 或创建 `000-current-state-and-gaps` 阶段时，worker 必须审计五类完成度。这个审计不依赖任何特定历史 topic；历史 sibling（同类主题）只能提供候选风险、结构 quality bar 和验证方向，不能被机械照搬成实现方案。

1. **production boundary**：公开入口、RVV dispatch、fallback、layout / point type / `Scalar` gate 是否清晰；未覆盖入口是否显式保持标量；test-only reference 是否没有混入 production detail；production detail helper 是否真实服务 runtime path 或明确服务生产可维护性。
2. **RVV test support architecture**：测试支撑是否有稳定聚合入口；reference、fixtures、row source adapter、RVV math、reduction / formula candidate、assertions、bench harness / bench cases 是否按职责可审查；大型单文件、重复 helper、混合 production-direct 与 diagnostic 职责、bench/test wrapper 相互缠绕，都是可列入本阶段的工程债。
3. **test harness layout and naming**：测试 / bench 源码、聚合头文件和内部职责拆分是否仍停在当前 topic 既有布局，是否应迁移到 `artifact_layout.source_subdir`、`artifact_layout.test_source_template`、`artifact_layout.bench_source_template`、`test_support.aggregator_directory` 和 `test_support.internal_directory` 解析出的结构；长 topic 是否应按 `test_support.topic_abbrev_policy` 使用缩写 topic token 作为文件名；Makefile、board target、日志路径、文档引用和现有 case 名在迁移后是否保持兼容。此项必须给出 `adopt / defer / reject` 决策，不能被包含在泛泛的“测试支撑可读性”里。
   审计对象按当前 topic 真实形态枚举：根目录长 `test_*.cpp` / `bench_*.cpp`、单个聚合头、多职责 helper header、旧 `test_support/` 目录、已有 `include/` / `include/impl/`、script 或其它等价测试支撑文件都要纳入；不要假设每个 topic 都有字面量 `test_support/` 目录，也不要因为没有该目录就跳过布局迁移审计。
   本项还必须包含 target granularity audit（测试 target 粒度审计）：从当前 topic 的 `Makefile`、`board.mk`、`src/test_*.cpp`、`src/bench_*.cpp`、topic-local `script/` 和 evidence registry 抽取真实 target，区分 correctness aggregate、correctness aliases、bench diagnostic aliases、QEMU smoke aliases、board smoke aliases、board repeated aliases、doctor / registry aliases 和 historical probe guarded aliases。若缺少某类 target，worker 应说明本阶段补齐、`not_applicable with evidence`、`phase_deferred + unblocked` 或 `turn_stop_deferred with stop_condition_hit`。成熟 sibling 只能校准粒度和读者路径，不能提供要照抄的 target 名、case 名或日志路径。
4. **evidence and docs**：correctness、fallback、asm、bench、board summary、Evidence Doctor、evaluation、topic docs 和 remaining risks 是否一致；QEMU correctness、diagnostic bench、production-shaped bench 和 production-dispatch board evidence 是否分层；stale helper、旧风险、旧结论或未登记覆盖日志是否需要刷新。
5. **closeout hygiene**：dirty isolation 是否只允许当前 topic 或当前 agent instruction patch；`git diff --check`、
   std/RVV correctness、必要 asm / bench / board 边界是否运行或有明确不运行理由；phase result /
   evaluation / Handoff 是否能让下一轮短 prompt 恢复。若本阶段新增 README、topic-local doc suite、
   phase result、summary 或长期 `doc-rvv` 引用，必须对当前 topic 路径执行包含未跟踪文件的扫描，例如
   `git status --short --untracked-files=all -- <topic-paths>` 或
   `git ls-files --others --exclude-standard -- <topic-paths>`。README、evaluation、roadmap 或 Handoff
   引用的新增文档若仍是 untracked（未跟踪）文件，不能声明 closeout / ready_for_review；必须把它们列入
   topic artifact tracking / commit boundary，或移除引用并说明原因。

若 test support 架构已经影响审计定位、证据边界或后续 candidate 扩展，框架化整理本身就是 unblocked next action。worker 不需要等用户显式说“重构测试框架”才把它纳入计划；但必须在 phase plan 中写清范围、保持 case 名 / bench 输出合同的策略和验证命令。
如果 topic 与相邻成熟 topic 有相同模块、相似数据流或相似 test_support 复杂度，worker 必须把相邻 topic 的测试支撑源码布局、聚合入口、内部职责拆分和 topic token 命名作为结构经验审计对象；具体目录和文件名仍按当前 topic 既有结构、`artifact_layout` 与 `test_support` 配置解析。`不要机械复制`
只禁止照搬算法、候选收益或 production 决策；它不允许跳过结构经验迁移审计。若不采用相邻结构经验，
`result.zh.md` 和 Handoff 必须写明为什么当前 topic 不适用、当前暂缓是否影响 reviewer 可读性、
以及下一轮恢复条件。

若配置解析出的 `test_support.internal_directory` 是 `include/impl`，且当前 topic 仍使用旧
`test_support/` 目录保存常规测试支撑内部头，worker 必须把 internal helper layout（内部 helper
布局）列为 structure parity 的独立 area 或与 test source split 合并处理。成熟 sibling 的
`include/impl` 结构是目录职责和 reviewer 路径的 quality bar，不是唯一实现模板；文件名、切分粒度
和具体 helper 仍按当前 topic 决定。不能仅因“路径重命名 churn”“现有 code map 可读”或“等 reviewer
要求”就关闭该 area。若没有真实 blocker，这类迁移是 `phase_deferred + unblocked`，应成为默认下一
phase，直到 adopted、rejected with evidence、not_applicable with evidence 或
turn_stop_deferred with stop_condition_hit。

成熟度审计还必须检查 topic-local doc suite（主题本地文档套件）是否达到当前 topic 复杂度需要。默认 quality bar 来自 `rvv-documentation/references/doc-suite-quality-bar.zh.md`，而不是某个具体 sibling topic。worker 应按该规范审计 `README.zh.md`、`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md`、`doc/<topic>-evaluation.zh.md`、phase index 和 `doc-rvv` 适用性。具体内容可以按当前 topic 裁剪，但结构、读者路径、证据白名单和代码地图必须做 `adopted / rejected with evidence / not_applicable with evidence / turn_stop_deferred with stop_condition_hit` 决策。evaluation 仍在 topic 根目录、缺少 README、缺少测试/bench/代码地图或长期 `doc-rvv` 与 topic-local docs 互相挤压时，都是可继续推进的 unblocked doc-suite action。
若用户 / reviewer 明确指出某个成熟 topic 作为质量参照，它只能作为 optional calibration（可选校准样例）：不复制算法、数值、phase 名、文件名或 production 结论，只补充检查本文 quality bar 是否漏掉了读者路径、证据白名单或代码地图问题。doc-suite parity 不能只写在 roadmap、Handoff 或最终回复里。worker 必须在当前 phase result 中完成审计，或新建明确的 `structure-parity-doc-suite` phase 并产出 `plan.zh.md` / `result.zh.md`。若缺口只涉及 topic-local docs、长期 `doc-rvv` 分工、evaluation、README 导航或 evidence path 对齐，且没有用户限定、dirty isolation、工具失败或真实外部依赖阻塞，则默认下一 phase 必须先补文档套件，不能声明 `ready_for_review`。
文档迁移默认不保留 legacy pointer（旧路径指针）、compatibility alias（兼容别名）或重复正文。只有存在明确外部依赖、用户限定必须兼容、跨 topic 脚本暂时无法同轮更新，或 dirty isolation 会误删用户改动时，才可以临时保留；保留时必须在 phase result / Handoff 写出依赖证据、删除条件和下一阶段删除动作。缺少证据的“避免旧引用断开”不是充分理由。

### Doc Suite Parity Closeout Gate

当当前 topic 命中复杂 topic 条件、存在 production direct 结论、board / Evidence Doctor 证据链，或用户 / reviewer 要求审计文档结构时，production closeout / production-ready / done / stop-for-review 声明必须先满足本门禁。成熟 sibling doc suite 被点名时，只作为 optional calibration；默认规范仍是 `doc-suite-quality-bar.zh.md`。

最小审计表必须覆盖以下文档 area，并使用与 structure-parity phase 相同的列：

```text
| area | current shape scan | quality bar / optional calibration | decision | blocker / evidence | next action |
```

`area` 至少包含：

- README navigation：是否提供“先读哪份文档”、目录分工、常用命令、当前可提交证据、默认不提交的生成产物和当前结果。
- `testing-overview`：是否说明 test / bench / board / QEMU 的入口分类、覆盖矩阵和证据白名单。
- target granularity audit：是否按当前 topic 真实 `Makefile`、`board.mk`、test / bench 源码、script 和 registry 抽取 target，并说明 aggregate target、细分 alias、board smoke、board repeated、doctor / registry、historical guarded probe 的采用、缺失或暂缓状态。
- `correctness-tests`：是否逐个 gtest 或测试族说明输入、被测路径、断言和证明范围。
- `benchmark-and-evidence`：是否说明 bench label、case-filter、QEMU smoke 边界、board repeated target、Evidence Doctor、manifest、registry 和提交边界。
- `optimization-evidence`：是否把 adopted / rejected / deferred 优化方式映射到 production / test_support / bench / board evidence。
- `test-support-code-map`：是否能从文档定位到聚合头、internal helper、src、script、production helper 和 evidence output。
- evaluation：是否承载 EvidenceDecision、Traceability Map、文档分工审计、accepted risk 和不覆盖范围。
- long-term `doc-rvv`：是否只写 adopted production 行为、当前优化方式、dispatch / fallback、范围边界、证据链和长期风险。
- phase index / result：是否记录本次 doc-suite parity 的采用、拒绝、暂缓理由和默认恢复动作。
- artifact tracking：README、evaluation、roadmap、phase index / result 或长期 `doc-rvv` 引用的
  topic-local doc-suite 文件是否已存在并出现在当前 topic 的 tracked / to-be-staged artifact 集合中。
  不能只凭普通 `git status --short` 或本地文件可读性判断文档已闭合；必须用包含未跟踪文件的路径限定扫描确认。

每个 area 的 `decision` 只能是 `adopted`、`rejected with evidence`、`not_applicable with evidence` 或
`turn_stop_deferred with stop_condition_hit`，除非下一阶段就是该 area 的补齐 phase。`deferred` 必须写成
`phase_deferred + unblocked` 并把 `next_phase_default` 指向具体文档补齐 phase。`当前 topic 更小`、`内容较少` 或
`reviewer 需要再补` 只能作为裁剪理由的输入，不能替代证据；若因此不采用 mature sibling 的某个文档形态，必须写明哪类测试、bench、helper 或证据职责在当前 topic 不存在。

### Structure Parity Completion Contract

若当前 topic 存在测试支撑布局、topic-local doc suite、evaluation 主路径、`doc-rvv` 分工或 legacy 清理缺口，worker 必须先按 `artifact_layout`、`test_support` 配置和 `doc-suite-quality-bar.zh.md` 执行 structure parity（结构对齐）审计，而不是只把它写入 roadmap。若当前 topic 与同模块成熟 sibling 有相似复杂度、相似数据流或相似 reviewer 负担，成熟 sibling 只能提供 optional calibration（可选校准样例）；它不能成为唯一参考对象，也不能固定算法、文件名或性能结论。

structure-parity phase 的最小审计表必须覆盖：

```text
| area | current shape scan | config / quality bar / optional calibration | decision | blocker / evidence | next action |
```

`area` 至少包含：

- test/bench source layout：根目录长 `test_*.cpp` / `bench_*.cpp`、`src/`、Makefile / board target / case-filter 是否匹配配置解析结构。
- aggregator and internal helpers：聚合头、单个大 header、旧 `test_support/` 目录、已有 `include/impl/`、reference / fixtures / row source / candidate / assertion / bench harness / bench cases 职责拆分。
- internal helper layout：当配置解析内部目录为 `include/impl` 且当前 topic 仍有旧 `test_support/`
  内部头时，是否迁移到配置目录、更新 include graph / Makefile / 文档引用，并删除无依赖旧入口。
- script and bench registry：topic-local script、case label 字典、bench case registry、checksum / trace / asm 输出合同。
- target granularity：Makefile / board.mk 中 correctness aggregate、correctness aliases、bench diagnostic aliases、QEMU smoke aliases、board smoke aliases、board repeated aliases、doctor / registry aliases 和 historical probe guarded aliases 是否与当前 topic 复杂度匹配；缺失项是否有证据化裁剪或下一阶段动作。
- topic-local docs：README、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map、evaluation 主路径和 phase index。
- long-term docs：`doc-rvv` 主题文档是否只保存长期 production 行为、当前采用方式、证据链和边界，不承担测试工程全量说明。
- legacy compatibility：root evaluation pointer、compatibility alias、旧路径 wrapper、重复正文和旧引用。
- evidence freshness：registry / manifest / Evidence Doctor / `.gitignore` allowlist 是否能支撑当前可提交证据。

如果任一 area 是 `deferred`，且只涉及当前 topic 的测试资产、topic-local 文档、长期主题文档分工、evidence registry 或无外部依赖 legacy 清理，它默认是 `phase_deferred + unblocked`。这类缺口不能因为“低风险”“独立后续范围”“reviewer 如果需要再做”“本阶段已闭合”而被排除在下一 phase 之外。合法暂不继续只允许写成 `turn_stop_deferred`，并必须命中 Continue / Stop Criteria 中的真实停止条件。

`ready_for_review` 只有在 structure-parity 审计表全部为 `adopted`、`rejected with evidence`、`not_applicable with evidence` 或 `turn_stop_deferred with stop_condition_hit` 时才有效。若表里仍有 `phase_deferred + unblocked`，`next_phase_default` 必须指向一个具体 phase，例如 `structure-parity-doc-suite`、`test-source-split`、`internal-helper-layout`、`legacy-cleanup` 或 `row-source-family-carryover`；不能写 `ready_for_review`。

## Optimization Roadmap

复杂 topic、返工 topic、已接入 production 但还有可扩展 row source / 点类型 / `Scalar` / code shape 的 topic，必须维护 roadmap。最小结构为：

```text
# Optimization Roadmap

## 当前边界
## 候选搜索空间
| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |

## 阶段反思新增路线
| phase | new idea | why now | evidence needed | priority |

## 暂缓 / 拒绝路线
| candidate family | reason | resume condition |
```

roadmap candidate family 可以来自模型自行分析、当前源码证据、历史 sibling、开源实现、论文启发、板卡异常或 reviewer / 用户反馈。典型 RVV 搜索维度包括 staging、gather、`vcompress`、scalar tail、vector reduction、block reduction、A/B/C/N block groups、fused formula、FMA contraction、ILP、LMUL m1/m2/m4、unroll、valid-index scan、buffer layout、row source policy、点类型 traits 和 `Scalar` 扩展。

roadmap 更新规则：

- 新 phase 开始前，先读取 roadmap，选择当前最值得闭合的 unblocked candidate 或结构 maturity action。
- roadmap 中的“默认恢复动作”、`next_phase_default`、`resume condition`、`next action` 或等价字段
  必须被解析成 `roadmap_default_recovery_queue`。该队列是短 prompt 恢复合同，不是人工 follow-up
  清单。每项必须标注 `phase_deferred + unblocked`、`turn_stop_deferred with stop_condition_hit`、
  `blocked`、`rejected with evidence` 或 `not_applicable with evidence`。
- 如果 roadmap 把某些动作写成“若 reviewer 要求继续”，worker 仍要重新判断它们是否其实已在当前
  prompt 授权范围内。测试源码拆分、internal helper layout、topic-local docs、evidence registry
  和无依赖 legacy 清理默认属于当前 topic 内的可执行结构动作；没有真实 blocker 时不得等待 reviewer
  二次触发。
- 多个未阻塞恢复动作不能被压缩成一个 `ready_for_review_validity_checked` 状态。worker 必须选择
  队列中第一个未阻塞 phase 并推进；若两个动作触碰同一批测试资产，例如 `test-source-split` 和
  `internal-helper-layout`，可以合并成一个窄结构 phase，但 phase plan 必须逐项列出二者的完成判据。
- 若 mature sibling parity audit 发现当前 topic 缺少 source / aggregator / internal helper 布局、完整 topic-local doc suite、evaluation 主路径迁移或 legacy 清理，且这些动作仍在当前 topic 测试 / 文档边界内，roadmap 必须把它们合并成高优先级 structure-parity phase，默认排在 evidence registry、helper shape 微调和可选性能探索之前，除非写出真实阻塞或更高风险证据动作。
- phase 结束后，必须把实际发现的新候选、负向解释、异常模式和可继续动作回填 roadmap。这个更新应像 evolutionary search（演化式搜索）的候选前沿：保留已经 validated（验证通过）的路线、带证据拒绝的路线、因阻塞暂停的路线，以及阶段反思中新产生的路线；下一 phase 从最高优先级未阻塞项中选择，而不是回到人工待办清单。
- 阶段反思不能只写“无新增”。若本阶段完成了结构迁移、证据登记、candidate A/B、负向归因或文档重构，worker 必须至少判断是否生成了新的测试输入、消融、ILP / LMUL、row source、doc-suite 或 agent asset candidate；确无新增时写证据化理由。
- `deferred` candidate 必须写 resume condition（恢复条件），例如需要板卡、asm、production direct、输入语义审计或先完成 test_support 拆分。
- roadmap 不写成“以后有空可以做”的松散清单。每个 unblocked 高优先级 candidate 应能导出下一阶段 plan，或明确说明为什么暂时不做。
- 若 roadmap 中仍有当前 topic 授权范围内的高优先级 unblocked candidate，worker 不应把 topic closeout 写成完成。
- roadmap 中禁止用 `reviewer-triggered`、`optional` 或 `low priority` 掩盖当前授权范围内的结构成熟度缺口。若缺口会影响短 prompt 恢复、reviewer 定位、证据边界或下一 candidate 扩展，它就是 high-priority structure maturity action。

## Optimization Matrix

复杂 topic 应维护一张可逐阶段增量更新的矩阵。最小列为：

```text
| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
```

推荐状态：

- `adopted`：当前边界下已采用，必须有足够的 correctness、性能和归属证据。
- `attempted`：已执行但证据或收益不足，必须记录结果和风险。
- `rejected`：当前边界下有证据表明不适用或不值得继续。
- `deferred`：尚未闭合，必须写恢复条件，不得伪装成完成。
- `blocked`：依赖板卡、工具、用户授权或矛盾证据，写明解除条件。
- `not_applicable`：当前 topic 边界确实不适用，必须给源码或输入语义理由。
- `planned`：仅有计划，不能在 Handoff 或 closeout 中写成已覆盖。

`deferred` 只表示当前 phase 未闭合，不自动表示本轮可以停止。建议区分：

- `phase_deferred`：当前 phase 不做或没做完，但仍在当前 topic 授权范围内，且没有高风险阻塞。worker 默认创建或修订下一 phase plan 并继续。
- `turn_stop_deferred`：本轮可以合法停止。必须命中明确 stop condition，例如用户限定范围、继续会扩大到未授权 production / public API / 其它 topic、板卡或工具不可用、证据矛盾、dirty isolation 不安全，或当前矩阵和 roadmap 已无授权未阻塞动作。低风险测试优化、测试支撑结构迁移、topic-local 文档拆分、evaluation 主路径迁移和无外部依赖的 legacy 清理，默认不能转成 `turn_stop_deferred`。

如果一个条目是 `phase_deferred + unblocked`，`result.zh.md`、optimization matrix、roadmap 和 Handoff 都必须写出下一阶段动作。最终回复也要明确“这些没有做，但仍可继续”，不能只写“已完成”。若 worker 选择停止，必须把这些条目放入面向用户的显式清单，而不是藏在 `remaining_risks`、roadmap 底部或 `follow-up only` 的宽泛描述里。

当 topic 存在多个 row source policy（尤其 registration topic 中常见的 `ordered-cloud-pair`、`source-indexed-cloud-pair`、`dual-indexed-cloud-pair` 和 `correspondence-pair`）时，它们必须独立批准。一个 policy 的 adopted family 不会自动关闭其它 policy。代表性点型、`Scalar`、布局和规模也必须在矩阵中单独标出；代表性性能不能外推成全泛型生产性能。

当候选涉及公式、FMA、reduction、staging、ILP 或 LMUL 时，矩阵中必须能回到对应的数值预算、反汇编归属和板卡 A/B 证据。只有源码形式变化、没有机器码或同边界性能差异的候选，标为实现形态诊断，不能写成独立收益。

## 执行循环

worker 按下列步骤循环，直到命中停止条件：

1. **恢复**：读取 Handoff 的 `phase_loop_state`、阶段 README、当前 plan/result、optimization matrix 和相关证据；核对当前 git status 与允许路径；如果 topic 已有 `evidence_registry.json` 或等价机制，先检查是否存在 `unregistered_change`、`unregistered_file` 或 `stale_doc_pending_refresh`。
2. **读/建 roadmap**：读取 `artifact_layout.optimization_roadmap_template` 解析出的 roadmap；不存在时先创建；已有内容与当前源码、phase result、Evidence Doctor 或文档结构冲突时先标记并修订。
3. **建计划**：没有当前 plan 时先写；已有 plan 与当前源码或证据不一致时先修订并记录变更原因。计划必须先于本阶段任何实现或测试资产修改。
4. **冻结门禁**：完成 `phase_plan_written_before_edits`、roadmap、范围、依赖、文档归属、Evidence Doctor 输入和继续 / 停止条件检查。
5. **连续推进**：按依赖顺序完成一个足够大的闭环，通常包括 candidate / test、correctness、QEMU 路径检查、bench / ablation、asm、板卡或明确的板卡阻塞处理。只完成隔离层、一个 target、一个局部 bench 或一个文档段落，不等于阶段完成。若板卡已配置、可达或用户已说明可用，而当前矩阵需要板卡证据，worker 必须在有界复跑预算内继续跑板卡 target、summary、Evidence Doctor 和 registry 刷新；不能停在“等待板卡验证”。
6. **解释证据**：把实际结果、输入口径、A/B 边界、checksum、长尾、异常频率、decision bucket、rerun budget、asm attribution、目标硬件和不能证明的范围写入 `result.zh.md`，并更新矩阵状态。
7. **阶段反思**：用本阶段证据反推是否出现新的 candidate family、消融需求、ILP / LMUL 取舍、文档结构缺口或测试输入缺口；把它们更新到 roadmap，并标注优先级、证据需求和恢复条件。
8. **更新计划**：将剩余动作按 `blocked` / `unblocked` 标记；为下一阶段写默认目标或创建下一阶段 plan。计划变更必须保留原因，不得把未执行动作直接勾成完成。
9. **继续 / 停止决策**：如果 roadmap 或矩阵中存在授权且未阻塞的下一动作，默认继续同轮推进；只有命中明确 stop condition 才输出 Handoff 并停止。若本阶段只是建立 roadmap、迁移单份 evaluation、补一个指针或完成一个局部 layout 子任务，而结构 parity / doc suite / legacy 清理仍未闭合，不能把 `next_phase_default` 写成 `ready_for_review`。如果最近 Handoff 或 phase README 已经写了 `ready_for_review`，但恢复扫描发现 roadmap / matrix / mature sibling parity 仍有 `phase_deferred + unblocked`，worker 必须把该 `ready_for_review` 降级为 stale stop decision（过期停止决策），并恢复到第一个未阻塞 phase。
   若 roadmap 的“默认恢复动作”列出多个候选，worker 不能只复述该列表后停止；必须执行或计划执行
   队列中的未阻塞项。`ready_for_review_validity_checked` 是检查结果标签，不是 stop condition。

worker 应优先完成能改变决策的证据链，而不是堆积无关 case。阶段大小由“是否形成可审查的决策闭环”决定，不由文件数量决定。

## Evidence Doctor 异常处理

benchmark、board summary、checksum summary、asm attribution 或 EvidenceDecision 前，必须运行 `artifact_layout.evidence_doctor_script_template` 解析出的 Evidence Doctor 脚本，或按 `evidence-doctor.zh.md` 人工记录结果。阶段 result 至少记录输入、严重级别数量、每项异常、处理动作和对结论的影响。

- **Error**：例如 checksum 不一致、strict A/B 缺一侧、boundary / wrapper / row source / solve / timer boundary 不一致且未降级、关键 metadata 缺失。先修复并重跑，或把证据降级 / 标为 blocked；不能用未处理 Error 关闭阶段。
- **Warning**：例如 B/A 方向异常、长尾、点型偏离、环境字段缺失、asm 归属不闭合、名称与 metadata 角色冲突。必须写可能解释、最小验证动作和结论边界；未解释 Warning 时不能写成 clean pass。
- **Suggestion**：例如扩大 runs、补 binary hash、补 trace、补温度 / governor / freq 或迁移 topic-local wrapper。可以不阻塞当前阶段，但必须放入矩阵或下一阶段动作，不能无记录丢弃。

如果只有 Markdown 汇总而没有完整机器可读 metadata，结果必须标记 `metadata_incomplete`；它只能作为 reviewer aid，不能替代完整 Evidence Doctor。异常数据不能先验删除，除非证明是测量污染并保留删除理由。

## Phase Result 最小合同

阶段完成或停止前，`result.zh.md` 至少回答：

- 计划版本和实际执行范围是否一致；偏差是什么、为什么发生。
- 每个计划动作的 `done / partial / deferred / blocked` 状态、命令、证据路径和结论。
- optimization matrix 的更新，以及 adopted / attempted / rejected / deferred 的理由。
- correctness、QEMU、asm、board performance 和 production boundary 的分层结论。
- Evidence Doctor 的 Errors / Warnings / Suggestions、异常解释、重跑 / 降级 / 阻塞动作。
- evidence registry 状态：是否 fresh、是否发现人工或未登记复跑、哪些文档需要刷新。
- 板卡 rerun budget 和 decision bucket：是否用完预算、桶是否稳定、是否因此降级或需要人工判断。
- 当前阶段是否完成，未完成项是否还有 unblocked。
- `continue_stop_decision`、`stop_condition_hit` 和 `next_phase_default`。

阶段 result 不替代 topic evaluation、主题文档或 Handoff：它保存阶段探索和测试事实，长期文档只引用已经稳定的结论和证据路径。

## Continue / Stop Criteria

默认继续，尤其测试优化、测试支撑结构整理、topic-local 文档拆分、evaluation 迁移、README / doc suite 对齐、无依赖 legacy pointer / alias 删除、诊断 candidate / bench / asm / Evidence Doctor 补齐等仍在当前 topic 测试资产或文档边界内的动作，通常应继续推进。以下任一条件成立才允许停止当前 worker 轮次：

1. 当前 phase plan 的完成矩阵已闭合，roadmap 和 optimization matrix 均没有授权、未阻塞的 high-priority next action；下一阶段已经明确标为 `not_yet_started`，并有可恢复的 plan 入口。
2. 用户明确限制本轮范围，且 worker 已完成该范围并记录剩余 loop 状态。
3. 继续需要扩大到未授权的 production 文件、public API、其它 topic、其它入口 / 点型 / `Scalar` / row source，或需要用户批准生产接入。
4. 需要板卡、工具链、远端环境或依赖，当前无法获得；已完成可运行的本地证据，并记录解除阻塞的命令与路径。若配置或当前会话已经确认板卡可用，`需要板卡验证` 不命中此停止条件，worker 必须继续执行有界板卡验证。
5. 板卡复跑预算已经按 plan 用完：如果 decision bucket 稳定，可以用该桶关闭当前证据动作；如果 bucket 仍摇摆，必须标为 `unstable`、降级 EvidenceDecision 或交给 reviewer / 用户判断，而不是继续自动复跑。
6. Evidence Doctor Error 未能修复，registry 显示未登记变更无法归属，或不同证据层之间矛盾，需要 reviewer / 用户判断。
7. dirty isolation 不安全，无法确认哪些文件属于当前阶段，或用户已有修改会被覆盖。
8. 当前结论已经满足 closeout 条件，且剩余方向属于另一个 topic 或明确的用户选择，而不是本阶段计划内动作。

下列情况不是合法停止理由：只补完一个 helper、一个隔离层、一个 target、一次 bench、一个 summary、一个 phase 表，或“已经有一个正向结果”。命中 `micro_stop_guard` 时必须继续当前计划的下一个 unblocked action，或明确写出上面的真实阻塞条件。

如果 worker 决定停止但仍存在 `phase_deferred + unblocked` 项，最终回复、`result.zh.md` 和 Handoff 必须用显式清单写出：没有做的事项、为什么本轮没有做、继续是否仍在当前授权范围、默认下一 phase 入口和需要用户 / reviewer 判断的点。不能把这些只藏在 “remaining risks” 或 “follow-up” 的泛泛表述里。

`ready_for_review` 合法性必须单独判断。以下任一情况存在时，`ready_for_review` 无效，必须继续或输出带真实 stop condition 的 blocked / turn-stop Handoff：

- mature sibling parity 审计表中仍有 source layout、internal helper、doc suite、evaluation 主路径、`doc-rvv` 分工或 legacy 清理的 `phase_deferred + unblocked` 项。
- roadmap 把当前授权范围内的结构成熟度或测试优化写成 `reviewer-triggered` / `optional`，但没有证明继续会扩大权限、破坏 dirty isolation、需要不可用板卡 / 工具或引入证据矛盾。
- phase result 写 `unblocked_next_actions=none inside Phase N`，但 roadmap / optimization matrix 仍列有下一个可执行 phase。
- 旧 pointer / alias 被保留，且缺少外部依赖、用户要求、dirty isolation 风险或同轮无法更新引用的证据。
- Handoff 没有把“没有做但可以继续做”的事项显式暴露给用户。

## Reviewer 早停检查

reviewer 必须从文件检查 worker 是否过早停止，而不是接受“本轮完成”这句总结。至少核对：

- 当前 plan 是否在首次编辑前存在，时间 / diff 顺序是否合理。
- plan 的每个动作是否在 result 和矩阵中有 `done / partial / deferred / blocked` 证据。
- 是否仍有 `unblocked_next_actions`，若有，worker 是否错误地停下。
- roadmap 是否存在、是否记录阶段反思新增 candidate、是否仍有 high-priority unblocked candidate；若有，worker 是否错误地把 `phase_deferred` 当作 `turn_stop_deferred`。
- mature sibling parity 是否被执行成默认下一 phase；如果只在 roadmap 中记录，reviewer 应检查是否存在无阻塞的 source / include / doc suite / legacy 缺口，并把错误的 `ready_for_review` 判为早停。
- shape scan 是否先枚举当前 topic 的真实文件形态，包括根目录源文件、`src/`、聚合头、单大 header、旧 `test_support/`、`include/impl`、script、bench registry、Makefile、topic-local docs、`doc-rvv` 和 evidence registry；不能只检查某个固定目录是否存在。
- 是否把 `planned`、局部 positive、QEMU timing 或 diagnostic evidence 写成 adopted / production 结论。
- 是否为板卡复跑预设预算和 decision bucket，且没有因数字小幅波动进入无限复跑。
- 是否检查 evidence registry 或等价 freshness 状态；若有人工复跑或未登记覆盖，是否标为 stale / refresh pending。
- Evidence Doctor Warning / Error 是否被解释、重跑、降级或阻塞。
- 阶段 result 是否说明继续 / 停止决定、停止条件和下一阶段默认入口。
- Handoff 的 phase loop 状态、计划路径、结果路径和 dirty isolation 是否能让下一轮一句短 prompt 恢复。

发现早停时，reviewer 应将其列为至少 `High` finding，给出可直接转发的 prompt patch，要求 worker 回到当前 plan 的第一个 unblocked action；不要用“建议以后补”掩盖当前工作流未完成。
