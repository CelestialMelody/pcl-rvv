# Phase 071 Result: more custom layout `Scalar=double` sampling

## 结论

EvidenceDecision（证据决策）：`positive_pending_user_confirmation`。

本阶段没有采纳 Phase 069 或 Phase 070，也没有写 adoption closeout（采纳收尾）。它只在 Phase 070 已经 positive 但仍等待用户确认的前提下，继续扩展 custom layout（自定义布局）`Scalar=double` 证据：用 compact、huge-padding 和 aligned 三组测试本地 registered xyz AoS layout（已注册 xyz 结构数组布局）覆盖 ordered、source-indexed、dual-indexed 和 correspondence public overload。

结果为 positive sampling reinforcement（正向取样增强）：Std/RVV correctness（正确性）37/37 通过，QEMU smoke（QEMU 小型路径验证）12 comparisons 且 Doctor `0/0/0`，board repeated（板卡重复性能测试）12/12 positive，Board Doctor `0/8/0`。这些数据增强 Phase 070 的 custom layout double 候选可信度，但仍不能把 Phase 069、Phase 070 或 Phase 071 写成 `adopted-by-user`。

## 执行范围

本阶段覆盖：

- ordered-cloud-pair、source-indexed、dual-indexed 和 correspondence public overload。
- `LocalSVDScaleCompactXYZSource -> LocalSVDScaleCompactXYZTarget`。
- `LocalSVDScaleHugePaddingXYZSource -> LocalSVDScaleHugePaddingXYZTarget`。
- `LocalSVDScaleAligned64XYZSource -> LocalSVDScaleAligned32XYZTarget`。
- `Scalar=double`、dense、64K board case、合法 index / correspondence。

本阶段不覆盖：

- Phase 069、Phase 070 或 Phase 071 的采纳确认。
- 全部 custom layout double、packed unaligned float、异常 alignment 全集或任意自定义点型全集。
- sorted-copy double、非法 index / correspondence、非 dense、小规模或更广规模矩阵。
- 新 RVV family selection（RVV 实现族选择）；本阶段比较的是 Std public fallback 和当前 RVV public candidate。

## TDD / Target 入口记录

| 步骤 | 命令 / 现象 | 结论 |
| --- | --- | --- |
| red | 在新增 target 前运行 Phase 071 证据入口，缺少 `more-custom-layout-scalar-double-sampling` target，按预期失败。 | 证明本阶段证据入口确实需要新增。 |
| green | 新增 gtest、bench case-filter、Makefile QEMU / board / registry target 后，`run_test_compare`、QEMU record 和 board record 均闭合。 | Phase 071 证据链可复现。 |

新增 gtest：

- `MoreCustomLayoutScalarDoubleSamplingMatchesReference`
- `MoreCustomLayoutScalarDoubleSamplingFallbackBoundaries`

新增 case-filter / target：

- `--case-filter more-custom-layout-scalar-double-sampling`
- `record_qemu_more_custom_layout_scalar_double_sampling_state`
- `record_board_more_custom_layout_scalar_double_sampling_state`
- `run_board_bench_more_custom_layout_scalar_double_sampling_repeated`

## Correctness

`make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare`：

- Std：37/37 passed。
- RVV：37/37 passed。

新增 correctness 覆盖：

- 三组 custom layout 的 ordered public double path 与 reference 对齐。
- 三组 custom layout 的 source-indexed、dual-indexed 和 correspondence row-source public double path 与 selected-cloud double reference 对齐。
- small、non-dense 和 unsupported fallback boundaries 保持父类 double fallback 语义。

## QEMU Smoke

Target：`record_qemu_more_custom_layout_scalar_double_sampling_state`。

- case-filter：`more-custom-layout-scalar-double-sampling`
- comparisons：12
- QEMU Evidence Doctor：`Errors=0`、`Warnings=0`、`Suggestions=0`
- 证据路径：
  - `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/more_custom_layout_scalar_double_sampling/analyze_bench_compare.log`
  - `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/more_custom_layout_scalar_double_sampling/evidence_manifest.json`
  - `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/more_custom_layout_scalar_double_sampling/evidence_doctor.md`

QEMU timing 不作为性能结论。

## Board Repeated

Target：`record_board_more_custom_layout_scalar_double_sampling_state`；完整 board 采集入口为 `run_board_bench_more_custom_layout_scalar_double_sampling_repeated`。

Board repeated 覆盖 12 个 64K case，全部 `positive`，checksum match，最大 reference error 约 `1.346e-13`。

| row source | point type pair | median B/A | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| ordered-cloud-pair | `LocalCompactXYZSource->LocalCompactXYZTarget` | `29.087x` | `28.677x` | `29.212x` | positive |
| ordered-cloud-pair | `LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget` | `6.253x` | `5.989x` | `6.393x` | positive |
| ordered-cloud-pair | `LocalAligned64XYZSource->LocalAligned32XYZTarget` | `4.717x` | `4.422x` | `4.814x` | positive |
| source-indexed | `LocalCompactXYZSource->LocalCompactXYZTarget` | `14.777x` | `14.309x` | `14.850x` | positive |
| source-indexed | `LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget` | `7.710x` | `7.220x` | `7.762x` | positive |
| source-indexed | `LocalAligned64XYZSource->LocalAligned32XYZTarget` | `7.808x` | `7.054x` | `7.842x` | positive |
| dual-indexed | `LocalCompactXYZSource->LocalCompactXYZTarget` | `12.730x` | `11.199x` | `12.874x` | positive |
| dual-indexed | `LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget` | `2.766x` | `2.716x` | `3.121x` | positive |
| dual-indexed | `LocalAligned64XYZSource->LocalAligned32XYZTarget` | `4.363x` | `4.124x` | `5.744x` | positive |
| correspondence | `LocalCompactXYZSource->LocalCompactXYZTarget` | `10.253x` | `9.939x` | `10.447x` | positive |
| correspondence | `LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget` | `3.727x` | `3.570x` | `3.788x` | positive |
| correspondence | `LocalAligned64XYZSource->LocalAligned32XYZTarget` | `4.334x` | `3.523x` | `5.894x` | positive |

Board Evidence Doctor：`Errors=0`、`Warnings=8`、`Suggestions=0`。

Warnings 分两类：

- 2 个 `long_tail_or_variance`：aligned correspondence 和 aligned dual-indexed 的 min / median / max 差异较大。处理方式是保留 min / median / max，不剔除异常。
- 6 个 `group_outlier`：compact、huge-padding 和 aligned 在同一 row-source 内收益差异明显。处理方式是按 layout / row source 分开解释，不把某个 layout 的收益直接外推给其它 layout。

证据路径：

- `test-rvv/registration/transformation_estimation_svd_scale/log/board/more_custom_layout_scalar_double_sampling_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_svd_scale/log/board/more_custom_layout_scalar_double_sampling_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_svd_scale/log/board/more_custom_layout_scalar_double_sampling_repeated/evidence_doctor.md`

## Evidence Registry

`log/evidence_registry.json` 已登记本阶段 QEMU 和 board evidence。登记项的 run label 为：

- `qemu-tesvd-scale-more-custom-layout-scalar-double-sampling-smoke-phase071`
- `board-tesvd-scale-more-custom-layout-scalar-double-sampling-repeated-phase071`

Makefile doc ref 指向：

- `doc/phases/071-more-custom-layout-scalar-double-sampling/result.zh.md`

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_more_custom_layout_scalar_double_sampling`，真实公开入口 custom layout double 取样增强证据。 |
| A/B boundary | public overload；Std 为 public double scalar fallback，RVV 为当前 production patch 下 layout-gated f64 widened RVV path。 |
| 当前决策问题 | RVV-vs-scalar：更多 custom layout sample 的 public double RVV path 是否快于 public double scalar fallback。 |
| 是否可外推到 production | 只能外推到本阶段列出的三组 layout sample；不能外推到全部自定义 layout、packed unaligned float 或异常 alignment 全集。 |
| comparison-boundary / baseline mismatch 风险 | 低；board summary 使用同一 public case-filter。但该证据不是 RVV-family-selection。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段本身是 bounded production-public sampling；若后续 layout weak / negative / unstable，只能按 layout / row source 降级。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段没有选择新 RVV family；若要采纳更广 custom layout double gate，仍需要用户明确确认，并写清代表性证据边界。 |

## 继续 / 停止判断

本阶段计划内动作已闭合，但不能自动采纳。当前停止在 `positive_pending_user_confirmation`：

- Phase 069 仍是 `positive_pending_user_confirmation`，未进入 adopted。
- Phase 070 仍是 `positive_pending_user_confirmation`，未进入 adopted。
- Phase 071 只是 Phase 070 custom layout double candidate 的更多布局取样增强，也停在 `positive_pending_user_confirmation`。
- 若用户确认采纳 Phase 069、Phase 070 或 Phase 071 的某个边界，应另开 adoption closeout phase，刷新长期 `doc-rvv`、matrix、roadmap 和 topic-local docs。
- 若用户暂不采纳，保留 pending candidate，下一条可评估路线仍是 sorted-copy double、更多 custom layout double 取样、更多 point-type expansion 或其它 roadmap 中未关闭方向。

## 文档同步状态

已同步：

- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/test-support-code-map.zh.md`
- `doc/transformation_estimation_svd_scale-evaluation.zh.md`
- `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`
- `tmp/rvv-work-logs/registration/transformation_estimation_svd_scale/current-handoff/`

同步边界：这些文档只把 Phase 071 写成 pending candidate sampling reinforcement，不写成 adopted production behavior。
