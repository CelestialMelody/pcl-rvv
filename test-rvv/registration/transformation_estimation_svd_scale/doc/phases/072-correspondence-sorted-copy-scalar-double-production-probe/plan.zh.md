# Phase 072 Plan: correspondence sorted-copy `Scalar=double` production probe

## 阶段意图和边界

本阶段在 Phase 069、Phase 070 和 Phase 071 均仍为 `positive_pending_user_confirmation` 的前提下，继续一条独立优化路线：评估 correspondence sorted-copy（对应关系排序副本）是否应扩展到 `Scalar=double`。本阶段不做任何 adoption closeout（采纳收尾），也不把 Phase 069 / 070 / 071 写成 `adopted-by-user`。

验证范围：

- public correspondence overload（公开对应关系入口）。
- `PointXYZ -> PointXYZ`、`Scalar=double`、dense xyz AoS（结构数组）。
- 合法 shuffled correspondences（洗牌式乱序对应关系），规模优先覆盖 64K / 256K；小规模 4K 只作为 gate / negative-control（门控 / 负向对照）。
- sorted-copy 触发条件复用既有 float production heuristic（生产启发式）：size 至少 64K 且 query index disorder（查询索引乱序程度）足够高。

不覆盖：

- Phase 069 / 070 / 071 的用户采纳确认。
- dual-indexed sorted-copy；Phase 058 已把 dual-indexed 256K source-sorted-copy 复核为 rejected / unstable，本阶段不复活该路线。
- ordered、source-indexed、contiguous affine fast path 或 custom layout double sorted-copy。
- 非 dense、小规模采纳、非法 correspondence 语义、任意自定义点型全集或全部 `Scalar=double` row-source family selection。

## 当前状态清单

- Phase 047 已采纳 `correspondence-sorted-copy-production-probe`，但范围只覆盖 `PointXYZ -> PointXYZ` / `Scalar=float` / dense xyz AoS / size >= 64K / shuffle-like disorder。
- Phase 058 已拒绝 dual-indexed source-sorted-copy residual：10-run board median B/A `1.010x`，5/10 run 低于 1；因此本阶段不得把 sorted-copy 泛化到 dual-indexed。
- Phase 067 已采纳 exact `PointXYZ -> PointXYZ` / `Scalar=double` row-source production branch，Phase 069 又把 common PCL xyz AoS / `Scalar=double` row-source probe 推到 pending confirmation；但 double correspondence 当前走 D64 gather，不走 sorted-copy helper。
- 当前 production helper `estimateRigidTransformationSVDScaleCorrespondencePairSortedCopyRVV` 只接受 `Eigen::Matrix<float, 4, 4>&`，double correspondence branch 在 contiguous fast path 后直接落到 `accumulateTransformationEstimationSVDScaleCorrespondencePairD64RVV`。

## 假设与候选族

候选族：`correspondence-sorted-copy-scalar-double-production-probe`。

假设：在 `Scalar=double` 的 shuffled correspondence public path 中，copy + sort 成本仍可能被更好的 query locality（查询侧局部性）抵消；当前 D64 gather 已能保证 correctness，但没有利用 Phase 047 float sorted-copy 的 locality mitigation（局部性缓解）形态。

风险：

- `Scalar=double` 后每个 lane 的吞吐、寄存器压力、D64 reduction（双精度规约）和 copy/sort 成本比例不同，float 的 positive 不能外推。
- public Std/RVV positive 只能证明当前 public RVV path 快于 scalar fallback，不能证明 sorted-copy double 优于现有 D64 gather family；若本阶段用于 family selection，必须补同一 production boundary 内的 RVV-vs-RVV detail A/B，或把结论降级为 bounded production candidate（有界生产候选）。
- sorted-copy 会改变 accumulation order（累加顺序）；correctness 必须用 selected-cloud double reference（选中子云双精度参考）和误差预算约束。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | bench / board | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `correspondence-sorted-copy-scalar-double-production-probe` | correspondence | `PointXYZ -> PointXYZ` / `double` / dense xyz AoS / shuffled 64K / 256K | 新增 correctness guard 或 phase target RED；public matrix 对齐 selected-cloud double reference | QEMU smoke + board repeated，case-filter 专门隔离 double sorted-copy | QEMU / board Evidence Doctor | pending |
| `correspondence-sorted-copy-scalar-double-production-probe` | correspondence | `PointXYZ -> PointXYZ` / `double` / 4K shuffled | fallback / no-adoption control | board 可保留为小规模对照，不作为 positive 采纳范围 | Doctor 按 size 分开解释 | expected non-adopted control |

## 实现和测试动作

1. 先做 RED：运行尚不存在的 `correspondence-sorted-copy-scalar-double-production-probe` case-filter / Make target，确认本阶段证据入口缺失。
2. 增加 correctness guard（正确性保护）：64K shuffled correspondences 下，`TransformationEstimationSVDScale<PointXYZ, PointXYZ, double>` public correspondence result 与 selected-cloud double reference 对齐；fallback boundary 不扩大到小规模 / non-dense / unsupported layout。
3. 增加 bench case-filter 和路径标签：输出必须区分 double sorted-copy candidate 和 double scalar fallback / D64 gather，不把 QEMU timing 写成性能结论。
4. 最小 production patch：新增 D64 sorted-copy helper，复用 sorted correspondences 后调用 `accumulateTransformationEstimationSVDScaleCorrespondencePairD64RVV` 与 `solveTransformationEstimationSVDScaleD64`；double correspondence branch 在 contiguous fast path 后尝试该 helper，失败则回落 D64 gather。
5. 增加 QEMU smoke、board repeated、Evidence Doctor 和 evidence registry target。
6. 运行 correctness、QEMU smoke + Doctor、board repeated + Doctor、evidence freshness、py_compile 和 diff check。
7. 更新 phase result、optimization matrix、roadmap、benchmark/evidence、optimization evidence、correctness tests、test-support code map、evaluation、长期 `doc-rvv` pending 边界和 Handoff。

## Evidence Doctor 和 registry 规则

- QEMU 只证明 build / path / log-shape（构建 / 路径 / 日志形状），不支撑性能结论。
- board repeated 才支撑 performance bucket（性能决策桶）。
- Doctor Error 必须修正或降级；Warning 必须解释，并保留 min / median / max，不剔除异常值。
- registry 登记到 `test-rvv/registration/transformation_estimation_svd_scale/log/evidence_registry.json`；最终运行 `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` 检查 freshness（新鲜度）。

## 板卡复跑预算和决策桶

- 使用既有 `TESVD_SCALE_BOARD_REPEATED_RUNS`、`TESVD_SCALE_BOARD_BENCH_ITERATIONS` 和 warm-up 设置。
- `positive`：64K / 256K planned case median B/A 大于 1，且退化频率不改变结论桶。
- `weak_positive`：median 只略大于 1、`B/A < 1` 频率偏高或 warning 显著；不能 clean-adopt。
- `negative`：median 小于 1 或多数 run 退化。
- `unstable`：预算内跨桶摇摆；降级 EvidenceDecision 或交给用户 / reviewer 判断。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public correspondence double sorted-copy probe（真实公开入口对应关系双精度排序副本探针）。 |
| A/B boundary | public overload；Std 为 public double scalar fallback，RVV 为当前 production patch 下的 correspondence double path。若要回答 sorted-copy double 是否优于 D64 gather family，需要另补 production-detail RVV-vs-RVV A/B。 |
| 当前决策问题 | RVV-vs-scalar 为本阶段基础问题；RVV-family-selection 只能在补同边界 detail A/B 后回答。 |
| 是否可外推到 production | 只能外推到本阶段列出的 `PointXYZ -> PointXYZ` / `double` / dense shuffled correspondence / size gate。不能外推到 dual-indexed、custom layout、generic point types 或小规模。 |
| comparison-boundary / baseline mismatch 风险 | 中等；public Std/RVV positive 可能被 scalar fallback 差异放大，不能单独证明 sorted-copy double 是最佳 RVV family。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段就是 bounded production probe；若 board weak / negative / unstable，只能保留为 attempted，不得拒绝其它 unrelated double 路线。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；因为当前 double correspondence 已有 D64 gather RVV family，clean adoption sorted-copy double 需要同边界 RVV-vs-RVV detail A/B 或用户明确接受 bounded candidate 风险。 |

## 完成条件

- Phase plan 先于 Phase 072 文件修改存在。
- RED 证据确认 phase case-filter / target 缺失。
- Std/RVV correctness 通过。
- QEMU smoke + Evidence Doctor 无 Error。
- board repeated 完成，或记录真实 blocker。
- result / matrix / roadmap / Handoff 明确 Phase 069 / 070 / 071 仍未采纳。

## 继续 / 停止条件

- 若 public probe positive 但尚未完成 RVV-vs-RVV detail A/B：停在 pending / bounded 状态，不自动采纳。
- 若 public probe weak / negative / unstable：写成 attempted / rejected-with-boundary，只关闭本阶段 `PointXYZ -> PointXYZ` / `double` / correspondence sorted-copy 子边界。
- 若还有未阻塞路线，继续 phase loop；只有 roadmap / matrix 中当前授权范围内无可推进方向，或命中用户确认 / 板卡 / 工具 / dirty isolation 真实停止条件时才停止。
