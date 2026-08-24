# Phase 055 结果：custom xyz AoS layout sampling

## 决策

`mixed_custom_layout_sampling / production_public_custom_xyz_aos_layout / correctness-qemu-board`

本阶段没有修改 production 源码。它新增两个测试本地注册点型，验证当前 `RVVXYZAoSFloatLayout` traits gate（字段布局门控）和 `TransformationEstimationSVDScale` public overload（公开入口）能处理非内建 PCL 点型、非 0/4/8 offset、不同 stride 的 xyz AoS（结构数组）样本。

已验证范围：

- source：`LocalSVDScalePaddedXYZSource`，`x/y/z` offset = `4/12/24`，`sizeof=32`
- target：`LocalSVDScaleWideXYZTarget`，`x/y/z` offset = `8/20/28`，`sizeof=40`
- row source：ordered-cloud-pair、source-indexed、dual-indexed、correspondence
- `Scalar=float`、dense、合法 deterministic indices / correspondences
- correctness 使用 4K selected pairs；QEMU / board 使用 4K、64K、256K

不覆盖范围保持独立：任意自定义点型全集、非 standard-layout POD、double xyz 字段、未注册字段、异常对齐、非法 index / correspondence、非 dense 输入和 `Scalar=double`。

## 实现回填

| action | status | 事实 |
| --- | --- | --- |
| A1 RED correctness | done_with_existing_behavior | 新增 `CustomXYZAoSLayoutSamplingMatchesReference` 后，Std/RVV correctness 直接通过；这说明当前 production traits gate 已支持该样本，不需要生产修复。 |
| A2 shared custom point types | done | 在 `include/impl/tesvd_scale_support.hpp` 中新增两个本地注册点型，供 test / bench 共用。 |
| A3 bench label | done | 新增 `custom-xyz-aos-layout-sampling` case-filter，输出 ordered 3 个 size + row-source 9 个 size，共 12 个 label。 |
| A4 QEMU / registry | done | QEMU smoke 12 comparisons，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；registry fresh。 |
| A5 board repeated | done | 5-run board repeated 12 comparisons；Doctor `Errors=1`、`Warnings=15`、`Suggestions=0`。 |
| A6 文档同步 | done | 本 result、optimization matrix、roadmap 和相关 topic-local 文档记录 mixed evidence 边界。 |

## Correctness / QEMU smoke

| command | result |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | Std/RVV 各 19 tests passed。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_xyz_aos_layout_state` | QEMU smoke manifest / Doctor 生成并登记。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correctness_state evidence_status` | registry fresh。 |

QEMU smoke 只用于 build、label、manifest shape、asm 边界和 `max_reference_error` 检查，不作为真实性能证据。12 个 RVV label 的最大 `max_reference_error` 为 `3.039837e-05`，低于 `2e-3` 预算。QEMU Evidence Doctor：

- path：`log/qemu/custom_xyz_aos_layout_sampling/evidence_doctor.md`
- result：`Errors=0`、`Warnings=0`、`Suggestions=0`
- comparisons：12
- run label：`qemu-tesvd-scale-custom-xyz-aos-layout-sampling-smoke-phase055`

## Board repeated evidence

Board summary / doctor：

- summary：`log/board/custom_xyz_aos_layout_sampling_repeated/summary.md`
- doctor：`log/board/custom_xyz_aos_layout_sampling_repeated/evidence_doctor.md`
- manifest：`log/board/custom_xyz_aos_layout_sampling_repeated/evidence_manifest.json`
- evidence role：`production_public_custom_xyz_aos_layout`
- comparisons：12
- Doctor：`Errors=1`、`Warnings=15`、`Suggestions=0`

聚合结果：

| slice | cases | median B/A range | min B/A range | bucket |
| --- | ---: | ---: | ---: | --- |
| ordered public | 3 | `14.259x` 到 `15.855x` | `13.573x` 到 `14.591x` | 3/3 positive |
| source-indexed | 3 | `3.427x` 到 `8.843x` | `1.589x` 到 `8.150x` | 3/3 positive |
| dual-indexed | 3 | `1.514x` 到 `4.652x` | `0.854x` 到 `4.550x` | 2 positive；256K negative / unstable |
| correspondence | 3 | `2.109x` 到 `4.425x` | `0.890x` 到 `4.278x` | 2 positive；256K negative / unstable |

Doctor 的 1 个 Error 是 dual-indexed 256K 的 `ba_degradation_frequency`：5 次 B/A 为 `1.514x, 0.854x, 2.260x, 2.409x, 0.911x`，其中 2/5 低于 1。它不能被 median 正向掩盖，因此本阶段不能把 custom layout 的 256K dual-indexed 写成 clean positive。

15 个 Warning 主要来自：

- 256K source-indexed、dual-indexed、correspondence 的 long-tail / variance；
- correspondence 256K 的 1/5 退化；
- ordered 与 row-source 之间收益差距较大，触发 group-outlier。处理方式是按 ordered / source-indexed / dual-indexed / correspondence、size 和点型 layout 分开报告，不把 ordered 强收益外推到 row-source 大规模。

## 证据登记

`make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` 当前为 fresh。Phase 055 新增并登记：

- QEMU smoke：`log/qemu/custom_xyz_aos_layout_sampling/*`
- board repeated：`log/board/custom_xyz_aos_layout_sampling_repeated/summary.md`
- board doctor：`log/board/custom_xyz_aos_layout_sampling_repeated/evidence_doctor.md`
- board manifest：`log/board/custom_xyz_aos_layout_sampling_repeated/evidence_manifest.json`
- correctness logs：`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_custom_xyz_aos_layout`，测试本地注册点型通过真实 production public overload。 |
| A/B boundary | Std 构建 public scalar path vs RVV 构建 public RVV path。 |
| 当前决策问题 | 当前 traits-gated production path 是否能覆盖非内建、不同 offset / stride 的 registered xyz AoS 样本。 |
| diagnostic 是否可外推到 production | 可外推到本阶段两个注册样本代表的 layout 形态；不可外推到任意自定义点型全集。 |
| comparison-boundary / baseline mismatch 风险 | 有。board Doctor 显示 row source、size 和 stride / layout 组合会影响收益稳定性。 |
| weak / negative / unstable 时是否允许 bounded production probe | 本阶段已经是真实 public overload；negative / unstable 只降级 custom layout 的对应 row-source / size slice，不回推否定 Phase 054 常见 PCL 点型。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段没有新 RVV family，只验证现有 adopted family 的 layout 证据边界。 |

## 后续队列

- Phase 055 关闭“自定义 registered xyz AoS layout 能否通过当前 production public path”的 correctness / QEMU smoke 问题：可以。
- ordered custom layout 和 4K/64K row-source custom layout 有 board positive evidence。
- 256K dual-indexed / correspondence custom layout 只能写成 `negative_or_unstable_slice`，不能作为泛化收益证据。若后续要继续扩大自定义 layout 生产证据，优先开窄 phase 复核 large row-source variance / locality，而不是直接扩大 gate。
- `Scalar=double` 仍需独立数值预算和用户确认，不在本阶段推进。
