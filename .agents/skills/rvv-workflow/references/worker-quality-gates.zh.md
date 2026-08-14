# RVV Worker 质量门禁

本文是短 prompt worker（执行者）的轻量质量门禁。它不替代各 skill（技能）的详细规则，
只规定 worker 在写代码、文档、bench 或 handoff 前必须产出的最小结构。

## 何时读取

- 短 prompt 进入普通 topic 后、开始写配置解析出的 topic 测试资产、topic 文档或 production 前读取。
- 如果 topic 涉及 staging（分阶段暂存）、gather（离散加载）、`vcompress`、scalar tail（标量尾段）、
  vector reduction（向量规约）、FMA（融合乘加）、目标硬件性能或 no-production closeout，
  必须继续读取本文列出的详细 reference。
- 如果 topic 从 `partial-production-candidate`（局部生产候选）继续到 PI1 production integration plan
  （生产接入计划），必须读取 `rvv-implementation/SKILL.md`、`point-load-store.md`、
  `fallback-and-dispatch.md`；若生产入口是模板点类型或要从 `PointNormal` 诊断扩展到泛型入口，
  必须读取配置或 adapter 指定的 generic point type strategy（泛型点类型策略）文档。
- 如果模板点类型算法会构造或写回 `PointT` 输出，或使用 `PointT` 运算符、`FieldList`、
  `copyPoint`、`CentroidPoint`、RGB/RGBA 特化等整点语义，不能只按 xyz traits gate 判断；
  必须读取 `RVV Generic Point Type Strategy.zh.md` 中“输入字段 Gate 不等于输出 PointT 语义”。
- 如果 worker 声明采用 sibling topic（同模块相邻主题）经验，或当前 topic 的设计明显来自
  相邻成功 / 负向案例，S0 后、写文件前必须做 experience-migration audit（经验迁移审计）。
  该审计不要求照搬相邻 topic 的具体算法，只要求列清哪些经验被采用、尝试、暂缓或拒绝。
- 如果短 prompt 是“继续完善 <topic> 的 RVV 优化工作”、当前 topic 已有 phase plan/result，
  或当前计划仍有 `unblocked_next_actions`，必须读取 `rvv-test/references/optimization-phase-loop.zh.md`，
  并把 phase loop 状态、optimization roadmap 状态和 `phase_deferred` / `turn_stop_deferred` 判断纳入写文件前自查和 Handoff Packet。

## 最小门禁

### 1. 偏好配置冻结

S0 必须读取 `.agents/config/defaults.yaml`。如果存在 `.agents/local/user-preferences.yaml`，也必须读取。
当前 prompt（提示词）中的明确要求覆盖配置文件。

worker 必须在 S0 报告和最终 Handoff Packet（交接数据包）中写清：

- `preferences_loaded`：读取了 defaults、local override（本机私有覆盖）或 prompt override（提示词覆盖）中的哪些层。
- `comment_policy_frozen`：配置解析出的测试资产、diagnostic（诊断代码）、prototype（原型代码）和 production（生产源码）的注释策略。
- `evidence_policy_frozen`：evidence logs（证据日志）策略，默认 `summary-only`；raw logs（原始日志）不默认提交。
- `documentation_policy_frozen`：closeout（收尾文档）是否 current-state-first（当前状态优先）、是否必须有数值算例、长期文档是否禁止保留对话流程话术。

如果 local override 中配置了板卡、依赖库、交叉编译工具链或私有路径，Handoff 只报告“已读取对应覆盖项”和使用的 env var（环境变量）名。不要复制 IP、用户名或个人绝对路径。

S0 字段级合同、artifact layout（产物布局）解析和 artifact publication（产物发布）判断见 `rvv-workflow/references/s0-preferences-and-recovery.zh.md`；S0 输出要把 `preferences_loaded`、`frozen_policies`、`resolved_artifacts`、`artifact_publication_decision`、`dirty_isolation`、`validation` 和 `next_action` 回填到 Handoff Packet（交接数据包），而不是散落在不同段落里。

### 2. 标量路径重建

S2 evaluation（函数级评估）不能只写函数名或数学名词。worker 必须写清：

- public entry（公开入口）和 wrapper / iterator / dispatch（包装层 / 迭代器 / 分流逻辑）如何进入目标 helper；
- 关键循环、有限值检查、关键公式、状态更新、solver（求解器）和输出构造；
- 哪些阶段在逐点热点循环内，哪些阶段每次调用只执行一次；
- RVV 计划覆盖哪一段标量路径，哪些阶段保留标量以及原因。

详细规则见 `rvv-documentation/references/evaluation-doc-structure.md`。

### 3. Production 数据流与诊断数据流映射

如果 production 源码通过 iterator、indices、correspondences、wrapper 或 dispatch 把多种入口统一，
而 diagnostic（诊断代码）为了 RVV 显式拆成 full-cloud（全云顺序扫描）、gather、scatter 或 staging，
worker 必须写清映射关系：

- 每条 diagnostic 路径来自哪个公开入口；
- 额外 index/weight 展开、offset 计算、buffer 写回是否计入 bench；
- 这些拆分能证明什么，不能证明什么。

详细规则见 `rvv-test/SKILL.md`、`rvv-test/references/entry-shapes-and-test-support.zh.md`、
`rvv-test/references/registration-topic-evidence.zh.md` 和 `rvv-documentation/references/topic-doc-structure.md`。

### 4. Sibling Experience Migration Audit

如果 worker 在 prompt、设计说明、Handoff Packet 或最终回复中写到“参考 / 迁移 / 复用 sibling topic
经验”，必须在 S3/S4 前输出 experience-migration audit（经验迁移审计）表。该表是防止历史经验只被
口头引用的输出合同，不是要求所有 topic 都实现相同候选。

推荐列：

```text
| sibling 经验维度 | sibling topic 里的机制 | 当前 topic 是否适用 | 状态 | 证据 / 理由 | 下一步 |
```

`状态` 使用：

- `adopted`：已经采用，并说明当前 topic 的落点和验证证据。
- `attempted`：已经尝试，但结果、风险或证据不足以采用；必须说明命令、diff 或实验记录。
- `deferred`：暂缓到下一轮或另开 topic；必须说明暂停条件和恢复所需证据。
- `rejected`：明确不适用或不建议；必须说明当前源码、数据流、语义或性能边界为什么不同。

至少覆盖这些维度：

- row source（行来源）：full-cloud、source-indexed、dual-indices、correspondences 或其它入口形态。
- source / weight policy（源 / 权重策略）：index、weight、field offset、valid-index-only 和展开成本。
- shared math pipeline（共享数学流水线）：finite mask、formula、accepted_points、ATA/ATb、输出容器或状态更新。
- staging / reduction（暂存 / 规约）：`vcompress`、buffer 写回、scalar tail、vector reduction、block reduction 或其它候选组织。
- formula / FMA（公式 / 融合乘加）：逐点公式树、FMA contraction（融合乘加收缩）、reduction tree 和误差预算。
- evidence model（证据模型）：correctness、QEMU path、反汇编归属、component ablation、repeated board A/B。
- production boundary（生产边界）：dispatch、fallback、点类型 traits、代表点型、`Scalar` 和不扩大范围。

若相邻 topic 有成功的 test support reduction role（测试支撑规约职责）、block-reduction、fused formula、staging split、
component ablation 或负向历史方案，worker 必须在表中审计它们。可以合理拒绝或暂缓，但不能只写
“已参考相邻经验”而不列出未采用的主线。

### 5. 文档发布边界和主题文档质量

`doc-rvv` 主题 RVV 文档是 production 长期主题文档，不是 no-production closeout 的默认载体。worker 必须先判断 `artifact_layout.topic_doc_template` 是否适用：

- `production_topic_doc_applicable`：存在 adopted production behavior（已采用生产行为）、production patch（生产补丁）或 PI5 生产证据闭环通过。
- `production_topic_doc_not_applicable`：当前结论是 `diagnostic`、`bench-only`、`rollback/no-production`，或未接 production 的 `partial-production-candidate`。此时不创建 `doc-rvv`；诊断证据链写入 topic-local evaluation / phase closeout / roadmap / matrix / Handoff。

适用时，production 长期主题文档至少要包含：

- 函数入口作用和标量路径；
- RVV 数据流、VL chunk（可变向量长度分块）、mask（掩码）、staging、tail 和 fallback（回退路径）；
- 实现选择审计，例如 buffer、`vcompress`、scalar tail、FMA、vector reduction 或数学函数向量化；
- production closeout 或 production-candidate 阶段的“当前采用的优化方式”小节，写清当前真实采用的 RVV 组织方式、采用理由、chunk 内部流程、分组职责、暂缓方案和证据边界；
- bench case 的输入构造、计时边界、证明点和不能证明的边界；
- QEMU、反汇编、板卡证据分别支持什么；
- production closeout 或 production-candidate 阶段的“正确性与高效性证据链”小节；
- partial-production-candidate（局部生产候选）时的生产直连缺口，例如真实公开入口 direct test、fallback、点类型 traits、`Scalar=double`、indices / correspondences 策略、生产 bench 重跑和人工确认点。

no-production closeout 至少要在 topic-local 文档中包含：

- evaluation 中的生产接入判断和候选拒绝理由；
- phase result 中的 EvidenceDecision、证据分层、Evidence Doctor 解释和继续 / 停止条件；
- “诊断证据链”，说明 diagnostic evidence 能证明什么，不能证明什么；
- 受证据约束的负向归因和后续 profiling / ablation 恢复条件；
- `doc-rvv` / `artifact_layout.topic_doc_template` 判为 `not_applicable` 的说明。

如果当前模块已有最近通过 reviewer 的 sibling topic（同模块相邻主题）文档，worker 应读取或抽样对照其结构、解释粒度和证据边界，不复制 topic 特有结论、参数或性能数字。

详细规则见 `rvv-documentation/SKILL.md`、`topic-doc-structure.md` 和 `evaluation-doc-structure.md`。

### 6. 测试资产 / diagnostic 注释

配置解析出的测试资产和 diagnostic 代码必须面向 reviewer（审查者）可读：

- 长文件有“本文件做什么”和阅读提示；
- 非平凡 helper 说明作用、调用者、production 语义映射和证据角色；
- 每个非显然 `TEST` 或 bench case 前说明验证什么、为什么需要、失败代表哪条证据断了；
- 专业英文术语首次出现时解释其在当前 topic 中的工程含义；
- production 注释保持克制，只解释维护边界、fallback、dispatch、数据布局和数值风险。

详细规则见 `rvv-workflow/references/reviewability-and-language.zh.md`。

如果单个测试支撑 helper header 超过 `.agents/config/defaults.yaml` 中
`test_support.helper_split_soft_line_limit` / `helper_split_hard_line_limit` 配置的约 800-1000 行，
或同时包含 reference、row source、RVV math、reduction candidate、bench wrapper、component ablation
中不少于 `test_support.helper_split_responsibility_threshold` 类职责，worker 必须优先按
`test_support` 配置拆分，或在 Handoff Packet 中写清 `deferred reason`。拆分本身不应扩大算法范围；
若暂缓拆分，必须说明暂缓是否影响 reviewer 可读性、后续测试维护和当前证据复核。
这里的“测试支撑 helper”是职责概念，不是固定目录名。worker 应先扫描当前 topic 的真实文件形态：
根目录长 `test_*.cpp` / `bench_*.cpp`、单个大聚合头、旧 `test_support/` 目录、已有 `include/` /
`include/impl/`、script、bench case registry 或其它等价支撑代码都可能命中拆分条件。只有当前确实存在
旧 `test_support/` 目录时，才把“移出 `test_support/`”作为具体动作；其它历史 topic 应按当前文件形态
迁移到配置解析出的 source / aggregator / internal helper 布局。

当 `.agents/config/defaults.yaml` 的 `test_support.internal_directory` 解析为 `include/impl`，且当前 topic
仍有旧 `test_support/` 目录承载 reference、fixture、row source、candidate、assertion 或 bench harness
等常规测试支撑职责时，`test_support/` 到 `include/impl` 的迁移属于 internal-helper-layout
（内部测试支撑布局）动作，不是可随意忽略的命名偏好。相邻成熟 topic 已采用 `src/`、`include/`
和 `include/impl` 结构时，可作为结构成熟度 quality bar；worker 不能复制其算法、候选结论或具体文件集，
但必须把同等级目录职责作为默认结构目标候选。单独写“只是路径重命名 churn”“code map 已经足够”
或“reviewer 如果需要再做”不足以把该缺口判为 `rejected` 或 `turn_stop_deferred`。若本轮不迁移，
且该动作只触碰当前 topic 的测试资产、Makefile include 路径和 topic-local 文档，它默认是
`phase_deferred + unblocked`，`next_phase_default` 必须指向 `internal-helper-layout`、`test-source-split`
或二者合并的窄 phase，而不是 `ready_for_review`。只有用户明确限定范围、dirty isolation 不安全、
存在外部旧路径依赖、同轮无法安全更新引用，或迁移会越过当前 topic / production / public API 边界时，
才能写成 `turn_stop_deferred`，并列出证据。

恢复旧 topic 或长 topic 时，还必须做 test harness layout audit（测试框架布局审计）。该审计
不只看 helper header 行数，还要检查：

- 测试和 bench 源码是否仍放在 topic 根目录，而不是 `artifact_layout.source_subdir`、`artifact_layout.test_source_template` 和 `artifact_layout.bench_source_template` 解析出的结构；
- 是否缺少 `test_support.aggregator_directory` 解析出的聚合入口，以及 `test_support.internal_directory` 解析出的内部职责拆分；
- 长 topic 是否仍使用超长文件名，是否应按 `test_support.topic_abbrev_policy` 采用缩写 topic token；
- Makefile、board target、日志路径和现有文档引用是否能在迁移后保持正确；
- 相邻成熟 topic 的测试支撑源码布局、聚合入口、内部职责拆分和 topic token 命名经验是否适用，哪些只作为 quality bar，不迁移实现细节；具体目录和文件名仍按当前 topic 既有结构、`artifact_layout` 与 `test_support` 配置解析。

worker 必须把结果写成 `adopted / deferred / rejected` 中的一种：`adopted` 表示本 phase
执行布局迁移；`deferred` 表示它是未阻塞但本 phase 因范围或风险暂缓的下一动作；`rejected`
表示当前源码或证据说明不该迁移。不能省略该决策，也不能只因一个局部 reference cleanup、
单个 target 或一次 correctness 通过，就把测试框架成熟度审计视为完成。

若相邻成熟 topic 已经形成更完整的测试工程结构，worker 还必须做 mature sibling parity audit（成熟相邻主题对齐审计）。该审计只迁移结构成熟度，不复制算法结论。至少检查：

- `src/`、`include/`、`include/impl/`、聚合入口和 legacy alias 是否与当前配置和 topic 复杂度匹配；
- evaluation 是否位于 `artifact_layout.evaluation_doc_template` 解析出的 `doc/` 路径；
- 是否需要 `README.zh.md`、`testing-overview`、`correctness-tests`、`benchmark-and-evidence`、`optimization-evidence` 和 `test-support-code-map`；
- `doc-rvv` production 长期主题文档是否只在适用时出现，并只保留长期 production 行为、当前采用方式和证据链，避免承载测试工程全量解释；no-production topic 是否明确判为 `not_applicable`；
- 暂缓项是否仍是 `phase_deferred + unblocked`，是否应继续到下一 phase。

成熟相邻主题对齐审计的默认结果不应是“记录后等待 reviewer”。若缺口只涉及当前 topic 的测试资产、
topic-local 文档、evaluation 路径或无依赖 legacy 清理，worker 应创建或修订下一阶段 plan 并继续推进。
只有用户限定范围、dirty isolation 不安全、存在明确外部依赖、需要板卡 / 工具或会扩大到 production /
public API / 其它 topic 时，才能停止并把该项写成 `turn_stop_deferred`。

compatibility alias（兼容别名）和 legacy pointer（旧路径指针）默认不保留。保留它们需要具体证据：
仍有脚本、文档、Make target、reviewer 工作流或用户指令依赖旧路径；同时必须写删除条件和下一阶段清理动作。
没有证据的“为避免旧引用断开”应改为更新引用并删除旧入口。

### 7. 证据和归因

worker 必须分开写：

- correctness（正确性）证据；
- QEMU path/log-shape（QEMU 路径 / 日志形状）证据；
- disassembly（反汇编）证据；
- board/target performance（板卡 / 目标硬件性能）证据；
- negative evidence（负向证据）；
- unsupported claims（当前证据不能支持的说法）。

QEMU 不能作为性能结论。板卡退化只能支持 no-production，不能自动证明 gather、buffer、
FMA 或 tail 是单一主因；没有消融 bench 或 profile 时必须写成假设。

bench 类性能结论默认只来自 board / target hardware。QEMU 默认可以编译 bench binary，但不运行
`run_bench_compare`、完整 bench matrix 或任何会生成 Std/RVV 数值对比表的 compare target；QEMU
bench compare 没有性能意义且浪费时间。若确实需要 QEMU bench，只能跑非 compare、小规模、少 case、
少 iteration 的 build / correctness / log-shape smoke。若本轮只跑了 QEMU bench smoke，worker 必须把
性能证据写成 missing / blocked，不能把 `analyze_bench_compare.log` 的 QEMU 计时用于排序或
EvidenceDecision。

每次复跑都要做 evidence freshness check（证据新鲜度检查）。若新 run 改变了文档中的 speedup、
方向、decision bucket、run count、Evidence Doctor 数量或证据角色，旧 run 只能保留为 historical evidence；
phase result、optimization matrix、evaluation、topic 文档和 Handoff 必须同步刷新，或明确写
`stale_doc_pending_refresh`。若 topic 有 `log/evidence_registry.json` 或等价登记表，恢复和提交前
必须检查 `unregistered_change`、`unregistered_file` 和 `manual_run_detected`；若没有 registry，
Handoff 必须写 `evidence_registry_status=not_available` 并列出人工检查路径。

板卡复跑必须有 bounded rerun budget（有界复跑预算）和 decision bucket（决策桶）。worker 不追求
每个精确数字完全稳定；预算用完后，如果 bucket 稳定即可关闭该证据动作，如果仍摇摆则标成
`unstable`、降级 EvidenceDecision 或交给 reviewer / 用户判断。

closeout 或 production-candidate topic 文档必须包含“正确性与高效性证据链”小节。该小节至少检查：

- correctness：public entry 是否真实命中；row semantics 是否清楚；`accepted_points`、中间态、matrix 和 fallback 是否有证据。
- performance：性能结论是否只来自 repeated board 或目标硬件；QEMU timing 不能作为性能结论。
- boundary：EvidenceDecision 是否没有超过证据范围；representative pointtypes（代表性点类型）、indexed、correspondences 和 row source policy 边界是否写清。
- document ownership：复杂 topic 是否先用文档归属矩阵把长期事实、候选取舍、bench 统计、output summary 和 Handoff Packet 主归属分开。
- traceability map：复杂 topic 是否提供 Traceability Map（可追踪性地图），能从文档跳到 production 入口、test helper、bench wrapper、analysis script 和 output summary。
- risk：未覆盖范围、保留标量路径和后续扩展条件。

未接 production 的诊断结论必须在 topic-local evaluation / phase closeout 写“诊断证据链”。该小节必须说明 diagnostic evidence 不能写成 production evidence；
public-entry-shaped、production-shaped diagnostic 或代表性点类型证据不能替代真实 production dispatch。没有 adopted production behavior 时，`doc-rvv` 必须写成 `not_applicable` 或删除遗留诊断文档。

详细规则见 `rvv-test/SKILL.md`、`rvv-test/references/performance-and-ablation.zh.md`、
`rvv-test/references/evidence-output-policy.zh.md` 和 `rvv-documentation/references/document-ownership-and-traceability.zh.md`。

### 8. 测试矩阵与 evidence policy

worker 在测试计划和 Handoff 中必须分开列出当前 topic 需要覆盖的测试类别：

- unit test（单元测试）、boundary/adversarial test（边界 / 对抗测试）、numerical consistency（数值一致性）、regression（回归测试）。
- production-shaped diagnostic、production direct、fallback tests。
- benchmark、component ablation（组件消融）、diagnostic/probing（诊断 / 探针测试）和 upstream/integration smoke（上游 / 集成冒烟）。
- sanitizer（运行时检查工具）和 profiling（性能剖析）为可选项；只有当前风险需要时才列为必须项。

full-cloud（全云顺序扫描）、source-indexed（源索引路径）、dual-indices（双索引路径）和
correspondences（对应关系路径）是不同 row source policy（行来源策略）。production 必须逐 policy
独立批准。RowSourcePolicy 只负责 row source；shared math pipeline（共享数学流水线）负责
finite mask（有限值掩码）、formula（公式）、staging/reduction、accepted_points、ATA/ATb。
如果当前 topic 已有 adopted math family，worker 写实现前必须检查该 family 是否已经按 row source
policy 做过 carry-over audit。未尝试的 policy 先在配置解析出的 RVV test 资产中补 candidate / bench / board 证据，
或写出不适用原因；不能只因为某个旧 helper 在一个 policy 上正向，就直接扩大 production。

mixed fields（混合字段）、point traits（点类型字段特征）、AoS stride（数组结构跨步）、
gather、valid-index-only（仅有效索引）、production predicate（生产谓词）和 invalid lane finite mask
（无效 lane 有限值掩码）应作为测试矩阵条目。staged candidate、production-shaped diagnostic
和 production direct 必须分层；public-entry-shaped 不能当作 production dispatch 证据。

FMA 和 reduction 相关测试必须说明反汇编归属、误差预算和必要板卡 A/B（对照测试）。不能因为源码写法看起来没有 fused（融合）表达式，就默认禁止 fused 指令。

correspondences 或 indexed 路径退化时，归因必须列出 query/match 展开、容器访问、baseline、分布局部性、后段成本、gather、`vcompress`、buffer 和自动规约等候选原因。没有消融或 profile 证据时，只能写成假设。

详细规则见 `rvv-test/references/test-taxonomy.zh.md`、`rvv-test/references/entry-shapes-and-test-support.zh.md`、
`rvv-test/references/registration-topic-evidence.zh.md`、`rvv-test/references/numerical-consistency.zh.md`
和 `rvv-test/references/performance-and-ablation.zh.md`。

### 8a. Phase loop 防早停门禁

复杂 topic、短 prompt 继续已有 topic 或任何含多阶段优化计划的 topic，必须把 `rvv-test/references/optimization-phase-loop.zh.md`
作为 phase loop 的 source of truth。worker 写配置解析出的 RVV test 资产、bench、production 或长期文档前至少闭合下列门禁：

```text
phase_plan_written_before_edits:
optimization_roadmap_ready:
roadmap_default_recovery_queue_ready:
phase_completion_matrix_ready:
optimization_matrix_ready:
micro_stop_guard:
continue_stop_decision:
ready_for_review_validity_check:
```

- `phase_plan_written_before_edits`：当前 phase 的 `plan.zh.md` 必须先于该 phase 的实现、测试、bench 或 production 修改存在。若历史 topic 没有阶段目录，先创建 `doc/phases/000-current-state-and-gaps/plan.zh.md`。
- `optimization_roadmap_ready`：复杂 topic 必须读取或创建 `artifact_layout.optimization_roadmap_template` 解析出的 roadmap。roadmap 必须列出候选 family、idea source、适用 row source / 点类型 / `Scalar`、预期收益、风险、证据需求、状态和 next phase；phase 结束后必须回填新增想法或调整优先级。
- `roadmap_default_recovery_queue_ready`：恢复旧 topic 时，worker 必须从 roadmap、phase README、最近 result
  和 Handoff 中解析“默认恢复动作”、`next_phase_default`、`resume condition` 或等价字段，形成有序恢复队列。
  队列项必须区分当前授权内的 `phase_deferred + unblocked`、命中真实停止条件的 `turn_stop_deferred`、
  `blocked`、`rejected with evidence` 和 `not_applicable with evidence`。若队列中仍有测试资产结构迁移、
  test source split、internal helper layout、doc suite、registry 或 legacy 清理，`ready_for_review` 无效。
- `phase_completion_matrix_ready`：分两个时间点检查。写文件前，`plan.zh.md` 必须已有可回填的 action / completion scaffold（计划动作表、依赖和完成判据），让后续 `result.zh.md` 能逐项回填；阶段结束或 Handoff 前，`result.zh.md` 或 Handoff 必须逐项列出计划动作的 `done / partial / deferred / blocked` 状态、证据路径和缺口，不能只写“完成本阶段”。
- `optimization_matrix_ready`：复杂 topic 必须维护 candidate family × row source policy × point type / `Scalar` / layout × test × bench × board × asm × doctor × decision 矩阵；`planned` 或 `deferred` 不能伪装成 adopted。
- `micro_stop_guard`：如果只完成一个小 helper、一个隔离层、一个 target、一次 bench、一个 summary 或一张表，但当前计划仍有授权且未阻塞 next action，worker 不允许停；必须继续推进下一个动作，或写出真实停止条件。
- `continue_stop_decision`：最终输出和 Handoff 必须解释为什么继续或为什么停。停止必须命中用户限定范围、权限扩大、板卡 / 工具阻塞、证据矛盾、dirty isolation 风险、生产接入需授权，或当前 phase 矩阵、optimization matrix 和 roadmap 都已闭合且没有 unblocked next action。
- `ready_for_review_validity_check`：如果任何 phase README、result、Handoff 或最终回复声称 `ready_for_review`，worker 必须重新验证 mature sibling parity、doc suite、legacy 清理、shape scan、roadmap 和 optimization matrix 是否仍有 `phase_deferred + unblocked`。只要有未闭合缺口，`ready_for_review` 就失效，必须恢复到下一 phase。

`micro_stop_guard` 是强规则：worker 不能把一个局部 positive / negative、row-source audit 表、Evidence Doctor warning 解释或 isolated bench 当作 topic 完成。若继续推进会扩大范围，则停止理由必须写清扩大到哪里、需要谁授权、恢复入口是什么。

`phase_deferred` 和 `turn_stop_deferred` 必须分开写。测试优化阶段、topic-local 文档重构、测试支撑结构迁移、evaluation 路径迁移、doc suite 对齐、无依赖 legacy 清理、candidate / bench / asm / Evidence Doctor 补齐，通常都属于可继续推进的 `phase_deferred + unblocked`。只有高风险、真实 blocker 或明确授权边界才允许转成 `turn_stop_deferred`。

### 9. PI1 生产接入计划门禁

当 worker 继续一个 `partial-production-candidate` topic 并进入 PI1 时，先产出 production integration plan，
再考虑生产补丁。若用户目标是“进入 / 推进 production integration loop（生产接入闭环）”，PI1 是同轮
PI2-PI5 的前置 gate（门禁），不是默认终点；PI1 闭合且未命中暂停条件时，worker 应继续生产补丁、
生产直连测试、生产证据重跑和 PI5 再决策。只有用户明确要求“只做 PI1 / 只写计划”，或 PI1 发现范围、
fallback、测试或泛型策略不能闭合，才停在 PI1。PI1 至少写清：

- 候选范围：入口形态、点类型、`Scalar`、数据布局、规模 gate、目标硬件和已证明收益。
- 不接入范围：indices、correspondences、泛型点类型、`Scalar=double` 或其它未闭合路径是否保持标量。
- dispatch / fallback：`__RVV10__` 关闭、小规模、VLEN、字段布局、点类型 traits、非覆盖入口如何回退。
- production direct test：真实公开入口如何证明命中 RVV 分流，fallback 如何单独证明。
- 泛型点类型策略：如果 production 模板入口不应收窄，使用 traits / offset / POD / standard-layout gate；如果只接 `PointNormal`，必须写明这是有意收窄且其它类型 fallback。
- 反汇编和板卡计划：如何按 production 符号范围确认 RVV 指令，如何重跑目标硬件 bench。

PI1 中不要把诊断路径 speedup 写成 production-ready。只有 PI2-PI5 后 production direct 证据闭合，才能升级 EvidenceDecision。

### 10. PI2-PI5 连续推进门禁

当用户用短 prompt 授权继续 production integration loop（生产接入闭环），且最近 Handoff Packet 的
`next_worker_action_if_review_passes` 已给出 PI2 范围时，worker 可以同轮连续推进 PI2-PI5。连续推进前必须冻结：

- `pi2_scope`：入口、点类型、`Scalar`、数据布局、规模 gate 和不可触碰路径。
- `forbidden_expansion`：不得扩大到 PI1 未授权的泛型、indices、correspondences、public API 或公共 helper 变更。
- `fallback_matrix`：非 RVV 构建、非覆盖点类型、`Scalar=double`、小输入、VLEN/buffer、indices、correspondences 等回退项。
- `entry_structure`：公开入口是否只做上游语义检查和短路分流；原标量路径是否抽成清晰的
  `*_Std` helper；RVV 主路径是否抽成清晰的 `*_RVV` helper；多个入口共享 policy 时，public
  overload 仍不能堆叠大段 RVV gate 或标量主体。
- `std_helper_extraction`：生产接入不得只在公开入口开头插入 RVV try-and-return，然后把原标量循环留在同一入口
  后半段。除非存在明确 ABI、模板可见性或旧接口约束，否则必须抽成 `*_Std` / `*_Standard` helper；若例外，
  Handoff Packet 必须列出保留原因、标量主体边界、fallback 测试和 reviewer 风险。
- `std_helper_shape`：`*_Std` / `*_Standard` helper 不强制必须是成员函数；若成员 helper 会要求修改公开
  header、protected API、显式实例化或模板声明表面，可使用邻近 internal / `detail` free helper。Handoff
  Packet 必须说明 helper 形态选择，且 public entry 仍只能呈现“语义检查 -> RVV 短路 -> Std fallback”。
- 复杂 eligibility（适用性）解析，例如动态 condition / field metadata / policy 分解，应收进窄
  RVV wrapper 或 `*_RVV` helper，由 public overload 维持“语义检查 -> RVV 短路 -> Std fallback”的形状。
- `evidence_commands`：PI3/PI4 需要运行的 test、bench、asm 和 board 命令。
- `pause_conditions`：命中 `topic-lifecycle.zh.md` 中连续推进暂停条件时停止并输出 Handoff Packet。

PI2-PI5 结束后，Handoff Packet 必须新增或等价覆盖：

- `production_patch_summary`：生产补丁范围和未触碰路径。
- `production_direct_results`：真实公开入口命中 RVV 的测试结果。
- `fallback_results`：每个 fallback gate 的测试或构建证据。
- `entry_structure_review`：是否复核 public entry / `*_Std` / `*_RVV` 分层符合所在文件已有
  SIMD 或 PCL 源码风格；若因旧接口或模板限制无法完全拆分，说明保留原因。
- `std_helper_extraction_review`：是否确认原标量主体已抽成命名清楚的 Std/Standard fallback helper；若没有抽，
  不能只写“自然 fallback”，必须把例外原因和额外验证写成 reviewer 可审查事实。
- `asm_hotspot_attribution`：按 production 符号范围归属关键 RVV 指令。
- `board_production_results`：目标硬件 production direct bench 结果；若未运行，写明阻塞原因。
- `pi5_evidence_decision`：基于 production direct 证据的新 EvidenceDecision。

PI2-PI5 结束后必须继续完成 S11 文档 closeout（收尾文档）。worker 不能只更新代码和日志后结束。
最终文档门禁至少覆盖：

- `production_doc_scope`：主题文档和 evaluation 文档是否写清真实生产补丁范围、覆盖入口、点类型、`Scalar`、数据布局和目标硬件。
- `production_doc_fallbacks`：是否用表格或等价结构列出 fallback 矩阵，包含非 RVV 构建、非覆盖点类型、`Scalar=double`、indices、correspondences、小规模和布局不满足路径。
- `production_doc_evidence`：是否把 production direct test、fallback test、asm 符号归属和 board production bench 分别写清，而不是沿用诊断 bench 结论。
- `production_doc_decision_delta`：是否说明诊断阶段结论如何被生产证据确认、缩窄、推翻或回退。
- `production_doc_remaining_scope`：是否写清仍保持标量或未覆盖的入口，以及下一轮扩展必须补的证据。

### 11. 窄范围结论后的后续路径门禁

当当前结论不是“整个模板入口都完成”，而是 `narrow`、`partial`、`bench-only`、`no-production`
或带有明确 fallback / 未覆盖范围时，worker 不能只写“进入下一个 topic”。必须在最终输出和
Handoff Packet 中给出 `followup_options_for_user`：

- `recommended_default`：默认建议，例如“评审通过后进入下一个 topic”或“先补一个 fallback 测试”。
- `continue_current_topic`：如果继续当前 topic，最值得做的扩展是什么，范围必须窄到入口、点类型、`Scalar` 和证据。
- `separate_followup_topic`：哪些方向应该另开 follow-up topic 或消融 topic，例如 indexed / correspondences 退化归因。
- `not_recommended_now`：哪些方向当前不建议直接做，以及阻塞证据是什么。

对 production-ready/narrow 结论，worker 必须主动回答：

- 这个窄范围是否漏掉了常见泛型入口。
- 如果要扩大到泛型点类型，应读取哪些策略文档、需要哪些 traits / offset / fallback / board 证据。
- 哪些入口虽然“看起来相近”，但因为负向性能、语义风险或测试缺口不能一起扩大。

`next_worker_action_if_review_passes` 仍只保留一个默认动作；其它重要选择放在 `followup_options_for_user`，
避免用户只能靠人工复查发现下一步。

## 开始写文件前的自查

worker 在创建或更新 topic 产物前，先确认：

```text
preferences_loaded:
comment_policy_frozen:
evidence_policy_frozen:
documentation_policy_frozen:
markdown_time_wording_check:
writing_style_trigger_check:
scalar_path_ready:
production_to_diagnostic_mapping_ready:
document_ownership_matrix_ready:
traceability_map_ready:
experience_migration_audit_ready:
phase_plan_written_before_edits:
optimization_roadmap_ready:
phase_completion_matrix_ready:
optimization_matrix_ready:
micro_stop_guard:
continue_stop_decision:
ready_for_review_validity_check:
doc_quality_refs_loaded:
current_optimization_section_ready:
test_comment_strategy_frozen:
test_support_split_decision_ready:
test_support_shape_scan_ready:
legacy_compatibility_decision_ready:
mature_sibling_parity_action_ready:
bench_timing_boundary_defined:
bench_backend_choice_ready:
qemu_bench_smoke_scope_ready:
rerun_budget_decision_ready:
alternative_designs_listed:
evidence_model_defined:
evidence_doctor_result_ready:
evidence_registry_status_ready:
evidence_freshness_check_ready:
correctness_efficiency_evidence_chain_ready:
stop_condition_defined:
pi1_production_scope_ready:
generic_point_type_strategy_ready:
fallback_dispatch_strategy_ready:
production_direct_test_plan_ready:
pi2_scope_ready:
fallback_matrix_ready:
pi2_to_pi5_pause_conditions_ready:
production_direct_results_ready:
fallback_results_ready:
dirty_isolation_ready:
implementation_review_ready:
candidates_added_or_deferred_ready:
ilp_lmul_decision_ready:
numerical_budget_result_ready:
asm_hotspot_attribution_ready:
asm_attribution_ready:
board_production_results_ready:
board_evidence_paths_ready:
pi5_evidence_decision_ready:
evidence_decision_ready:
production_decision_ready:
validation_summary_ready:
production_doc_closeout_ready:
followup_options_ready:
```

若其中任一项为 false，先补评估或设计，不要直接开始堆代码和结论。

## 交接前的证据化门禁

最终 Handoff Packet（交接数据包）里的 `worker_quality_gate_check` 不能只写 true / false。worker 必须把上面的自查改写成可复核表格：

```text
| gate（门禁项） | status（状态） | evidence（文件 / 章节 / 行或段落） | missing_items（缺口） |
```

要求：

- `status` 可写 `pass`、`partial`、`fail` 或 `not_applicable`；不要用没有证据的 `true`。
- `evidence` 至少指向当前 topic 的 evaluation、topic-local phase / diagnostic 文档、适用的 production 长期主题文档、测试资产注释、bench 说明、证据日志或 Handoff 段落。
- `missing_items` 必须写成陈述句；没有缺口时写 `none`。
- 表格必须包含 `preferences_loaded`、`comment_policy_frozen`、`evidence_policy_frozen`
  和 `documentation_policy_frozen`。证据指向 S0 报告、Handoff Packet 或配置读取摘要。
- 表格必须包含 `evidence_doctor_result_ready`。凡本轮涉及 benchmark、board summary、checksum summary、asm attribution 或 EvidenceDecision，证据必须指向 `artifact_layout.evidence_doctor_script_template` 解析出的脚本生成的 report，或按 `rvv-test/references/evidence-doctor.zh.md` 人工填写的 Errors / Warnings / Suggestions 摘要；未运行脚本时说明原因和当前 doctor 边界。
- 表格必须包含 `bench_backend_choice_ready`。凡本轮涉及 bench，证据必须说明性能结论是否来自 board / target hardware；若只跑 QEMU bench smoke，状态应为 `partial` 或 `not_applicable`，并写清它只用于 build / correctness / log-shape smoke；若没有运行 QEMU bench，应写明默认策略是只编译或只跑 gtest / correctness。
- 表格必须包含 `qemu_bench_smoke_scope_ready`。如果运行了 QEMU bench，证据必须列出 case-filter、规模、iteration、是否显式绕过 guard，以及为什么它不是完整 bench compare；如果没有运行，写 `not_applicable` 并说明性能验证只走 board / target hardware。
- 表格必须包含 `rerun_budget_decision_ready`。凡本轮涉及 board performance 或 repeated summary，证据必须指向 phase plan / summary 中的 run budget、decision bucket、是否用完预算和是否需要降级 / 人工判断。
- 表格必须包含 `evidence_registry_status_ready`。证据指向 `log/evidence_registry.json`、`make evidence_status` / `make check_evidence_freshness` 输出或等价人工检查；若 topic 尚未接入 registry，写 `partial` 并列出应补的 target / script。
- 表格必须包含 `evidence_freshness_check_ready`。证据指向 Handoff Packet 的 `evidence_freshness_status`、phase result、evaluation 或 topic 文档；若复跑改变了旧数值、decision bucket 或证据角色，必须列出已刷新和待刷新的路径。
- 表格必须包含 `production_topic_doc_applicability_ready`。证据说明 `artifact_layout.topic_doc_template` 是 applicable 还是 not_applicable；no-production 时必须说明没有新建 `doc-rvv`，或已有遗留 `doc-rvv` 已删除 / 标为历史归档。
- 表格必须包含 `correctness_efficiency_evidence_chain_ready`。production 结论的证据指向 production 长期主题文档中的“正确性与高效性证据链”；no-production 结论的证据指向 evaluation / phase closeout 中的“诊断证据链”。
- 表格必须包含 `document_ownership_matrix_ready`。证据指向文档归属矩阵章节、evaluation 中的决策审计或 Handoff Packet 的定位字段。
- 表格必须包含 `traceability_map_ready`。证据指向 Traceability Map 章节或独立 traceability 文档，说明文档、测试、输出和代码位置可以互相定位。
- 表格必须包含 `phase_plan_written_before_edits`、`phase_completion_matrix_ready`、`optimization_matrix_ready`、
  `roadmap_default_recovery_queue_ready`、`micro_stop_guard` 和 `continue_stop_decision`。证据指向当前 phase plan/result、optimization matrix、
  `unblocked_next_actions`、`stop_condition_hit` 和 Handoff 的 `phase_loop_state`；若当前任务不是多阶段优化，写 `not_applicable` 并说明为什么没有 phase loop。
- 表格必须包含 `ready_for_review_validity_check`。如果本轮输出 `ready_for_review`、`done`、
  `stop_for_review` 或 `unblocked_next_actions=none`，证据必须指向 roadmap、optimization matrix、
  mature sibling parity 状态、test support shape scan、legacy compatibility decision 和 doc suite 审计；
  若这些位置仍有 `phase_deferred + unblocked`，该项必须写 `fail`，且 `next_phase_default` 不能是
  `ready_for_review`。
- 表格必须包含 `dirty_isolation_ready`。证据指向 Handoff Packet 的 `dirty_isolation`，说明当前 worktree 的无关 diff、raw logs、build 输出和本轮可审查 / 可提交路径边界。
- 表格必须包含 `implementation_review_ready`。若本轮改了 production、diagnostic helper、bench-facing helper 或 RVV kernel，证据指向 Handoff Packet 的 `implementation_review` 或适用的 production 长期主题文档“当前采用的优化方式”；若纯文档 cleanup，写 `not_applicable` 并说明原因。
- 表格必须包含 `candidates_added_or_deferred_ready`。证据指向本轮候选路线表、experience-migration audit 或 Handoff Packet 的 `candidates_added_or_deferred`，说明新增、尝试、暂缓或拒绝的候选。
- 表格必须包含 `ilp_lmul_decision_ready`。含 RVV kernel、reduction、staging 或性能候选时，证据必须说明 LMUL、VLEN gate、accumulator 数、ILP / unroll、寄存器压力或 spill 风险；不适用时说明原因。
- 表格必须包含 `numerical_budget_result_ready`。含 FMA、reduction、浮点阈值、`ATA/ATb`、matrix 或 checksum 风险时，证据必须指向误差预算和结果；不适用时说明原因。
- 表格必须包含 `asm_attribution_ready`。证据指向反汇编归属字段或未运行原因；若只有 QEMU correctness 没有反汇编，不能写成已闭合。
- 表格必须包含 `board_evidence_paths_ready`。证据指向 summary / sanitized / raw 的板卡证据边界；未跑板卡时说明阻塞原因和当前 EvidenceDecision 限制。
- 表格必须包含 `evidence_decision_ready` 和 `production_decision_ready`。前者写 S10 / PI5 证据决策，后者独立说明是否进入 production integration loop、是否修改 production、哪些路径保持标量。
- 表格必须包含 `validation_summary_ready`。证据指向 Handoff Packet 的 `validation`，列出已运行和未运行的 test、bench、asm、board 或 sanitizer。
- production closeout 或 production-candidate 文档必须列出 `current_optimization_section_ready`、`document_ownership_matrix_ready` 和 `traceability_map_ready`。证据指向 production 长期主题文档中的“当前采用的优化方式”小节、文档归属矩阵章节和 Traceability Map 章节，并说明它们是否覆盖 dispatch / fallback、layout gate、当前优化机制、chunk 内部流程、分组职责、暂缓方案和证据边界。no-production closeout 中 `current_optimization_section_ready` 可写 `not_applicable`，但必须用 evaluation / phase closeout 指向候选拒绝理由和诊断证据链。
- 如果 worker 声明采用 sibling topic 经验，表格必须包含 `experience_migration_audit_ready`；
  证据指向 adopted / attempted / deferred / rejected 对照表。若未声明且无相邻经验可迁移，可写 `not_applicable` 并说明原因。
- 表格必须包含 `test_support_shape_scan_ready`。证据必须列出当前 topic 的测试支撑形态：根目录
  test / bench 源码、聚合头、内部 helper、旧 `test_support/` 目录、script、bench case registry
  或等价文件；没有某种形态时写 `not_present`，不能把“不存在 `test_support/` 目录”当成未审计理由。
- 如果当前 topic 的测试支撑 helper、源文件或等价支撑代码命中行数或职责阈值，表格必须包含
  `test_support_split_decision_ready`；证据指向按 `test_support` 配置拆分后的结构，或 Handoff 中的 deferred reason。
- 恢复旧 topic、长 topic 或测试 / bench 仍在 topic 根目录的 topic 时，表格必须包含
  `test_harness_layout_audit_ready`；证据必须说明是否采用 `artifact_layout` 与 `test_support`
  解析出的 source、aggregator、internal header 和长 topic 缩写文件名策略，以及 sibling 结构经验是 adopted、deferred 还是 rejected。
  若暂缓迁移，必须把它写入 `unblocked_next_actions` 或说明阻塞条件。
- 若相邻成熟 topic 已经形成更完整的测试工程或 topic-local doc suite，表格必须包含
  `mature_sibling_parity_action_ready`；证据必须说明结构差距是否已经采用、拒绝，或作为高优先级
  `phase_deferred + unblocked` 继续推进。若停止，必须指向真实 stop condition。
- 表格必须包含 `legacy_compatibility_decision_ready`。证据必须说明是否存在 legacy pointer / alias /
  旧路径 wrapper；默认处理是删除并更新引用。若保留，必须列出具体外部依赖、删除条件和下一阶段。
- 若 `language_check` 声称通过，必须能在同一张表或相邻段落中指出诊断代码、测试、bench 和文档的术语 / 中文注释证据。
- 表格必须包含 `writing_style_trigger_check`。检查范围至少覆盖主题文档、evaluation / closeout 文档、workflow 文档、Handoff Packet、worker / reviewer 最终回复和 `agent_asset_feedback`；触发词清单来自 `rvv-documentation/references/writing-style.md`。若某个命中词是必要技术术语，必须写清保留理由。
- 若当前结论强于 no-production，例如 `partial-production-candidate`，表格必须额外列出 production direct 尚未闭合的证据项，避免把诊断收益误写成 production-ready。
- 若本轮进入 PI1，表格必须额外列出 `pi1_production_scope_ready`、`generic_point_type_strategy_ready`、
  `fallback_dispatch_strategy_ready` 和 `production_direct_test_plan_ready`。不适用时写明原因，不能省略。
- 若本轮连续推进 PI2-PI5，表格必须额外列出 PI2 scope、fallback matrix、暂停条件、production direct
  results、fallback results、asm hotspot attribution、board production results、PI5 EvidenceDecision
  和 production doc closeout。
- 若当前结论是窄范围 production-ready、partial-production-candidate、bench-only/no-production 或保留重要未覆盖范围，
  表格必须列出 `followup_options_ready`，并指向 Handoff Packet 或文档中的可选后续路径。
- 文档 cleanup 或 closeout 任务必须列出 `markdown_time_wording_check`。检查范围是非 raw output/log 的长期 Markdown 和 evaluation / closeout 文档；禁止把“最新一次”“最新日志”“截至今日”当作长期事实。允许日期保留在 work log、manifest、run id、handoff / recovery path、用户指定目录名、checksum、常量、指令立即数、数据规模和 ICP “最近点”等技术术语中。

这张表是给 reviewer 复核的，不是为了加长最终回复。证据可以用章节名或稳定路径摘要，不需要复制长文档内容。
