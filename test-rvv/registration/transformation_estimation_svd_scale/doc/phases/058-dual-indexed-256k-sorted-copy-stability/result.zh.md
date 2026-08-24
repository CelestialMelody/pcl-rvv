# Phase 058 结果：dual-indexed 256K sorted-copy stability

## 执行摘要

本阶段按 `plan.zh.md` 只复核 Phase 046 残留的 `dual-indexed shuffle 256K` sorted-copy 弱正向线索。比较边界保持为同一 production row-source public boundary 内的 RVV-vs-RVV detail A/B：

```text
current RVV shuffled dual-indexed gather
vs
copy/sort by source + same RVV public dual-indexed path
```

本阶段没有修改 production 源码，没有扩大 Phase 047 correspondence sorted-copy branch，也没有把 dual-indexed sorted-copy 接入生产。

`EvidenceDecision`：`rejected_or_unstable_with_evidence / sorted-copy-dual-indexed-256k-stability`。

## 动作回填

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| A1 窄 case-filter | done | `src/bench_tesvd_scale.cpp` 新增 `row-source-shuffle-dual-indexed-256k-sorted-copy-stability` | 只输出 `dual-indexed 256K` current / sorted-copy 一对 label，不再混入 4K / 64K / correspondence。 |
| A2 Make / registry target | done | `Makefile` 新增 QEMU / board / Doctor / registry target | Phase 058 证据写入独立 qemu / board 目录，不覆盖 Phase 046 历史 summary。 |
| A3 QEMU correctness + smoke | done | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded record_qemu_dual_indexed_256k_sorted_copy_stability_state` | Std/RVV 各 20 tests passed；QEMU smoke 可解析，QEMU timing 不作为性能结论。 |
| A4 board repeated | done | `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_dual_indexed_256k_sorted_copy_stability_repeated` | 10-run board repeated 完成；summary / manifest / Doctor 已生成并登记。 |
| A5 文档回填 | done | 本 result、phase index、matrix、roadmap、topic-local docs | dual-indexed sorted-copy 路线不进入 production；Phase 046 的 residual next action 已关闭。 |

## Correctness 与 QEMU

Correctness freshness：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded
```

结果：Std 20 tests passed、RVV 20 tests passed。

Phase 058 QEMU smoke：

- 路径：`log/qemu/row_source_shuffle_dual_indexed_256k_sorted_copy_stability/summary.md`
- case：`dual-indexed 256K`
- B/A：`0.605x`
- max reference error：`2.777576e-05`
- Doctor：`Errors=1`、`Warnings=0`、`Suggestions=0`

QEMU 的退化 Error 只说明 QEMU timing 不能作为性能结论；本阶段真实性能判断使用 board repeated。

## Board repeated 结果

Board summary 路径：

- `log/board/row_source_shuffle_dual_indexed_256k_sorted_copy_stability_repeated/summary.md`
- `log/board/row_source_shuffle_dual_indexed_256k_sorted_copy_stability_repeated/evidence_manifest.json`
- `log/board/row_source_shuffle_dual_indexed_256k_sorted_copy_stability_repeated/evidence_doctor.md`

B/A = current RVV ms / sorted-copy RVV ms；大于 1 表示 sorted-copy 更快。

| case | runs B/A | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| dual-indexed 256K | `0.979, 1.087, 1.119, 1.167, 0.957, 0.804, 1.231, 0.902, 0.946, 1.041` | 1.010 | 0.804 | 1.231 | `negative` |

## Evidence Doctor

Board Doctor：

- `Errors=1`
- `Warnings=1`
- `Suggestions=1`

| signal | 观察 | 处理方式 |
| --- | --- | --- |
| `ba_degradation_frequency` | 5/10 runs 低于 1。 | 按退化频率降级为 rejected / unstable，不用 median 略大于 1 证明收益。 |
| `long_tail_or_variance` | min `0.804x`、median `1.010x`、max `1.231x`，max/min `1.53`。 | 保留 min / median / max，不追加复跑，不剔除低值 run。 |
| `near_threshold_ba` | median 距离 1.0 阈值不足 0.05。 | 不进入 production probe；若未来继续 dual-indexed mitigation，需要新 candidate family。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_detail_rvv_vs_rvv`。 |
| A/B boundary | current RVV shuffled dual-indexed public path vs copy/sort-by-source + same RVV public dual-indexed path。 |
| 当前决策问题 | Phase 046 的 dual-indexed 256K weak-positive 是否稳定到值得进入 production probe 讨论。 |
| diagnostic 是否可外推到 production | 不可。10-run board 显示退化频率过高。 |
| comparison-boundary / baseline mismatch 风险 | 有；candidate 计时包含 copy + sort，且只覆盖一个 synthetic shuffle shape。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许；本阶段已命中 negative / unstable。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 是；本阶段 detail A/B 复核结果不支持进入 PI1-PI5。 |

## Optimization matrix 更新

- `sorted-copy-dual-indexed-256k-stability`：回填为 `rejected_or_unstable_with_evidence`。10-run board median 只有 `1.010x`，5/10 run 低于 1，Doctor `Errors=1`、`Warnings=1`、`Suggestions=1`。
- Phase 046 的 dual-indexed weak-positive residual next action 已关闭；不再建议基于 source-sorted copy 开 dual-indexed production probe。
- Phase 047 correspondence sorted-copy adopted branch 不受影响；它仍只覆盖 correspondence、size >= 64K、shuffle-like disorder。

## 继续 / 停止决策

`continue_stop_decision`：`phase_complete_no_production`。

`stop_condition_hit`：本阶段按计划完成 QEMU correctness、QEMU smoke、10-run board repeated、Evidence Doctor、registry 和文档回填；证据支持关闭 dual-indexed sorted-copy 残留路线。

`next_phase_default`：

- `Scalar=double` 仍需要用户确认数值预算和误差门槛。
- 更广 custom layout / alignment 采样仍需要用户定义采样空间和 board budget。
- 新 row-source mitigation 必须是新的 candidate family；不能恢复 staged-selected-cloud、target-sorted 或 dual-indexed source-sorted copy 作为默认动作。

## 证据新鲜度

执行：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status
```

结果：`evidence registry check: fresh`。
