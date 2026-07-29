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
- trade-off analysis（取舍分析）：回答“为什么采用 A 而不是 B”，需要把 correctness、性能、维护成本和证据缺口一起写清。

### 4. RVV 生产接入证据

- production-shaped diagnostic（生产形态诊断）：尽量复用真实入口状态和调用方式的 test-only 诊断。它接近 production，但仍不是生产证据。
- public-entry-shaped（公开入口形态相似）：参数和调用形态接近公开入口，但可能仍走测试专用 wrapper；它不等于 production dispatch。
- production direct（真实生产路径证据）：真实公开入口命中 production 分流后的测试、反汇编和板卡证据。只有它能证明生产分流成立。
- upstream/integration smoke（上游 / 集成冒烟测试）：证明上游调用形态未被破坏。它不能替代专项 correctness 或板卡 bench。

## 证据顺序

推荐顺序：

```text
unit/boundary correctness -> numerical consistency -> production-shaped diagnostic
  -> benchmark / ablation -> asm attribution -> board evidence -> production direct
```

并非每个 topic 都需要所有类别。worker（执行者）必须说明缺失类别为什么不阻塞当前结论。

## 测试金字塔

高质量 RVV topic 应形成分层金字塔，而不是把所有证据都塞进一个 bench 或一个 diagnostic：

```text
少量：upstream / integration / production direct
中量：benchmark、component ablation、numerical consistency、fallback
大量：unit、boundary、regression
```

unit / boundary 提供快速反馈，numerical consistency 保护 RVV 与标量参考链路，benchmark 和 ablation 解释性能与取舍，production direct 决定真实生产分流是否成立。

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

fallback 测试要隔离触发原因。一个 case 同时命中小规模、非连续和类型不支持时，不能写成完整 fallback 证据。
