# Phase 075 Result: correspondence sorted-copy `Scalar=double` rollback closeout

## 结论

EvidenceDecision：`rolled_back_no_production_for_scalar_double_sorted_copy`。

用户已明确同一 production boundary（生产边界）下选择更快实现。Phase 072 的 correspondence sorted-copy `Scalar=double` public Std/RVV probe 是历史 positive：64K / 256K board median B/A `5.727x` / `5.408x`，Doctor `0/0/0`。但 Phase 073 的同边界 RVV-vs-RVV detail A/B 已证明 sorted-copy double 慢于既有 D64 gather RVV family：64K / 256K board median B/A `0.283x` / `0.431x`，Doctor `2/1/0`。

本阶段因此移除 sorted-copy double 生产分流。当前 production behavior（生产行为）是：`Scalar=double` correspondence 在 contiguous fast path 不命中后直接使用 D64 gather RVV accumulation；Phase 047 已采纳的 `Scalar=float` correspondence sorted-copy 分支保持不变。

## Production Diff

- 移除 `estimateRigidTransformationSVDScaleCorrespondencePairSortedCopyRVV(... Eigen::Matrix<double, 4, 4>&)` overload。
- 移除 `estimateRigidTransformationSVDScaleCorrespondencePairRVV` 的 double branch 中的 sorted-copy helper 尝试。
- 保留 `estimateRigidTransformationSVDScaleCorrespondencePairSortedCopyRVV(... Eigen::Matrix<float, 4, 4>&)` 和 size / disorder gate，继续服务 Phase 047 `Scalar=float` adopted branch。

## Correctness

`make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare`：

- Std：38/38 passed。
- RVV：38/38 passed。

测试 `CorrespondenceSortedCopyScalarDoubleProductionProbeMatchesReference` 已改名为 `CorrespondenceScalarDoubleGatherProductionPathMatchesReference`，语义也同步改成：shuffled correspondence `Scalar=double` 公开入口仍由 D64 gather production path 接管，并与 selected-cloud double reference 对齐。

## Evidence Role

| evidence | role | conclusion |
| --- | --- | --- |
| Phase 072 public Std/RVV board positive | historical production-public probe | 证明 sorted-copy double 曾快于 public scalar fallback，但不能证明优于已有 RVV family。 |
| Phase 073 RVV-vs-RVV detail A/B negative | production-detail family selection | 证明 sorted-copy double 慢于 D64 gather；支撑本阶段 rollback / no-production。 |
| Phase 075 correctness | current production correctness | 证明移除 sorted-copy double 后，当前 Std/RVV 公开入口仍通过 38-test 对拍。 |

## 边界

本阶段只关闭：

- correspondence row source；
- `PointXYZ -> PointXYZ`；
- `Scalar=double`；
- dense shuffled correspondence 64K / 256K family-selection 边界。

本阶段不改变：

- Phase 047 `Scalar=float` correspondence sorted-copy adopted branch；
- Phase 069 / 070 / 071 已采纳的 `Scalar=double` generic / custom layout D64 gather branches；
- contiguous affine index fast path；
- broader custom layout double 或任意自定义点型全集。

## Continue / Stop Decision

`continue_stop_decision`：`phase_closeout_verified_then_scan_roadmap`。

当前 Phase 072 用户决策边界已关闭。若最终 registry、脚本和 whitespace 门禁通过，再扫描 roadmap；若剩余路线都需要新的输入分布定义、board budget 或扩大 topic scope，则暂停并报告现状。
