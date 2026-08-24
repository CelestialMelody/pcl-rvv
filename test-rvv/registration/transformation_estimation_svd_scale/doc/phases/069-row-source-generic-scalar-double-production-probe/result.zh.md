# Phase 069 Result: row-source generic `Scalar=double` production probe

## 结论

EvidenceDecision：`positive_pending_user_confirmation`。

本阶段把 Phase 067 已采纳的 exact `PointXYZ -> PointXYZ` / `Scalar=double` row-source 分支，扩展为 common PCL xyz AoS whitelist（常见 PCL xyz 结构数组白名单）下的 row-source generic double production probe。真实 source-indexed、dual-indexed 和 correspondence public overload 已接入 f64 widened RVV accumulation，并完成 correctness、QEMU smoke、board repeated 和 Evidence Doctor。

当前证据支持进入 PI5 用户确认点；在用户确认前，不写成 `adopted-by-user`。

## 实现范围

- production header 新增 templated D64 row-source accumulation helper：
  - `accumulateTransformationEstimationSVDScaleContiguousOffsetPairD64RVV`
  - `accumulateTransformationEstimationSVDScaleSourceIndexedCloudPairD64RVV`
  - `accumulateTransformationEstimationSVDScaleDualIndicesCloudPairD64RVV`
  - `accumulateTransformationEstimationSVDScaleCorrespondencePairD64RVV`
- 三类 `Scalar=double` row-source dispatch 从 exact `PointXYZ -> PointXYZ` 扩到：
  - `kTransformationEstimationSVDScaleCommonDoublePair<PointSource, PointTarget>`
  - source / target 均满足 `RVVXYZAoSFloatLayout`
  - dense、size、gather byte-offset、index / correspondence 合法性和 fallback gate 保持不变
- exact `PointXYZ` D64 helper 保留，避免把本阶段变成无关重构。

## Correctness

`make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare`：

- Std：33/33 passed。
- RVV：33/33 passed。
- 新增：
  - `RowSourceGenericScalarDoubleProductionProbeMatchesReference`
  - `RowSourceGenericScalarDoubleProductionProbeFallbackBoundaries`

新增 gtest 覆盖 source-indexed、dual-indexed 和 correspondence 的 `PointXYZI->PointXYZI`、`PointXYZRGB->PointXYZRGB`、`PointXYZI->PointXYZRGB` 代表组合，并验证 small / non-dense / unsupported fallback boundary。

## QEMU Smoke

Target：`record_qemu_row_source_generic_scalar_double_production_probe_state`。

- case-filter：`row-source-generic-scalar-double-production-probe`
- comparisons：9
- Std path：`public-generic-double-row-source-scalar-fallback`
- RVV path：`public-generic-double-row-source-rvv-f64-widened-probe`
- RVV max public error：
  - source-indexed：`9.769963e-14`
  - dual-indexed / correspondence：`8.748557e-14`
- Evidence Doctor：`Errors=0`、`Warnings=0`、`Suggestions=0`

QEMU timing 只作为 build / path / log-shape smoke，不写性能结论。

## Board Repeated

Target：`run_board_bench_row_source_generic_scalar_double_production_probe_repeated`。

Board repeated 覆盖 9 个 64K case，全部 `positive`，checksum match，`max ref error <= 9.770e-14`。

| row source | point type pair | median B/A | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| source-indexed | `PointXYZI->PointXYZI` | `17.391x` | `17.201x` | `17.730x` | positive |
| dual-indexed | `PointXYZI->PointXYZI` | `12.463x` | `12.245x` | `13.099x` | positive |
| correspondence | `PointXYZI->PointXYZI` | `11.388x` | `10.992x` | `11.804x` | positive |
| source-indexed | `PointXYZI->PointXYZRGB` | `17.273x` | `15.377x` | `17.770x` | positive |
| dual-indexed | `PointXYZI->PointXYZRGB` | `12.025x` | `10.758x` | `12.592x` | positive |
| correspondence | `PointXYZI->PointXYZRGB` | `11.309x` | `9.759x` | `11.672x` | positive |
| source-indexed | `PointXYZRGB->PointXYZRGB` | `17.433x` | `14.963x` | `17.708x` | positive |
| dual-indexed | `PointXYZRGB->PointXYZRGB` | `12.650x` | `11.189x` | `12.746x` | positive |
| correspondence | `PointXYZRGB->PointXYZRGB` | `11.465x` | `10.967x` | `11.771x` | positive |

Board Evidence Doctor：`Errors=0`、`Warnings=4`、`Suggestions=0`。

Warnings 都是 long-tail / variance，集中在：

- source-indexed `PointXYZI->PointXYZRGB`
- dual-indexed `PointXYZI->PointXYZRGB`
- correspondence `PointXYZI->PointXYZRGB`
- source-indexed `PointXYZRGB->PointXYZRGB`

处理方式：保留 min / median / max，不剔除异常；这些 warning 不改变 9 个 case 的 positive bucket。

## 边界

本阶段只覆盖：

- source-indexed、dual-indexed、correspondence public row-source overload。
- common PCL xyz AoS whitelist。
- `Scalar=double`。
- dense、64K、`nr_points >= 16`、合法 index / correspondence、合法 gather byte-offset。

本阶段不覆盖：

- custom layout double、任意用户自定义点型全集、异常 padding / alignment 全集。
- sorted-copy double、affine fast path double 或新的 RVV family selection。
- non-dense、小规模、非法 index / correspondence 语义。
- 更广输入规模、随机 stress 或其它未列点型组合。

## 下一步

若用户确认采纳，后续应另开新的 adoption closeout phase（当前下一个可用编号为 Phase 074，因为 Phase 070 / 071 / 072 / 073 已分别用于 custom layout double scout、更多 custom layout double 取样、correspondence sorted-copy double probe 和同边界 detail A/B），刷新长期 `doc-rvv`、optimization matrix、roadmap 和 topic-local docs 为 `adopted-by-user`。若暂不确认，本阶段保持 `positive_pending_user_confirmation`，production patch 可作为有证据的候选停留。
