# Phase 054 结果：row-source all more-generic xyz AoS matrix

## 决策

`positive_row_source_all_more_generic_public_board_complete / row-source-all-more-generic-xyz-aos-matrix / production public row-source / correctness-qemu-board`

本阶段没有修改 production 源码。它只把 Phase 051 的 5 个常见 PCL xyz AoS（结构数组）点型组合，扩展到 source-indexed、dual-indexed 和 correspondence 三类 production public row-source overload（生产公开行源入口）的全交叉证据矩阵。

已验证范围：

- point type combos：`PointXYZRGBA -> PointXYZRGBA`、`PointXYZL -> PointXYZ`、`PointNormal -> PointXYZRGB`、`PointWithRange -> PointWithRange`、`PointWithViewpoint -> PointXYZ`
- row source：source-indexed、dual-indexed、correspondence
- `Scalar=float`、dense、traits-gated xyz AoS、确定性 stride indices / correspondences
- correctness 使用 4K selected pairs；QEMU / board 使用 4K、64K、256K

不覆盖范围保持独立：自定义点型、异常 padding / stride、`Scalar=double`、非法 index / correspondence、NaN / Inf、非 dense 输入和新的 locality mitigation（局部性缓解）实现族。

## 实现回填

| action | status | 事实 |
| --- | --- | --- |
| A1 correctness 扩展 | done | 新增 `RowSourceAllMoreGenericXYZAoSMatrixMatchesReference`；helper 按 5 个点型组合分别覆盖 source-indexed、dual-indexed 和 correspondence。 |
| A2 bench 扩展 | done | 新增 `row-source-all-more-generic-xyz-aos-matrix` case-filter，输出 45 个 row-source / point-type / size label。 |
| A3 manifest / target | done | 新增 QEMU target `record_qemu_row_source_all_more_generic_state` 和 board target `run_board_bench_row_source_all_more_generic_xyz_aos_matrix_repeated`；QEMU / board manifest 均识别 Phase 054 边界。 |
| A4 QEMU 证据 | done | Std/RVV correctness 各 18 tests passed；QEMU smoke 45 comparisons，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 |
| A5 board 证据 | done | 5-run board repeated 45 case 全部 `positive`；Doctor `Errors=0`、`Warnings=18`、`Suggestions=0`。 |
| A6 文档同步 | done | result、matrix、roadmap、README、testing overview、correctness、benchmark/evidence、optimization evidence、test-support map、evaluation 和长期 doc-rvv 边界已同步。 |

## QEMU correctness / smoke

| command | result |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | Std/RVV 各 18 tests passed。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_all_more_generic_state` | QEMU smoke manifest / Doctor 生成并登记。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correctness_state evidence_status` | registry fresh。 |

QEMU smoke 只用于 build、label、manifest shape 和 `max_reference_error` 检查，不作为性能证据。45 个 RVV label 的最大 `max_reference_error` 为 `3.039837e-05`，低于 `2e-3` 预算。QEMU Evidence Doctor：

- path：`log/qemu/row_source_all_more_generic_xyz_aos_matrix/evidence_doctor.md`
- result：`Errors=0`、`Warnings=0`、`Suggestions=0`
- comparisons：45
- run label：`qemu-tesvd-scale-row-source-all-more-generic-xyz-aos-matrix-smoke-phase054`

## Board repeated evidence

Board summary / doctor：

- summary：`log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/summary.md`
- doctor：`log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/evidence_doctor.md`
- manifest：`log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/evidence_manifest.json`
- evidence role：`production_public_row_source_all_more_generic`
- comparisons：45
- Doctor：`Errors=0`、`Warnings=18`、`Suggestions=0`

聚合结果：

| slice | cases | median B/A range | min B/A range | bucket |
| --- | ---: | ---: | ---: | --- |
| all | 45 | `7.277x` 到 `12.565x` | `7.119x` 到 `12.309x` | 45/45 positive |
| source-indexed | 15 | `10.329x` 到 `12.565x` | `9.364x` 到 `12.309x` | 15/15 positive |
| dual-indexed | 15 | `7.321x` 到 `9.518x` | `7.144x` 到 `9.233x` | 15/15 positive |
| correspondence | 15 | `7.277x` 到 `10.316x` | `7.119x` 到 `9.226x` | 15/15 positive |

最低 median case 是 correspondence `PointXYZRGBA -> PointXYZRGBA` 256K，median B/A `7.277x`，min `7.234x`，仍为 positive。最高 median case 是 source-indexed `PointXYZRGBA -> PointXYZRGBA` 64K，median B/A `12.565x`，max `12.620x`。

Doctor 的 18 个 warning 分两类：

- 4 个 `long_tail_or_variance`：dual-indexed `PointNormal -> PointXYZRGB` 4K、source-indexed `PointXYZL -> PointXYZ` 4K、correspondence `PointXYZL -> PointXYZ` 4K、correspondence `PointXYZRGBA -> PointXYZRGBA` 4K。处理方式是保留 min / median / max，不剔除低值 run；这些 case 的 min 仍在 `7.168x` 以上。
- 14 个 `group_outlier`：主要来自 source-indexed case 的 median 明显高于组内 median。处理方式是按 row source / point type / size 分开报告，不把 source-indexed 高收益继承到 dual-indexed 或 correspondence。

## 证据登记

`make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` 当前为 fresh。Phase 054 新增并登记：

- QEMU smoke：`log/qemu/row_source_all_more_generic_xyz_aos_matrix/*`
- board repeated：`log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/summary.md`
- board doctor：`log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/evidence_doctor.md`
- board manifest：`log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/evidence_manifest.json`
- correctness logs：`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_row_source_all_more_generic`，真实 production public row-source overload。 |
| A/B boundary | Std 构建父类标量公开入口 vs RVV 构建 scale 子类 row-source RVV 公开入口。 |
| 当前决策问题 | Phase 051 的 5 个 more-generic 点型组合是否在 source-indexed、dual-indexed 和 correspondence 下都有同边界 correctness / board support。 |
| diagnostic 是否可外推到 production | 可外推到这 15 个具体 row-source / 点型组合；不可外推到全部自定义点型、异常 layout、非法 index / correspondence 或 `Scalar=double`。 |
| comparison-boundary / baseline mismatch 风险 | 有。Doctor warning 说明 point type、row source、size 和 locality 会影响收益大小，必须分 case 报告。 |
| weak / negative / unstable 时是否允许 bounded production probe | 本阶段无 weak / negative / unstable；若未来扩展失败，只降级对应组合，不回推否定 Phase 043 / 044 / 053 adoption。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段没有新 RVV family 选择问题，也没有 production patch。 |

## 后续队列

- Phase 054 已关闭 Phase 051 五个常见 PCL xyz AoS 组合在三类 row source 下的全交叉 public evidence gap。
- 全部自定义 xyz AoS / padding 采样仍需独立 traits / layout 计划。
- `Scalar=double` 仍需独立数值预算。
- 当前 Doctor warning 没有导出新的 production patch；若继续优化，应从新的 locality mitigation family、自定义点型采样或 double 数值计划另开 phase。
