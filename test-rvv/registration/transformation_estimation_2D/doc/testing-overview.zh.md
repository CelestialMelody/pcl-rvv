# 测试体系总览

## 本文职责

本文说明 `transformation_estimation_2D` topic 当前 test-rvv 资产如何分层，以及每类证据能证明什么、不能证明什么。当前 production 源码保留 traits-gated ordered-cloud-pair RVV dispatch、Phase 091 用户已采纳的 source-indexed `PointXYZ -> PointXYZ` narrow production dispatch、Phase 112 用户已采纳的 source-indexed exact `PointXYZI -> PointXYZI` dispatch，以及 Phase 107/109 保留的 dual-indexed `PointXYZ -> PointXYZ` exact dispatch。Phase 099 已完成 source-indexed generic PointXYZ-like diagnostic，结果为 mixed-negative，不修改 production dispatch。Phase 103/104 又把 source-indexed generic widening 放到真实 public boundary 做 guarded probe：16-case public board 为 15/16 positive，`PointNormal->PointNormal 256K` 单独复跑为 positive 但仍有 variance Warning；Phase 106 用独立 20-run representative variance 复核后为 12 positive、1 weak_positive、3 negative，board Doctor `1/27/0`，因此 source-indexed generic widening 仍不能 clean-adopt。Phase 100 已完成 dual-indexed generic PointXYZ-like diagnostic，结果为 negative，同样不修改 production dispatch。Phase 101 已完成 correspondence generic PointXYZ-like diagnostic，结果为 negative，同样不修改 production dispatch。Phase 094 correspondence public probe positive 但 Phase 107 family A/B negative，production dispatch 已退回；Phase 095 staged-dual profile、Phase 096 locality/order profile、Phase 097 component ablation 和 Phase 098 chunked staging 均为 negative。Phase 050 的 exact PointXYZ production-public probe、Phase 030/090 的三类 row-source test-only 诊断和 Phase 080/099/100/101/103/104/106 的 generic evidence 分开记录。

## 文档阅读路径

| 问题 | 文档 |
| --- | --- |
| 函数级数据流和生产边界 | `doc/transformation_estimation_2D-evaluation.zh.md` |
| 每个 gtest 覆盖什么 | `doc/correctness-tests.zh.md` |
| bench label、QEMU 和 board 证据边界 | `doc/benchmark-and-evidence.zh.md` |
| 候选族到证据的映射 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码怎么定位 | `doc/test-support-code-map.zh.md` |

## 测试类型定义

| 类型 | 当前状态 | 证明范围 | 不能证明 |
| --- | --- | --- | --- |
| public semantics（公开入口语义） | `run_test_compare` 已覆盖 84 个 gtest | ordered-cloud-pair、source-indexed-cloud-pair、dual-indexed-cloud-pair 和 correspondence-pair 的当前边界；size mismatch、非有限 x/y/z、generic fallback、source-indexed / dual-indexed / correspondence production probe fallback、direct-gather fallback、source/dual/correspondence family equivalence、staged-dual equivalence、locality/order equivalence、source-indexed / dual-indexed / correspondence generic fallback boundary、Phase 103 source-indexed generic public fallback boundary。 | 非法 index / correspondence 的安全行为；production RVV 收益。 |
| numerical consistency（数值一致性） | `run_test_compare` 已覆盖 84 个 gtest | 两遍中心化 fused 2D correlation candidate 与标量参考链路一致；ordered、source-indexed、dual-indexed 和 correspondence generic 四类 same-type、四组 mixed pair、extra fields 和 fallback 通过；三类 row-source materialize 与 direct gather candidate 对拍通过；source-indexed / dual-indexed / correspondence public probe 与 double scalar reference 对齐；Phase 092 / 093 / 094 direct public 与 materialize+ordered public 对齐；Phase 095 staged-dual 和 Phase 096 locality/order 三路 public family 对齐；Phase 103 source-indexed generic public same/mixed pair 与 fallback 对齐。 | 目标硬件性能；未逐类型上板的自定义点型适用性；source-indexed generic clean adoption。 |
| QEMU path / log shape（QEMU 路径 / 日志形状） | diagnostic、production-public、row-source materialize/direct 和 generic smoke 均已运行 | RVV bench binary 可运行，能输出 label、iterations、warmup 和 checksum；manifest / doctor 可解析。 | 真实性能。 |
| asm attribution（反汇编归因） | diagnostic、production-public、row-source materialize/direct 和 generic summary 均已生成 | 能区分 candidate lambda、row-source wrapper、production scalar、Eigen / stdlib、public overload 或 `runPublicCase` 内联边界。 | 指令存在不证明 board 性能。 |
| board performance（板卡性能） | ordered / production-public / source-indexed public / source-indexed PointXYZI exact / source-indexed family A/B / source-indexed generic diagnostic / source-indexed generic public probe / source-indexed generic public variance / dual-indexed family A/B / dual-indexed generic / correspondence public/family/staging/locality/component/chunked/generic / row-source materialize/direct / generic diagnostic / generic public 均有 repeated | Phase 050 exact public 为 `4.222x / 5.310x / 4.947x`，Doctor 0/0/0；Phase 091 source-indexed public 为 `4.103x / 4.818x / 4.575x`，Doctor 0/0/0；Phase 110/112 source-indexed PointXYZI exact 为 `3.979x / 3.547x / 3.602x`，`B/A<1=0/20`，Doctor 0/3/0；Phase 092 source-indexed family A/B 为 `1.089x / 1.044x / 1.037x`，Doctor 0/0/2；Phase 099 source-indexed generic diagnostic 为 mixed-negative，Doctor 5/10/1；Phase 103 source-indexed generic public probe 为 15/16 positive，Doctor 1/11/0；Phase 104 复查 `PointNormal->PointNormal 256K` 为 median `2.879x`、`B/A<1=0/5`、Doctor 0/1/0；Phase 106 source-indexed generic public variance 为 12 positive、1 weak_positive、3 negative，Doctor 1/27/0；Phase 109 dual-indexed family A/B 为 `1.080x / 1.661x / 1.646x`，Doctor 0/3/0；Phase 100 dual-indexed generic 为 negative，Doctor 13/17/1；Phase 101 correspondence generic 为 negative，Doctor 11/19/2；Phase 107 correspondence family A/B 为 256K `4/20` below-1，Doctor 0/4/0；Phase 095 staged-dual D/S 为 `0.833x / 0.729x / 0.848x`；Phase 096 locality/order overall negative；Phase 097 component ablation 和 Phase 098 chunked staging 均 negative；generic public 16 cases 中 15 个 `0/5`，PointNormal->PointNormal 64K 为 `1/5`，Doctor 0/6/0。 | negative bucket、长尾和未记录环境不能被总体 median 覆盖；QEMU timing 不能替代板卡；Phase 103/104/106 guarded evidence 不自动等于 adopted。 |
| production direct（真实生产路径证据） | traits-gated generic patch adopted-by-user；source-indexed `PointXYZ` narrow patch adopted-by-user；source-indexed `PointXYZI` exact patch adopted-by-user；source-indexed generic public widening guarded / not clean-adoptable as full representative widening；dual-indexed narrow patch retained with 4K caveat | Phase 080 public generic correctness、QEMU、production-symbol asm、board repeated 和 fallback correctness 已闭合；Phase 091 source-indexed public correctness、QEMU、asm、board 和 fallback 已闭合并由用户确认采纳；Phase 110/112 source-indexed PointXYZI exact correctness、QEMU、asm、20-run board 和 Doctor 已闭合并由用户确认采纳；Phase 103/104 source-indexed generic public probe 已闭合但仍有 variance Warning，Phase 106 20-run representative variance 为 negative，不能写成 adopted；Phase 109 dual-indexed 20-run variance 支持保留 exact dispatch 和 4K caveat。 | ordered generic 只覆盖代表性四类点型和四组 mixed；source-indexed exact 只覆盖 `PointXYZ -> PointXYZ` 与 `PointXYZI -> PointXYZI`；source-indexed generic widening 仍是 guarded 且 full representative variance 不支持 clean adoption；dual-indexed 只覆盖 exact `PointXYZ -> PointXYZ`；correspondence、未逐类型上板自定义点型、RGB/RGBA、double 仍标量或待独立阶段。 |

## 运行入口分类

| target | 后端 | 主证据角色 | 输出 / 说明 |
| --- | --- | --- | --- |
| `run_test_compare` | QEMU / configured runner | correctness | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`；当前 Std/RVV 均 84/84。 |
| `run_test_public_semantics` | QEMU / configured runner | public semantics | gtest filter 为 `TransformationEstimation2D.Public*`。 |
| `run_test_candidates` | QEMU / configured runner | candidate correctness | gtest filter 为 `TransformationEstimation2D.Fused*`。 |
| `run_bench_ordered_cloud_pair_smoke` | QEMU smoke only | bench log shape | `log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log`；不比较 Std/RVV timing。 |
| `run_bench_ordered_cloud_pair_public_smoke` | QEMU smoke only | production-public probe log shape | `log/qemu/production_public/run_bench_ordered_cloud_pair_public_rvv.log`；不写性能结论。 |
| `run_bench_source_indexed_public_smoke` | QEMU smoke only | source-indexed production-public probe log shape | `log/qemu/source_indexed_public/run_bench_source_indexed_public_rvv.log`；不写性能结论。 |
| `run_bench_source_indexed_family_ab_smoke` | QEMU smoke only | source-indexed production-detail family A/B log shape | `log/qemu/source_indexed_family_ab/run_bench_source_indexed_family_ab_rvv.log`；不写性能结论。 |
| `run_bench_source_indexed_generic_xyz_point_types_smoke` | QEMU smoke only | source-indexed generic point-type diagnostic log shape | `log/qemu/source_indexed_generic_xyz_point_types/run_bench_source_indexed_generic_xyz_point_types_rvv.log`；16 cases；不写性能结论。 |
| `run_bench_source_indexed_generic_xyz_point_types_public_variance_smoke` | QEMU smoke only | source-indexed generic public variance log shape | `log/qemu/source_indexed_generic_xyz_point_types_public_variance/run_bench_source_indexed_generic_xyz_point_types_public_variance_rvv.log`；16 cases；不写性能结论。 |
| `run_bench_source_indexed_pointxyzi_public_smoke` | QEMU smoke only | source-indexed exact `PointXYZI -> PointXYZI` adopted public path log shape | `log/qemu/source_indexed_pointxyzi_public/run_bench_source_indexed_pointxyzi_public_rvv.log`；3 cases；不写性能结论。 |
| `run_bench_dual_indexed_generic_xyz_point_types_smoke` | QEMU smoke only | dual-indexed generic point-type diagnostic log shape | `log/qemu/dual_indexed_generic_xyz_point_types/run_bench_dual_indexed_generic_xyz_point_types_rvv.log`；16 cases；不写性能结论。 |
| `run_bench_correspondence_generic_xyz_point_types_smoke` | QEMU smoke only | correspondence generic point-type diagnostic log shape | `log/qemu/correspondence_generic_xyz_point_types/run_bench_correspondence_generic_xyz_point_types_rvv.log`；16 cases；不写性能结论。 |
| `run_bench_dual_indexed_family_ab_smoke` | QEMU smoke only | dual-indexed production-detail family A/B log shape | `log/qemu/dual_indexed_family_ab/run_bench_dual_indexed_family_ab_rvv.log`；不写性能结论。 |
| `run_bench_correspondence_public_smoke` | QEMU smoke only | correspondence production-public log shape | `log/qemu/correspondence_public/run_bench_correspondence_public_rvv.log`；不写性能结论。 |
| `run_bench_correspondence_family_ab_smoke` | QEMU smoke only | correspondence production-detail family A/B log shape | `log/qemu/correspondence_family_ab/run_bench_correspondence_family_ab_rvv.log`；不写性能结论。 |
| `run_bench_correspondence_staging_profile_smoke` | QEMU smoke only | correspondence staged-dual profile log shape | `log/qemu/correspondence_staging_profile/run_bench_correspondence_staging_profile_rvv.log`；不写性能结论。 |
| `run_bench_correspondence_locality_order_profile_smoke` | QEMU smoke only | correspondence locality/order profile log shape | `log/qemu/correspondence_locality_order_profile/run_bench_correspondence_locality_order_profile_rvv.log`；36 comparisons；不写性能结论。 |
| `run_bench_row_source_smoke` | QEMU smoke only | 三类 row-source candidate 的日志形状 | `log/qemu/row_source/run_bench_row_source_fused_rvv.log`；9 个 case；不写性能结论。 |
| `run_bench_row_source_direct_gather_smoke` | QEMU smoke only | 三类 row-source direct gather candidate 的日志形状 | `log/qemu/row_source_direct_gather/run_bench_row_source_direct_gather_rvv.log`；9 个 case；不写性能结论。 |
| `run_bench_generic_xyz_point_types_smoke` | QEMU smoke only | 四类代表性点型和四组 mixed generic candidate 的日志形状 | `log/qemu/generic_xyz_point_types/run_bench_generic_xyz_point_types_rvv.log`；16 个 case；不写性能结论。 |
| `run_bench_generic_xyz_point_types_public_smoke` | QEMU smoke only | 真实 production generic public dispatch 的日志形状 | `log/qemu/generic_xyz_point_types_public/run_bench_generic_xyz_point_types_public_rvv.log`；16 个 case；不写性能结论。 |
| `generate_asm_attribution_summary` | 编译 + objdump | diagnostic asm attribution | `log/qemu/asm_attribution.md`、`log/qemu/asm_attribution.json`。 |
| `run_qemu_production_public_evidence_doctor` | QEMU + host Python | production-public QEMU contract | `log/qemu/production_public/evidence_manifest.json`、`log/qemu/production_public/evidence_doctor.md`。 |
| `run_qemu_source_indexed_public_evidence_doctor` | QEMU + host Python | source-indexed production-public QEMU contract | `log/qemu/source_indexed_public/evidence_manifest.json`、`log/qemu/source_indexed_public/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_source_indexed_family_ab_evidence_doctor` | QEMU + host Python | source-indexed production-detail family A/B QEMU contract | `log/qemu/source_indexed_family_ab/evidence_manifest.json`、`log/qemu/source_indexed_family_ab/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_source_indexed_generic_evidence_doctor` | QEMU + host Python | source-indexed generic point-type QEMU contract | `log/qemu/source_indexed_generic_xyz_point_types/evidence_manifest.json`、`log/qemu/source_indexed_generic_xyz_point_types/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_source_indexed_generic_public_variance_evidence_doctor` | QEMU + host Python | source-indexed generic public variance QEMU contract | `log/qemu/source_indexed_generic_xyz_point_types_public_variance/evidence_manifest.json`、`log/qemu/source_indexed_generic_xyz_point_types_public_variance/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_dual_indexed_generic_evidence_doctor` | QEMU + host Python | dual-indexed generic point-type QEMU contract | `log/qemu/dual_indexed_generic_xyz_point_types/evidence_manifest.json`、`log/qemu/dual_indexed_generic_xyz_point_types/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_correspondence_generic_evidence_doctor` | QEMU + host Python | correspondence generic point-type QEMU contract | `log/qemu/correspondence_generic_xyz_point_types/evidence_manifest.json`、`log/qemu/correspondence_generic_xyz_point_types/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_dual_indexed_family_ab_evidence_doctor` | QEMU + host Python | dual-indexed production-detail family A/B QEMU contract | `log/qemu/dual_indexed_family_ab/evidence_manifest.json`、`log/qemu/dual_indexed_family_ab/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_correspondence_public_evidence_doctor` | QEMU + host Python | correspondence production-public QEMU contract | `log/qemu/correspondence_public/evidence_manifest.json`、`log/qemu/correspondence_public/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_correspondence_family_ab_evidence_doctor` | QEMU + host Python | correspondence production-detail family A/B QEMU contract | `log/qemu/correspondence_family_ab/evidence_manifest.json`、`log/qemu/correspondence_family_ab/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_correspondence_staging_profile_evidence_doctor` | QEMU + host Python | correspondence staged-dual profile QEMU contract | `log/qemu/correspondence_staging_profile/evidence_manifest.json`、`log/qemu/correspondence_staging_profile/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_correspondence_locality_order_profile_evidence_doctor` | QEMU + host Python | correspondence locality/order profile QEMU contract | `log/qemu/correspondence_locality_order_profile/evidence_manifest.json`、`log/qemu/correspondence_locality_order_profile/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_correspondence_chunked_xyz_staging_evidence_doctor` | QEMU + host Python | correspondence chunked xyz staging QEMU contract | `log/qemu/correspondence_chunked_xyz_staging/evidence_manifest.json`、`log/qemu/correspondence_chunked_xyz_staging/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_row_source_evidence_doctor` | QEMU + host Python | row-source QEMU contract | `log/qemu/row_source/evidence_manifest.json`、`log/qemu/row_source/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_row_source_direct_gather_evidence_doctor` | QEMU + host Python | row-source direct gather QEMU contract | `log/qemu/row_source_direct_gather/evidence_manifest.json`、`log/qemu/row_source_direct_gather/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_generic_evidence_doctor` | QEMU + host Python | generic point-type QEMU contract | `log/qemu/generic_xyz_point_types/evidence_manifest.json`、`log/qemu/generic_xyz_point_types/evidence_doctor.md`；当前 0/0/0。 |
| `run_qemu_generic_public_evidence_doctor` | QEMU + host Python | generic production-public QEMU contract | `log/qemu/generic_xyz_point_types_public/evidence_manifest.json`、`log/qemu/generic_xyz_point_types_public/evidence_doctor.md`；当前 0/0/0。 |
| `run_board_bench_ordered_cloud_pair_repeated` | board / target hardware | board repeated diagnostic | `log/board/ordered_cloud_pair_repeated/summary.md`、manifest、doctor。 |
| `run_board_bench_ordered_cloud_pair_public_repeated` | board / target hardware | production-public board repeated probe | `log/board/ordered_cloud_pair_public_repeated/summary.md`、manifest、doctor。 |
| `run_board_bench_source_indexed_public_repeated` | board / target hardware | source-indexed production-public board repeated probe | `log/board/source_indexed_public_repeated/summary.md`、manifest、doctor；5 runs × 20 iterations × 5 warmup，Doctor 0/0/0，已采纳为 narrow production evidence。 |
| `run_board_bench_source_indexed_family_ab_repeated` | board / target hardware | source-indexed production-detail family A/B board repeated probe | `log/board/source_indexed_family_ab_repeated/summary.md`、manifest、doctor；5 runs × 20 iterations × 5 warmup，Doctor 0/0/2，weak-positive 保留 direct gather。 |
| `run_board_bench_source_indexed_generic_xyz_point_types_repeated` | board / target hardware | source-indexed generic point-type diagnostic repeated | `log/board/source_indexed_generic_xyz_point_types_repeated/summary.md`、manifest、doctor；5 runs × 20 iterations × 5 warmup，Doctor 5/10/1，mixed-negative，不接 production。 |
| `run_board_bench_source_indexed_generic_xyz_point_types_public_variance_repeated` | board / target hardware | source-indexed generic public representative variance repeated | `log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/summary.md`、manifest、doctor；20 runs × 20 iterations × 5 warmup，12 positive、1 weak_positive、3 negative，Doctor 1/27/0，不支持 clean-adopt。 |
| `run_board_bench_dual_indexed_generic_xyz_point_types_repeated` | board / target hardware | dual-indexed generic point-type diagnostic repeated | `log/board/dual_indexed_generic_xyz_point_types_repeated/summary.md`、manifest、doctor；5 runs × 20 iterations × 5 warmup，Doctor 13/17/1，negative，不接 production。 |
| `run_board_bench_correspondence_generic_xyz_point_types_repeated` | board / target hardware | correspondence generic point-type diagnostic repeated | `log/board/correspondence_generic_xyz_point_types_repeated/summary.md`、manifest、doctor；5 runs × 20 iterations × 5 warmup，Doctor 11/19/2，negative，不接 production。 |
| `run_board_bench_dual_indexed_family_ab_phase109_repeated` | board / target hardware | dual-indexed production-detail family A/B board repeated probe | `log/board/dual_indexed_family_ab_phase109_variance_repeated/summary.md`、manifest、doctor；20 runs × 20 iterations × 5 warmup，Doctor 0/3/0，64K / 256K positive，4K 保留 caveat。 |
| `run_board_bench_correspondence_public_repeated` | board / target hardware | correspondence production-public board repeated probe | `log/board/correspondence_public_repeated/summary.md`、manifest、doctor；5 runs，Doctor 0/2/0，public positive。 |
| `run_board_bench_correspondence_family_ab_repeated` | board / target hardware | correspondence production-detail family A/B board repeated probe | `log/board/correspondence_family_ab_repeated/summary.md`、manifest、doctor；20 runs，Doctor 1/3/0，guarded candidate。 |
| `run_board_bench_correspondence_staging_profile_repeated` | board / target hardware | correspondence staged-dual profile board repeated | `log/board/correspondence_staging_profile_repeated/summary.md`、manifest、doctor；20 runs，Doctor 3/2/0，staged-dual negative。 |
| `run_board_bench_correspondence_locality_order_profile_repeated` | board / target hardware | correspondence locality/order profile board repeated | `log/board/correspondence_locality_order_profile_repeated/summary.md`、manifest、doctor；20 runs，Doctor 12/5/0，overall negative。 |
| `run_board_bench_correspondence_chunked_xyz_staging_repeated` | board / target hardware | correspondence chunked xyz staging board repeated | `log/board/correspondence_chunked_xyz_staging_repeated/summary.md`、manifest、doctor；20 runs，Doctor 3/3/0，chunked staging negative。 |
| `run_board_bench_row_source_repeated` | board / target hardware | row-source materialize diagnostic repeated | `log/board/row_source_fused_repeated/summary.md`、manifest、doctor；已按同输入语义刷新，Doctor 1/1/6。 |
| `run_board_bench_row_source_direct_gather_repeated` | board / target hardware | row-source direct gather diagnostic repeated | `log/board/row_source_direct_gather_repeated/summary.md`、manifest、doctor；source-indexed 可进 PI1，dual/correspondence 降级，Doctor 5/6/1。 |
| `run_board_bench_generic_xyz_point_types_repeated` | board / target hardware | generic point-type diagnostic repeated | `log/board/generic_xyz_point_types_repeated/summary.md`、manifest、doctor；5 runs × 20 iterations × 5 warmup，16 cases，Doctor 0/3/2。 |
| `run_board_bench_generic_xyz_point_types_public_repeated` | board / target hardware | generic point-type production-public repeated | `log/board/generic_xyz_point_types_public_repeated/summary.md`、manifest、doctor；5 runs × 20 iterations × 5 warmup，16 cases，Doctor 0/6/0，含一个 negative bucket。 |
| `record_qemu_row_source_state` | host Python | row-source evidence registry update | 登记 row-source QEMU log、asm、manifest 和 Doctor。 |
| `record_qemu_source_indexed_public_state` / `record_board_source_indexed_public_state` | host Python | source-indexed production-public registry update | 登记 Phase 091 QEMU/board source-indexed public summary、manifest 和 Doctor。 |
| `record_qemu_source_indexed_family_ab_state` / `record_board_source_indexed_family_ab_state` | host Python | source-indexed family A/B registry update | 登记 Phase 092 QEMU/board source-indexed family A/B summary、manifest 和 Doctor。 |
| `record_qemu_source_indexed_generic_state` / `record_board_source_indexed_generic_xyz_point_types_state` | host Python | source-indexed generic point-type registry update | 登记 Phase 099 QEMU/board source-indexed generic summary、manifest 和 Doctor。 |
| `record_qemu_dual_indexed_family_ab_phase109_state` / `record_board_dual_indexed_family_ab_phase109_state` | host Python | dual-indexed family A/B registry update | 登记 Phase 109 QEMU/board dual-indexed family A/B summary、manifest 和 Doctor。 |
| `record_qemu_correspondence_public_state` / `record_board_correspondence_public_state` | host Python | correspondence production-public registry update | 登记 Phase 094 QEMU/board correspondence public summary、manifest 和 Doctor。 |
| `record_qemu_correspondence_family_ab_state` / `record_board_correspondence_family_ab_state` | host Python | correspondence family A/B registry update | 登记 Phase 094 QEMU/board correspondence family A/B summary、manifest 和 Doctor。 |
| `record_qemu_correspondence_staging_profile_state` / `record_board_correspondence_staging_profile_state` | host Python | correspondence staging/profile registry update | 登记 Phase 095 QEMU/board staged-dual profile summary、manifest 和 Doctor。 |
| `record_qemu_correspondence_locality_order_profile_state` / `record_board_correspondence_locality_order_profile_state` | host Python | correspondence locality/order registry update | 登记 Phase 096 QEMU/board locality/order profile summary、manifest 和 Doctor。 |
| `record_qemu_correspondence_chunked_xyz_staging_state` / `record_board_correspondence_chunked_xyz_staging_state` | host Python | correspondence chunked staging registry update | 登记 Phase 098 QEMU/board chunked xyz staging summary、manifest 和 Doctor。 |
| `record_qemu_row_source_direct_gather_state` / `record_board_row_source_direct_gather_state` | host Python | row-source direct gather registry update | 登记 direct gather QEMU/board summary、manifest 和 Doctor。 |
| `record_qemu_generic_state` / `record_board_generic_xyz_point_types_state` | host Python | generic evidence registry update | 登记 generic QEMU / board summary、manifest 和 Doctor。 |
| `evidence_status` | host Python | registry freshness | 检查 `log/evidence_registry.json` 与文档引用。 |

## Target 粒度审计

| target 类别 | 当前状态 | 决策 |
| --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `run_test_compare` 覆盖 Std/RVV 84/84。 | adopted |
| correctness aliases（正确性细分入口） | `run_test_public_semantics` 和 `run_test_candidates` 可分组运行。 | adopted |
| bench diagnostic aliases（bench 诊断入口） | `ordered-cloud-pair-fused`、`ordered-cloud-pair-public`、`source-indexed-public`、`source-indexed-family-ab`、`source-indexed-generic-xyz-point-types`、`dual-indexed-family-ab`、`dual-indexed-generic-xyz-point-types`、`correspondence-generic-xyz-point-types`、`correspondence-public`、`correspondence-family-ab`、`correspondence-staging-profile`、`correspondence-locality-order-profile`、`correspondence-chunked-xyz-staging`、`row-source-fused`、`row-source-direct-gather` 和 `generic-xyz-point-types` 可区分候选族、production-public probe、production-detail family A/B、profile、三类 row source 两种实现族和 generic point-type candidate。 | adopted |
| QEMU smoke aliases（QEMU 小型验证入口） | diagnostic、production-public、row-source materialize/direct 和 generic smoke 都有独立 target。 | adopted |
| board smoke aliases（板卡小型验证入口） | repeated target 生成主证据；单次 compare 只作为 smoke。 | adopted |
| board repeated aliases（板卡重复采集入口） | diagnostic repeated、production-public repeated、row-source materialize/direct repeated、ordered generic repeated、source-indexed PointXYZI exact repeated、source-indexed generic repeated、dual-indexed generic repeated 和 correspondence generic repeated 均有 target，且本轮均已运行。 | adopted |
| doctor / registry aliases（证据体检和登记入口） | QEMU、board 和 row-source registry target 已接入。 | adopted |

## 输入数据总览

| corpus | helper | 作用 |
| --- | --- | --- |
| deterministic ordered pair | `makePointXYZCloud` + `transformCloud2D` | 稳定覆盖常规 2D 刚体变换。 |
| near-cancellation | `makeNearCancellationCloud` | 保护较大公共偏移下的中心化 correlation 数值预算。 |
| valid indexed / correspondence scalar boundary | `makePrefixIndices` / `makePrefixCorrespondences` | 证明非 ordered public overload 保持当前标量 row pairing。 |
| strided indexed / correspondence direct gather | `makeStridedIndices` / `makeStridedCorrespondences` | 证明 direct gather 不混用 source / target 两条 index stream。 |
| locality/order correspondence profiles | `makePrefixIndices` / `makeReverseIndices` / `makeShuffledIndices` / `makeStridedIndices` | Phase 096 identity、reverse、shuffled、strided query/match 分布；证明三条 public RVV family 输出一致后再解释 profile 性能。 |
| size mismatch | public tests 内构造 | 验证公开入口在数量不匹配时保持输出矩阵不变。 |
| non-finite x/y/z | public tests 内构造 | 刻画 centroid finite check 与 demean 全量写出的既有组合语义。 |
| generic PointXYZ-like corpus | `makeXYZLikeCloud<PointT>` + `transformCloud2DTo<Source,Target>` / indexed-pair helper | 覆盖 ordered、source-indexed 和 dual-indexed generic 四类 same-type、四组 mixed pair、额外字段、source/target 独立 stride 以及 selected-row fallback。 |

## 覆盖矩阵

| scope | point type / Scalar | row source policy | correctness | QEMU smoke | board | decision |
| --- | --- | --- | --- | --- | --- | --- |
| public scalar baseline | `PointXYZ` / `float` | ordered-cloud-pair | pass | not_applicable | not_applicable | current scalar truth |
| fused 2D correlation diagnostic | dense finite `PointXYZ` / `float` | ordered-cloud-pair | pass | pass / log shape only | 5-run `weak_positive` | retained diagnostic |
| production-public candidate | exact `PointXYZ` / `float` | ordered-cloud-pair | pass | pass / log shape only | 5-run `4.222x / 5.310x / 4.947x`, Doctor 0/0/0 | historical production evidence |
| source-indexed production-public candidate | exact `PointXYZ` / `float` | source indexed | public hit + fallback pass | pass / 3 cases | 5-run `4.103x / 4.818x / 4.575x`, Doctor 0/0/0 | adopted-by-user with narrow row-source caveat |
| source-indexed family A/B | exact `PointXYZ` / `float` | source indexed | direct public and materialize+ordered public pass | pass / 6 comparisons | 5-run `1.089x / 1.044x / 1.037x`, Doctor 0/0/2 | direct family kept; weak-positive; no production change |
| source-indexed PointXYZI exact production-public | exact `PointXYZI` / `float` | source indexed | public hit + fallback corpus pass | pass / 3 cases | 20-run `3.979x / 3.547x / 3.602x`, Doctor 0/3/0 | adopted-by-user exact gate; no generic widening |
| source-indexed generic PointXYZ-like diagnostic | four representative point types / `float` | source indexed | same-type + mixed + fallback pass | pass / 16 cases | 5-run mixed-negative；Doctor 5/10/1 | attempted / no production dispatch |
| source-indexed generic public variance | four representative point types / `float` | source indexed | same-type + mixed + fallback pass | pass / 16 cases | 20-run 12 positive、1 weak_positive、3 negative；Doctor 1/27/0 | guarded / no clean adoption |
| dual-indexed family A/B | exact `PointXYZ` / `float` | dual indexed | direct public and materialize+ordered public pass | pass / 6 comparisons | Phase 109 20-run `1.080x / 1.661x / 1.646x`, Doctor 0/3/0 | retained exact production dispatch; positive with 4K caveat |
| dual-indexed generic PointXYZ-like diagnostic | four representative point types / `float` | dual indexed | same-type + mixed + fallback pass | pass / 16 cases | 5-run negative；Doctor 13/17/1 | attempted / no production dispatch |
| correspondence public probe | exact `PointXYZ` / `float` | correspondences | public hit + fallback pass | pass / 3 comparisons | 5-run `4.517x / 3.705x / 3.149x`, Doctor 0/2/0 | public positive; not clean-adopted |
| correspondence family A/B | exact `PointXYZ` / `float` | correspondences | direct public and materialize+ordered public pass | pass / 6 comparisons | Phase 107 20-run 256K `4/20` below-1, Doctor 0/4/0 | rolled back / not adopted |
| correspondence staged-dual profile | exact `PointXYZ` / `float` | correspondences | direct public and staged-dual public pass | pass / 9 comparisons | 20-run D/S `0.833x / 0.729x / 0.848x`, Doctor 3/2/0 | attempted / profile-only negative |
| correspondence locality/order profile | exact `PointXYZ` / `float` | correspondences | direct / staged-dual / materialize public pass under four profiles | pass / 36 comparisons | 20-run overall negative, Doctor 12/5/0 | attempted / profile-only negative |
| source-indexed materialize | `PointXYZ` / `float` | source indexed | public + candidate pass | pass / 3 cases | 5-run 1.096x / 1.019x / 1.033x; Doctor 1/1/6 | attempted / diagnostic-only |
| dual-indexed materialize | `PointXYZ` / `float` | dual indexed | public + candidate pass | pass / 3 cases | 5-run 1.085x / 1.050x / 1.042x; Doctor 1/1/6 | attempted / diagnostic-only |
| correspondence materialize | `PointXYZ` / `float` | correspondences | public + candidate pass | pass / 3 cases | 5-run 1.076x / 1.025x / 1.025x; Doctor 1/1/6 | attempted / diagnostic-only |
| source-indexed direct gather | `PointXYZ` / `float` | source indexed | public + direct candidate + fallback pass | pass / 3 cases | 5-run 1.199x / 1.135x / 1.130x; no case `B/A<1` | production-ready-for-PI1, not adopted |
| dual-indexed direct gather | `PointXYZ` / `float` | dual indexed | public + direct candidate + fallback pass | pass / 3 cases | 5-run 1.073x / 1.023x / 0.994x; Doctor Error at 64K/256K | attempted / diagnostic-only |
| correspondence direct gather | `PointXYZ` / `float` | correspondences | public + direct candidate + fallback pass | pass / 3 cases | 5-run 1.101x / 0.965x / 0.922x; Doctor Error all sizes | attempted / diagnostic-only |
| generic PointXYZ-like diagnostic | four representative point types / `float` | ordered-cloud-pair | same-type + mixed + fallback pass | pass / 16 cases | 5-run Milkv-Jupiter；Doctor 0/3/2，按 case 分桶 | historical evidence-closed / test-only |
| generic PointXYZ-like production public | four representative point types / `float` | ordered-cloud-pair | public same-type + mixed + fallback pass | pass / 16 cases | 5-run Milkv-Jupiter；15 cases `B/A<1=0/5`，PointNormal->PointNormal 64K `1/5`，Doctor 0/6/0 | adopted-by-user with representative-scope caveat |

## 当前可提交证据

当前可提交候选是源码 scaffold、Makefile、board.mk、topic-local script 和 topic-local 文档。`log/`、`build/` 与 `log/evidence_registry.json` 是本地生成的证据状态，默认不提交。

## 当前结论边界

Phase 050 当前形成的是 exact `PointXYZ -> PointXYZ` 历史窄范围 production evidence。
Phase 080 已把 PCL traits-gated generic path 接入真实 public ordered-cloud-pair dispatch，
并完成接入后的 correctness、QEMU、production-symbol asm、board repeated 和 Doctor；用户已
确认当前 patch 可以采纳，但 `PointNormal -> PointNormal 64K` 为 negative bucket，不能被其它
正向 case 覆盖。Phase 090 已把 source-indexed、dual-indexed 和 correspondence row source
的 materialize-to-ordered 与 direct gather 分开登记。Phase 091 已把 source-indexed
`PointXYZ -> PointXYZ` 接入 narrow production dispatch 并由用户确认采纳；Phase 092 已补
同边界 family A/B，弱正向支持保留 direct gather。Phase 110/112 已把 source-indexed exact
`PointXYZI -> PointXYZI` positive probe 采纳为 exact production dispatch。Phase 093/107/109 已补
dual-indexed `PointXYZ -> PointXYZ` 同边界 family A/B，positive with 4K caveat 支持保留当前
direct gather exact dispatch。correspondence 当前已有 public positive probe、family A/B negative、staged-dual negative、
locality/order negative、component ablation negative 和 chunked staging negative 证据；它只能
保留为 rolled-back / not adopted 结论。Phase 099 已完成
source-indexed generic point-type diagnostic，结果为 mixed-negative，因此 091 的 exact
production gate 不扩大为泛型 source-indexed dispatch。Phase 100 已完成 dual-indexed
generic point-type diagnostic，结果为 negative，因此 Phase 093 的 exact production gate
也不扩大为泛型 dual-indexed dispatch。Phase 101 已完成 correspondence generic
point-type diagnostic，结果为 negative，因此 Phase 094 的 guarded exact candidate
也不扩大为泛型 correspondence dispatch。当前默认恢复入口转为 topic-only commit closeout，
或另开一个明确有界的新候选；不再把 generic point-type expansion 当作
未完成项。
