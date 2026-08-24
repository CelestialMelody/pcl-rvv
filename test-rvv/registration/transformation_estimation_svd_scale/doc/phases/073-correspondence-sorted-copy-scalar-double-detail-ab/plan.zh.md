# Phase 073 Plan: correspondence sorted-copy `Scalar=double` detail A/B

## 阶段意图和边界

本阶段只回答 Phase 072 留下的 implementation-family selection（实现族选择）问题：在同一个 production correspondence boundary（生产对应关系边界）内，`Scalar=double` 的 sorted-copy（排序副本）候选是否优于既有 D64 gather RVV family。

验证范围：

- production-detail RVV-vs-RVV（生产细节 RVV 对 RVV）A/B。
- correspondence row source（对应关系行来源）。
- `PointXYZ -> PointXYZ`、`Scalar=double`、dense xyz AoS。
- legal shuffled correspondences，64K / 256K。

不覆盖：

- Phase 069 / 070 / 071 / 072 的 adoption closeout。
- public Std/RVV（标量 / RVV）收益；Phase 072 已证明该层为 positive。
- dual-indexed sorted-copy、custom layout sorted-copy、generic point type sorted-copy、小规模或非法 correspondence。
- production dispatch 变更；本阶段优先只改 test-rvv bench / evidence target。

## 当前状态清单

- Phase 073 计划时，Phase 072 已把 public correspondence `Scalar=double` sorted-copy probe 推到 bounded public-positive candidate：Std/RVV 38/38 correctness 通过，QEMU Doctor `0/0/0`，board 64K / 256K median B/A `5.727x` / `5.408x`，board Doctor `0/0/0`。
- Phase 072 的 public Std/RVV positive 不能证明 sorted-copy double 优于当前 D64 gather RVV family。
- production header 当前已有两个可在 RVV 构建中同边界调用的 detail helper：
  - baseline：`accumulateTransformationEstimationSVDScaleCorrespondencePairD64RVV` + `solveTransformationEstimationSVDScaleD64`。
  - candidate：`estimateRigidTransformationSVDScaleCorrespondencePairSortedCopyRVV(..., Eigen::Matrix<double, 4, 4>&)`。

## 假设与候选族

候选族：`correspondence-sorted-copy-scalar-double-detail-ab`。

假设：对于 shuffled correspondence 的 `Scalar=double`，copy + sort 成本可能仍被 query-side locality（查询侧局部性）改善抵消；如果同边界 RVV-vs-RVV B/A 明显大于 1，则 Phase 072 的 bounded candidate 可升级为 clean adoption 的候选证据输入。若 B/A 为 weak / neutral / negative / unstable，Phase 072 只能继续作为 public positive bounded probe，不能 clean-adopt sorted-copy double family。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test / smoke | board evidence | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `correspondence-sorted-copy-scalar-double-detail-ab` | correspondence | `PointXYZ -> PointXYZ` / `double` / dense shuffled 64K / 256K | RVV-only bench smoke；baseline 和 candidate 都对齐 selected-cloud double reference | repeated board RVV-vs-RVV detail A/B | QEMU / board Evidence Doctor | pending |

## 实现和测试动作

1. RED：运行 `make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_correspondence_sorted_copy_scalar_double_detail_ab_smoke`，确认当前 target 缺失。
2. 在 `src/bench_tesvd_scale.cpp` 增加 `correspondence-sorted-copy-scalar-double-detail-ab` case-filter。baseline 直接调用 D64 gather detail helper，candidate 调用 sorted-copy double detail helper；两侧共享 selected-cloud double reference、同一 solve、同一 checksum policy。
3. 在 Makefile 增加 QEMU RVV-only smoke、manifest / Evidence Doctor / registry target，以及 board repeated / summary / doctor / registry target。QEMU 只看路径和日志形状，不作为性能证据。
4. 如现有 `generate_tesvd_scale_detail_ab_summary.py` 的 manifest metadata 仍是 float row-source gate 文案，增加参数让 Phase 073 写出 double correspondence detail boundary。
5. 运行 correctness、QEMU smoke + Doctor、board repeated + Doctor、evidence freshness、py_compile 和 diff check。
6. 回填 Phase 073 result、optimization matrix、roadmap、README、benchmark/evidence、optimization evidence、evaluation、长期 `doc-rvv` pending/bounded 边界和 Handoff。

## Evidence Doctor 和 registry 规则

- evidence role：`production_detail_rvv_vs_rvv`。
- A/B formula：`B/A = D64 gather RVV ms / sorted-copy double RVV ms`，大于 1 表示 sorted-copy double 更快。
- strict A/B equal fields：boundary、row source、point type、Scalar、layout、solve、checksum policy、gate 和 mask。
- allowed mismatch：wrapper / timer boundary / reduction，因为 candidate 额外包含 correspondence copy + sort，且 reduction order 不同。
- Doctor Error 必须修正或降级；Warning 必须解释并保留 min / median / max。

## 板卡复跑预算和决策桶

- 使用既有 `TESVD_SCALE_BOARD_REPEATED_RUNS=5`、`TESVD_SCALE_BOARD_BENCH_ITERATIONS` 和 warm-up 设置。
- `positive`：64K / 256K median B/A 明显大于 1，且 `B/A < 1` 频率不改变结论。
- `weak_positive`：median 略大于 1 或 warning 明显；只能作为人工判断输入。
- `neutral` / `negative`：不能 clean-adopt sorted-copy double family。
- `unstable`：预算内跨桶摇摆，降级为需要人工判断。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-detail RVV-vs-RVV。 |
| A/B boundary | production detail helper；baseline 为 D64 gather RVV，candidate 为 sorted-copy double RVV。 |
| 当前决策问题 | RVV-family-selection。 |
| 是否可外推到 production | 只能外推到 Phase 072 的 public correspondence / `PointXYZ -> PointXYZ` / `Scalar=double` / dense shuffled 64K + 256K boundary。 |
| comparison-boundary / baseline mismatch 风险 | 低到中等；两侧同在 RVV binary 内，但 candidate timer 有 copy + sort，reduction order 不同，必须由 max reference error 和 checksum policy 约束。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 072 bounded public probe 可保留，但不能 clean-adopt sorted-copy double family。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段就是该缺口；若正向且 Doctor 无阻塞，再等待用户确认采纳。 |

## 完成条件

- RED target 缺失被记录。
- RVV-only QEMU smoke 生成成对 baseline / candidate labels，并由 detail A/B summary 脚本解析。
- Board repeated 完成，或记录真实 blocker。
- Evidence Doctor 无 Error；Warnings 已解释。
- Phase 069 / 070 / 071 仍为 `positive_pending_user_confirmation`，Phase 072 不自动 adopted。

## 继续 / 停止条件

若 detail A/B positive，Phase 072 可从 bounded candidate 升级为 `detail_ab_positive_pending_user_confirmation`，但仍不能自动 adopted。若 detail A/B weak / neutral / negative / unstable，则 Phase 072 保持 bounded / attempted 状态，并继续 roadmap 中的其它未阻塞路线或等待用户确认策略。
