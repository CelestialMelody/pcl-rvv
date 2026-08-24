# Phase 049 结果：dual-indexed target-sorted detail A/B

## EvidenceDecision

`attempted_negative_or_mixed`。

本阶段尝试了 `dual-indexed-target-sorted-detail-ab`：在双索引 shuffle 输入上，把 current shuffled gather 与按 target 排序的同一 public dual-indexed estimate 做 detail A/B 对照。结果没有形成 production probe：64K 为 negative，256K 只有 weak-positive，board overall bucket 仍为 negative，不能接入 production。

## 执行摘要

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| QEMU smoke | 完成 | `record_qemu_row_source_target_sorted_detail_ab_state` 通过；只作为 build / label / manifest / correctness-adjacent smoke。 |
| QEMU correctness | 完成 | `run_test_compare_recorded` 现为 15 tests；新增 `RowSourceTargetSortedMatchesShuffledPublicPath`。 |
| board repeated | 完成 | 5 runs；dual-indexed 64K negative，dual-indexed 256K weak_positive。 |
| board Evidence Doctor | 完成 | QEMU doctor `Errors=2`、`Warnings=0`；board doctor `Errors=1`、`Warnings=1`。 |

## Board 结果

| case | median B/A | bucket | 结论 |
| --- | ---: | --- | --- |
| dual-indexed 64K | `0.897x` | negative | 按 target 排序后，RVV 仍退化；64K 不适合作为 production probe。 |
| dual-indexed 256K | `1.170x` | weak_positive | 有收益但不够稳定，且 64K 已经负向，不能 clean-adopt。 |

## 结论边界

- 这条路线不进入 production。
- 不能把 target-sorted 的 256K 弱正向外推到 64K。
- 不能把 current board 结论外推到 correspondence 或 staged-selected-cloud。
- 如果后续还想继续 dual-indexed shuffle，必须另开新 candidate family 或更窄诊断，而不是直接收口到 production probe。

## 文档同步

已同步或需要同步的状态：

- `doc/phases/README.zh.md`：新增 Phase 049 状态。
- `doc/phases/optimization-matrix.zh.md`：新增 target-sorted detail A/B 记录并标记 negative / mixed。
- `doc/optimization-roadmap.zh.md`：把 target-sorted 路线放入暂缓 / 拒绝区。
- `README.zh.md`、`doc/testing-overview.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/transformation_estimation_svd_scale-evaluation.zh.md`、`doc/test-support-code-map.zh.md`、`doc/correctness-tests.zh.md`：补充 Phase 049 target、证据边界和负向结论。

## 下一步

如果还要继续优化，必须换候选家族或再收窄边界；target-sorted dual-indexed 这条线本阶段收束。
