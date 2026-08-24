# Phase 044 结果：row-source generic xyz point type expansion

## 结论

本阶段已把 `row-source-generic-point-type-expansion`（row-source 泛型点型扩展）从 roadmap 的 `phase_deferred + unblocked_after_user_confirmation` 推进为：

```text
positive_row_source_generic_public_board_complete /
row-source-direct-fused-scale-accum /
source-indexed + dual-indexed + correspondence /
representative xyz AoS point types / Scalar=float / dense
```

本阶段不修改 production 源码；它验证 Phase 043 已采纳 row-source patch 在 traits-gated xyz AoS 代表点型上的 correctness、QEMU smoke 和 board repeated performance。结论只覆盖 `PointXYZI -> PointXYZI`、`PointXYZRGB -> PointXYZRGB`、`PointXYZI -> PointXYZRGB` 这三个代表组合，不外推到全部自定义点型、`Scalar=double`、非法 index / correspondence 或非 dense 输入。

## 实际变更

| area | 实际变更 | 位置 |
| --- | --- | --- |
| correctness | 新增 `RowSourceGenericXYZPointTypesMatchReference`，覆盖 source-indexed `PointXYZI -> PointXYZI`、dual-indexed `PointXYZRGB -> PointXYZRGB`、correspondence `PointXYZI -> PointXYZRGB`。 | `src/test_tesvd_scale.cpp` |
| bench | 新增 `row-source-generic-xyz-point-types` case-filter，覆盖三类 row source x 三个规模。 | `src/bench_tesvd_scale.cpp` |
| QEMU evidence target | 新增 `record_qemu_row_source_generic_state`。 | `Makefile` |
| board evidence target | 新增 `run_board_bench_row_source_generic_xyz_point_types_repeated`。 | `Makefile` |
| manifest / summary | QEMU 和 board wrapper 能识别 row-source generic 点型 label，并把 gate 写成 traits-gated xyz AoS。 | `script/generate_tesvd_scale_qemu_evidence_manifest.py`、`script/generate_tesvd_scale_board_repeated_summary.py` |

## 计划回填

| 计划项 | 结果 | 证据 |
| --- | --- | --- |
| A1 correctness 扩展 | 完成。Std/RVV 各 12 个 gtest 全通过。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`。 |
| A2 bench 扩展 | 完成。QEMU smoke 输出 9 个 row-source generic label，最大 `max_reference_error` 为 `3.040e-05`。 | `log/qemu/row_source_generic_xyz_point_types/analyze_bench_compare.log`。 |
| A3 target 接线 | 完成。QEMU / board / doctor / registry target 均可运行。 | `Makefile`。 |
| A4 QEMU 证据 | 完成。QEMU Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | `log/qemu/row_source_generic_xyz_point_types/evidence_doctor.md`。 |
| A5 board 证据 | 完成。5-run repeated 覆盖 9 个 case，全部 bucket 为 `positive`。 | `log/board/row_source_generic_xyz_point_types_repeated/summary.md`。 |
| A6 文档同步 | 本 result 已回填；matrix / roadmap / README / evaluation / topic-local docs 待本轮同步。 | 当前文档 diff。 |

## 结果矩阵

| row source policy | representative point type | QEMU correctness | QEMU smoke | board median B/A | board Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| source-indexed | `PointXYZI -> PointXYZI` | passed | clean | 4K `9.938x`，64K `10.371x`，256K `9.507x` | long-tail 和 group-outlier warnings；所有 case 仍 positive | `positive_row_source_generic_public_board_complete` |
| dual-indexed | `PointXYZRGB -> PointXYZRGB` | passed | clean | 4K `8.136x`，64K `6.486x`，256K `6.691x` | long-tail warnings；所有 case 仍 positive | `positive_row_source_generic_public_board_complete` |
| correspondence | `PointXYZI -> PointXYZRGB` | passed | clean | 4K `7.619x`，64K `7.061x`，256K `6.325x` | long-tail warnings；所有 case 仍 positive | `positive_row_source_generic_public_board_complete` |

Board summary 中的 `log checksum` mismatch 是预期风险：RVV reduction tree（规约树）与标量路径不同，正确性以 gtest 和 `max_reference_error <= 2e-3` 为准。board 最大 `max_reference_error` 为 `3.040e-05`，低于当前预算。

## Evidence Doctor 处理

QEMU row-source generic smoke Doctor 为 `Errors=0`、`Warnings=0`、`Suggestions=0`。

Board row-source generic Doctor 为 `Errors=0`、`Warnings=12`、`Suggestions=0`。这些 warning 不阻塞 positive bucket，但限制结论边界：

- 9 个 case 全部 positive，最小 run B/A 仍为 `5.527x`，不存在接近阈值的 case。
- 所有 case 都有 long-tail / variance warning，因此结论使用 min / median / max 全量报告，不剔除低值或高值 run。
- source-indexed 三个 size 是组内高收益离群，不能把 source-indexed 的 `9.5x` 到 `10.4x` median 外推给 dual-indexed 或 correspondence。
- warning 更像 measurement / layout / row-source cost 差异提示，适合导出后续 locality/order profile；它不回推否定 Phase 043 `PointXYZ -> PointXYZ` row-source adopted 结论。

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_public_row_source`。本阶段使用真实 source-indexed、dual-indexed 和 correspondence public overload。 |
| A/B boundary | Std 构建为父类 scale 标量公开入口；RVV 构建为 scale 子类 row-source RVV 公开入口。 |
| 当前决策问题 | 已采纳 row-source patch 的 traits-gated xyz AoS 代表点型是否有同边界 correctness / board support。 |
| diagnostic 是否可外推到 production | QEMU smoke 不可外推性能；board repeated 支撑当前 representative generic row-source public boundary。 |
| comparison-boundary / baseline mismatch 风险 | 有。row source 和点型 stride 成本不同，必须按 row source / point type / size 分开报告。 |
| weak / negative / unstable 时是否允许 bounded production probe | 本阶段结果为 positive；若未来扩展点型出现 weak / negative / unstable，只降级对应点型证据，不回推否定当前 adopted row-source patch。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；当前没有同 row-source generic 的既有 RVV family 选择问题。若后续尝试 locality/order/staging 新 family，则需要同边界 A/B。 |

## 后续候选

| candidate | why now | state | resume condition |
| --- | --- | --- | --- |
| `row-source-locality-order-profile` | Phase 043 / 044 board Doctor 都有 long-tail 或 group-outlier warning，尤其 correspondence 和 generic source-indexed 的稳定性值得拆分解释。 | `phase_deferred + unblocked` | 下一 phase 可按 index order / row source / point type 分组跑 locality profile。 |
| `more-generic-xyz-point-types` | Phase 041/044 只覆盖代表点型；Phase 051 后更多常见 PCL xyz AoS ordered public correctness / QEMU smoke 已补齐，Phase 052 又补这些点型的 64K ordered public board repeated。 | `completed_in_phase_052_for_ordered_public_board` | 若要证明 row-source 更广点型或全部自定义 xyz AoS，另开独立 scope。 |
| `Scalar=double` | 当前仍无 f64 数值预算。 | `turn_stop_deferred with scope guard` | 用户另开 double 数值预算。 |

## 验证命令

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_generic_state
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_generic_xyz_point_types_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status
```

本阶段最后一次 registry 检查为 fresh。

## 停止 / 继续判断

本阶段自身已 closed positive。由于 optimization matrix 仍有 `row-source-locality-order-profile` 这个当前授权范围内的 `phase_deferred + unblocked` 动作，默认下一步不是提交，而是继续 Phase 045 locality/order profile，除非用户要求先进入提交审计。
