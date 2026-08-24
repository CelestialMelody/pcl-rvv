# Phase 053 结果：row-source more-generic xyz AoS point types

## 决策

`positive_row_source_more_generic_public_board_complete / row-source-more-generic-xyz-aos-point-types / production public row-source / correctness-qemu-board`

本阶段没有修改 production 源码。它只把已采纳 row-source public overload 的点型证据，从 Phase 044 的 `PointXYZI` / `PointXYZRGB` 代表组合，扩展到 3 个更多常见 PCL xyz AoS 具体组合：

- source-indexed：`PointXYZRGBA -> PointXYZRGBA`
- dual-indexed：`PointNormal -> PointXYZRGB`
- correspondence：`PointWithViewpoint -> PointXYZ`

结论只覆盖这些组合、`Scalar=float`、dense、traits-gated xyz AoS 和 4K / 64K / 256K 确定性 stride inputs；不外推到 Phase 051 全部 5 个点型在所有 row source 下的全交叉矩阵、全部自定义点型、非法 index / correspondence 或 `Scalar=double`。

## 实现回填

| action | status | 事实 |
| --- | --- | --- |
| A1 correctness 扩展 | done | 新增 `RowSourceMoreGenericXYZAoSPointTypesMatchReference`；`expectRowSourceScaleMatchesReference` 加入 source / target traits static_assert。 |
| A2 bench 扩展 | done | 新增 `row-source-more-generic-xyz-aos-point-types` case-filter，输出 9 个 row-source / point-type / size label。 |
| A3 manifest / target | done | 新增 QEMU target `record_qemu_row_source_more_generic_state` 和 board target `run_board_bench_row_source_more_generic_xyz_aos_point_types_repeated`；QEMU / board manifest 均识别 Phase 053 边界。 |
| A4 QEMU 证据 | done | Std/RVV correctness 各 17 tests passed；QEMU smoke 9 comparisons，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 |
| A5 board 证据 | done | 5-run board repeated 9 case 全部 `positive`；Doctor `Errors=0`、`Warnings=3`、`Suggestions=0`。 |
| A6 文档同步 | done | result、matrix、roadmap、README、testing overview、correctness、benchmark/evidence、optimization evidence、test-support map、evaluation 和长期 doc-rvv 边界已同步。 |

## QEMU correctness / smoke

| command | result |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | Std/RVV 各 17 tests passed。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_more_generic_state` | QEMU smoke manifest / Doctor 生成并登记。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correctness_state evidence_status` | registry fresh。 |

QEMU smoke 只用于 build、label、manifest shape 和 `max_reference_error` 检查，不作为性能证据。9 个 RVV label 的最大 `max_reference_error` 为 `3.039837e-05`，低于 `2e-3` 预算。QEMU Evidence Doctor：

- path：`log/qemu/row_source_more_generic_xyz_aos_point_types/evidence_doctor.md`
- result：`Errors=0`、`Warnings=0`、`Suggestions=0`
- comparisons：9
- manifest boundary：`production_public_row_source_more_generic_xyz_aos_point_types`

## Board repeated evidence

| case | median B/A | min | max | bucket |
| --- | ---: | ---: | ---: | --- |
| source-indexed `PointXYZRGBA->PointXYZRGBA` 4K | `10.923x` | `10.658x` | `11.295x` | positive |
| source-indexed `PointXYZRGBA->PointXYZRGBA` 64K | `12.614x` | `12.444x` | `12.653x` | positive |
| source-indexed `PointXYZRGBA->PointXYZRGBA` 256K | `12.016x` | `11.682x` | `12.204x` | positive |
| dual-indexed `PointNormal->PointXYZRGB` 4K | `8.131x` | `8.087x` | `8.404x` | positive |
| dual-indexed `PointNormal->PointXYZRGB` 64K | `7.856x` | `7.492x` | `7.943x` | positive |
| dual-indexed `PointNormal->PointXYZRGB` 256K | `7.729x` | `7.618x` | `7.845x` | positive |
| correspondence `PointWithViewpoint->PointXYZ` 4K | `9.329x` | `9.139x` | `9.414x` | positive |
| correspondence `PointWithViewpoint->PointXYZ` 64K | `8.664x` | `8.481x` | `8.775x` | positive |
| correspondence `PointWithViewpoint->PointXYZ` 256K | `8.388x` | `8.345x` | `8.480x` | positive |

Board summary / doctor：

- summary：`log/board/row_source_more_generic_xyz_aos_point_types_repeated/summary.md`
- doctor：`log/board/row_source_more_generic_xyz_aos_point_types_repeated/evidence_doctor.md`
- manifest：`log/board/row_source_more_generic_xyz_aos_point_types_repeated/evidence_manifest.json`
- evidence role：`production_public_row_source_more_generic`
- Doctor：`Errors=0`、`Warnings=3`、`Suggestions=0`

3 个 warning 都是 source-indexed `PointXYZRGBA->PointXYZRGBA` 4K / 64K / 256K 的 `group_outlier`：它们的 median 高于本组整体 median。处理方式是按 row source / point type / size 分开报告，不把 source-indexed 的高收益继承到 dual-indexed 或 correspondence，也不把 Phase 053 的代表组合写成全部 row-source 点型结论。

## 证据登记

`make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` 当前为 fresh。Phase 053 新增并登记：

- QEMU smoke：`log/qemu/row_source_more_generic_xyz_aos_point_types/*`
- board repeated：`log/board/row_source_more_generic_xyz_aos_point_types_repeated/summary.md`
- board doctor：`log/board/row_source_more_generic_xyz_aos_point_types_repeated/evidence_doctor.md`
- board manifest：`log/board/row_source_more_generic_xyz_aos_point_types_repeated/evidence_manifest.json`

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_row_source_more_generic`，真实 production public row-source overload。 |
| A/B boundary | Std 构建父类标量公开入口 vs RVV 构建 scale 子类 row-source RVV 公开入口。 |
| 当前决策问题 | 已采纳 row-source traits-gated xyz AoS gate 是否有更多常见 PCL 点型组合的同边界 correctness / board support。 |
| diagnostic 是否可外推到 production | 可外推到这 3 个具体组合的 production public row-source 边界；不可外推到全部点型。 |
| comparison-boundary / baseline mismatch 风险 | 有。Doctor 的 group-outlier 说明必须按 row source / point type / size 分开解释。 |
| weak / negative / unstable 时是否允许 bounded production probe | 本阶段无 weak / negative / unstable；若未来扩展失败，只降级对应组合，不回推否定 Phase 043 / 044 adoption。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段没有新 RVV family 选择问题。 |

## 后续队列

- Phase 051 的 5 个点型组合没有在所有 row source 下做全交叉；若用户需要更广证据，可另开 row-source all-more-generic-matrix phase。
- 全部自定义 xyz AoS / padding 采样仍需独立 traits / layout 计划。
- `Scalar=double` 仍需独立数值预算。
- 当前没有新的 production 源码接入动作；本阶段只扩大已采纳 row-source production path 的证据边界。
