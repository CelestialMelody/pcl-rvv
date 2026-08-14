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
| `doc/testing-overview.zh.md` | 测试体系总览 | 测试类型定义、运行入口分类、覆盖矩阵、QEMU / board / production direct 证据边界、证据白名单。 |
| `doc/correctness-tests.zh.md` | gtest 语义 | 每个 TEST 或测试族的输入、被测路径、断言、证明范围、不能证明的范围、默认 target 和日志路径。 |
| `doc/benchmark-and-evidence.zh.md` | bench 和证据口径 | CLI / case-filter 字典、bench label 语法、计时边界、checksum 来源、QEMU smoke、board repeated target、summary / manifest / doctor、registry、复现命令和提交边界。 |
| `doc/optimization-evidence.zh.md` | 候选证据索引 | adopted / attempted / rejected / deferred / not_applicable 候选到 production / test support / target / board / asm / doctor / decision 的映射。 |
| `doc/test-support-code-map.zh.md` | 测试支撑代码地图 | 聚合入口、internal helper、fixtures、reference、candidate、bench harness、script、production 对照、调用图和拆分审计。 |
| `doc/optimization-roadmap.zh.md` | 跨阶段搜索空间 | candidate family、idea source、风险、所需证据、状态、默认恢复队列、阶段反思新增路线和拒绝 / 暂缓条件。 |
| `doc/phases/README.zh.md` | phase 恢复入口 | 当前恢复入口、阶段表、文档归属、早停规则、默认下一动作。 |
| `doc/<topic>-evaluation.zh.md` | 函数级评估和 closeout 主归属 | 标量路径、RVV 边界、EvidenceDecision、生产接入判断、诊断证据链、Traceability Map（可追踪性地图）、遗留风险。 |
| `doc-rvv/<module>/<topic>-RVV.zh.md` | production 长期主题文档 | 只在 adopted production behavior（已采用生产行为）、production patch（生产补丁）或 PI5 生产证据闭环通过后适用；不承载 no-production 的测试工程全量说明。 |

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
- 若 sibling 与本文 quality bar 冲突，以本文和当前 topic 的源码 / 证据为准，并在 phase result 或 Handoff 写出 `agent_asset_feedback`。
- 若某个好做法会跨 topic 复用，应沉淀回本文或其它 `.agents` reference，而不是让未来 worker 继续依赖那个 sibling 路径。
