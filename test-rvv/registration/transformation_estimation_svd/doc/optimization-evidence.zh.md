# 优化证据索引

## 本文职责

本文把 SVD topic 的 candidate family（候选族）、代码路径、测试 target 和证据状态放在一个索引中，避免把诊断结论误写成 production evidence（生产证据）。

## 当前结论摘要

当前是 `production-ready / adopted for ordered-cloud-pair, source-indexed-cloud-pair, dual-indices-cloud-pair and correspondence-pair`。`fused_ordered_cloud_pair_accum`（历史 alias：`fused_full_cloud_accum`）作为 Phase 010 诊断已支持 ordered-cloud-pair PI1，Phase 020 已接入真实 public ordered-cloud-pair production dispatch。`source_indexed_fused_accum` 在 Phase 030 诊断中正向，Phase 040 已接入真实 public source-indexed production dispatch。`dual_indices_fused_accum` 和 `correspondence_fused_accum` 在 Phase 050 诊断中正向，Phase 060 已接入真实 public dual-indices / correspondence production dispatch。QEMU Std/RVV correctness 各 22 tests passed，board smoke 22/22 passed。四条 production direct board median 分别为 ordered 4K `14.372x`、64K `24.471x`、256K `23.841x`；source-indexed 4K `9.634x`、64K `12.217x`、256K `11.558x`；dual-indices 4K `6.805x`、64K `6.404x`、256K `5.964x`；correspondence 4K `8.649x`、64K `8.644x`、256K `7.872x`。

## 优化方式总表

| candidate family | 状态 | 代码路径 | test target | bench target | board evidence | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `public_umeyama_baseline` | adopted as baseline | production `TransformationEstimationSVD` public ordered / indexed / dual / correspondence path | `run_test_compare` 中 semantic anchor | `public-umeyama` case-filter | Std build baseline | baseline only |
| `fused_ordered_cloud_pair_accum` | board positive diagnostic | `include/impl/tesvd_candidates.hpp` | `run_test_compare` pass | `ordered-cloud-pair` case-filter；兼容 `fused-full-cloud` | `summary.md` same-boundary median `3.081x` / `3.179x` / `3.157x` | partial-production-candidate / PI1 candidate |
| `source_indexed_fused_accum` | board positive diagnostic | `include/impl/tesvd_candidates.hpp` | `run_test_compare` pass | `source-indexed-cloud-pair` case-filter | same-boundary median `1.917x` / `1.843x` / `1.785x`；mixed-boundary median `7.012x` / `9.031x` / `8.827x` | partial-production-candidate / Phase 040 input |
| `dual_indices_fused_accum` | board positive diagnostic | `include/impl/tesvd_candidates.hpp` | `run_test_compare` pass | `dual-indices-cloud-pair` case-filter | same-boundary median `1.801x` / `1.683x` / `1.425x`；overall bucket `positive` | partial-production-candidate / Phase 060 input |
| `correspondence_fused_accum` | board positive diagnostic | `include/impl/tesvd_candidates.hpp` | `run_test_compare` pass | `correspondence-pair` case-filter | same-boundary median `2.174x` / `1.905x` / `1.772x`；overall bucket `positive` | partial-production-candidate / Phase 060 input |
| `production_direct_dispatch_ordered` | adopted | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` | `run_test_compare` + `run_board_test_smoke` | `run_board_bench_production_ordered_cloud_pair_repeated` | public Std/RVV median 4K `14.372x`、64K `24.471x`、256K `23.841x` | production-ready for ordered-cloud-pair |
| `production_direct_dispatch_source_indexed` | adopted | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` | `run_test_compare` + `run_board_test_smoke` | `run_board_bench_production_source_indexed_cloud_pair_repeated` | public Std/RVV median 4K `9.634x`、64K `12.217x`、256K `11.558x` | production-ready for source-indexed-cloud-pair |
| `production_direct_dispatch_dual_indices` | adopted | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` | `run_test_compare` + `run_board_test_smoke` | `run_board_bench_production_dual_indices_cloud_pair_repeated` | public Std/RVV median 4K `6.805x`、64K `6.404x`、256K `5.964x` | production-ready for dual-indices-cloud-pair |
| `production_direct_dispatch_correspondence` | adopted | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` | `run_test_compare` + `run_board_test_smoke` | `run_board_bench_production_correspondence_pair_repeated` | public Std/RVV median 4K `8.649x`、64K `8.644x`、256K `7.872x` | production-ready for correspondence-pair |
| `generic_xyz_aos_gate` | correctness adopted / performance bounded | `src/test_tesvd.cpp` + production direct helpers | `run_test_compare` | not split | `PointXYZI` / `PointXYZRGB` correctness path-hit | layout-gated representative boundary |
| `Scalar=double` | rejected / fallback | production helper gates | `run_test_compare` | not planned | fallback covered | no double RVV plan |

## 标量路径与 RVV 路径差异

| 维度 | public baseline / fallback | production RVV / fused candidate |
| --- | --- | --- |
| 数据装填 | fallback 在 `use_umeyama_ == true` 时装成动态矩阵。 | production RVV 直接求 source / target sum 与 target-source cross sum。 |
| 求解器 | `pcl::umeyama(..., false)`，内部仍是 3x3 SVD。 | Eigen 3x3 SVD，保持 no-scale rigid transform。 |
| 加法树 | Eigen rowwise sum / matrix product。 | 标量 fused 或 RVV reduction tree，允许小误差预算。 |
| production 状态 | fallback truth。 | 四条 row source production path 已采用；test-only fused helper 保留为 reference / diagnostic。 |

## 当前可提交证据

以下证据已被 README、benchmark 文档或 phase result 引用，可作为 summary-only review-required（摘要证据，需审查）候选；当前 `.gitignore` 只自然暴露 `log/qemu/evidence_doctor.md`。registry、manifest 和 raw logs 保持 ignored-local，是否提交需要用户明确授权、allowlist / force-add 和脱敏审查。

| 路径 | 证据角色 | 边界 |
| --- | --- | --- |
| `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU correctness | 证明 Std/RVV gtest 通过，不证明性能。 |
| `log/qemu/analyze_bench_compare.log` | QEMU smoke summary | 证明 bench 输出形状；QEMU timing 不作性能结论。 |
| `log/qemu/evidence_manifest.json`、`log/qemu/evidence_doctor.md` | QEMU smoke doctor | Errors=0、Warnings=0、Suggestions=0；只覆盖 QEMU smoke 合同。 |
| `log/board/fused_full_cloud_repeated/summary.md` | board repeated diagnostic | same-boundary fused Std/RVV positive；mixed-boundary public-vs-fused 只支持 PI1 候选。该目录名保留为历史 alias。 |
| `log/board/fused_full_cloud_repeated/evidence_manifest.json`、`log/board/fused_full_cloud_repeated/evidence_doctor.md` | board doctor | Errors=0、Warnings=4、Suggestions=0；Warning 已按证据边界处理。 |
| `log/board/production_ordered_cloud_pair_repeated/summary.md` | production direct board repeated | public ordered-cloud-pair Std/RVV positive。 |
| `log/board/production_ordered_cloud_pair_repeated/evidence_manifest.json`、`log/board/production_ordered_cloud_pair_repeated/evidence_doctor.md` | production direct doctor | Errors=0、Warnings=1、Suggestions=0。 |
| `log/board/source_indexed_cloud_pair_repeated/summary.md` | source-indexed board diagnostic | same-boundary fused Std/RVV positive；mixed-boundary public-vs-fused 只支持 Phase 040 候选。 |
| `log/board/source_indexed_cloud_pair_repeated/evidence_manifest.json`、`log/board/source_indexed_cloud_pair_repeated/evidence_doctor.md` | source-indexed diagnostic doctor | Errors=0、Warnings=4、Suggestions=0。 |
| `log/board/production_source_indexed_cloud_pair_repeated/summary.md` | source-indexed production direct board repeated | public source-indexed Std/RVV positive。 |
| `log/board/production_source_indexed_cloud_pair_repeated/evidence_manifest.json`、`log/board/production_source_indexed_cloud_pair_repeated/evidence_doctor.md` | source-indexed production direct doctor | Errors=0、Warnings=0、Suggestions=0。 |
| `log/board/dual_indices_cloud_pair_repeated/summary.md` | dual-indices board diagnostic | same-boundary fused Std/RVV positive。 |
| `log/board/correspondence_pair_repeated/summary.md` | correspondence board diagnostic | same-boundary fused Std/RVV positive。 |
| `log/board/production_dual_indices_cloud_pair_repeated/summary.md` | dual-indices production direct board repeated | public dual-indices Std/RVV positive。 |
| `log/board/production_dual_indices_cloud_pair_repeated/evidence_manifest.json`、`log/board/production_dual_indices_cloud_pair_repeated/evidence_doctor.md` | dual-indices production direct doctor | Errors=0、Warnings=1、Suggestions=0。 |
| `log/board/production_correspondence_pair_repeated/summary.md` | correspondence production direct board repeated | public correspondence Std/RVV positive。 |
| `log/board/production_correspondence_pair_repeated/evidence_manifest.json`、`log/board/production_correspondence_pair_repeated/evidence_doctor.md` | correspondence production direct doctor | Errors=0、Warnings=1、Suggestions=0。 |
| `log/evidence_registry.json` | freshness registry | `evidence_status` 报告 fresh。 |

## 结论边界

当前 production 结论覆盖四条 row source policy、`Scalar=float`、`use_umeyama_ == true`、dense xyz AoS layout。历史 case-filter 和日志路径中的 `full-cloud` 是 legacy label；row-source policy 结论统一使用 ordered-cloud-pair。`PointXYZI` / `PointXYZRGB` 只保留代表性 correctness 和布局边界说明，不自动升级为逐类型板卡性能结论。
