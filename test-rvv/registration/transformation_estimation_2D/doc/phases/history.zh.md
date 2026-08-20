# transformation_estimation_2D Phase History

本页是 Phase 000-107 的压缩历史索引，用来降低 `doc/phases/README.zh.md` 的阅读负担。
它不替代各阶段的 `plan.zh.md` / `result.zh.md`，也不改变当前 production truth（生产真实状态）。
需要逐条复核证据时，优先看本页列出的 board summary、QEMU asm / manifest 和 Evidence Doctor
输出；未纳入本次提交的旧 phase 目录只作为本机恢复记录，不作为提交后的主入口。

## 分段索引

| 范围 | 主线 | 当前结论 |
| --- | --- | --- |
| Phase 000-020 | 建立 scaffold（脚手架）、标量路径重建、ordered-cloud-pair 2D fused correlation 诊断、QEMU / asm / board 初证据。 | 证明 two-pass centered fused 2D correlation 在窄范围有价值，但当时仍是 diagnostic evidence。 |
| Phase 030-050 | row source materialize 诊断和 ordered-cloud-pair production-public probe。 | ordered-cloud-pair exact `PointXYZ -> PointXYZ` production evidence positive，后续由 Phase 080 generic ordered dispatch 覆盖。 |
| Phase 060-080 | production boundary 审阅和 ordered-cloud-pair PointXYZ-like generic widening。 | traits-gated ordered generic public dispatch 已被用户采纳并保留；不外推到 indexed 或 correspondence row source。 |
| Phase 090-098 | source-indexed、dual-indexed、correspondence 的 direct gather、family A/B、staging、locality/order、component ablation 和 chunked staging。 | source-indexed exact positive 并进入 Phase 091 adoption；dual-indexed exact 后续进入 Phase 107 production patch；correspondence 多条路线为 negative / unstable，不 clean-adopt。 |
| Phase 099-101 | source-indexed、dual-indexed、correspondence generic PointXYZ-like representative diagnostics。 | 三类 indexed / correspondence generic widening 均不能直接接入 production。 |
| Phase 102-107 | production decision package、source-indexed generic guarded public probe、20-run variance、beneficial path adoption loop。 | source-indexed exact retained；source-indexed generic Phase 106 negative；dual-indexed exact Phase 107 接入并由 Phase 109 复核后继续保留；correspondence trial rolled back。 |

## 关键决策锚点

| phase | 角色 | 结论 |
| --- | --- | --- |
| 080-generic-xyz-production-integration-plan | ordered generic production integration | 用户已采纳 traits-gated ordered-cloud-pair generic public dispatch。 |
| 091-row-source-bounded-production-probe | source-indexed exact production probe | 用户已采纳 exact `PointXYZ -> PointXYZ` source-indexed public dispatch。 |
| 092-source-indexed-family-ab | source-indexed direct vs materialize family A/B | weak-positive，支持保留 current direct gather family，但不扩大 row source 或点型。 |
| 093-dual-indexed-family-ab | dual-indexed exact historical family A/B | 64K / 256K positive，4K caveat；Phase 107 已刷新并按用户授权纳入 topic-only commit。 |
| 094-correspondence-guarded-probe-or-staging-profile | correspondence exact guarded probe | public probe 曾 positive，但 family A/B 不干净；Phase 107 试接入后已退回。 |
| 099-source-indexed-generic-xyz-point-type-expansion | source-indexed generic diagnostic | mixed-negative，不支持直接扩大 Phase 091 exact gate。 |
| 100-dual-indexed-generic-xyz-point-type-expansion | dual-indexed generic diagnostic | negative，不支持扩大 Phase 107 exact gate。 |
| 101-correspondence-generic-xyz-point-type-expansion | correspondence generic diagnostic | negative，不恢复 correspondence production dispatch。 |
| 103-source-indexed-generic-bounded-production-probe | source-indexed generic public probe | guarded only；15/16 positive，但 `PointNormal -> PointNormal 256K` 触发退化频率 Error。 |
| 104-source-indexed-generic-pointnormal-256k-long-tail-diagnostic | Phase 103 负向 case 单独复跑 | below-1 未复现，但仍是 guarded evidence，不 clean-adopt。 |
| [`106-source-indexed-generic-public-variance`](106-source-indexed-generic-public-variance/result.zh.md) | source-indexed generic public representative 20-run variance | full representative widening negative，不能 clean-adopt Phase 103/104。 |
| [`107-production-adoption-and-regression-loop`](107-production-adoption-and-regression-loop/result.zh.md) | beneficial RVV production adoption loop | ordered generic、source-indexed exact、dual-indexed exact 留在当前 patch；correspondence exact 退回。 |
| [`109-dual-indexed-exact-public-variance`](109-dual-indexed-exact-public-variance/result.zh.md) | dual-indexed exact 20-run variance recovery phase | Phase 109 独立 20-run board 已完成；64K/256K positive，4K median positive 但 1/20 below-1，Doctor `0/3/0`，结论为保留 exact dispatch 和 4K caveat。 |
| [`110-source-indexed-pointxyzi-exact-public-probe`](110-source-indexed-pointxyzi-exact-public-probe/result.zh.md) | source-indexed exact PointXYZI production-public probe | Phase 110 独立 20-run board 已完成；4K/64K/256K `3.979x / 3.547x / 3.602x`，`B/A<1=0/20`，Doctor `0/3/0`。结论为 positive bounded production probe。 |
| [`111-source-indexed-normal-negative-case-investigation`](111-source-indexed-normal-negative-case-investigation/result.zh.md) | source-indexed generic Normal negative-case audit | Phase 111 未改 production gate；复核 Phase 106 20-run 中 `PointNormal->PointNormal 256K` `7/20` below-1 和 48B Normal layout 风险，结论为 Normal 类 source-indexed generic widening 继续不接入生产。 |
| [`112-source-indexed-pointxyzi-adoption-closeout`](112-source-indexed-pointxyzi-adoption-closeout/result.zh.md) | source-indexed exact PointXYZI adoption closeout | Phase 112 按用户确认采纳 exact `PointXYZI -> PointXYZI` source-indexed public dispatch；只转换该 exact gate，不扩大 source-indexed generic widening。 |

## 当前生产事实

当前 header 中保留的 RVV production paths（生产路径）只有三类：

- ordered-cloud-pair generic：`Scalar=float`，source/target 分别满足
  `RVVXYZAoSFloatLayout<PointT>`，dense finite，size >= 16。
- source-indexed exact：`PointXYZ -> PointXYZ`，`Scalar=float`，valid source indices，
  dense finite，size >= 16。
- source-indexed exact：`PointXYZI -> PointXYZI`，`Scalar=float`，valid source
  indices，dense finite，size >= 16；Phase 110 positive，Phase 112 按用户确认采纳。
- dual-indexed exact：`PointXYZ -> PointXYZ`，`Scalar=float`，valid source / target indices，
  dense finite，size >= 16。

当前 header 中没有 correspondence RVV production dispatch。source-indexed generic、
dual-indexed generic、correspondence generic 和 `Scalar=double` 都没有 production adoption。
Phase 111 已明确 Normal 类 source-indexed generic widening 不从 Phase 110 `PointXYZI`
exact probe 继承。

## 历史保留原则

这些 phase 看起来多，是因为每个 row source、point type、`Scalar`、layout 和 production
boundary 都必须独立批准。负向 phase 也要保留，因为它们防止后续把同一 correspondence /
generic indexed 方向重复当作新候选接入。

本页只压缩导航，不删除证据。提交时如果需要进一步减少体量，应优先提交 summary / manifest /
Evidence Doctor 和当前决策文档；raw logs、本地 build 输出和未被文档引用的临时文件默认不提交。

## Evidence registry 历史锚点

`evidence_status` 需要能从提交后的文档入口找到已登记的 evidence files（证据文件）。下列路径是
历史 phase 仍要保留可追溯性的锚点；它们不表示对应候选已采纳。

| 证据组 | 路径 |
| --- | --- |
| aggregate correctness logs | `test-rvv/registration/transformation_estimation_2D/log/qemu/run_test_std.log` |
| aggregate correctness logs | `test-rvv/registration/transformation_estimation_2D/log/qemu/run_test_rvv.log` |
| row-source fused historical board | `test-rvv/registration/transformation_estimation_2D/log/board/row_source_fused_repeated/summary.md` |
| row-source fused historical board | `test-rvv/registration/transformation_estimation_2D/log/board/row_source_fused_repeated/evidence_manifest.json` |
| row-source fused historical board | `test-rvv/registration/transformation_estimation_2D/log/board/row_source_fused_repeated/evidence_doctor.md` |
| source-indexed generic public guarded probe board | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_repeated/summary.md` |
| source-indexed generic public guarded probe board | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_repeated/evidence_manifest.json` |
| source-indexed generic public guarded probe board | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_repeated/evidence_doctor.md` |
| source-indexed generic PointNormal 256K rerun board | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_pointnormal_256k_public_repeated/summary.md` |
| source-indexed generic PointNormal 256K rerun board | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_pointnormal_256k_public_repeated/evidence_manifest.json` |
| source-indexed generic PointNormal 256K rerun board | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_pointnormal_256k_public_repeated/evidence_doctor.md` |
| source-indexed generic public guarded probe QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public/run_bench_source_indexed_generic_xyz_point_types_public_rvv.log` |
| source-indexed generic public guarded probe QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public/asm_attribution.md` |
| source-indexed generic public guarded probe QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public/asm_attribution.json` |
| source-indexed generic public guarded probe QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public/evidence_manifest.json` |
| source-indexed generic public guarded probe QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public/evidence_doctor.md` |
| source-indexed generic PointNormal 256K QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_pointnormal_256k_public/run_bench_source_indexed_generic_pointnormal_256k_public_rvv.log` |
| source-indexed generic PointNormal 256K QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_pointnormal_256k_public/asm_attribution.md` |
| source-indexed generic PointNormal 256K QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_pointnormal_256k_public/asm_attribution.json` |
| source-indexed generic PointNormal 256K QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_pointnormal_256k_public/evidence_manifest.json` |
| source-indexed generic PointNormal 256K QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_pointnormal_256k_public/evidence_doctor.md` |
| dual-indexed exact Phase 109 board variance | `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase109_variance_repeated/summary.md` |
| dual-indexed exact Phase 109 board variance | `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase109_variance_repeated/evidence_manifest.json` |
| dual-indexed exact Phase 109 board variance | `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase109_variance_repeated/evidence_doctor.md` |
| source-indexed PointXYZI exact Phase 110 board probe | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_pointxyzi_public_phase110_repeated/summary.md` |
| source-indexed PointXYZI exact Phase 110 board probe | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_pointxyzi_public_phase110_repeated/evidence_manifest.json` |
| source-indexed PointXYZI exact Phase 110 board probe | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_pointxyzi_public_phase110_repeated/evidence_doctor.md` |
| source-indexed PointXYZI exact Phase 110 QEMU probe | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_pointxyzi_public/run_bench_source_indexed_pointxyzi_public_rvv.log` |
| source-indexed PointXYZI exact Phase 110 QEMU probe | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_pointxyzi_public/asm_attribution.md` |
| source-indexed PointXYZI exact Phase 110 QEMU probe | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_pointxyzi_public/asm_attribution.json` |
| source-indexed PointXYZI exact Phase 110 QEMU probe | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_pointxyzi_public/evidence_manifest.json` |
| source-indexed PointXYZI exact Phase 110 QEMU probe | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_pointxyzi_public/evidence_doctor.md` |
| row-source direct gather historical board | `test-rvv/registration/transformation_estimation_2D/log/board/row_source_direct_gather_repeated/summary.md` |
| row-source direct gather historical board | `test-rvv/registration/transformation_estimation_2D/log/board/row_source_direct_gather_repeated/evidence_manifest.json` |
| row-source direct gather historical board | `test-rvv/registration/transformation_estimation_2D/log/board/row_source_direct_gather_repeated/evidence_doctor.md` |
| correspondence chunked xyz staging QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_chunked_xyz_staging/run_bench_correspondence_chunked_xyz_staging_rvv.log` |
| correspondence chunked xyz staging QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_chunked_xyz_staging/asm_attribution.md` |
| correspondence chunked xyz staging QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_chunked_xyz_staging/asm_attribution.json` |
| correspondence chunked xyz staging QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_chunked_xyz_staging/evidence_manifest.json` |
| correspondence chunked xyz staging QEMU | `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_chunked_xyz_staging/evidence_doctor.md` |
