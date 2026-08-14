# 主题 RVV 文档结构

主题文档面向 production 长期维护，强调算法、实现设计、staging 边界、数值语义和生产接入理由。
`artifact_layout.topic_doc_template` 解析出的 `doc-rvv` 主题文档只在存在 adopted production behavior
（已采用生产行为）、用户确认保留的 production patch（生产补丁）或 PI5 生产证据闭环通过且用户确认采纳后适用。`diagnostic`、
`bench-only`、`rollback/no-production` 或未接 production 的 `partial-production-candidate` 的诊断证据链
应写在 topic-local evaluation / phase closeout；不得为了 no-production closeout 新建 `doc-rvv`。

推荐结构：

1. 函数入口作用：公开 API 调用路径、输入输出、算法管线职责。
2. 标量路径与诊断边界：真实上游源码、当前是否修改生产入口。
3. 生产接入前置观察：对象状态、staging、隐藏标量 tail、成本归因或语义风险。
4. 覆盖范围与 fallback：点类型、dense/indexed、小规模、NaN/Inf、FRM/FCSR、非 RVV 行为。
5. 详细设计：helper、traits、staging、mask helper、数值 helper、对象状态展开。
6. 当前采用的优化方式：当前真实使用的 RVV 组织方式、采用理由、内部流程和暂缓方案。
7. Traceability Map（可追踪性地图）：复杂 topic 列出 production、RVV test 资产、script、output 和文档章节之间的定位关系；详细规则见 [document-ownership-and-traceability.zh.md](document-ownership-and-traceability.zh.md)。
8. 关键实现片段：展示完整阶段边界，不能只贴公式。
9. 数值算例与 VL chunk 图示。
10. Bench case 说明。
11. 测试、QEMU、反汇编和板卡证据。
12. 正确性与高效性证据链。
13. 生产接入评估。
14. 生产接入后的 closeout 更新。
15. 结论与后续方向。
16. 阶段探索与测试证据（仅 `artifact_layout.phase_root_template` 解析目录，不进入 `artifact_layout.topic_doc_template` 解析出的最终生产行为说明）。
17. 优化路线图（仅 `artifact_layout.optimization_roadmap_template` 解析出的 topic-local roadmap；主题文档只引用当前采用或暂缓状态）。

## 必写要点

- 被优化函数对象或函数入口在库中的作用。
- 公开入口、wrapper、dispatch、真实实现层之间的调用链。
- 源码中的数据流形态和诊断中的显式数据流形态是否一致。若 production 通过 iterator（迭代器）、wrapper（包装层）、callback（回调）、dispatch（分流逻辑）或模板 helper 隐藏了顺序点云对（ordered-cloud-pair，source/target 按相同下标一一对应）、indices（索引）、mask（掩码）、correspondences（对应关系）等差异，主题文档必须先说明源码如何统一这些入口，再说明 RVV 诊断为什么要重新拆成跨步加载（stride load）、离散加载（gather）、连续加载（contiguous load）、离散写回（scatter）或分阶段暂存（staging）路径。
- 标量实现的可读解释：输入如何进入关键循环，关键局部变量、公式和状态如何生成，输出或 solver 如何使用这些中间量。不要只列函数名或公式片段。
- RVV 实现的可读解释：每个 VL chunk 如何取数，使用 stride/gather/segment/contiguous load 的原因，mask 如何构造，staging 或输出如何写回，后续消费者是谁。
- RVV 覆盖原标量代码的哪一段，哪些阶段仍是标量，原因是什么。
- 实现选择审计：如果使用 buffer/staging、`vcompress`、scatter、标量 tail、显式/非显式 fused multiply-add（融合乘加）、vector reduction（向量规约）或数学函数 helper，必须说明为什么这样做、替代方案是什么、当前证据是否足以排除或暂缓替代方案。
- 哪些 gate 触发 fallback，fallback 后语义如何保持。
- 当前主题属于 production direct、production-shaped diagnostic、bench 诊断主题，还是生产回退说明；若不是 adopted production behavior，说明 `doc-rvv` 是否 `not_applicable`。
- 每个 bench case 的入口、规模、参数、是否命中 RVV、speedup 计算方式和证明点。
- 板卡收益是否足以覆盖 staging、buffer 和维护成本。
- production closeout 或 production-candidate 阶段必须包含“当前采用的优化方式”小节。该小节面向维护者解释当前代码实际采用的优化组织方式，不能只列历史尝试、bench 数字或最终 EvidenceDecision。no-production closeout 没有当前采用的 production 优化方式时，该内容应写成 evaluation / phase result 中的候选审计，不新建 `doc-rvv`。
- 复杂 topic 必须包含或引用 Traceability Map。该表只覆盖 reviewer 需要定位的关键 production、RVV test 资产、script、output 和文档章节，不要求枚举每个小函数，也不要求默认新建巨型函数文档。
- 多阶段优化 topic 必须包含或引用 topic-level optimization roadmap。roadmap 记录 candidate family、idea source、阶段反思新增路线、优先级和恢复条件；主题文档只引用最终采用和仍暂缓的路线，不复制搜索过程。
- production closeout 或 production-candidate 阶段必须包含“正确性与高效性证据链”小节。该小节是 reviewer 判断依据，不能只写说明文字。未接 production 的 no-production 结论使用 topic-local “诊断证据链”。
- 若当前结论是 partial-production-candidate（局部生产候选），必须写清“候选范围”和“尚不能生产接入的原因”。候选范围要窄到入口形态、点类型、数据布局、规模、fallback 条件和目标硬件；不能把局部诊断收益写成整个函数族可接入。
- 若已经接入 production（生产源码），主题文档必须从“诊断原型说明”升级为“生产实现说明”：写清真实 production patch（生产补丁）、真实 dispatch / fallback、production direct（真实生产入口直连）测试、反汇编符号归属、板卡 production bench 和 PI5 EvidenceDecision（生产证据决策）。不要把早期诊断 speedup 当作最终生产结论。
- 每一轮 production 接入或窄范围 production candidate 都必须在 `artifact_layout.phase_root_template`
  解析目录保留对应 plan/result 和矩阵记录。`doc-rvv` 只能描述已经用户确认采纳的长期 production 行为；
  第一轮具体点型、代表性点型或单一 row source 的 closeout 只能说明当前阶段采用范围，不能伪装成整个
  模板入口、所有 row source、所有 `Scalar` 或所有 layout 的最终实现。

## 当前采用的优化方式

production closeout（收尾）或 production-candidate（生产候选）文档必须新增或更新本小节。该小节回答“当前到底采用了什么优化方式、为什么采用、如何工作、证据支持到哪里”。它放在详细设计之后、证据链之前，作为维护者理解代码形态的入口。no-production closeout 不应伪造“当前采用的优化方式”；候选尝试和拒绝理由写入 evaluation / phase docs。

本小节至少覆盖：

- dispatch（分流逻辑）与 fallback（回退路径）：公开入口如何命中 RVV，哪些 gate 会回退到标量或既有实现。
- 输入布局和对象状态：source、target、weight、index、correspondence、对象成员或外部 buffer 分别由什么 traits、offset、stride、mask 或状态 gate 证明。
- 当前采用的优化机制：例如 block reduction（分块规约）、vector reduction、`vcompress` staging（压缩暂存）、gather staging（离散加载暂存）、scatter 写回或 scalar tail（标量尾段）。
- 采用理由：为什么当前机制替代早期 baseline，或为什么继续保留某个 fixed buffer、staging、tail 或 scalar stage。
- VL chunk（可变向量长度分块）内部流程：如何 load / gather，如何构造 mask，如何计算公式，如何 staging、store 或 reduction，如何处理 tail。
- 分组或阶段职责：如果有 A/B/C/N、predicate group、staging group、lane helper 或 block group，必须说明每组累加、筛选、写回或交给后续阶段的标量语义。
- 暂缓或拒绝的替代方案：例如 fused formula（融合公式）、FMA contraction（融合乘加收缩）、额外 row source policy、`Scalar=double`、泛型点类型或更多 production 入口。每项写清状态、原因和恢复条件。
- 证据边界：当前证据覆盖哪些入口、点类型、`Scalar`、数据布局、规模和目标硬件；不能把 representative pointtypes（代表性点类型）、diagnostic bench 或 QEMU timing（QEMU 计时）写成更宽范围的生产性能结论。
- 如果当前 production gate 是 exact-type gate（具体类型门控），必须在“覆盖范围与 fallback”和“生产接入后的 closeout”
  中列出未覆盖点类型、其它模板实例 fallback、`point_type_expansion_queue` 和下一 phase 的证据要求。
  后续 PointXYZ-like 泛型、PointNormal-like 泛型、其它点型、row source、`Scalar` 或 layout 扩展必须新建
  phase，并重新补 production direct 测试、bench、asm、板卡和 Evidence Doctor。

推荐用一张表把采用和暂缓状态列清：

```text
| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
```

状态建议使用 `adopted`、`attempted`、`deferred`、`rejected` 或 `not_now`。如果当前主题迁移了 sibling topic（同模块相邻主题）经验，该表可以和 experience-migration audit（经验迁移审计）互相引用，但不能只写“参考了相邻经验”。

## Traceability Map

复杂 topic 的 production 长期主题文档应包含或引用 Traceability Map（可追踪性地图）。no-production topic 的 Traceability Map 主归属是 evaluation 或 topic-local doc suite。本小节回答“读者从长期文档如何跳到代码、测试、脚本和 output 复核”。

推荐表格：

```text
| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
```

覆盖范围按当前结论裁剪，至少包括：

- production public entry、dispatch / fallback gate、Std helper、RVV helper 或保持标量的入口。
- RVV test 侧的 reference、row source、candidate、reduction / staging、production-shaped diagnostic、production direct test 和 bench wrapper。
- analysis script、output summary、QEMU / board output、反汇编或 profile 证据入口。
- evaluation 的实现方式审计、适用的 production 长期主题文档证据链或 topic-local 诊断证据链，以及 Handoff Packet 中恢复字段。

每一行都要写清证据角色，例如 correctness gate、RVV-vs-RVV B/A summary、fallback coverage、asm attribution、production boundary 或 recovery pointer。不要只写自然语言说明。

如果表格过长，可以拆到独立 `*-traceability.zh.md`，production 长期主题文档只保留路径、anchor（章节 / 符号 / run label）和 role（证据角色）；no-production topic 则由 evaluation 或 topic-local doc suite 引用。

## 正确性与高效性证据链

production closeout（收尾）或 production-candidate 文档必须新增或更新本小节。小节至少回答：

- correctness（正确性）：public entry（公开入口）是否真实命中目标路径；row semantics（行语义）是否清楚；`accepted_points`、中间态、matrix（矩阵）和 fallback 是否有测试、日志或源码证据。
- performance（性能）：性能结论是否来自 repeated board（重复板卡测试）或目标硬件结果；QEMU timing（QEMU 计时）不能作为性能结论。
- boundary（证据边界）：EvidenceDecision 是否只覆盖证据已经证明的入口、点类型、row source policy（行来源策略）、indices、correspondences、`Scalar`、数据布局和规模。
- risk（风险）：未覆盖范围、保留标量路径、后续扩展条件和需要补的 test、bench、asm（反汇编）或板卡证据。

未接 production 的诊断结论应在 topic-local evaluation / phase closeout 写对应“诊断证据链”。该小节必须说明 diagnostic evidence（诊断证据）
能证明什么，不能写成 production evidence（生产证据）。如果只有 representative pointtypes
（代表性点类型）、public-entry-shaped wrapper（公开入口形态包装）或 production-shaped diagnostic，
必须写清真实 production dispatch、indexed / correspondences、泛型点类型或 fallback 仍未闭合；`doc-rvv`
应标为 `not_applicable`，除非已有 adopted production behavior 需要维护。

## Staging 与特殊实体

如果出现 staging、metadata、history entry、mask helper、数值 helper或对象状态展开 helper，必须逐项说明：

- 实体类型。
- 输入输出。
- 对应的原标量语义。
- 不能直接复用原结构或公共 helper 的原因。
- 覆盖 / fallback 边界。
- 是否改变公开 API 或对象可见状态。

若诊断数据流不是源码中直接可见的形态，必须额外说明映射关系。例如源码 helper 只看到 iterator 同步前进，但 RVV 诊断拆成“顺序点云对”和“对应关系索引扫描”；这时文档要写清每条诊断路径来自哪个公开入口、为什么必须显式展开 index/weight、额外成本是否计入 bench，以及该拆分不能证明哪些真实 production 分流。

多个 staging 名称应给出对照表：

```text
| 暂存路径（staging） | 结构体 | 生成 helper / 代码位置 | 字段来源 | 下一段消费者 / tail | gate |
```

## 关键片段要求

关键 RVV 片段不能只摘几行数学公式。若实现包含类型 / 布局检查、数值 helper、finite / predicate mask、AoS stride、gather load、`vcompress`、scatter、标量写回、staging 结构或后续标量状态机，片段必须覆盖完整阶段并回答：

- 哪些条件进入 RVV，哪些条件 fallback。
- VL chunk 如何取数。
- mask 如何构造。
- 有效 lane 如何保序压缩或写回。
- staging 字段如何被后续标量 tail 消费。
- 为什么整体仍等价于标量语义。

代码片段中的注释只解释边界、fallback、访存、mask、压缩、写回、FRM/FCSR 或 staging 衔接；不要逐行复述 intrinsic。

## RVV Helper + Scalar Tail 双片段模板

当 RVV 只接管前置阶段、staging 生成、mask/压缩或局部纯函数，后续仍由标量 tail 消费 staging 时，主题文档必须同时展示两个片段。

RVV helper 片段应覆盖：

```cpp
// RVV helper: gate -> VL chunk load/gather -> formula/mask -> compress/write staging
```

标量 tail 片段应覆盖：

```cpp
// Scalar tail: read staging -> projection/search/state update/output append
```

两个片段之间必须说明：

- RVV helper 接管原标量代码的哪一段。
- staging 字段分别来自哪些原局部变量、对象成员或输入 lane。
- scalar tail 从哪一个原标量状态继续执行。
- 为什么 tail 当前保留标量，例如语义安全、间接读取、状态机、输出顺序、测试证据不足或目标硬件收益不足。
- full diagnostic 或 production case 是否证明 staging 的额外内存流量没有抵消收益。
- 若 tail 是后续增量诊断方向，列出下一步需要证明的语义和性能证据。

不要只写“前置 RVV staging + 标量尾段”。读者应能从两个片段直接定位职责分割和保留边界。

## 特殊模式要求

如果出现新 RVV 组织模式，必须单独说明：

- 模式名称和动机。
- 与既有单公式筛选、规约、图像式邻域、gather、压缩或 staging 模式的差异。
- 为什么适合当前函数。
- 哪些条件保证标量语义一致。
- 哪些 fallback 边界不能套用。
- 反汇编如何证明路径命中。

典型模式包括多谓词几何 mask、分阶段 mask 收敛、多路压缩输出、显式 FRM 控制、共享输入向量的多个线性谓词 helper/lambda、前置纯函数 RVV staging + 后续标量状态机。

## 性能章节要求

性能章节不能只列 case 名和 speedup。每个 case 至少写清：

- 对应函数入口。
- 数据规模、点类型、字段、kernel、indices、correspondences、权重、随机/合成数据来源或其它参数组合。
- 是否命中 RVV 主路径、fallback 路径或间接受益路径。
- speedup 计算方式。
- 该 case 证明的语义或性能点。
- 该 case 不能证明什么，例如真实 production dispatch、泛型点类型、其它输入形态、规约方案或目标硬件之外的性能。

fallback case 用于证明未覆盖路径保持语义和成本接近，不作为 RVV 主路径性能结论。

如果 production 长期主题文档已经过长，应把完整 bench label 字典、checksum 公式、trace 输出格式和日志提交白名单移到 topic-local benchmark/evidence 文档。production 长期主题文档只保留证据路径、关键结果和边界。拆分后，主题文档必须链接该细分文档。

topic-local benchmark/evidence 文档应把 `run_bench_*`、`run_board_bench_*` 和 repeated board collect target 分开列出。`run_board_bench_*` 是单次板卡 smoke，必须写清默认输出目录和证据等级。性能结论只能引用 repeated board summary 或目标硬件重复采集摘要。

如果一个 topic 同时存在多个 adopted / attempted / deferred 优化方式，或用户需要按优化方式复核“为什么采纳 A、暂缓 B”，应新增或引用 topic-local `optimization-evidence` 文档。该文档按优化方式列出 production / test_support 代码路径、test target、bench target、board evidence、当前结果和不能外推的边界；production 长期主题文档只保留当前 adopted production 方式和该索引入口。

## Topic-Local Doc Suite

复杂 topic 命中下列任一条件时，应把测试和证据说明拆成 topic-local doc suite（主题本地文档套件），而不是把所有内容塞进 evaluation 或适用的 `doc-rvv` production 长期主题文档：

- evaluation 已经同时承担测试说明、bench label、代码地图、候选取舍和 EvidenceDecision。
- test_support / `include/impl` / `src` 中存在三类以上角色，例如 reference、fixtures、row source、candidate、reduction、assertion、bench harness、bench cases、script。
- 存在多个 public entry、row source policy、点类型 / `Scalar` / layout、candidate family 或 repeated board evidence。
- 用户、reviewer 或 worker 从文档难以回答“这个测试名是什么意思、bench label 对应哪条代码路径、checksum 怎么来、日志为什么提交”。
- 相邻成熟 topic 已经通过 reviewer，且提供了清晰的 README、测试总览、正确性测试说明、benchmark/evidence 说明、optimization evidence 和 test-support code map。

若当前 topic 已具备多阶段优化、多个测试支撑角色、多个 public entry / row source / 点型组合，或评审者
需要从文档回答测试语义、bench label、checksum、证据白名单和代码地图，topic-local doc suite 应视为
结构成熟度的一部分，而不是可选装饰。只做 evaluation 迁移、只新增 roadmap 或只保留一个指针文件，
不足以关闭 doc-suite 缺口。
如果相邻成熟 topic 已经用 doc suite 解决了这些读者问题，worker 应把它作为 structure-parity 审计的
quality bar，而不是写成“reviewer 需要时再做”。具体内容不能复制，文件名也可按当前 topic 和配置调整；
但 README、测试总览、正确性测试说明、benchmark/evidence 说明、optimization evidence 和 code map 的
读者路径必须逐项 `adopt / defer / reject`。未阻塞的 `defer` 属于 `phase_deferred + unblocked`，
不能支持 `ready_for_review`。

推荐 doc suite：

```text
README.zh.md
doc/testing-overview.zh.md
doc/correctness-tests.zh.md
doc/benchmark-and-evidence.zh.md
doc/optimization-evidence.zh.md
doc/test-support-code-map.zh.md
doc/optimization-roadmap.zh.md
doc/<topic>-evaluation.zh.md
doc/phases/
```

evaluation 必须放在 `artifact_layout.evaluation_doc_template` 解析路径。旧 topic 如果仍把 `<topic>-evaluation.zh.md`
放在 topic 根目录，worker 应把 legacy evaluation migration（旧评估文档迁移）列入成熟度审计，并做
`adopt / defer / reject` 决策。若暂缓且仍无风险阻塞，通常属于 `phase_deferred + unblocked`，不应让本轮早停。
迁移后默认更新引用并删除根目录旧文件；不要为了泛泛的“避免旧引用断开”保留 legacy pointer。只有明确
外部依赖、用户要求兼容、同轮无法安全更新引用或 dirty isolation 风险时才可临时保留，并必须写出删除条件
和下一阶段清理动作。

README 只负责导航、常用命令和可提交证据入口。`testing-overview` 解释测试类型、运行入口、覆盖矩阵和证据边界；`correctness-tests` 解释每个 gtest 名称、输入、断言和代码位置；`benchmark-and-evidence` 解释 case-filter、bench label、checksum、trace、asm、QEMU/board 边界和日志提交白名单；`test-support-code-map` 解释聚合入口、内部头文件、`src`、script 和 production helper 的调用关系；`optimization-evidence` 按优化方式索引代码、target 和证据；`optimization-roadmap` 保留还可以尝试的搜索空间和下一阶段候选。

## 生产接入后的 Closeout 章节

topic 完成 PI2-PI5 且用户确认采纳后，production 长期主题文档应新增或更新生产 closeout 章节。该章节不需要复述完整代码，
但必须让 reviewer 能从文档直接看出“实际接入了什么、如何回退、证据是否仍成立”：

```text
| 项 | 最终状态 | 证据 |
```

至少覆盖：

- 生产补丁范围：文件、helper、dispatch、编译宏和是否改变 public API（公开接口）。
- 覆盖范围：入口形态、点类型、`Scalar`、数据布局、规模 gate、目标硬件。
- 不覆盖范围：indices、correspondences、泛型点类型、`Scalar=double` 或其它保持标量的路径。
- fallback 矩阵：每个非覆盖路径如何回到原标量语义，以及对应测试 / 构建证据。
- production direct 证据：真实公开入口测试、fallback 测试、反汇编 production 符号归属、板卡 production bench。
- 结论差异：诊断阶段结果与生产直连结果是否一致；若有差异，最终采用哪个结论以及为什么。
- 回退策略：若 PI5 证据不成立，说明生产改动是否回收，保留哪些配置解析出的测试资产或 diagnostic 资产供后续消融。

生产接入后的文档质量不以“新增一段结论”为准，而以读者能否不看对话、只靠源码和证据路径复核生产接入边界为准。

## Partial-production-candidate 写法

当板卡结果显示某一条诊断路径有稳定收益，但还没有修改 production（生产源码）或没有真实公开入口 direct evidence（直接生产路径证据）时，可以写 `partial-production-candidate`。此时文档必须比 no-production closeout 更谨慎：

- 先写候选范围，例如“只限顺序点云对 `PointNormal` / `float` / 连续 AoS 布局 / 目标板卡”。
- 再写明确不覆盖的范围，例如 indices、correspondences、泛型点类型、`Scalar=double`、非 RVV fallback、生产 dispatch、上游完整测试。
- 列出下一轮 production integration loop（生产接入闭环）前必须补的证据：最小生产补丁、真实入口测试、fallback gate、反汇编符号归属、板卡 production bench、误差预算或数值审计。
- 对负向路径保持独立结论。一个入口有收益不能抵消另一个入口退化；对应关系索引路径慢时，应明确保持标量或先做消融。
- 避免使用“可接入 production”“production-ready”这类表达，除非 production direct 证据、维护边界和性能复测都已闭合。

推荐表格：

```text
| 范围 | 当前证据 | 仍缺什么 | 下一轮动作 |
```

## 负向性能归因

如果 board（板卡）或目标硬件结果不支持生产接入，topic-local evaluation / phase closeout 不能只写“不加速”。必须补一段受证据约束的归因；已有 production 长期主题文档只有在需要记录生产回退历史时才更新：

- 哪些源码结构或 RVV 实现选择可能造成退化，例如 gather、不规则访存、压缩到 buffer、额外 store/load、标量 tail、solver/状态机主成本、数据重排或分流开销。
- 哪些证据支持这个判断，例如 asm 中的指令归属、bench case 对比、规模放大趋势、QEMU 只作为路径证据、消融 bench 或 profile。
- 当前还不能确认哪些原因，以及下一轮若要重做，需要新增什么实验。不要把未验证猜测写成事实。
