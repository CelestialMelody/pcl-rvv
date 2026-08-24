# Phase 059 结果：custom layout alignment sensitivity

## 执行摘要

本阶段补充一个测试本地 registered xyz AoS layout（已注册 xyz 结构数组布局）采样：`LocalAligned64XYZSource -> LocalAligned32XYZTarget`。该组合显式提高 `alignof(PointT)`，并让 source / target 的 `x/y/z` offset 与 stride 不同于常见 PCL 点型。

本阶段没有修改 production 源码，也没有扩大 production gate。它只验证当前已采纳的 traits-gated xyz AoS / `Scalar=float` / dense row-source public path 在这个 alignment-sensitive（对齐敏感）采样组合上的正确性和板卡表现。

`EvidenceDecision`：`sampled_positive_with_alignment_sensitivity_warnings / custom-layout-alignment-sensitivity`。

## 动作回填

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| A1 新增点型与 correctness | done | `LocalAligned64XYZSource`、`LocalAligned32XYZTarget`；`CustomLayoutAlignmentSensitivityMatchesReference` | static_assert 覆盖 offset、sizeof、alignof；ordered 和三类 row-source reference 均通过。 |
| A2 新增 bench case-filter | done | `custom-layout-alignment-sensitivity` | 输出 6 个 case：source-indexed / dual-indexed / correspondence × 64K / 256K。 |
| A3 新增 Make / registry target | done | `record_qemu_custom_layout_alignment_sensitivity_state`、`run_board_bench_custom_layout_alignment_sensitivity_repeated` | QEMU 与 board 证据写入独立 alignment 目录，不覆盖 Phase 057 padding 证据。 |
| A4 脚本识别 | done | QEMU manifest / board summary wrapper | evidence role 正确写成 `production_public_custom_layout_alignment_sensitivity`。 |
| A5 文档回填 | done | 本 result、matrix、roadmap、README、benchmark/evidence、optimization evidence、evaluation | 当前结论同步为 sampled positive with warnings。 |

## Correctness 与 QEMU

Correctness freshness：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded
```

结果：Std 21 tests passed、RVV 21 tests passed。

QEMU smoke：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_layout_alignment_sensitivity_state
```

- 路径：`log/qemu/custom_layout_alignment_sensitivity/`
- comparisons：6
- max reference error：`3.039837e-05`
- Doctor：`Errors=0`、`Warnings=0`、`Suggestions=0`

QEMU timing 只作为日志形状和路径检查，不作为性能结论。

## Board repeated 结果

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_custom_layout_alignment_sensitivity_repeated
```

Board summary 路径：

- `log/board/custom_layout_alignment_sensitivity_repeated/summary.md`
- `log/board/custom_layout_alignment_sensitivity_repeated/evidence_manifest.json`
- `log/board/custom_layout_alignment_sensitivity_repeated/evidence_doctor.md`

B/A = Std custom layout alignment public scale ms / RVV custom layout alignment public scale ms；大于 1 表示 RVV public path 更快。

| case | median | min | max | bucket |
| --- | ---: | ---: | ---: | --- |
| source-indexed 64K | 2.181 | 2.167 | 2.642 | `positive` |
| dual-indexed 64K | 2.308 | 2.182 | 2.338 | `positive` |
| correspondence 64K | 2.258 | 2.116 | 2.283 | `positive` |
| source-indexed 256K | 1.636 | 1.623 | 2.056 | `positive` |
| dual-indexed 256K | 2.309 | 2.113 | 2.478 | `positive` |
| correspondence 256K | 2.138 | 2.081 | 2.250 | `positive` |

## Evidence Doctor

Board Doctor：

- `Errors=0`
- `Warnings=3`
- `Suggestions=0`

| signal | 观察 | 处理方式 |
| --- | --- | --- |
| `long_tail_or_variance` / source-indexed 64K | min `2.17x`、median `2.18x`、max `2.64x`。 | 保留 min / median / max；不把单个高值作为收益结论。 |
| `long_tail_or_variance` / source-indexed 256K | min `1.62x`、median `1.64x`、max `2.06x`。 | 结论保持 positive，但按 row source / size 分开报告。 |
| `long_tail_or_variance` / dual-indexed 256K | min `2.11x`、median `2.31x`、max `2.48x`。 | 作为 variance warning 记录；不需要追加复跑。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_custom_layout_alignment_sensitivity`。 |
| A/B boundary | Std public row-source path vs RVV public row-source path，同一测试本地 alignas registered xyz AoS 点型组合。 |
| 当前决策问题 | 当前 adopted traits-gated row-source public path 是否能覆盖一个 alignment-sensitive custom layout 采样。 |
| diagnostic 是否可外推到 production | 只能外推到“该采样组合命中当前 production public path 时表现 positive”；不能外推到全部 custom layout、packed unaligned float 或异常 alignment 全集。 |
| comparison-boundary / baseline mismatch 风险 | 无 strict A/B 错配；checksum mismatch 由 RVV reduction tree 解释，correctness 以 gtest 和 max reference error 为准。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段全部 positive，无需 production probe；若未来新增 layout 出现 negative，应只降级对应 slice。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段不是新 RVV family selection，只是已采纳 public path 的证据扩展。 |

## Optimization matrix 更新

- 新增 `custom-layout-alignment-sensitivity`：source-indexed、dual-indexed、correspondence × 64K/256K 全部 positive。
- 该结论是 sampled positive with alignment sensitivity warnings；不扩大 production gate，不替代更广 layout / alignment 取样空间。
- `Scalar=double` 仍为 not_applicable，需要用户定义数值预算。

## 继续 / 停止决策

`continue_stop_decision`：`phase_complete_sampled_positive_with_warnings`。

`next_phase_default`：

- 若继续 custom layout 方向，需要用户定义更广 layout / alignment 取样空间和 board budget。
- 若继续数值类型方向，`Scalar=double` 需要先定义 reduction order 误差预算、fallback 边界和 f64 性能计划。
- 若继续 row-source mitigation，必须提出新的 candidate family；不能恢复 staged-selected-cloud、target-sorted 或 dual-indexed source-sorted-copy 旧路线。
