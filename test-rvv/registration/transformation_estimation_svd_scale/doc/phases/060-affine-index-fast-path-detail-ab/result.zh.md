# Phase 060 结果：affine index fast-path detail A/B

## 执行摘要

本阶段新增 `affine-index-fast-path-detail-ab` 诊断候选：当 row-source 输入是 step=1 的 contiguous offset slice（连续偏移片段）时，用 test-only contiguous offset RVV accumulation（连续偏移 RVV 累加）替代当前 public row-source gather accumulation（索引离散加载累加），并与 current public RVV path 做同一 RVV binary 内的 detail A/B。

本阶段没有修改 production 源码，也没有扩大 production gate。它只回答一个生产探针前置问题：contiguous source-indexed / dual-indexed / correspondence 输入是否值得进入 bounded production probe。

`EvidenceDecision`：`positive_production_probe_candidate / affine-index-fast-path-detail-ab`。

## 动作回填

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| A1 诊断 candidate | done | `estimateScaleContiguousOffsetCandidate` / `accumulateScaleRVVContiguousOffsets` | RVV 构建下提供 test-only contiguous offset accumulation；Std 构建用同构 scalar fallback。 |
| A2 correctness guard | done | `AffineIndexFastPathCandidateMatchesReference` | source-indexed、dual-indexed、correspondence 的 contiguous offset 输入均对齐 selected-cloud reference。 |
| A3 bench case-filter | done | `row-source-affine-index-fast-path-detail-ab` | 输出 current / affine-fast-path 成对 label，覆盖 64K / 256K × 三类 row source。 |
| A4 Make / registry target | done | `record_qemu_affine_index_fast_path_detail_ab_state`、`run_board_bench_affine_index_fast_path_detail_ab_repeated` | QEMU / board 证据写入独立 Phase 060 目录。 |
| A5 文档回填 | done | 本 result、matrix、roadmap、README、benchmark/evidence、optimization evidence、evaluation | 结论同步为 positive production probe candidate，但不写成 adopted production behavior。 |

## Correctness 与 QEMU

Correctness freshness：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded
```

结果：Std 22 tests passed、RVV 22 tests passed。

QEMU detail smoke：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_affine_index_fast_path_detail_ab_state
```

- 路径：`log/qemu/row_source_affine_index_fast_path_detail_ab/`
- comparisons：6
- QEMU Doctor：`Errors=0`、`Warnings=0`、`Suggestions=0`
- checksum：6/6 paired labels match
- max reference error：最大 `6.974e-05`

QEMU timing 只作为日志形状和路径检查，不作为性能结论。

## Board repeated 结果

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_affine_index_fast_path_detail_ab_repeated
```

Board summary 路径：

- `log/board/row_source_affine_index_fast_path_detail_ab_repeated/summary.md`
- `log/board/row_source_affine_index_fast_path_detail_ab_repeated/evidence_manifest.json`
- `log/board/row_source_affine_index_fast_path_detail_ab_repeated/evidence_doctor.md`

B/A = current public gather RVV ms / affine fast-path RVV ms；大于 1 表示 fast path 更快。

| case | median | min | max | bucket |
| --- | ---: | ---: | ---: | --- |
| source-indexed 64K | 1.676 | 1.603 | 1.684 | `positive` |
| dual-indexed 64K | 2.282 | 2.260 | 2.324 | `positive` |
| correspondence 64K | 2.686 | 2.589 | 2.747 | `positive` |
| source-indexed 256K | 1.695 | 1.686 | 1.707 | `positive` |
| dual-indexed 256K | 2.278 | 2.273 | 2.327 | `positive` |
| correspondence 256K | 2.750 | 2.748 | 2.764 | `positive` |

## Evidence Doctor

Board Doctor：

- `Errors=0`
- `Warnings=3`
- `Suggestions=0`

| signal | 观察 | 处理方式 |
| --- | --- | --- |
| `group_outlier` / correspondence 256K | median `2.750x`，高于组内 median `2.28x`。 | 不把 correspondence 256K 的收益外推到其它 row source；按 row source / size 分开报告。 |
| `group_outlier` / source-indexed 256K | median `1.695x`，低于组内 median `2.28x`。 | source-indexed 仍 positive，但 production probe 应单独保留 row-source 维度。 |
| `group_outlier` / source-indexed 64K | median `1.676x`，低于组内 median `2.28x`。 | 同上；不使用单一 overall median 作为全部 row-source 收益。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_detail_affine_index_fast_path_diagnostic`。 |
| A/B boundary | 同一 RVV bench binary 内 current public gather path vs test-only contiguous offset fast-path candidate。 |
| 当前决策问题 | contiguous row-source 输入是否值得进入 bounded production probe。 |
| diagnostic 是否可外推到 production | 不能直接外推。当前 candidate 固定使用已知 contiguous offset 输入，未计入通用 index pattern 检测成本，也没有接入 production dispatch。 |
| comparison-boundary / baseline mismatch 风险 | 有。current path 是 public row-source overload；candidate 是 test-only helper。结果只能支持 production probe candidate，不能 clean adopt。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段 6/6 board case positive，因此允许进入有界 production probe；probe 必须计入 contiguous 检测和 fallback。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。本阶段已提供 detail A/B；后续若接 production，还要补 public probe correctness / QEMU / board / Doctor。 |

## Optimization matrix 更新

- 新增 `affine-index-fast-path-detail-ab`：source-indexed、dual-indexed、correspondence × 64K/256K 全部 positive。
- 该结论是 `positive_production_probe_candidate`，不等于 production adopted。
- 下一阶段默认入口应是 affine contiguous production probe plan：补 production detection、fallback、public correctness、QEMU smoke、board repeated 和 Doctor。

## 继续 / 停止决策

`continue_stop_decision`：`phase_complete_positive_probe_candidate`。

`next_phase_default`：

- 优先进入 `061-affine-index-fast-path-production-probe`，只覆盖 step=1 contiguous offset 的 source-indexed、dual-indexed 和 correspondence。
- production probe 必须计入 index pattern 检测成本，且不能影响 stride / reverse / shuffle 当前路径。
- 若用户不希望继续接 production，则本候选保留为 positive diagnostic，不修改 production。
