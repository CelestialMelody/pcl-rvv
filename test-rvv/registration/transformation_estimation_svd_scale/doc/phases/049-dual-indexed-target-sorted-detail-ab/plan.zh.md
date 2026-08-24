# Phase 049 计划：dual-indexed target-sorted detail A/B

## 阶段意图和边界

本阶段只验证一个窄诊断问题：Phase 046 的 dual-indexed shuffle 256K source-sorted copy 只有 weak-positive，是否因为只改善 source gather locality，而 target gather 仍保持乱序。本阶段新增 target-sorted copy detail A/B（按 target index 排序的双索引副本细节对照），比较：

```text
current shuffled dual-indexed RVV path
vs target-sorted dual-indexed RVV path
```

本阶段不修改 production 源码，不改变 Phase 047 adopted correspondence sorted-copy branch，不覆盖 4K、小规模、correspondence、source-indexed、泛型点型、非法 index 或 `Scalar=double`。

## 当前状态清单

| area | 当前状态 | 路径 |
| --- | --- | --- |
| source-sorted detail A/B | Phase 046 已完成；dual-indexed 256K median `1.185x`、min `1.050x`，只能作为 weak candidate。 | `doc/phases/046-row-source-shuffle-mitigation-detail-ab/result.zh.md` |
| correspondence production probe | Phase 047 已采纳，只覆盖 correspondence + size >= 64K + shuffle-like disorder。 | `doc/phases/047-correspondence-sorted-copy-production-probe/result.zh.md` |
| staged-selected-cloud | Phase 048 negative，不恢复为 production candidate。 | `doc/phases/048-row-source-shuffle-staged-selected-cloud-detail-ab/result.zh.md` |
| 当前 correctness | 阶段开始前 Std/RVV 各 14 tests passed；本阶段若新增 correctness guard，结果文档必须更新为 15 tests。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |

## 假设与候选族

| candidate | 假设 | 风险 |
| --- | --- | --- |
| `dual-indexed-target-sorted-copy-detail-ab` | 对 dual-indexed shuffle，target gather locality 可能比 source gather locality 更影响 256K；按 target 排序可能改善当前弱正向或解释为何 source-sorted 不稳定。 | 排序改善一侧 locality 会恶化另一侧；copy + sort 成本可能吞掉收益；64K 可能仍不稳定。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `dual-indexed-target-sorted-copy-detail-ab` | dual-indexed only | `PointXYZ -> PointXYZ` / `float` / dense xyz AoS；shuffle 64K / 256K | test-rvv detail A/B；不进入 production | 复用 selected-cloud scalar reference；若新增 TEST，`run_test_compare` 结果更新为 15 tests | 新增 `row-source-shuffle-dual-indexed-target-sorted-detail-ab` case-filter | 需要 board repeated 后决策；QEMU timing 不作为性能结论 | same RVV binary detail A/B；candidate wrapper 是 copy/sort + same public dual-indexed overload | 需要 QEMU / board Doctor | pending | 若 256K positive 且 64K 不退化，可考虑 production probe；否则收束。 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 support helper | 新增 `makeSortedIndexPairsByTarget`。 | target 排序保持 source/target pair 语义。 |
| A2 bench case-filter | 新增 `row-source-shuffle-dual-indexed-target-sorted-detail-ab`，只覆盖 64K / 256K。 | 输出 current / target-sorted paired labels 和 `max_reference_error`。 |
| A3 Make / registry | 新增 QEMU smoke、board repeated、Doctor、registry targets。 | `record_qemu_*` 可生成 summary / manifest / doctor。 |
| A4 phase result | 跑完 QEMU smoke 后回填；若板卡可用再补 board repeated。 | 不把 QEMU timing 写成性能结论。 |

## Evidence Doctor 和 registry 规则

QEMU summary / doctor 只验证 label、manifest、checksum 和误差形状；性能判断必须来自 board repeated。新增 evidence 必须登记到 `log/evidence_registry.json`，并在 `evidence_status` 中保持 fresh。

## 板卡复跑预算和决策桶

默认沿用 topic 的 5 runs、20 iterations、5 warmup。决策桶沿用 detail A/B 脚本：B/A = current RVV ms / target-sorted RVV ms；大于 1 表示 target-sorted 更快。若 64K 或 256K 出现高频退化，candidate 不能进入 production probe。

## 继续 / 停止条件

- 256K median >= 1.20 且 min > 1.0：可考虑下一阶段 production-shaped probe plan。
- 256K weak-positive 但 64K negative / unstable：只记录为 weak diagnostic，不接 production。
- 64K / 256K 都 negative：关闭该路线。
- 板卡不可用时：保留为 `turn_stop_deferred with board_required`，不能用 QEMU timing 决策。

## 文档更新清单

本阶段完成后更新 phase result、phase README、optimization matrix、roadmap、README、testing overview、benchmark/evidence、optimization evidence 和 code map。除非后续 production probe 被用户确认采纳，否则不更新长期 `doc-rvv` 为 adopted behavior。

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_detail_rvv_vs_rvv` diagnostic。 |
| A/B boundary | current shuffled dual-indexed public RVV path vs target-sorted copy + same dual-indexed public RVV path。 |
| 当前决策问题 | target-sorted 是否比 source-sorted 更适合 dual-indexed shuffle。 |
| diagnostic 是否可外推到 production | 不能直接外推；最多生成下一阶段 bounded production probe 条件。 |
| comparison-boundary / baseline mismatch 风险 | 有，candidate 计时包含 copy + sort，并改变 source/target gather locality 分布。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许，除非 256K 明确 positive 且 64K 不退化。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 是，本阶段就是前置 detail A/B；production adoption 还需真实 probe 和用户确认。 |
