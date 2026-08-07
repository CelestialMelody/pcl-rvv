# 诊断入口与 Row Source Policy

本文定义 production-shaped diagnostic（生产形态诊断）、staged candidate（分阶段候选）和
production direct（真实生产路径证据）的边界。

## 分层规则

- staged candidate：可以只覆盖标量路径中的一个阶段，例如公式、load/gather、staging（分阶段暂存）或 reduction（规约）。它不能替代完整入口证据。
- production-shaped diagnostic：用 test-only 代码模拟真实公开入口、对象状态和数据流。它用于判断是否值得接 production，但仍不是 production direct。
- production direct：真实 production 源码中的公开入口命中 RVV dispatch（分流逻辑）后的证据。只有它能证明生产分流成立。

public-entry-shaped（公开入口形态相似）不等于 production dispatch。测试专用 wrapper 即使参数和公开入口相同，也不能证明真实源码已经接入 RVV。

diagnostic evidence（诊断证据）不等于 production evidence（生产证据）。诊断中的板卡 speedup 只能支持候选判断，不能直接写成 production-ready。

## Row Source Policy

full-cloud（全云顺序扫描）、source-indexed（源索引路径）、dual-indices（双索引路径）和
correspondences（对应关系路径）是不同 row source policy（行来源策略）。production 必须逐 policy 独立批准。

- full-cloud：source 和 target 按同一下标一一对应。
- source-indexed：source 由 indices 指定，target 可能顺序扫描或另有策略。
- dual-indices：source 和 target 分别由两个索引数组指定。
- correspondences：点对由 correspondence 结构中的 query/match 索引指定。

RowSourcePolicy 只负责 row source。shared math pipeline（共享数学流水线）负责 finite mask（有限值掩码）、公式、staging/reduction、`accepted_points` 和 `ATA/ATb` 等后段。

设计时不要把 row source 和 math pipeline 混在一个不可审查 helper 中。推荐文档显式写：

```text
row source -> field load/gather -> finite mask -> formula -> staging/reduction -> accepted_points -> ATA/ATb
```

## 长 Test Support 文件拆分

当配置解析出的测试资产中的 helper header（辅助头文件）过长，或同时包含标量 reference、RVV math、row source policy、reduction candidate、component ablation 和 bench/test wrapper 时，应优先拆成稳定聚合头和内部头文件：

- 默认偏好来自 `.agents/config/defaults.yaml` 的 `test_support` 配置：helper header 超过
  `helper_split_soft_line_limit`（默认约 800 行）时应评估拆分；超过
  `helper_split_hard_line_limit`（默认约 1000 行），或同时包含不少于
  `helper_split_responsibility_threshold`（默认 3）类职责时，worker 必须优先拆到
  配置指定的内部目录，或在 Handoff Packet 中写清 deferred reason（暂缓理由）。
- 外部 include 入口保持当前 topic 的稳定命名。新 topic 默认放在 `test_support.aggregator_directory`
  解析出的目录下，并按 `test_support.aggregator_prefix`、topic abbreviation（主题缩写）和
  `test_support.aggregator_extension` 生成宽口径 aggregator header（聚合头文件）；本文不固定任何
  topic abbreviation、目录字面量或完整文件名。
- `.agents/local/user-preferences.yaml` 可覆盖本机命名偏好，例如聚合入口目录、聚合入口前缀、
  是否必须提供 topic abbreviation、内部目录、扩展名和内部头文件是否带 topic abbreviation 前缀。不要默认创建或提交本机
  override（覆盖）文件；若使用本机覆盖，Handoff Packet 只说明读取了哪些覆盖项和最终生效行为。
- 历史 topic 若已有兼容聚合入口，在 `test_support.compatibility_aggregator_alias_allowed` 为 true 时可暂作
  compatibility alias（兼容别名）保留；它不是新 topic 的默认命名。若继续保留兼容别名，且
  `test_support.compatibility_aggregator_alias_requires_handoff_reason` 为 true，Handoff Packet 必须说明保留原因。
- 只有内容确实是狭义 diagnostic/probing（诊断 / 探针）时，才使用配置或当前 topic 既有结构指定的狭义诊断位置。
- 内部实现优先放到 `test_support.internal_directory` 解析出的目录，按 `test_support.internal_header_roles` 中的职责拆分；
  是否使用 topic abbreviation 前缀由 `test_support.prefer_topic_prefixed_internal_headers` 决定。例如该配置为
  true 时，内部头文件名应由 topic abbreviation、职责名和 `test_support.internal_header_extension` 组合生成。
- topic 的 test / bench 源码默认按 `artifact_layout.test_source_template` 和
  `artifact_layout.bench_source_template` 解析。topic Makefile 或等价 harness 应引用解析后的源码路径，
  并把 `test_support.aggregator_directory` 解析出的目录加入 include path，使 test / bench 只 include
  聚合入口，不直接依赖内部实现目录。
- 如果内部实现确实只包含狭义 diagnostic/probing helper，也可以使用狭义诊断位置；一旦混入 bench-facing wrapper 或 component ablation，优先使用宽口径 test support 位置。
- 每个内部头文件开头写中文说明：本文件负责什么，属于 RVV 测试证据支撑，不能证明 production dispatch。
- 拆分应先保持算法逻辑、case 名、bench 逻辑和证据口径不变；若同时重写算法或 bench，应作为单独变更说明并重跑对应验证。
- 文档和 handoff 要说明 aggregator 入口仍稳定，避免 reviewer 误以为 public test/bench include 合同改变。

## 退化归因边界

correspondences 或 indexed 路径退化不能单因归因为 gather（离散加载）。可能原因还包括：

- query/match 展开。
- 容器访问和 bounds check（边界检查）。
- baseline 标量路径本身更短。
- 索引分布局部性差。
- weight 或 field offset 额外加载。
- `vcompress`、buffer 写回或 scalar tail。
- Eigen solver 或后段成本稀释。

没有 component ablation（组件消融）或 profile（性能剖析）时，归因必须写成假设。
