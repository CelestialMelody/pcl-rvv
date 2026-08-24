# Phase 046 计划：row-source shuffle mitigation detail A/B

## 阶段意图和边界

Phase 045 已证明 row-source locality / order profile 全部 positive，但 deterministic shuffle（确定性乱序）下 dual-indexed / correspondence 的 64K / 256K B/A 明显低于 contiguous / stride / reverse。本阶段只评估一个同边界 mitigation（缓解）想法：

```text
current RVV row-source gather vs sorted-copy mitigation
```

本阶段不是 Std/RVV public adoption evidence（公开入口标量 / RVV 采纳证据）。Phase 043 已经证明 public RVV path 快于 public scalar path并已采纳；Phase 046 只回答“在同一个 RVV production boundary 内，排序副本是否比当前 RVV gather 更好”。

验证范围：

| 维度 | 本阶段覆盖 |
| --- | --- |
| production entry | dual-indexed 和 correspondence row-source public overload 优先；source-indexed 可作为对照 |
| point type | `PointXYZ -> PointXYZ` |
| `Scalar` | `float` |
| layout | dense xyz AoS |
| order pattern | deterministic shuffle |
| sizes | 64K / 256K 优先，4K 可保留为成本 sanity check |
| candidate | sorted source-index / query-index copy，排序成本计入计时 |

不验证范围：

- 不修改 production 源码，不改变公开 API。
- 不把 sorted-copy positive 直接写成 production patch；若 positive，还需要生产接入计划、correctness/fallback/ASM/board 复核和用户确认。
- 不验证全部泛型点型、非法 index / correspondence、`Scalar=double`、non-dense 或 NaN / Inf。
- 不把 Phase 046 结果外推到所有 shuffle 分布；它只覆盖当前 deterministic shuffle corpus。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| Phase 043 row-source public | adopted-by-user；9/9 board positive，Doctor `Errors=0`、`Warnings=7`。 |
| Phase 044 row-source generic | representative generic positive；9/9 board positive，Doctor `Errors=0`、`Warnings=12`。 |
| Phase 045 locality profile | complete positive；36/36 board positive，Doctor `Errors=0`、`Warnings=29`；shuffle dual-indexed / correspondence 64K/256K 约 `2.2x-2.4x`。 |
| current gap | 仍不知道排序 / staging 成本是否能换来更好的 locality；这必须用 RVV-current vs RVV-mitigation detail A/B 回答。 |

## 假设与候选族

| candidate | hypothesis | risk |
| --- | --- | --- |
| `sorted-copy-by-source` | 对 dual-indexed 的 `(source_idx, target_idx)` 按 source index 排序，可改善 source gather 局部性；对 correspondence 按 query index 排序，可改善 source gather 局部性。 | 排序 O(n log n) 和 index copy 成本可能远高于 gather 收益；target gather 仍可能随机；reduction order 改变但应在误差预算内。 |
| `staged-selected-cloud` | 将 shuffled pairs 先写成临时 selected source/target cloud 后走 ordered path，可能把双 gather 变成 strided loads。 | 需要拷贝 xyz 数据，内存带宽可能吃掉收益；更像生产新 family，若 sorted-copy 已明显 negative，本阶段暂不做。 |

本阶段先只实现 `sorted-copy-by-source`。若它 negative 或 weak，不继续做更重的 staged-selected-cloud；若它 positive，再把 staged-selected-cloud 加入下一 phase。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness | bench / evidence | decision rule |
| --- | --- | --- | --- | --- | --- |
| `row-source-shuffle-sorted-copy-detail-ab` | dual-indexed | `PointXYZ -> PointXYZ` / `float` / dense / shuffle | output 与当前 shuffled public path 误差 <= `5e-4` | RVV-only detail A/B summary：current gather vs sorted-copy, sorting cost included | median sorted/current >= `1.05` 且 min >= `0.97` 才可继续生产探针；否则 rejected / attempted。 |
| `row-source-shuffle-sorted-copy-detail-ab` | correspondence | 同上 | 同上 | 同上 | 同上。 |
| `row-source-shuffle-sorted-copy-detail-ab` | source-indexed | optional sanity | 同上 | 同上 | 只作为对照，不优先影响决策。 |

## 实现和测试动作

| action | artifact / command | 完成判据 |
| --- | --- | --- |
| A0 bootstrap helper | `include/impl/tesvd_scale_support.hpp` 新增 sorted index / sorted correspondence helper。 | helper 只构造合法输入，不改变 production；本计划记录该 bootstrap helper。 |
| A1 bench detail A/B | `src/bench_tesvd_scale.cpp` 新增 `row-source-shuffle-sorted-copy-detail-ab` case-filter。 | RVV build 输出 current 和 sorted-copy paired labels，排序成本计入 sorted-copy case。 |
| A2 correctness guard | 新增或复用 gtest，证明 sorted-copy 输出与原 shuffled public path / selected-cloud reference 对齐。 | Std/RVV correctness 通过；若只在 RVV bench detail 中输出 max error，也必须 QEMU smoke clean。 |
| A3 RVV-only summary script | topic-local script 解析 RVV log 内 current/sorted-copy paired labels。 | 输出 summary / manifest / doctor，B/A 定义为 current_ms / sorted_copy_ms。 |
| A4 QEMU smoke | 新增 `record_qemu_row_source_shuffle_sorted_copy_detail_ab_state`。 | QEMU 只检查 label、max error、summary / manifest / doctor。 |
| A5 board detail A/B | 新增 board repeated target。 | 5-run summary / doctor / registry 完成；按 row source / size 分开报告。 |
| A6 文档同步 | result、matrix、roadmap、testing overview、benchmark/evidence、evaluation。 | 根据 detail A/B 结果更新候选状态。 |

## Evidence Doctor 和 Registry 规则

- 本阶段 evidence role 是 `production_detail_rvv_vs_rvv`，不是 `production_public`。
- QEMU 只证明 build、label、误差和 manifest shape，不作为性能结论。
- board summary 的 B/A 定义为 `current RVV ms / sorted-copy RVV ms`；大于 1 表示 sorted-copy 更快。
- sorted-copy 计时必须包含 index/correspondence copy 和 sort，不允许只计排序后的 public call。
- Doctor warning 必须按 row source / size 解释；若 sorted-copy 只在 4K positive 但 64K/256K negative，则不可进入 production probe。
- registry doc-ref 必须包含本 plan/result、optimization matrix、roadmap 和 benchmark/evidence。

## 板卡复跑预算和决策桶

| 字段 | 值 |
| --- | --- |
| repeated runs | 5 |
| warmup / iterations | 5 / 20 |
| positive | 每个决策 case B/A > `1.20` |
| weak-positive | median >= `1.05` 且 min >= `0.97` |
| neutral / negative / unstable | 沿用 summary script 口径 |
| 复跑策略 | 若 bucket 不变，不无限复跑；若出现 negative / unstable 或 Doctor error，先解释并降级候选。 |

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_detail_rvv_vs_rvv`，同一 production row-source boundary 内的 detail A/B。 |
| A/B boundary | RVV-current public row-source gather vs sorted-copy + same public row-source gather。 |
| 当前决策问题 | sorted-copy 是否值得进入 production integration probe。 |
| diagnostic 是否可外推到 production | 只可作为 production detail probe 的前置信号；不能直接外推为 production patch。 |
| comparison-boundary / baseline mismatch 风险 | 有。sorted-copy wrapper 在 bench 内构造临时 index/correspondence，production 若接入需要相同成本模型和异常语义。 |
| weak / negative / unstable 时是否允许 bounded production probe | 若 64K/256K weak 或 negative，默认不进入 production probe；若仅 4K weak，可按规模门槛暂缓。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段就是该 A/B；若结果 positive，仍需 production integration loop 和用户确认。 |

## 完成条件

- QEMU smoke、board repeated、Doctor 和 registry 完成后，写 `result.zh.md`。
- 若 sorted-copy 64K/256K dual-indexed / correspondence 为 positive 或 strong weak-positive，可把 `staged-selected-cloud` 作为下一 phase。
- 若 sorted-copy neutral / negative，则把候选写成 attempted / rejected，不再继续更重 staging，除非用户要求探索生产外策略。

## 继续 / 停止条件

默认继续到 board detail A/B，因为当前会话已有板卡证据链且用户要求继续优化矩阵。合法停止条件包括：板卡不可用、RVV-only summary target 无法安全接入、QEMU correctness 失败、sorted-copy 改变结果超过误差预算、或用户要求先提交当前 adopted patch。
