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

这里的 row source policy（行来源策略）更准确地说是 row-source ingress policy（行来源入口策略）。它只描述每一行从哪里来、如何展开成 source/target/weight 三元组，以及这些展开是否计入 bench；它不等于整条 RVV 优化 family（优化族）或完整的生产策略。full-cloud、source-indexed、dual-indices 和 correspondences 可以共享同一段 math kernel（数学内核）或 reduction/formula family，但每个 policy 都需要单独的 entry adapter（入口适配器）和证据边界。

如果已有某个 policy 采纳了更强的 math family，其他 policy 不能默认继承“同一 family 也一定适用”。必须按 policy 逐一补 candidate、bench、board 和 production direct 证据，或者写出为什么 dataflow / gather / spill / staging 成本让 family 不适用。

## 长 Test Support 文件拆分

当配置解析出的测试资产中的 helper header（辅助头文件）过长，或同时包含标量 reference、RVV math、row source policy、reduction candidate、component ablation 和 bench/test wrapper 时，应优先拆成稳定聚合头和内部头文件：

- 短 prompt 恢复 phase loop 时，即使用户没有显式要求“重构测试框架”，也要把 RVV test support architecture（RVV 测试支撑架构）作为完成度审计项。若当前 topic 的测试入口、helper、bench case、文档或 evidence boundary 已经难以区分 reference / diagnostic / production direct 职责，应把拆分或重构列为当前 phase 的候选未完成项。
- 不要把某个 sibling topic（同类主题）的文件清单、缩写、目录结构或实现族机械复制过来；只继承通用职责划分和质量 bar。具体拆分应从当前 topic 的源码、测试、bench、证据和 dirty isolation 推导。

- 默认偏好来自 `.agents/config/defaults.yaml` 的 `test_support` 配置：helper header 超过
  `helper_split_soft_line_limit`（默认约 800 行）时应评估拆分；超过
  `helper_split_hard_line_limit`（默认约 1000 行），或同时包含不少于
  `helper_split_responsibility_threshold`（默认 3）类职责时，worker 必须优先拆到
  配置指定的内部目录，或在 Handoff Packet 中写清 deferred reason（暂缓理由）。
- 外部 include 入口保持当前 topic 的稳定命名。每个 topic 必须先确定一个稳定的 topic token（主题短标识）。
  短 topic 默认直接使用完整 topic 名；只有完整 topic 名过长、会让 `src` / `include` 文件名明显难读时，
  才使用清晰、唯一、可追溯的 abbreviation（缩写）。使用缩写时，应在 README 或 handoff 中说明
  `<topic> -> <topic-token>` 的映射。
- 新 topic 默认放在 `test_support.aggregator_directory` 解析出的目录下，并按
  `test_support.aggregator_prefix`、topic token 和 `test_support.aggregator_extension`
  生成宽口径 aggregator header（聚合头文件）。当前默认 `test_support.aggregator_prefix` 可为空，
  此时宽口径入口形如 `<topic-token>.h`；长名 topic 可以是 `teptplw.h`，短 topic 可直接是
  `<topic>.h`。本文不固定任何 topic token、目录字面量或完整文件名。
- role-specific aggregator（按入口角色拆出的聚合头）优先和 `src` 入口同向命名。默认
  `test_support.role_aggregator_name_order: role_topic` 表示使用 `<role>_<topic-token>.h`，
  例如 `test_teptplw.h` 和 `bench_teptplw.h`，分别对应 `src/test_teptplw_*.cpp` 和
  `src/bench_teptplw.cpp`。若 topic 使用旧命名或本机 override，应在 README / handoff 中说明。
- `.agents/local/user-preferences.yaml` 可覆盖本机命名偏好，例如聚合入口目录、聚合入口前缀、
  role-specific aggregator 命名顺序、topic token / abbreviation 策略、内部目录、扩展名和内部头文件是否带
  topic token 前缀。不要默认创建或提交本机 override（覆盖）文件；若使用本机覆盖，Handoff Packet
  只说明读取了哪些覆盖项和最终生效行为。
- 历史 topic 若已有兼容聚合入口，在 `test_support.compatibility_aggregator_alias_allowed` 为 true 时可暂作
  compatibility alias（兼容别名）保留；它不是新 topic 的默认命名。若继续保留兼容别名，且
  `test_support.compatibility_aggregator_alias_requires_handoff_reason` 为 true，Handoff Packet 必须说明保留原因。
- 只有内容确实是狭义 diagnostic/probing（诊断 / 探针）时，才使用配置或当前 topic 既有结构指定的狭义诊断位置。
- 内部实现优先放到 `test_support.internal_directory` 解析出的目录。`test_support.internal_header_roles`
  是通用职责词表，不是每个 topic 必须照抄的文件清单。默认角色应保持跨模块通用，例如
  `core_types`、`fixtures`、`references`、`adapters`、`candidates`、`assertions`、
  `bench_harness` 和 `bench_cases`；topic 可按领域再细分，但不要把某个 topic 的算法形状写回
  defaults（默认配置）。
  是否使用 topic token 前缀由 `test_support.prefer_topic_token_prefixed_internal_headers` 决定。例如该配置为
  true 时，内部头文件名应由 topic token、职责名和 `test_support.internal_header_extension` 组合生成。
- 领域细分应从通用职责派生：registration topic 可以在 `fixtures` / `adapters` / `candidates`
  下继续拆出 row source policy（行来源策略）、reduction（规约）或 formula candidate（公式候选）；
  math topic 可以拆 special values（特殊值）或 kernels（内核）；filter topic 可以拆 neighborhood
 （邻域）和 boundary cases（边界样本）。这些是局部命名，不是全局默认角色。
- topic 的 test / bench 源码默认按 `artifact_layout.test_source_template` 和
  `artifact_layout.bench_source_template` 解析；默认模板使用 `{topic_token}`，不是强制使用完整 `{topic}`。
  topic Makefile 或等价 harness 应引用解析后的源码路径，
  并把 `test_support.aggregator_directory` 解析出的目录加入 include path，使 test / bench 只 include
  聚合入口，不直接依赖内部实现目录。
- `src/test_*.cpp` 和 `src/bench_*.cpp` 本身也要保持窄职责。短文件可以只按章节分段；当 test / bench
  源码超过 helper soft limit、包含 3 类以上职责，或 review 时已经难以定位 case 意图，应按 evidence
  layer（证据层）和入口职责拆成多个 `.cpp`，并更新 topic Makefile 的 `SRCS_TEST` / `SRCS_BENCH`。
  拆分后 test 源码应主要呈现 TEST 清单；bench 源码应主要呈现 `main`、CLI 入口和输出合同。
- 如果 bench case registry（用例注册表）已经成为主要长度来源，优先把具体 case 组装逻辑拆到
  `bench_cases` 角色的内部头或源文件，保持外层 bench 源码薄入口。case 名、case-filter、计时边界、
  checksum 和 trace 输出格式属于证据合同，结构拆分时必须保持不变；若确实改动，应单独说明并重跑
  bench 解析脚本。
- 如果内部实现确实只包含狭义 diagnostic/probing helper，也可以使用狭义诊断位置；一旦混入 bench-facing wrapper 或 component ablation，优先使用宽口径 test support 位置。
- 每个内部头文件开头写中文说明：本文件负责什么，属于 RVV 测试证据支撑，不能证明 production dispatch。
- 拆分应先保持算法逻辑、case 名、bench 逻辑和证据口径不变；若同时重写算法或 bench，应作为单独变更说明并重跑对应验证。
- header-only test support 拆分后，验证时不要只依赖已有二进制或直接 `make build/...`。如果 topic
  harness 只把 `.cpp` 写进 `SRCS_TEST` / `SRCS_BENCH`，应先使用对应 `clean_test_*`、`clean_bench_*`
  或会主动清理的 compare/run target，确保 std/RVV 两边都重新编译。
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
