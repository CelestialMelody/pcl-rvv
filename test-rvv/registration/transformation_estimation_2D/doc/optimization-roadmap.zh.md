# Optimization Roadmap

本路线图记录 `transformation_estimation_2D` 的跨阶段候选搜索空间。阶段 `plan/result` 负责本阶段闭环，`doc/phases/optimization-matrix.zh.md` 记录证据状态；本文件保存后续仍可恢复的 candidate family（候选族）、暂缓 / 拒绝路线和默认恢复动作。

## 当前边界

当前 production 源码保留一个窄范围 RVV behavior（RVV 行为）：exact `PointXYZ -> PointXYZ`、`Scalar=float`、dense finite ordered-cloud-pair、点数不少于 16。Phase 050 的 production-public probe 已在最新真实板卡复跑中得到稳定正向结果，当前 EvidenceDecision 是 `production-candidate-supported / user-review-pending`。其它 row source、点型和 `Scalar` 仍不自动继承该结论。

当前 topic 边界：

- 目标入口为 `TransformationEstimation2D` 四个 `estimateRigidTransformation` overload。
- Phase 010/020 已覆盖顺序点云对（ordered-cloud-pair，source/target 按相同下标一一对应）的 `PointXYZ -> PointXYZ`、`Scalar=float` 诊断。
- Phase 050 最新证明 exact `PointXYZ -> PointXYZ`、`Scalar=float`、dense finite ordered-cloud-pair 的 production-public probe 在目标板卡上 4K/64K/256K 分别达到 `4.222x / 5.310x / 4.947x`，Doctor `0/0/0`，支持保留窄范围 production patch。
- Phase 030 已为 source-indexed-cloud-pair、dual-indexed-cloud-pair 和 correspondence-pair 建立 materialize-to-ordered test-only candidate；三类 candidate 的 correctness、QEMU、asm 归属和板卡 repeated 已完成，但 row-source Doctor `1/2/6`，不能继承 ordered-cloud-pair 的 production 结论。
- `Scalar=double`、泛型点型和 production direct 作为后续扩展，需要独立证据。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| scalar baseline characterization | 当前 production 源码 | all row source policies | 建立 public semantics、耗时基线和 correctness reference（正确性参考链路）。 | 非法 index / correspondence 不宜写成安全合同。 | public semantics tests、QEMU correctness、bench baseline。 | partial / current scalar truth | `row-source-board-repeated` for current candidate comparison |
| two-pass centered fused 2D correlation accumulator | Phase 010 / Phase 020 证据 | ordered-cloud-pair, dense finite `PointXYZ`, `Scalar=float` | 避免两份 demean dynamic matrix（动态矩阵）和 Eigen correlation multiply。 | 已在窄范围 production public path 获得正向板卡证据。 | same-chain tests、bench、asm attribution、board repeated summary、Evidence Doctor。 | retained diagnostic and production-backed narrow shape | review current patch |
| production-public fused 2D correlation probe | Phase 050 证据 | exact `PointXYZ -> PointXYZ`, `Scalar=float`, dense finite ordered-cloud-pair | 真实 public overload 窄范围分流。 | 最新板卡 4K/64K/256K 为 4.222x/5.310x/4.947x，Doctor 0/0/0。 | PI3 correctness、QEMU smoke、asm、board repeated、Evidence Doctor。 | production-candidate-supported / user-review-pending | review current patch before adoption |
| raw sums fused formula | Phase 010 负向样本 | ordered-cloud-pair | 理论上遍历更少。 | near-cancellation（近抵消）样本下对 reduction tree 过敏。 | 已由 Phase 010 初次 RVV run 暴露问题。 | rejected for now | 只有有误差补偿方案时恢复。 |
| dense finite gate | 当前 `compute3DCentroid` / `demeanPointCloud` 语义审计 | ordered-cloud-pair dense corpus | 首个 diagnostic 收窄到语义清晰且常见的 dense input。 | 非有限 public semantics 不能被改写。 | public semantics tests 和 fallback/gate tests。 | adopted in test-only diagnostic | new PI1 only if a new family appears |
| common RVV reuse audit | common `centroid.hpp` 已有 RVV overload | ordinary cloud API vs iterator path | 判断专用 2D RVV 是否比复用 common RVV 更有价值。 | 2D helper 走 iterator overload，可能无法命中 common ordinary-cloud RVV 路径。 | vectorization report（可选）、bench 分段、asm attribution。 | attempted via asm categories | optional profile only |
| source-indexed carry-over | registration row-source evidence 规则 | source-indexed-cloud-pair | 先物化 source row，再复用同公式 fused math pipeline。 | 64K 接近阈值，收益可能被访存稀释。 | policy-specific correctness、QEMU smoke、asm、5-run board summary、Evidence Doctor。 | attempted / diagnostic-only | `060-production-candidate-review-and-row-source-boundaries` |
| dual-indices carry-over | registration row-source evidence 规则 | dual-indexed-cloud-pair | 双侧物化后复用 fused math pipeline。 | 64K 出现 0.956x 长尾，双 gather / staging 成本不稳定。 | candidate correctness、QEMU smoke、asm、5-run board summary、Evidence Doctor。 | attempted / diagnostic-only | `060-production-candidate-review-and-row-source-boundaries` |
| correspondences carry-over | registration dataflows doc | correspondence-pair | 由 query/match 物化点对，再复用 fused math pipeline。 | 64K 2/5 退化，展开成本不能被 median 小幅正向掩盖。 | public semantics、candidate correctness、QEMU smoke、asm、5-run board summary。 | attempted / diagnostic-only | `060-production-candidate-review-and-row-source-boundaries` |
| topic-local doc suite parity | doc-suite quality bar | README、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map | 让下一轮 worker 和 reviewer 不依赖聊天上下文。 | 后续 evidence / scripts 增长时需刷新。 | doc-suite quality bar 审计和 artifact tracking scan。 | adopted for Phase 050 | refresh if new phase starts |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000-current-state-and-gaps | fused 2D correlation accumulator | 源码先写 demean matrix，再做 correlation；2D angle 只需要 2x2 `H`。 | correctness、bench、asm、board 和 Evidence Doctor。 | high |
| 000-current-state-and-gaps | finite semantics audit | `compute3DCentroid` 用 `pcl::isFinite`，但 `demeanPointCloud` 全量写出。 | non-finite corpus 和 public semantics tests。 | high |
| 010-scaffold-and-ordered-cloud-pair-correlation-diagnostic | two-pass centered correlation | raw sums 在近抵消样本上对 reduction tree 过敏；中心化两遍更接近 production demean 语义。 | Phase 020 board / asm。 | high |
| 020-board-and-asm-evidence | PI1 production integration plan | Board diagnostic 5-run 为 `weak_positive`，asm 归属和 Evidence Doctor 已闭合到诊断层。 | PI1 fallback matrix、production direct tests、production asm 和 board production evidence plan。 | completed |
| 040-production-integration-plan | exact `PointXYZ` first production gate | generic point type strategy 已读取；Phase 020 只覆盖 `PointXYZ -> PointXYZ`，因此 PI1 选择窄 gate。 | PI3 fallback tests、PI4 production asm 和 board public repeated summary。 | completed / archived |
| 050-pi2-production-patch-and-direct-evidence | production candidate supported | 最新 production-public board repeated 为 4.222x / 5.310x / 4.947x，Doctor 0/0/0。 | 保留窄范围 patch，刷新 evaluation、matrix、roadmap 和 Handoff。 | completed / current |
| 050-pi2-production-patch-and-direct-evidence | row-source family carry-over remains separate | ordered-cloud-pair 的 production-public 证据只覆盖 exact ordered row source，不能把其它 row source 写成 production follow-up。 | 新 phase 只在 test-rvv 范围补 policy-specific diagnostic。 | medium |
| 030-row-source-family-carryover | materialize-to-ordered bounded diagnostic | 三类 row source 的展开成本必须进入 bench，先把 row pairing 与数学 candidate 分开验证。 | board repeated 可用后决定是否继续 gather / staging 设计。 | medium |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| broadening production patch | ordered-cloud-pair 当前正向，但 indexed / correspondence 仍有 64K 不稳定证据。 | 保留 exact gate，禁止自动扩大 row source / 点型 / Scalar；新边界必须独立 PI1。 |
| raw sums fused formula | Phase 010 近抵消样本失败，说明它对 reduction tree 过敏。 | 有补偿求和、double accumulator 或其它误差预算证据时再恢复。 |
| indexed / correspondences production | 当前只有 test-only materialize candidate，没有 production dispatch；row-source board Doctor 为 1/2/6，64K 存在退化或长尾。 | 先完成布局 / 稳定性分析；即使后续正向也必须重新写 PI1，不能直接进入 production。 |
| `Scalar=double` RVV | 当前候选和板卡计划只覆盖 float。double reduction 和 trig output 需要独立预算。 | double-specific correctness、asm 和目标硬件证据。 |
| generic point type production | 点型 traits、offset、POD / standard-layout gate 未审计；当前正向证据只覆盖 exact `PointXYZ`。 | 需要新的实现族和泛型证据，暂不作为当前默认续作。 |

## 默认恢复动作

`roadmap_default_recovery_queue`：

1. `060-production-candidate-review-and-row-source-boundaries`：审阅当前 production diff、fallback、16/16 correctness、最新 board manifest 和 row-source Doctor；冻结是否采用窄范围 patch。
2. `row-source-layout-stability-diagnostic`：若继续 indexed / correspondence，先分析 materialize、缓存局部性和 64K 长尾，再决定是否设计 gather/staging candidate。
3. `new-production-attempt`：只有新的 row-source 实现族或用户明确要求扩大 production，才重新写 PI1；不能把当前 ordered-cloud-pair patch 自动复制到其它入口。

QEMU timing 始终不能替代板卡性能结论。
