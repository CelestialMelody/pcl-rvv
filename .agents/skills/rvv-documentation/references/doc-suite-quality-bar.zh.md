# Topic-local Doc Suite Quality Bar

本文定义复杂 RVV topic（主题）的 topic-local doc suite（主题本地文档套件）质量门槛。它是默认规范源，不依赖任何具体 sibling topic（同模块相邻主题）。成熟 sibling 可以作为校准样例，但只能补充审计，不能替代本文的规范、模板和裁剪规则。

## 何时读取

- 新建或重排 `artifact_layout.topic_test_dir_template` 解析目录下的 `README.zh.md`、`doc/*.zh.md` 或 `doc/phases/**` 时读取。
- closeout、production-ready、done、stop-for-review 或 `ready_for_review` 前，当前 topic 命中复杂 topic 条件、存在 production direct（真实生产路径证据）、board summary（板卡摘要）、Evidence Doctor（证据体检）或多阶段 phase loop（阶段循环）时读取。
- 用户或 reviewer 要求“文档对齐”“可审查性”“文档结构是否像成熟 topic”时读取。若用户点名某个成熟 sibling，只把它作为 `optional calibration`，不要复制 topic-specific（当前主题特有）的算法、数值、phase 名或结论。

## 质量目标

topic-local doc suite 应让下一轮 worker 或 reviewer 不依赖聊天上下文，就能回答：

- 当前 EvidenceDecision（证据决策）是什么，production 是否适用。
- public entry（公开入口）、标量路径、diagnostic candidate（诊断候选）、bench case（性能测试用例）和 output summary（输出摘要）如何互相定位。
- 每个 test、bench、board target 和 Evidence Doctor 报告能证明什么，不能证明什么。
- 若 no-production（不接入生产）或 rollback/no-production（回滚且不接入生产），诊断证据为什么不能替代 production evidence（生产证据）。
- 若还有未阻塞动作，下一 phase 应从哪里恢复。

## 推荐文档套件

复杂 topic 默认维护以下文档。当前 topic 确实没有对应职责时，可以裁剪，但必须写 `not_applicable with evidence`，列出不存在的测试、bench、script、row source 或 production 行为。

| 文档 area | 主职责 | 最小内容 |
| --- | --- | --- |
| `README.zh.md` | 入口导航 | 当前结论、production 文件、先读哪份文档、目录分工、常用命令、当前可提交证据、默认不提交的生成产物、`doc-rvv` 是否适用。 |
| `doc/testing-overview.zh.md` | 测试体系总览 | 测试类型定义、运行入口分类、target 粒度审计、覆盖矩阵、QEMU / board / production direct 证据边界、证据白名单。 |
| `doc/correctness-tests.zh.md` | gtest 语义 | 每个 TEST 或测试族的输入、被测路径、断言、证明范围、不能证明的范围、默认 target 和日志路径；若缺少细粒度 correctness alias，说明是否补齐或暂缓。 |
| `doc/benchmark-and-evidence.zh.md` | bench 和证据口径 | CLI / case-filter 字典、bench label 语法、计时边界、checksum 来源、QEMU smoke、board smoke / repeated target、summary / manifest / doctor、registry、复现命令和提交边界。 |
| `doc/optimization-evidence.zh.md` | 候选证据索引 | adopted / attempted / rejected / deferred / not_applicable 候选到 production / test support / target / board / asm / doctor / decision 的映射。 |
| `doc/test-support-code-map.zh.md` | 测试支撑代码地图 | 聚合入口、internal helper、fixtures、reference、candidate、bench harness、script、production 对照、调用图和拆分审计。 |
| `doc/optimization-roadmap.zh.md` | 跨阶段搜索空间 | candidate family、idea source、风险、所需证据、状态、默认恢复队列、阶段反思新增路线和拒绝 / 暂缓条件。 |
| `doc/phases/README.zh.md` | phase 恢复入口 | 当前恢复入口、阶段表、文档归属、早停规则、默认下一动作。 |
| `doc/<topic>-evaluation.zh.md` | 函数级评估和 closeout 主归属 | 标量路径、RVV 边界、EvidenceDecision、生产接入判断、诊断证据链、Traceability Map（可追踪性地图）、遗留风险。 |
| `doc-rvv/<module>/<topic>-RVV.zh.md` | production 长期主题文档 | 只在 adopted production behavior（已采用生产行为）、production patch（生产补丁）或 PI5 生产证据闭环通过后适用；不承载 no-production 的测试工程全量说明。 |

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
| correctness aggregate（正确性汇总入口） | 是否有一条总入口能跑完整 Std / RVV correctness；日志是否稳定可引用。 | `testing-overview`、`correctness-tests`、README。 |
| correctness aliases（正确性细分入口） | 是否需要按 public semantics（公开入口语义）、input semantics（输入语义）、candidate correctness（候选正确性）、fallback（回退路径）或 production direct（真实生产路径）拆分 gtest target。 | `testing-overview` 的运行入口分类、`correctness-tests` 的 TEST 字典。 |
| bench diagnostic aliases（bench 诊断入口） | case-filter 是否能隔离 row source（行来源）、candidate family（候选族）、component ablation（组件消融）或 output contract（输出合同）。 | `benchmark-and-evidence`、`optimization-evidence`。 |
| QEMU smoke aliases（QEMU 小型验证入口） | QEMU 是否只用于 build / correctness / log-shape（日志形状）；是否避免完整 bench compare 被写成性能证据。 | `testing-overview`、`benchmark-and-evidence`。 |
| board smoke aliases（板卡小型验证入口） | 单次板卡 target 证明什么：可运行、correctness、checksum 或输出形状；是否没有被写成 repeated performance。 | `testing-overview`、`benchmark-and-evidence`。 |
| board repeated aliases（板卡重复采集入口） | 哪些 target 生成 repeated summary、manifest、Evidence Doctor；run budget 和 decision bucket 是否明确。 | `benchmark-and-evidence`、phase result、Handoff。 |
| doctor / registry aliases（证据体检和登记入口） | 是否有生成 / 检查 manifest、Evidence Doctor 和 evidence registry 的 target 或脚本；未接入时是否有人工检查路径。 | `benchmark-and-evidence`、phase result、Handoff。 |
| historical probe guarded aliases（历史探针保护入口） | 历史 production probe、回滚探针或不再默认运行的 target 是否有显式开关、误用保护和证据降级说明。 | README、`testing-overview`、`benchmark-and-evidence`、phase result。 |

如果 target 粒度不足已经影响 reviewer 定位、证据边界或后续 candidate 扩展，worker 应在当前 phase 补齐 alias target，或把它写成 `phase_deferred + unblocked` 的下一阶段动作。只有用户限定范围、dirty isolation 风险、工具 / 板卡不可用、需要扩大到 production / public API / 其它 topic，或存在外部脚本依赖时，才能写成 `turn_stop_deferred with stop_condition_hit`。

文档不得虚构不存在的 target。若当前工程只有一个 aggregate target，`testing-overview` 可以先把 gtest、case-filter、board summary 和 EvidenceDecision 的映射写清；同时在 doc-suite parity 审计表中说明是否需要补 correctness aliases、bench aliases、board repeated aliases、doctor / registry aliases 或 historical probe guard。

## 最小模板骨架

### README

```text
# <topic> RVV 主题入口
## 当前结论
## 先读哪份文档
## 目录分工
## 常用命令
## 当前可提交证据
## 默认不提交的生成产物
## doc-rvv 适用性
```

### testing-overview

```text
# 测试体系总览
## 本文职责
## 文档阅读路径
## 测试类型定义
## 运行入口分类
## Target 粒度审计
## 细粒度 Target
## 测试流程
## 输入数据总览
## 覆盖矩阵
## 当前可提交证据
## 默认不提交的生成产物
## 当前结论边界
```

### correctness-tests

```text
# 正确性测试说明
## 本文职责
## 测试文件分工
## 共同输入和断言
## TEST / 测试族字典
## 边界和随机样本策略
## 验证命令
```

### benchmark-and-evidence

```text
# Benchmark 与证据说明
## 本文职责
## Bench 输出格式
## CLI 参数
## Bench Label / case-filter 字典
## 推荐 Target
## 计时边界
## Checksum 来源
## 当前 QEMU 证据
## 当前 Board 证据
## Evidence Doctor / Manifest 边界
## ASM Attribution 口径
## 复现命令
## 提交边界
```

### optimization-evidence

```text
# 优化证据索引
## 本文职责
## 当前结论摘要
## 优化方式总表
## 标量路径与 RVV 路径差异
## 代码级证据索引
## 细粒度 target 字典
## 当前可提交证据
## 结论边界
```

### test-support-code-map

```text
# 测试支撑代码地图
## 本文职责
## 总调用图
## 稳定聚合入口
## Fixtures 与输入构造
## 标量 Reference
## Candidate / Diagnostic Helper
## Bench Harness 与 Case Registry
## Scripts 与 Evidence Output
## Production 与 Test Support 边界
## 拆分审计
```

### phases/README

```text
# 阶段索引
## 当前恢复入口
## 阶段表
## 文档归属
## 当前早停规则
```

## 审计表

doc-suite quality bar 审计表使用以下列。`quality bar / optional calibration` 先写本文规范；只有用户、reviewer 或本地历史明确点名成熟 sibling 时，才在同一格补充 sibling calibration（成熟样例校准）。

```text
| area | current shape scan | quality bar / optional calibration | decision | blocker / evidence | next action |
```

`decision` 只使用：

- `adopted`：当前 topic 已达到或本 phase 已补齐。
- `rejected with evidence`：当前 topic 不采用该文档形态，并有源码、测试、bench、script 或 production 边界证据。
- `not_applicable with evidence`：当前 topic 确实没有对应职责，并列出不存在的对象。
- `phase_deferred + unblocked`：仍在当前 topic 授权范围内，下一 phase 默认继续。
- `turn_stop_deferred with stop_condition_hit`：本轮合法停止，必须命中用户限定、dirty isolation 风险、工具 / 板卡不可用、扩大到未授权 production / public API / 其它 topic，或真实外部依赖。

不能使用“topic 较小”“等 reviewer 要求”“内容已经够看”单独关闭缺口。它们只能作为裁剪判断的输入，必须配合证据说明当前缺少哪类职责。

## Artifact Tracking

README、evaluation、roadmap、phase result 或长期 `doc-rvv` 引用的 topic-local doc-suite 文件必须存在，并在当前 topic 的 tracked / to-be-staged artifact 集合中，或明确标为 local-only / excluded 且不作为提交后入口。

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
