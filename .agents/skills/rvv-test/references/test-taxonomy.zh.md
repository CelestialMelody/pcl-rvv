# RVV 测试分类与证据层级

本文定义 RVV topic（主题）的测试分类。测试名称可以随项目风格变化，但证据边界必须清楚。

## 分类总览

`rvv-test` 是测试与证据的总称，不等同于狭义 diagnostic（诊断）。一个 topic 不需要机械堆满所有类别，但必须说明当前结论依赖哪些类别，缺失类别为什么不阻塞。

### 1. 基础工程质量测试

- unit test（单元测试）：验证单个 helper、公式或分支。它证明局部 correctness（正确性），不证明入口覆盖或性能。
- integration test（集成测试）：验证多个模块、公开入口、wrapper 或对象状态协同工作时接口和数据流是否正常。
- E2E / system smoke（端到端 / 系统冒烟测试）：从接近真实调用方的入口运行一条主链路，用于证明上游调用形态没有明显破坏。
- regression test（回归测试）：保护已经修复或已接入的行为，防止后续 topic 破坏证据。
- boundary/adversarial test（边界 / 对抗测试）：覆盖 NaN/Inf、阈值附近、空输入、小规模、重复索引、乱序索引、非 dense 输入、无效 lane（向量通道）等边界。
- deterministic corpus（确定性样本集）+ seeded random stress（带种子随机压力样本）：复杂 topic 应同时保留固定样本用于稳定回归，以及带 seed 的随机 / fuzz 样本用于找漏项；文档必须写清 corpus label 和 seed，不能把一次偶然通过当成完整覆盖。
- public input semantics（公开入口输入语义测试）：先审计 production public API 对输入数量、权重长度、空输入、非法索引或异常权重的实际行为，再写测试。若 production 路径是 `PCL_ERROR` 后返回，测试应验证输出状态是否保持调用前值；若行为未定义或 iterator 不做边界检查，不能把 test-only reference 的 defensive skip 写成 public API 合同。
- fallback tests（回退路径测试）：逐 gate（会导致回退或失败的验收条件）证明何时走标量路径。

### 2. 性能与资源证据

- benchmark / bench（性能测试）：测量端到端或组件耗时。真实性能结论只来自板卡或目标硬件。
- stress / load test（压力 / 负载测试）：使用极大规模、高重复次数、极端分布或资源上限样本观察崩溃边界和稳定性。
- sanitizer/profiling（运行时检查 / 性能剖析）：用于定位内存、未定义行为、资源泄漏、cache miss（缓存未命中）或热点，不替代可复现 test 和 bench。
- performance regression（性能回归检查）：比较固定 case 的历史耗时或 speedup（加速比），用于发现隐蔽退化；它需要稳定环境，QEMU timing 不能作为真实性能回归结论。

### 3. 算法与数值验证

- numerical consistency（数值一致性）：对比标量参考链路和 RVV 执行链路，记录误差预算、最大误差、checksum（校验和）和失败阈值。
- component ablation（组件消融）：拆分 gather、staging、reduction、FMA、index 展开等成本。它只提供瓶颈线索，不能单独决定 production。
- diagnostic/probing（诊断 / 探针测试）：用于定位特定边界、路径命中、计数、分布或日志形状，不作为生产性能结论。
- compiler auto-vectorization diagnostic（编译器自动向量化诊断）：用编译器报告检查某段标量代码为什么没有自动向量化，或实际是否被编译器生成了向量路径。该诊断默认不开启，适合作为 S2 早期评估或 S4 证据计划中的辅助输入。
- trade-off analysis（取舍分析）：回答“为什么采用 A、暂缓 B”，需要把 correctness、性能、维护成本和证据缺口一起写清。

### 4. RVV 生产接入证据

- production-shaped diagnostic（生产形态诊断）：尽量复用真实入口状态和调用方式的 test-only 诊断。它接近 production，证据角色仍是诊断。
- public-entry-shaped（公开入口形态相似）：参数和调用形态接近公开入口，但可能仍走测试专用 wrapper；它不等于 production dispatch。
- production direct（真实生产路径证据）：真实公开入口命中 production 分流后的测试、反汇编和板卡证据。只有它能证明生产分流成立。
- upstream/integration smoke（上游 / 集成冒烟测试）：证明上游调用形态未被破坏。它不能替代专项 correctness 或板卡 bench。

### 5. 接入生产前 / 接入生产后分层

复杂 topic 必须把测试分成两类再写：

- 接入生产前测试：筛选 candidate，证明“值得接入”。
- 接入生产后测试：include 生产代码，证明“真实 production 已接入且当前实现值得保留”。

共享语义基线可以跨阶段复用，但文档必须标清它服务的是哪一层。pre-production 的通过不能自动升级成 post-production 的证据。

| 测试层级 | 中文含义 | 典型 target | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- |
| 接入生产前诊断（pre-production diagnostic） | 用 test_support candidate、row source candidate、fused formula candidate 或 component ablation 筛选方向。 | `run_test_candidates`、`run_bench_row_sources`、`run_bench_fused_formula`、`run_board_bench_row_sources` | 某个候选值得继续推进。 | 真实 production dispatch、fallback 或 board 性能主结论。 |
| 生产形态诊断（production-shaped diagnostic） | 尽量贴近 production 调用形态，但仍使用 test-only helper。 | `run_bench_production_shaped_fused_formula`、`run_bench_generic_fused_abc_trace` | wrapper、layout、formula 形态和局部消融是否可行。 | 真实 public overload 或 production direct。 |
| 接入生产后正确性（post-production direct correctness） | include 生产代码，验证真实 public overload、fallback 和 gate。 | `run_test_production_direct`、`run_test_source_indices`、`run_board_test_production_direct`、`run_board_test_source_indices` | 真实 production dispatch / fallback / gate 正确。 | 板卡性能主结论或当前实现族最优性。 |
| 接入生产后性能（post-production performance） | 在目标硬件上复核真实 public overload。 | `run_bench_production_dispatch`、`run_bench_production_source_indices`、`collect_board_production_dispatch_repeated`、`collect_board_production_source_indices_repeated` | 目标硬件上的 repeated 性能结论；需要 run budget、decision bucket、summary / manifest / doctor / registry 状态。 | 未覆盖入口、未尝试的实现族或 QEMU timing。 |
| 实现族比较（implementation-family comparison） | 把当前已采纳的实现族和新入口候选放在同边界下对比。 | `planned` / `audit` / 专门的 family-comparison target | 当前实现族是否真的是该入口的最佳候选。 | 仅凭旧实现的正向 speedup 就直接停止。 |
| 共享语义基线（shared semantics baseline） | 公开入口输入语义和对象状态回归，前后都要保留。 | `run_test_public_semantics`、`run_test_input_semantics`、`run_board_test_public_semantics`、`run_board_test_input_semantics` | 公开入口语义是否仍保持稳定。 | 生产性能和实现族最优性。 |

## 证据顺序

推荐顺序：

```text
unit/boundary correctness -> numerical consistency -> production-shaped diagnostic
  -> benchmark / ablation -> asm attribution -> board evidence -> production direct
```

并非每个 topic 都需要所有类别。worker（执行者）必须说明缺失类别为什么不阻塞当前结论。

## 测试金字塔

高质量 RVV topic 应形成分层金字塔。不要把所有证据都塞进一个 bench 或一个 diagnostic：

```text
少量：upstream / integration / production direct
中量：benchmark、component ablation、numerical consistency、fallback
大量：unit、boundary、regression
```

unit / boundary 提供快速反馈，numerical consistency 保护 RVV 与标量参考链路，benchmark 和 ablation 解释性能与取舍，production direct 决定真实生产分流是否成立。

## 运行入口和测试类型

Make target、脚本入口和测试类型必须分开写。`run_test`、`run_test_compare`、`run_bench_compare` 或 board collect target 是执行入口；unit、public semantics、production direct、component ablation、trace、checksum validation 和 asm attribution 才是测试或证据类型。

topic 文档表格中的“测试类型”栏应优先写中文名称，例如“公开入口语义测试”“公开入口输入语义测试”“局部正确性测试”“组件消融”“真实生产路径测试”“校验和稳定性”和“反汇编归因”。需要保留英文术语时，把英文别名放在括号内。target、TEST 名称、case-filter 和 bench label 保留源码原文。

复杂 topic 的文档至少应有一张入口分类表，说明每个 target 做什么、生成哪些文件、证据类型是什么、能否给出性能结论。QEMU target 只能证明 correctness、路径和日志形状；默认不在 QEMU 上运行完整 bench，只允许窄范围 `qemu_smoke_only`；板卡或目标硬件 repeated summary 才能支撑性能结论。若某个 target 同时被前置诊断和后置回归复用，表格必须写清主层级，不能让 reviewer 猜。

如果同一 topic 的复跑改变了已经写入文档的数值、decision bucket 或证据角色，旧 summary 必须降级为 historical run，新的 run 才能进入当前 truth；pre-production diagnostic、post-production direct 和 repeated board 的表不得混写成一张“最新表”。若 evidence registry 或扫描发现日志被覆盖但文档未刷新，测试分类表必须把相关 target 状态写成 `stale_doc_pending_refresh` 或 `manual_run_detected`。

证据承载型测试类型必须能映射到 make target、脚本 target 或 board collect target。若当前还没有可执行入口，只能写成 `planned`、`audit` 或 `not_yet_covered`，不能写成已覆盖。`run_test_compare`、`run_bench_compare` 可以作为汇总入口，但复杂 topic 应提供细粒度 alias 或明确的 filter / case-filter，使 reviewer 能单独复核 public semantics、input semantics、production direct、component ablation、trace、checksum 或 asm attribution。

复杂 topic 中，重要 gtest filter 应提供 `run_board_test_*` 或等价板卡 smoke 入口。每个 `run_bench_*` case-filter 应提供对应的 `run_board_bench_*` 单次板卡入口，或在表格中指向已有 repeated board collect target。单次板卡入口只证明板卡可执行和日志形状；性能结论仍需要 repeated board summary。会覆盖日志或 summary 的 target 应登记到 topic-local `log/evidence_registry.json` 或等价状态文件。

复杂 topic 的细粒度 target 表应至少包含 `target`、`主测试类型`、`附带类型或 case-filter`、`主要日志 / summary` 和 `证据边界`。一个 target 可以承载多个测试类型，但表格必须写清主角色。综合入口应标为 aggregate 或 summary entry，不要把它写成某个单独测试类型的唯一证据。

`implementation-family comparison` 也是测试类型，但它通常属于接入生产后的性能审计项。若当前还没有可执行 target，只能写成 `planned`、`audit` 或 `not_yet_covered`，不能把现有生产路径的正向结果直接升级成“该入口全家族最优已证实”。

当某个测试类型从 `planned`、`audit`、`diagnostic gap` 或 `not_yet_covered` 升级为 `adopted` 时，target 表、coverage matrix 和 evidence index 必须同步修改。不能让同一条测试类型同时出现在“仅诊断”和“已采纳”两侧。

## 测试矩阵条目

涉及点云、索引或字段布局时，测试矩阵至少审计：

- row source policy（行来源策略）：full-cloud、source-indexed、dual-indices、correspondences。
- mixed fields（混合字段）：xyz、normal、intensity、label 或自定义字段是否同时满足布局条件。
- point traits（点类型字段特征）：字段是否存在、类型是否匹配、offset（字段偏移）是否可用。
- AoS stride（结构数组跨步）：字段间距是否固定，是否满足 load/store 计划。
- gather（离散加载）：索引是否有效、乱序、重复、局部性差或越界。
- valid-index-only（仅有效索引）：索引预过滤是否属于 bench 计时边界。
- production predicate（生产谓词）：production 中的有限值检查、mask、状态条件是否被复刻。
- invalid lane finite mask（无效通道有限值掩码）：NaN/Inf lane 是否按 production 语义跳过。
- input cardinality（输入数量）：source/target、indices、weights、correspondences 或其它并行输入的长度不匹配时，公开入口实际如何处理。
- weight domain（权重取值域）：0、负数、NaN/Inf 和极大/极小权重是否属于有效输入，是否参与 finite mask，是否传播到中间态。

fallback 测试要隔离触发原因。一个 case 同时命中小规模、非连续和类型不支持时，不能写成完整 fallback 证据。
