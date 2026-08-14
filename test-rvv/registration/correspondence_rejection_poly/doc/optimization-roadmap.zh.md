# correspondence_rejection_poly 优化路线图

## 当前边界

当前 topic 已完成 Phase 000 诊断 scaffold、Phase 010 board repeated evidence（重复板卡证据）、Phase 020 production-shaped gather diagnostic（生产形态 gather 诊断）和 Phase 030 production direct probe（真实生产入口探针）。目标生产文件当前无本 topic diff：

- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`
- `registration/include/pcl/registration/correspondence_rejection_poly.h`

当前 EvidenceDecision 是 `rollback/no-production`。回滚前生产探针按 public entry dispatch（公开入口分流）到 `Standard` / `RVV` helper 的形态实现；回滚后只保留 test-rvv 诊断和历史负向证据。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `edge_length_batch` | `thresholdEdgeLength` 边长比公式 | correspondences row source、`PointXYZ` / `float` | 批量计算 edge similarity（边相似度）。 | 只测预构造 squared distance 数组。 | production-shaped gather 和 production direct。 | historical diagnostic | completed |
| `edge_gather_staging` | Phase 010 弱正向后的真实 row source audit（行来源审计） | correspondence index -> PointXYZ AoS -> squared distance staging -> RVV formula | 检查真实点云 gather 和 staging 成本。 | 不覆盖完整 public entry 的采样、histogram / Otsu 和输出 append。 | production direct board repeated。 | historical diagnostic | completed |
| `production_edge_batch_rvv` | 用户授权进入生产接入阶段 | real `getRemainingCorrespondences` public entry | 通过 `Standard` / `RVV` 分层尝试真实生产分流。 | 完整入口里随机采样、vector staging、标量后段和容器操作抵消收益。 | QEMU correctness、asm、board production-direct repeated、Evidence Doctor。 | rejected by board evidence | no default phase |
| `accept_rate_filter` | 连续 `num_samples` / `num_accepted` 数组 | accept rate 和最终筛选 mask | 连续数组适合 RVV 除法和 mask。 | 输出 append 保序仍需 scalar tail。 | scalar-tail attribution 或 profile。 | attempted diagnostic; neutral | not scheduled |
| `histogram_otsu_scalar` | `computeHistogram` / `findThresholdOtsu` | histogram 和 Otsu threshold | 保持语义。 | histogram 是 data-dependent scatter，Otsu 小循环无热点证据。 | profile 指向时再开候选。 | adopted scalar | not scheduled |
| `structure_parity_doc_suite` | canonical doc-suite quality bar（规范文档套件质量门槛） | topic-local README、doc suite、phase index、evaluation、Handoff | 让 reviewer 可从文档恢复当前 no-production 证据链和提交边界。 | 只涉及文档，不改变性能结论。 | Phase 040 plan/result、artifact tracking scan、`git diff --check`。 | completed | ready_for_review |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `030-pi1-production-integration-plan` | full-entry profile / ablation | production-direct 探针正确但板卡退化，说明完整入口成本结构不同于局部诊断。 | profile、component ablation、asm hot region、board repeated。 | optional |
| `030-pi1-production-integration-plan` | avoid staging / reduce allocation family | 回滚前 RVV helper 为完整采样结果构建多个 staging buffer，可能引入内存流量和分配成本。 | 新 plan、same-boundary correctness、board repeated。 | optional |
| `030-pi1-production-integration-plan` | keep histogram / Otsu scalar | 生产探针已经显示只向量化 edge predicate 不成立。 | 无默认新增证据。 | low |
| `040-structure-parity-doc-suite` | doc-suite parity closeout | 旧 Handoff 写 `ready_for_review`，当前规则要求先完成 canonical quality bar 审计和 artifact tracking。 | Phase 040 result、Handoff、路径限定 git status。 | completed |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `production_edge_batch_rvv` | production-direct board repeated 为 negative，2048 和 8192 两个 case 均 5/5 degradation。 | 只有新 profile 证明退化原因并提出更窄候选时恢复。 |
| `generic point type production` | `PointXYZ` / `float` 生产探针已被性能否决，泛型扩大没有生产价值基础。 | 新 production candidate 先在 `PointXYZ` 上通过 production-direct board。 |
| `accept_rate_filter` | board confirm 为 neutral，且有 Evidence Doctor warning。 | profile 指向 accept-rate / scalar append 是主成本。 |
| `histogram RVV scatter` | 无热点证据，且会引入 data-dependent scatter 风险。 | profile 指向 histogram / Otsu。 |

## 默认恢复动作

当前默认恢复动作为 `ready_for_review`。Phase 040 已证明 doc-suite 审计、artifact tracking、dirty isolation 和 Handoff 已同步。

若用户希望继续性能探索，推荐另开窄范围 Phase 050 profile / ablation plan，先证明完整 public entry 的退化来源，再决定是否设计新候选。该方向需要新 profile、component ablation（组件消融）、asm hot region（反汇编热点区域）和 board repeated evidence（板卡重复证据），不能直接恢复 Phase 030 的生产补丁。
