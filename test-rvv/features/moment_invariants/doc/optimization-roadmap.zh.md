# moment_invariants optimization roadmap

## 当前边界

当前 topic 已有 adopted production behavior（已采纳生产行为）。生产接入只覆盖真实 `MomentInvariantsEstimation::computeFeature` 的 indexed neighbor path（索引邻域路径）：`__RVV10__` 开启、`PointOutT=pcl::MomentInvariants`、输入点型满足 `RVVXYZAoSFloatLayout<PointT>`、`cloud.is_dense`、邻域规模至少 16 且 u32 byte offset 可表达时，centroid 后六个中心矩由 RVV gather（离散加载）和 vector reduction（向量规约）完成。

已由板卡证据关闭的点型为 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA`。未验证范围包括 `PointXYZRGBNormal`、`PointXYZINormal`、用户自定义点型、full-cloud overload、其它输出类型、`Scalar=double`、非 dense surface 和 KdTree/search 优化。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| helper indexed moment accumulation RVV | retained-candidate rescreen + MOI fused reduction experience | centroid 后 6 个中心矩累加 | helper-only indexed gather 和规约缩短 | 输出只有三值，search 稀释明显 | QEMU correctness、asm、board repeated、Evidence Doctor | attempted: Phase 000 weak-positive diagnostic | closed |
| public-search dilution check | current source shape | KdTree search + helper replacement | 判断 helper weak-positive 是否能穿透公开入口形态 | 仍不是真实 production dispatch | public-shaped board repeated | attempted: Phase 010 near-threshold weak-positive | closed |
| production indexed moment accumulation RVV | user-authorized bounded production probe | `PointXYZ / float / dense AoS / PointOutT=MomentInvariants` | 真实 public path 保留小幅收益 | vector reduction 改变累加树；fallback 必须保守 | production direct tests、asm、board repeated、doctor、registry | adopted: Phase 030 | closed |
| PointXYZ-like production expansion | generic point type strategy | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` | 常见 xyz AoS 点型复用同一 production path | stride 变大可能影响 gather 局部性 | typed correctness、asm、board repeated、doctor | adopted: Phase 040 | closed |
| evidence hardening | Evidence Doctor suggestion | already adopted production cases | 补强长尾和复现解释能力 | 需要记录 taskset/governor/freq/temperature/binary hash | new repeated run with richer metadata | deferred, low-risk hygiene | optional future phase |
| normal compound point expansion | point-type expansion queue | `PointXYZRGBNormal`、`PointXYZINormal` | 可能复用 xyz-only input path | 更大 stride、normal 字段不参与算法但 layout/bench 未证明 | dedicated correctness、asm、board repeated、doctor | deferred with evidence | new phase only on request |
| custom xyz traits expansion | generic point type strategy | user-defined traits-compatible xyz AoS | 保留模板泛型价值 | 无代表性输入和 board evidence | compile/layout tests and representative board case | deferred with evidence | new phase only with real point type |
| full-cloud production path | helper-only evidence | full-cloud overload | 避免只带 cloud 的 helper 保持标量 | 缺真实 public caller/profile；可能不是热点 | caller/profile, production direct bench | not_now | separate phase if profile appears |
| centroid / search profile | future profile | centroid or KdTree search | 若真实主成本在 search/centroid，另选优化点 | 超出本 helper topic | workload profile and module boundary check | separate_followup_topic | separate topic |

## 默认恢复队列

| action | 状态 | 恢复条件 |
| --- | --- | --- |
| review / commit decision | ready_for_review | Phase 030/040 文档、matrix、roadmap、production doc、summary/manifest/doctor registry 和 asm gates 已同步。 |
| evidence hardening | deferred with evidence | 需要更严格复现或出现长尾 / 方向反转时，补 taskset、governor、freq、temperature 和 binary hash 后重跑 adopted cases。 |
| normal compound / custom point type expansion | deferred with evidence | 用户明确要扩大点型范围，并接受每种点型独立补 correctness、asm、board repeated 和 Evidence Doctor。 |
| full-cloud overload | not_now | 出现真实 full-cloud caller 或 profile 证明该 overload 是热点。 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | helper-only RVV 可保留为诊断资产。 | `1.141x` weak-positive 表明局部公式可被 RVV 缩短。 | 后续若复用，需要补 binary hash 和环境 metadata。 | low |
| Phase 010 | bounded production probe 可以在用户确认后执行。 | public-search-shaped 虽近阈值，但没有退化，候选实现小。 | PI1-PI5 production direct evidence。 | completed by Phase 030 |
| Phase 030 | exact `PointXYZ` gate 只是阶段局部例外。 | production-public `PointXYZ` 证据正向，泛型策略建议扩展常见 xyz AoS 点型。 | typed correctness、asm、board repeated、doctor。 | completed by Phase 040 |
| Phase 040 | evidence hardening 只作为卫生改进。 | 四个 production-public case 都是 0/5 退化，但 Doctor 仍建议补环境和 binary identity。 | richer metadata repeated run。 | optional |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| normal compound point expansion | 当前算法只读 xyz，但 `PointXYZRGBNormal` / `PointXYZINormal` 的 stride 和实例化成本未被本阶段板卡证明。 | 新 phase 冻结点型列表并补 typed public tests、asm、board repeated 和 doctor。 |
| custom point type expansion | 没有真实用户点型和代表性 layout 证据，不能从 PCL 内置点型外推。 | 用户提供或选定代表性 traits-compatible 点型。 |
| full-cloud production path | 当前 public `computeFeature` 使用 indexed 邻域 helper；full-cloud overload 缺真实调用性能证据。 | 出现调用 profile 或上游路径证明该 overload 是热点。 |
| KdTree/search optimization | search 成本属于其它模块/公共 helper，不能混入 moment accumulation production patch。 | 另开 search 或更高层 pipeline topic。 |
