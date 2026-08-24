# Phase 056 结果：custom row-source large variance profile

## 执行范围

本阶段按 `plan.zh.md` 只复核两个测试本地 registered custom xyz AoS layout（已注册 xyz 结构数组布局）样本在 256K large row-source（大规模行来源）下的顺序 / 局部性差异：

- source：`LocalSVDScalePaddedXYZSource`，`x/y/z` offset = `4/12/24`，`sizeof=32`
- target：`LocalSVDScaleWideXYZTarget`，`x/y/z` offset = `8/20/28`，`sizeof=40`
- row source：dual-indexed、correspondence
- order pattern：contiguous、stride、reverse、shuffle
- size：256K selected pairs
- `Scalar=float`、dense、当前 production public overload

本阶段没有修改 production 源码，不扩大 production gate，不覆盖任意自定义点型全集、异常 alignment、double xyz 字段、非 dense 输入、非法 index / correspondence 或 `Scalar=double`。

## 动作回填

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| A1 bench case-filter | done | `run_bench_rvv USE_PCL_RVV10=1 BENCH_ARGS='--case-filter custom-row-source-large-variance-profile --iterations 1 --warmup-iterations 0'` | 新增 8 个 case：2 row source x 4 order pattern x 256K；RVV 最大 reference error `5.722046e-05`。 |
| A2 QEMU / registry target | done | `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_row_source_large_variance_profile_state` | QEMU smoke 8 comparisons；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；registry 已登记。QEMU timing 不作为性能结论。 |
| A3 board repeated target | done | `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_custom_row_source_large_variance_profile_repeated` | Board 5-run repeated 8 comparisons；summary / manifest / Doctor 已生成并登记。 |
| A4 script classification | done | `generate_tesvd_scale_qemu_evidence_manifest.py`、`generate_tesvd_scale_board_repeated_summary.py` | manifest 使用 `production_public_custom_row_source_profile`，summary 能按 dual-indexed / correspondence 拆组。 |
| A5 文档同步 | done | 本 result、phase README、optimization matrix、roadmap 和 topic-local evidence docs | 结论写成 controlled profile positive with variance warnings，不写成 custom layout 全量 clean positive。 |

## Correctness 与 QEMU

窄 RVV bench 的 8 个 case 都满足 `max_reference_error <= 2e-3`，最大值为 `5.722046e-05`。

本轮还重新执行并登记了 QEMU correctness freshness：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded record_qemu_smoke_evidence_state
```

结果为 Std 19 tests passed、RVV 19 tests passed；随后 `evidence_status` 为 fresh。

QEMU custom row-source large variance smoke 只用于 correctness、case label、manifest 和 Doctor 形状。它输出 8 comparisons，Doctor 为 `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU speedup 不进入性能结论。

## Board repeated 结果

Board summary 路径：

- `log/board/custom_row_source_large_variance_profile_repeated/summary.md`
- `log/board/custom_row_source_large_variance_profile_repeated/evidence_manifest.json`
- `log/board/custom_row_source_large_variance_profile_repeated/evidence_doctor.md`

5-run B/A = Std public path ms / RVV public path ms；大于 1 表示 RVV 更快。

| row source | order pattern | runs B/A | median | min | max | bucket |
| --- | --- | --- | ---: | ---: | ---: | --- |
| dual-indexed | contiguous | `9.382, 8.487, 9.109, 8.976, 9.254` | 9.109 | 8.487 | 9.382 | `positive` |
| correspondence | contiguous | `7.021, 7.076, 7.180, 7.315, 7.233` | 7.180 | 7.021 | 7.315 | `positive` |
| dual-indexed | stride | `2.581, 2.296, 1.267, 1.249, 2.473` | 2.296 | 1.249 | 2.581 | `positive_with_long_tail` |
| correspondence | stride | `1.366, 2.129, 2.300, 2.161, 1.378` | 2.129 | 1.366 | 2.300 | `positive_with_long_tail` |
| dual-indexed | reverse | `3.577, 3.542, 2.432, 3.574, 3.543` | 3.543 | 2.432 | 3.577 | `positive_with_variance_warning` |
| correspondence | reverse | `3.629, 3.615, 3.638, 3.628, 3.649` | 3.629 | 3.615 | 3.649 | `positive` |
| dual-indexed | shuffle | `2.442, 2.417, 2.428, 2.440, 2.432` | 2.432 | 2.417 | 2.442 | `positive` |
| correspondence | shuffle | `4.893, 4.682, 5.102, 4.633, 4.331` | 4.682 | 4.331 | 5.102 | `positive_with_variance_warning` |

本阶段没有复现 Phase 055 里 256K dual-indexed / correspondence 出现 `<1x` 的退化频率。受控 order pattern 下 8/8 case 均为 positive，但 stride 和部分组内离群说明该方向仍有方差敏感性，不能升级成“任意 custom layout large row-source clean positive”。

## Evidence Doctor

QEMU Doctor：

- `Errors=0`
- `Warnings=0`
- `Suggestions=0`

Board Doctor：

- `Errors=0`
- `Warnings=8`
- `Suggestions=0`

Warnings 分两类：

| warning type | cases | 处理方式 |
| --- | --- | --- |
| `long_tail_or_variance` | dual-indexed stride、correspondence stride、dual-indexed reverse、correspondence shuffle | summary 和结论保留 min / median / max；不按单一均值判断，不先验剔除低值 run。 |
| `group_outlier` | dual-indexed contiguous、dual-indexed stride、correspondence contiguous、correspondence stride | 按 row source / order pattern 分开报告；不能把同组 median 互相外推。 |

Board Doctor 无 Error，因此本阶段 board evidence 可以用于受控 profile 结论；warning 要求降级外推边界。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_custom_row_source_profile`。 |
| A/B boundary | Std build public scalar path vs RVV build public RVV path；不是 RVV-vs-RVV family selection。 |
| 当前决策问题 | Phase 055 的 256K custom layout dual-indexed / correspondence mixed / negative slice 是否是 generic custom layout failure，还是 order/locality 和测量方差敏感。 |
| diagnostic 是否可外推到 production | 只可外推到本阶段两个测试本地 custom layout 样本、两个 row source、四种 order pattern 和 256K size；不能外推到任意自定义点型全集。 |
| comparison-boundary / baseline mismatch 风险 | 有。不同 order pattern 输入分布不同；Board Doctor 也要求按 row source / order pattern 独立解释。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不新增 production probe。若后续要提出 mitigation，需要另开 RVV-vs-RVV detail A/B。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要，因为本阶段没有新 implementation family；若后续提出 custom shuffle mitigation，则需要。 |

## Optimization matrix 更新

- `custom-row-source-large-variance-profile / dual-indexed / LocalPaddedXYZSource->LocalWideXYZTarget / 256K`：从 `phase_deferred + unblocked` 回填为 `profile_positive_with_variance_warnings`。contiguous、reverse、shuffle positive；stride positive 但 min `1.249x`、max/min `2.07`，不能写成 clean stable。
- `custom-row-source-large-variance-profile / correspondence / LocalPaddedXYZSource->LocalWideXYZTarget / 256K`：回填为 `profile_positive_with_variance_warnings`。contiguous、reverse、shuffle positive；stride positive 但 long-tail；correspondence shuffle median `4.682x`，仍有 variance warning。
- Phase 055 的 256K dual-indexed / correspondence negative / mixed slice 降级为 historical mixed input slice；Phase 056 不删除该历史事实，但说明受控 order-pattern profile 未复现 `<1x`，因此不能把 Phase 055 写成 generic custom layout failure。

## 继续 / 停止决策

本阶段完成：case-filter、QEMU smoke、board repeated、Doctor、registry 和文档回填均已闭合；`evidence_status` 为 fresh。

`continue_stop_decision`：`phase_complete_continue_possible`。

`stop_condition_hit`：当前 Phase 056 的 8 个 case 已完成并回填；继续到下一优化方向需要从 roadmap 选择新的候选，不属于本阶段未完成项。

`next_phase_default`：

- 若继续 custom layout 方向，可以开 `more-custom-xyz-aos-board` 或 `custom-layout-padding-sensitivity`，但必须先定义新的 layout 取样空间和 board budget。
- 若继续数值类型方向，`Scalar=double` 仍需要用户确认数值预算和误差门槛。
- 若继续 row-source mitigation 方向，需要新 candidate family，并先做 RVV-vs-RVV detail A/B；Phase 056 本身没有生成可直接接 production 的新 family。

## 证据新鲜度

执行：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status
```

结果：`evidence registry check: fresh`。
