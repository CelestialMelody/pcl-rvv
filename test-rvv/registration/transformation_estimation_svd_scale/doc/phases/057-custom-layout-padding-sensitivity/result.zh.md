# Phase 057 结果：custom layout padding sensitivity

## 执行范围

本阶段按 `plan.zh.md` 新增两个测试本地 registered custom xyz AoS layout（已注册 xyz 结构数组布局）组合，并只验证当前 production public row-source overload（生产公开 row-source 重载）在 `Scalar=float`、dense、合法 indices / correspondences 下的表现：

- compact-ish：`LocalSVDScaleCompactXYZSource -> LocalSVDScaleCompactXYZTarget`
- huge-padding：`LocalSVDScaleHugePaddingXYZSource -> LocalSVDScaleHugePaddingXYZTarget`
- row source：source-indexed、dual-indexed、correspondence
- size：64K、256K selected pairs
- order pattern：沿用 stride source / alt target index 形状

本阶段没有修改 production 源码，不扩大 production gate，不覆盖任意自定义点型全集、异常 alignment、未注册字段、非 dense 输入、非法 index / correspondence 或 `Scalar=double`。

## 动作回填

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| A1 新 layout fixture | done | `include/impl/tesvd_scale_support.hpp` | 新增 compact-ish 与 huge-padding 两组测试本地 registered xyz AoS 点型；static assertions 锁定 offset / sizeof 取样。 |
| A2 correctness case | done | `CustomLayoutPaddingSensitivityMatchesReference`；`make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded record_qemu_smoke_evidence_state` | Std/RVV 各 20 tests passed；新增 TEST 覆盖 ordered public path 和三类 row-source reference。 |
| A3 bench case-filter | done | `--case-filter custom-layout-padding-sensitivity` | QEMU / board 输出 12 个 case：2 layout × 3 row source × 2 size。 |
| A4 QEMU / registry target | done | `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_layout_padding_sensitivity_state` | QEMU smoke 12 comparisons；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；registry 已登记。QEMU timing 不作为性能结论。 |
| A5 board repeated target | done | `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_custom_layout_padding_sensitivity_repeated` | Board 5-run repeated 12 comparisons；summary / manifest / Doctor 已生成并登记。 |
| A6 文档同步 | done | 本 result、phase README、optimization matrix、roadmap、topic-local evidence docs | 结论写成 sampled positive with padding sensitivity warnings，不写成全部 custom layout clean positive。 |

## Correctness 与 QEMU

新增 correctness test：

- `CustomLayoutPaddingSensitivityMatchesReference`

该 TEST 对 compact-ish 和 huge-padding 两组 layout 分别执行 ordered public path 与 source-indexed / dual-indexed / correspondence selected-cloud reference 对拍；traits gate 为 `RVVXYZAoSFloatLayout == true`，matrix 最大误差预算为 `5e-4`。

本阶段重新执行 correctness freshness：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded record_qemu_smoke_evidence_state
```

结果：Std 20 tests passed、RVV 20 tests passed。

Phase 057 QEMU smoke：

- 路径：`log/qemu/custom_layout_padding_sensitivity/evidence_doctor.md`
- comparisons：12
- 最大 RVV reference error：`3.039837e-05`
- Doctor：`Errors=0`、`Warnings=0`、`Suggestions=0`

QEMU smoke 只用于 correctness、case label、manifest 和 Doctor 形状；QEMU speedup 不进入性能结论。

## Board repeated 结果

Board summary 路径：

- `log/board/custom_layout_padding_sensitivity_repeated/summary.md`
- `log/board/custom_layout_padding_sensitivity_repeated/evidence_manifest.json`
- `log/board/custom_layout_padding_sensitivity_repeated/evidence_doctor.md`

5-run B/A = Std public path ms / RVV public path ms；大于 1 表示 RVV 更快。

| layout | row source | size | runs B/A | median | min | max | bucket |
| --- | --- | --- | --- | ---: | ---: | ---: | --- |
| compact-ish | source-indexed | 64K | `14.521, 14.718, 14.761, 14.243, 14.408` | 14.521 | 14.243 | 14.761 | `positive` |
| compact-ish | dual-indexed | 64K | `9.597, 9.667, 9.440, 9.653, 9.443` | 9.597 | 9.440 | 9.667 | `positive` |
| compact-ish | correspondence | 64K | `11.813, 12.004, 11.891, 11.893, 11.898` | 11.893 | 11.813 | 12.004 | `positive` |
| compact-ish | source-indexed | 256K | `14.032, 14.310, 14.180, 14.030, 13.963` | 14.032 | 13.963 | 14.310 | `positive` |
| compact-ish | dual-indexed | 256K | `9.321, 9.504, 9.465, 9.524, 9.477` | 9.477 | 9.321 | 9.524 | `positive` |
| compact-ish | correspondence | 256K | `9.204, 9.260, 9.257, 9.234, 9.248` | 9.248 | 9.204 | 9.260 | `positive` |
| huge-padding | source-indexed | 64K | `2.917, 2.898, 2.912, 2.939, 2.924` | 2.917 | 2.898 | 2.939 | `positive` |
| huge-padding | dual-indexed | 64K | `3.548, 3.547, 3.338, 3.250, 3.606` | 3.547 | 3.250 | 3.606 | `positive` |
| huge-padding | correspondence | 64K | `4.002, 2.919, 4.071, 3.970, 4.075` | 4.002 | 2.919 | 4.075 | `positive` |
| huge-padding | source-indexed | 256K | `2.693, 2.814, 2.833, 2.819, 2.823` | 2.819 | 2.693 | 2.833 | `positive` |
| huge-padding | dual-indexed | 256K | `2.754, 2.255, 2.518, 2.706, 1.542` | 2.518 | 1.542 | 2.754 | `positive` |
| huge-padding | correspondence | 256K | `2.889, 2.792, 2.337, 3.975, 3.107` | 2.889 | 2.337 | 3.975 | `positive` |

## Evidence Doctor

QEMU Doctor：

- `Errors=0`
- `Warnings=0`
- `Suggestions=0`

Board Doctor：

- `Errors=0`
- `Warnings=15`
- `Suggestions=0`

Warnings 分两类：

| warning type | cases | 处理方式 |
| --- | --- | --- |
| `long_tail_or_variance` | huge-padding correspondence 64K、huge-padding dual-indexed 256K、huge-padding correspondence 256K | summary 和结论保留 min / median / max；不按单一均值判断，不剔除低值 run。 |
| `group_outlier` | 12 个 case 均相对 row-source 组内 median 偏离 | 这是本阶段想测的 layout / stride 敏感性信号；按 layout × row source × size 分开报告，不能把 compact-ish 收益继承到 huge-padding。 |

Board Doctor 无 Error，因此本阶段 board evidence 可以用于 sampled custom layout padding sensitivity 结论；warning 要求降级外推边界。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_custom_layout_padding_sensitivity`。 |
| A/B boundary | Std build public scalar path vs RVV build public RVV path；不是 RVV-vs-RVV family selection。 |
| 当前决策问题 | 更紧凑和更宽 padding / stride 的 registered xyz AoS layout 是否仍能走当前 adopted production path，并保持正向性能。 |
| diagnostic 是否可外推到 production | 只可外推到本阶段两个新增 layout 组合、三类 row source、两个 size 和当前合法 dense 输入。 |
| comparison-boundary / baseline mismatch 风险 | 有。layout、row source、size 同时变化；Board Doctor 也要求按组合独立报告。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不新增 production probe；弱 / 负结果只用于降级 custom layout evidence。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要，因为本阶段没有新 implementation family；若后续提出 layout-specific mitigation，则需要。 |

## Optimization matrix 更新

- `custom-layout-padding-sensitivity / compact-ish / source-indexed + dual-indexed + correspondence / 64K + 256K`：回填为 `sampled_positive_custom_layout_padding_sensitivity`。6/6 board case 全 positive，median B/A `9.248x` 到 `14.521x`，Doctor 只有 group-outlier warning。
- `custom-layout-padding-sensitivity / huge-padding / source-indexed + dual-indexed + correspondence / 64K + 256K`：回填为 `sampled_positive_with_padding_sensitivity_warnings`。6/6 board case 全 positive，median B/A `2.518x` 到 `4.002x`，但 3 个 long-tail / variance warning 和 6 个 group-outlier warning 说明 padding / stride 对收益影响显著。
- `custom-xyz-aos-layout-sampling` 和 `custom-row-source-large-variance-profile` 的历史结论不被覆盖：Phase 057 只新增两个 layout 样本，不删除 Phase 055 mixed 事实，也不把 Phase 056 的受控 order-profile 外推到全集。

## 继续 / 停止决策

本阶段完成：case-filter、correctness、QEMU smoke、board repeated、Doctor、registry 和文档回填均已闭合；`evidence_status` 为 fresh。

`continue_stop_decision`：`phase_complete_continue_possible`。

`stop_condition_hit`：当前 Phase 057 的 12 个 case 已完成并回填；继续到下一优化方向需要从 roadmap 选择新的候选，不属于本阶段未完成项。

`next_phase_default`：

- 若继续 custom layout 方向，需要用户定义更广 custom layout / padding 取样空间和 board budget；当前两个新增样本仍不能代表任意自定义点型全集。
- 若继续 row-source mitigation 方向，需要新 candidate family，并先做 RVV-vs-RVV detail A/B。
- `Scalar=double` 仍需要用户确认数值预算和误差门槛。

## 证据新鲜度

执行：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status
```

结果：`evidence registry check: fresh`。
