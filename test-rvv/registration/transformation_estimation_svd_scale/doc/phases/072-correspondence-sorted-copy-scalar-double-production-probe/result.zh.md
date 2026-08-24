# Phase 072 Result: correspondence sorted-copy `Scalar=double` production probe

## 结论

EvidenceDecision：`bounded_public_positive_detail_ab_negative`。

本阶段执行时，Phase 069 / 070 / 071 仍在用户确认点；Phase 074 后它们已经按用户确认收口为 `adopted-by-user`。本阶段本身只评估 correspondence sorted-copy（对应关系排序副本）是否值得扩展到 `Scalar=double`。真实 public correspondence overload 已新增 double sorted-copy probe：size / disorder gate 命中时复制并按 query / match 排序 correspondences，再复用 f64 widened correspondence accumulation；不命中时仍回到当前 D64 gather RVV path 或父类 fallback。

board repeated 显示 64K / 256K 两个 public Std/RVV case 均为 positive，QEMU 和 board Evidence Doctor 均为 `Errors=0`、`Warnings=0`、`Suggestions=0`。但该证据角色是 production-public（生产公开入口）Std/RVV probe，只证明当前 public RVV path 快于 public scalar fallback；它不能证明 sorted-copy double 优于既有 D64 gather RVV family。Phase 073 已补同一 production boundary 内的 RVV-vs-RVV detail A/B，结果为 negative，因此本阶段状态回填为 `bounded_public_positive_detail_ab_negative`。当前 production patch 不自动回滚；接受 bounded candidate 风险或回滚该分支都需要用户明确确认。

## 实现范围

- production header 新增 `estimateRigidTransformationSVDScaleCorrespondencePairSortedCopyRVV(...)` 的 `Eigen::Matrix<double, 4, 4>&` overload。
- double correspondence branch 在 contiguous fast path 后尝试 sorted-copy helper；若 size / disorder gate 不满足或 helper 不接管，则回到 `accumulateTransformationEstimationSVDScaleCorrespondencePairD64RVV` gather path。
- sorted-copy gate 复用既有 float production heuristic：至少 64K 且 query index disorder 足够高。

## Correctness

`make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare`：

- Std：38/38 passed。
- RVV：38/38 passed。
- 新增 `CorrespondenceSortedCopyScalarDoubleProductionProbeMatchesReference`，覆盖 shuffled correspondences 下 public `TransformationEstimationSVDScale<PointXYZ, PointXYZ, double>` 与 selected-cloud double reference 对齐。

## QEMU Smoke

Target：`record_qemu_correspondence_sorted_copy_scalar_double_production_probe_state`。

- case-filter：`correspondence-sorted-copy-scalar-double-production-probe`
- comparisons：2（64K / 256K）
- Std path：`std-correspondence-sorted-copy-scalar-double-production-probe`
- RVV path：`rvv-correspondence-sorted-copy-scalar-double-production-probe`
- RVV hit label：`public-double-correspondence-sorted-copy-rvv-f64-widened-probe`
- Evidence Doctor：`Errors=0`、`Warnings=0`、`Suggestions=0`

QEMU timing 只作为 build / path / log-shape smoke，不写性能结论。

## Board Repeated

Target：`run_board_bench_correspondence_sorted_copy_scalar_double_production_probe_repeated`。

| case | runs B/A | median | min | max | bucket | max ref error | checksum |
| --- | --- | ---: | ---: | ---: | --- | ---: | --- |
| 64K | `5.590, 5.982, 5.727, 6.155, 5.709` | `5.727x` | `5.590x` | `6.155x` | positive | `8.038e-14` | match |
| 256K | `5.407, 5.473, 5.247, 5.408, 5.419` | `5.408x` | `5.247x` | `5.473x` | positive | `2.474e-13` | match |

Board Evidence Doctor：`Errors=0`、`Warnings=0`、`Suggestions=0`。

## 边界

本阶段只覆盖：

- public correspondence overload。
- `PointXYZ -> PointXYZ`。
- `Scalar=double`。
- dense xyz AoS、合法 shuffled correspondences、sorted-copy size / disorder gate。
- 64K / 256K board performance。

本阶段不覆盖：

- dual-indexed sorted-copy；Phase 058 已把 dual-indexed 256K source-sorted-copy 复核为 rejected / unstable。
- custom layout double sorted-copy、generic point type double sorted-copy 或全部 row-source sorted-copy double。
- 小规模、non-dense、非法 correspondence 语义。
- sorted-copy double 与既有 D64 gather RVV family 的同边界 RVV-vs-RVV family selection。

## Evidence Doctor 处理

QEMU Doctor 和 board Doctor 均为 `0/0/0`，没有需要解释的 warning。性能结论只使用 board repeated；QEMU smoke 仅用于路径命中、日志形状和 manifest 可解析性。

## 下一步

Phase 074 已把 Phase 069 / 070 / 071 根据用户确认收口为 `adopted-by-user`；这些采纳不改变本阶段的 sorted-copy double family-selection 边界。Phase 073 已完成同一 production boundary 内的 `correspondence sorted-copy double` vs `D64 gather` RVV-vs-RVV detail A/B，结果不支持 clean adoption；后续只能在用户明确接受 bounded public-positive 风险时收口，或在用户授权后回滚 / no-production closeout。
