# Phase 058 计划：dual-indexed 256K sorted-copy stability

## 阶段意图和边界

本阶段只复核 Phase 046 中 `dual-indexed shuffle 256K` sorted-copy（排序副本）弱正向子边界的稳定性。比较边界保持为同一 production row-source public boundary（生产 row-source 公开边界）内的 RVV-vs-RVV detail A/B：

```text
current RVV shuffled dual-indexed gather
vs
copy/sort by source + same RVV public dual-indexed path
```

本阶段不修改 production 源码，不扩大 Phase 047 correspondence sorted-copy branch，不接入 dual-indexed sorted-copy，不覆盖 4K / 64K、correspondence、泛型点型、自定义 layout、非法 indices、非 dense 输入或 `Scalar=double`。

## 当前状态清单

| item | 当前状态 |
| --- | --- |
| Phase 046 dual-indexed 256K | 5-run board median B/A `1.185x`，min `1.050x`，bucket `weak_positive`。 |
| Phase 046 dual-indexed 64K | 有一次 `0.789x` 退化，不能作为 clean probe。 |
| Phase 047 correspondence sorted-copy | 已按 correspondence + size/disorder gate 接入并采纳；不能外推到 dual-indexed。 |
| Phase 048 staged-selected-cloud | 6/6 case negative，已拒绝。 |
| Phase 049 target-sorted | dual-indexed 64K negative，256K weak-positive，已拒绝。 |
| 当前生产状态 | ordered、row-source、correspondence sorted-copy 和 matrix-local simplification 已按历史确认采纳；本阶段不改变这些行为。 |

## 假设与候选族

`sorted-copy-dual-indexed-256k-stability` 的假设是：Phase 046 的 256K weak-positive 可能不是偶然噪声，若把测试范围收窄为单一 dual-indexed 256K shuffle case，并增加 board repeated run count，可以判断它是否稳定到足以进入后续 bounded production probe 讨论。

风险是 copy + sort 成本只带来边际收益；即使复核仍为 weak-positive，也可能不足以抵消 production gate、维护和误判输入 disorder 的复杂度。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `sorted-copy-dual-indexed-256k-stability` | dual-indexed | `PointXYZ -> PointXYZ` / `float` / dense xyz AoS / shuffle 256K | test-local detail A/B；不接 production | current 20-test QEMU correctness；Phase 046 correctness guard | `row-source-shuffle-dual-indexed-256k-sorted-copy-stability` | 计划 10-run board repeated，B/A=current RVV/sorted-copy RVV | same RVV binary detail A/B；candidate 计时包含 copy + sort | QEMU / board Doctor | pending |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 窄 case-filter | `src/bench_tesvd_scale.cpp` | 新增只输出 current / sorted-copy dual-indexed 256K pair 的 case-filter。 |
| A2 Make / registry target | `Makefile` | 新增 QEMU smoke、board repeated、summary、Doctor、registry target。 |
| A3 QEMU smoke | `log/qemu/row_source_shuffle_dual_indexed_256k_sorted_copy_stability/` | manifest 可解析；Doctor 输出并登记；QEMU timing 不作为性能结论。 |
| A4 board repeated | `log/board/row_source_shuffle_dual_indexed_256k_sorted_copy_stability_repeated/` | 10-run board summary、manifest、Doctor 和 registry。 |
| A5 文档回填 | phase result、matrix、roadmap、topic-local docs | 若证据仍弱或 negative，写成拒绝或暂缓；只有强正向才进入后续 production probe 讨论。 |

## Evidence Doctor 和 registry 规则

QEMU 和 board summary 使用 `script/generate_tesvd_scale_detail_ab_summary.py`。Evidence Doctor 必须在 EvidenceDecision 前运行；Errors / Warnings 不能被静默忽略。registry 记录本阶段 summary、manifest、doctor 和 case-filter；`evidence_status` 必须 fresh。

## 阶段完成条件

| condition | decision |
| --- | --- |
| 10/10 runs B/A > `1.20`，median >= `1.30`，Doctor 无 Error | `positive_probe_candidate`，但仍需另开 PI1-PI5 才能讨论 production。 |
| median >= `1.05` 且 min >= `0.97`，但不满足强正向 | `weak_positive_not_production_ready`。 |
| 任一 run < `0.97` 或 Doctor 有退化 Error | `rejected_or_unstable_with_evidence`。 |
| board 不可用 | `turn_stop_deferred with board_required`，保留 QEMU smoke 和恢复命令。 |

## 板卡复跑预算和决策桶

- run count：10。
- warm-up / iteration：沿用 topic board target 默认 `20` iterations、`5` warm-up iterations。
- 不因单次低值自动剔除 run；summary 保留 min / median / max。
- 若 10-run 后仍只是 weak-positive，本阶段不继续追加复跑，避免围着边际收益无限打转。

## 继续 / 停止条件

本阶段完成后：

- strong positive 也只允许进入新的 dual-indexed bounded production probe plan，不能直接改 production。
- weak-positive、unstable 或 negative 则关闭 dual-indexed sorted-copy 路线，不再作为默认恢复动作。
- `Scalar=double` 和更广 custom layout / alignment 仍需要用户另给数值预算或采样空间。

## 文档更新清单

- `doc/phases/058-dual-indexed-256k-sorted-copy-stability/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/testing-overview.zh.md`
- `doc/transformation_estimation_svd_scale-evaluation.zh.md`
- `README.zh.md`

## roadmap 同步动作

若本阶段弱正向或负向，roadmap 应把 `dual-indexed sorted-copy` 从 Phase 046 残留 next action 中移到 rejected / closed，并说明 resume condition 只能是新的 candidate family 或用户要求更大规模 / 不同输入分布。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_detail_rvv_vs_rvv`。 |
| A/B boundary | current RVV shuffled dual-indexed public path vs copy/sort-by-source + same RVV public dual-indexed path。 |
| 当前决策问题 | Phase 046 的 dual-indexed 256K weak-positive 是否稳定到值得进入 production probe 讨论。 |
| diagnostic 是否可外推到 production | 不能直接外推。强正向也只产生 bounded production probe candidate。 |
| comparison-boundary / baseline mismatch 风险 | 有；candidate 计时包含 copy + sort，且只覆盖一个 synthetic shuffle shape。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许；weak-positive 只能记录为边际收益，不接 production。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要；本阶段就是 detail A/B 复核，但 production adoption 仍需 PI1-PI5 和用户确认。 |
