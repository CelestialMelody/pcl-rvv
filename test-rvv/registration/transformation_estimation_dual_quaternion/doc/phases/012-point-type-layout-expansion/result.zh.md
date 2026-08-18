# Phase 012 Result：point type / layout expansion

## 当前结论

Phase 012 已完成本地 correctness、QEMU smoke、asm smoke 和 board repeated。
`correspondence-pair` direct index stream 已扩展到 `PointXYZI` 和 `PointXYZRGB`
代表性 xyz AoS layout；这仍是 test-rvv diagnostic（诊断）证据，不是 production
dispatch 证据。

本阶段没有修改 production TEDQ header。`registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`
保持无 diff。

## 动作回填

| action | 状态 | 证据 / 产物 | 结论 |
| --- | --- | --- | --- |
| A1 fixtures | done | `include/impl/tedq_adapters.hpp` 新增 `makePointXYZRGBCloud` | RGB 字段保留，`transformCloudXYZ` 只变换 x/y/z。 |
| A2 correctness | done | `make run_test_compare` | Std/RVV 各 `24/24 tests passed`；新增 `PointXYZI` / `PointXYZRGB` direct index stream 测试通过。 |
| A3 QEMU smoke | done | `make record_qemu_smoke_evidence_state BENCH_ARGS="--iterations 2 --warmup-iterations 1 --case-filter correspondence-point-type-layout"` | 6 个 point-type layout smoke case 生成 manifest；Evidence Doctor `Errors=0 / Warnings=0 / Suggestions=0`。 |
| A4 asm smoke | done | `make dump_bench_rvv` | bench asm 中可见 `vlse32`、`vluxei32` 和 `vfredosum`，归属仍是 test-support bench binary。 |
| A5 board repeated | done | `make run_board_bench_correspondence_point_type_layout_repeated`；`log/board/correspondence_point_type_layout_repeated/summary.md` | 六个 point-type layout case 均为 `positive`；Evidence Doctor `Errors=0 / Warnings=5 / Suggestions=0`。 |
| A6 accidental Phase 008 freshness | done | `make record_board_correspondence_direct_index_stream_state` | 误触发覆盖了 Phase 008 raw run，因此已刷新对应 summary / manifest / doctor / registry。新 board doctor 为 `0/6/0`，总体仍 positive / weak-positive。 |

## Phase 012 证据

QEMU correctness：

- `log/qemu/run_test_std.log`：24 tests passed。
- `log/qemu/run_test_rvv.log`：24 tests passed。

QEMU smoke：

- case-filter：`correspondence-point-type-layout`
- cases：`PointXYZI` / `PointXYZRGB` × 4K / 64K / 256K
- Evidence Doctor：`Errors=0 / Warnings=0 / Suggestions=0`
- QEMU timing 只证明日志形状，不进入性能结论。

QEMU smoke 中部分 Std/RVV path fingerprint checksum 不同；这是当前 manifest 的
`path_fingerprint_only_correctness_covered_by_gtest_matrix_budget` 口径。正确性结论
以 gtest 矩阵误差预算为准。

Board repeated：

- case-filter：`correspondence-point-type-layout`
- device：`Milkv-Jupiter`
- runs：5；iterations=20；warmup=5
- Evidence Doctor：`Errors=0 / Warnings=5 / Suggestions=0`

| point type | 4K | 64K | 256K | decision |
| --- | ---: | ---: | ---: | --- |
| `PointXYZI` | `5.494x` | `6.687x` | `5.764x` | `positive` |
| `PointXYZRGB` | `5.719x` | `6.138x` | `4.285x` | `positive` |

Warning 边界：`PointXYZI 64K/256K`、`PointXYZRGB 64K/256K` 有 long-tail /
variance；`PointXYZRGB 256K` 还有 group outlier。结论按 point type / size 分开写，
不把其它 case 的收益直接继承到该 case。

## Phase 008 刷新说明

本轮误触发了既有 `correspondence-direct-index-stream-comparison` 的板卡 5-run
collection，覆盖了 raw run 目录。已立即重新生成并登记 summary / manifest / doctor。
刷新后的 median B/A：

| comparison | 4K | 64K | 256K | overall |
| --- | ---: | ---: | ---: | --- |
| staged ordered reuse / direct indexed gather | `1.565x` | `1.832x` | `1.663x` | `weak_positive` |
| staged ordered reuse / direct index stream | `2.505x` | `3.076x` | `2.527x` | `positive` |
| direct indexed gather / direct index stream | `1.575x` | `1.679x` | `1.442x` | `weak_positive` |

Evidence Doctor 为 `Errors=0 / Warnings=6 / Suggestions=0`。这些 warning 是
long-tail / group-outlier 风险；它们不推翻 diagnostic positive，但阻止把该结果写成
production evidence。

## 继续 / 停止决定

- 当前阶段：`diagnostic_positive_with_warnings`。
- 当前 topic：继续保持 `no-production`。
- `next_phase_default`：先解释 Phase 012 warning；若继续窄诊断，优先考虑 source-indexed /
  dual-indexed direct gather 的 `PointXYZI` / `PointXYZRGB` 扩展，或真实 workload
  correspondence 分布采样。
- production integration loop：未授权，不启动。
