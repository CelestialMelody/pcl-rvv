# 优化证据索引

## 本文职责

本文把 `transformation_estimation_2D` 的候选优化方式映射到测试支撑代码、target、证据路径和当前决策。它不替代 phase result，也不把 diagnostic evidence（诊断证据）升级成 production evidence（生产证据）。

## 当前结论摘要

Phase 010/020 建立了顺序点云对（ordered-cloud-pair，source/target 按相同下标一一对应）的 test-only fused 2D correlation candidate。候选从最初的 raw sums（原始和）公式调整为两遍中心化结构：先求 x/y 质心，再直接累加中心化后的 2x2 `H`。这样避免两份 4xN dynamic demean matrix（动态去中心化矩阵）和 Eigen correlation multiply，同时降低 near-cancellation（近抵消）风险。

Phase 050 已完成 production-public probe（生产公开入口探针）。QEMU smoke 和 asm attribution 证明路径可命中；`Milkv-Jupiter` board production-public repeated 为 4K `4.222x`、64K `5.310x`、256K `4.947x`，Doctor `0/0/0`，该 exact PointXYZ 历史窄范围结论保留。Phase 030 随后在 test-rvv 范围完成三类 row-source materialize-to-ordered candidate 的 correctness、QEMU、asm 和板卡 repeated；Phase 090 修正输入语义后已重跑 materialize，并新增 direct gather candidate。Phase 070 完成了 traits-gated generic diagnostic candidate；Phase 080 已把同一 source/target gate 接入真实 public ordered-cloud-pair dispatch，并完成接入后 public QEMU、production-symbol asm 和 5-run board evidence。Phase 091 已完成 source-indexed `PointXYZ -> PointXYZ` production probe：source-indexed public QEMU/board Doctor 均为 `0/0/0`，board 为 `4.103x / 4.818x / 4.575x`；用户已在 2026-08-18 确认采纳该 narrow patch。Phase 092 又补齐同一 source-indexed production boundary 内的 RVV-vs-RVV family A/B：direct public 相对 materialize+ordered public 为 4K `1.089x`、64K `1.044x`、256K `1.037x`，overall `weak_positive`，Doctor `0/0/2`；因此保留当前 direct gather family，但不扩大 production dispatch。Phase 110/112 已完成 source-indexed exact `PointXYZI -> PointXYZI` production-public adoption：QEMU Doctor `0/0/0`，board 20-run 为 `3.979x / 3.547x / 3.602x`，`B/A<1=0/20`，Board Doctor `0/3/0`；该证据只采纳 exact gate，不扩大为泛型 source-indexed production dispatch。Phase 099 已补 source-indexed generic PointXYZ-like diagnostic：correctness Std/RVV 64/64，QEMU Doctor `0/0/0`，board Doctor `5/10/1`，结果 mixed-negative，不支持把 Phase 091 exact gate 直接扩大为泛型 source-indexed production dispatch。Phase 103/104 把 source-indexed generic widening 放到真实 public boundary 后仍是 guarded probe；Phase 106 又用独立 20-run representative variance 复核，结果为 12 positive、1 weak_positive、3 negative，board Doctor `1/27/0`，其中 `PointNormal->PointNormal 256K` 为 `B/A<1=7/20`。因此 full source-indexed generic widening 不能 clean-adopt。Phase 093/107/109 已补 dual-indexed 同一 production boundary 的 RVV-vs-RVV family A/B：Phase 109 direct public 相对 materialize+ordered public 为 4K `1.080x`、64K `1.661x`、256K `1.646x`，overall `positive`，Doctor `0/3/0`；4K `1/20` below-1 作为小规模 caveat 保留，当前 exact dual-indexed dispatch 已保留。Phase 100 已补 dual-indexed generic PointXYZ-like diagnostic：correctness Std/RVV 72/72，QEMU Doctor `0/0/0`，board Doctor `13/17/1`，结果 negative，不支持把 Phase 093 exact gate 直接扩大为泛型 dual-indexed production dispatch。Phase 094/107 已补 correspondence public probe 和同 boundary family A/B：public probe 为 `4.517x / 3.705x / 3.149x`，Doctor `0/2/0`；Phase 107 family A/B 256K 有 `4/20` 低于 1，Doctor `0/4/0`，因此 correspondence production dispatch 已退回。Phase 095 已补 correspondence staged-dual profile：direct/staged D/S 为 `0.833x / 0.729x / 0.848x`，Doctor `3/2/0`，因此 staged-dual 为 profile-only negative，不支持 production dispatch switch。Phase 096 已补 correspondence locality/order profile：identity、reverse、shuffled、strided 的 D/S 均未支持 staged-dual switch，overall `negative`，Doctor `12/5/0`。Phase 097 已补 correspondence component ablation（组件消融）：strided full-anchor D/S 为 `0.834x / 0.761x / 0.841x`，Doctor `3/2/0`，只说明 materialize 写回和 selected xyz 读取成本线索，不支持 production dispatch switch。Phase 098 又补 chunked xyz staging candidate，20-run board D/C 为 `0.813x / 0.941x / 0.983x`，Doctor `3/3/0`，仍不支持 production dispatch switch。Phase 101 已补 correspondence generic PointXYZ-like diagnostic：historical correctness Std/RVV 80/80，QEMU Doctor `0/0/0`，asm `correspondence_generic_candidate_lambda_boundary` 860 RVV lines，board Doctor `11/19/2`，结果 negative，不支持把 Phase 094 guarded exact gate 扩成泛型 production dispatch。当前 Std/RVV correctness 为 84/84；Phase 101 的 80/80 只作为历史快照。adopted 覆盖当前 traits-gated ordered-cloud-pair patch、source-indexed `PointXYZ -> PointXYZ` narrow patch、source-indexed `PointXYZI -> PointXYZI` exact patch 和 dual-indexed `PointXYZ -> PointXYZ` exact patch；Phase 094 / 095 / 096 / 097 / 098 / 099 / 100 / 101 / 103 / 104 / 106 都不能自动写成 adopted。

## 优化方式总表

| 优化方式 | 代码路径 | target | 当前证据 | 决策 |
| --- | --- | --- | --- | --- |
| scalar public baseline | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` | `run_test_compare` | public ordered-cloud-pair、source-indexed、dual-indexed、correspondence valid scalar-boundary、generic fallback、source-indexed generic fallback、dual-indexed generic fallback、correspondence generic fallback、direct-gather fallback、source-indexed / dual-indexed / correspondence public fallback、Phase 092 / 093 / 094 materialize family equivalence、Phase 095 staged-dual equivalence 和 Phase 096 locality/order equivalence 通过；Phase 097 component no-solve 只做 checksum smoke；Std/RVV 84/84。 | current scalar truth |
| two-pass centered fused 2D correlation | `include/impl/te2d_ordered_candidates.hpp` | `run_test_candidates`、`run_board_bench_ordered_cloud_pair_repeated` | diagnostic correctness 通过；near-cancellation 样本通过；diagnostic board 5-run 为 `weak_positive`。 | retained diagnostic |
| production-public fused probe | historical exact narrow patch | `run_qemu_production_public_evidence_doctor`、`run_board_bench_ordered_cloud_pair_public_repeated` | QEMU Doctor 0/0/0；asm public boundary present；最新 board public repeated 为 4.222x / 5.310x / 4.947x，Doctor 0/0/0。 | historical production evidence |
| raw sums fused formula | 无保留实现 | 首次 RVV run 暴露 | near-cancellation 样本曾出现大误差，已改为中心化结构。 | rejected for Phase 010 |
| dense finite gate | `estimateFused2DCandidate` | `run_test_compare` | z 非有限输入触发 fallback，x/y 非有限 public 行为被记录。 | adopted in test-only diagnostic |
| traits-gated generic xyz diagnostic gate | `RVVXYZAoSFloatLayout<PointT>` + `estimateFused2DCandidate<PointSource,PointTarget>` | `run_test_compare`、`run_bench_generic_xyz_point_types_smoke`、`run_board_bench_generic_xyz_point_types_repeated` | source/target 分别审计 xyz datatype、offset、POD、sizeof、alignment、stride；四类 same-type 和四组 mixed pair 通过；diagnostic board Doctor 0/3/2。 | historical evidence-closed / test-only |
| traits-gated generic xyz production dispatch | `tryTransformationEstimation2DOrderedCloudPairRVV<PointSource,PointTarget,Scalar>` | `run_test_compare`、`run_bench_generic_xyz_point_types_public_smoke`、`run_board_bench_generic_xyz_point_types_public_repeated` | 真实 public generic boundary；当前 Std/RVV 84/84；public QEMU 16 cases Doctor 0/0/0；production asm 406 lines；board 15 cases `0/5`，PointNormal->PointNormal 64K `1/5` negative，Doctor 0/6/0。 | adopted-by-user with representative-scope caveat |
| source-indexed narrow production dispatch | `tryTransformationEstimation2DSourceIndexedCloudPairRVV` | `run_test_compare`、`run_bench_source_indexed_public_smoke`、`run_board_bench_source_indexed_public_repeated` | 真实 source-indexed public boundary；current Std/RVV 84/84；public QEMU 3 cases Doctor 0/0/0；production asm 46 lines；board 4.103x / 4.818x / 4.575x，`B/A<1=0/5` each，Doctor 0/0/0。 | adopted-by-user with narrow row-source caveat |
| source-indexed PointXYZI exact production dispatch | `tryTransformationEstimation2DSourceIndexedCloudPairRVV` | `run_test_compare`、`record_qemu_source_indexed_pointxyzi_public_state`、`run_board_bench_source_indexed_pointxyzi_public_phase110_repeated` | 真实 source-indexed public boundary；current Std/RVV 84/84；public QEMU Doctor 0/0/0；production asm focused category 85 RVV lines；20-run board 3.979x / 3.547x / 3.602x，`B/A<1=0/20` each，Doctor 0/3/0。 | adopted-by-user exact gate; no generic widening |
| source-indexed production-detail family A/B | `estimatePublicSourceIndexedMaterializedOrdered2D` vs current public source-indexed RVV | `run_test_compare`、`record_qemu_source_indexed_family_ab_state`、`run_board_bench_source_indexed_family_ab_repeated` | 同一 RVV bench binary 内比较 direct source-indexed public 与 materialize+ordered public；correctness equivalence 通过；QEMU Doctor 0/0/0；asm family wrapper 9 lines；board 1.089x / 1.044x / 1.037x，Doctor 0/0/2。 | kept direct gather family; weak-positive; no production change |
| source-indexed generic PointXYZ-like diagnostic | `estimateFused2DSourceIndexedDirectGatherCandidate<PointSource,PointTarget>` | `run_test_compare`、`record_qemu_source_indexed_generic_state`、`run_board_bench_source_indexed_generic_xyz_point_types_repeated` | 四类 same-type、四组 mixed pair、extra-field ignored 和 fallback 通过；QEMU Doctor 0/0/0；asm `source_indexed_generic_candidate_lambda_boundary` 546 RVV lines；board mixed-negative，Doctor 5/10/1。 | attempted / mixed-negative / no production dispatch change |
| source-indexed generic public variance | production public source-indexed generic guarded probe | `run_test_compare`、`record_qemu_source_indexed_generic_public_variance_state`、`run_board_bench_source_indexed_generic_xyz_point_types_public_variance_repeated` | 真实 public source-indexed generic boundary；QEMU Doctor 0/0/0；asm `production_public_source_indexed_generic_boundary` 343 RVV lines；20-run board 为 12 positive、1 weak_positive、3 negative，Doctor 1/27/0。 | guarded / negative for full widening / no clean adoption |
| dual-indexed production-detail family A/B | `estimatePublicDualIndexedMaterializedOrdered2D` vs current public dual-indexed RVV | `run_test_compare`、`record_qemu_dual_indexed_family_ab_phase109_state`、`run_board_bench_dual_indexed_family_ab_phase109_repeated` | 同一 RVV bench binary 内比较 direct dual-indexed public 与 materialize+ordered public；correctness equivalence 通过；QEMU Doctor 0/0/0；production dual-indexed boundary 50 RVV lines；Phase 109 20-run board 1.080x / 1.661x / 1.646x，Doctor 0/3/0，Warning 全在 4K。 | retained exact dispatch; positive with 4K caveat |
| dual-indexed generic PointXYZ-like diagnostic | `estimateFused2DDualIndexedDirectGatherCandidate<PointSource,PointTarget>` | `run_test_compare`、`record_qemu_dual_indexed_generic_state`、`run_board_bench_dual_indexed_generic_xyz_point_types_repeated` | 四类 same-type、四组 mixed pair、extra-field ignored 和 fallback 通过；QEMU Doctor 0/0/0；asm `dual_indexed_generic_candidate_lambda_boundary` 880 RVV lines；board negative，Doctor 13/17/1。 | attempted / negative / no production dispatch change |
| correspondence production-public probe | `tryTransformationEstimation2DCorrespondencePairRVV` | `run_test_compare`、`record_qemu_correspondence_public_state`、`run_board_bench_correspondence_public_repeated` | 真实 correspondence public boundary；current Std/RVV 84/84；public QEMU 3 cases Doctor 0/0/0；production asm 47 lines；board 4.517x / 3.705x / 3.149x，`B/A<1=0/5` each，Doctor 0/2/0。 | positive public probe; not clean-adopted |
| correspondence production-detail family A/B | `estimatePublicCorrespondenceMaterializedOrdered2D` vs current public correspondence RVV | `run_test_compare`、`record_qemu_correspondence_family_ab_state`、`run_board_bench_correspondence_family_ab_repeated` | 同一 RVV bench binary 内比较 direct correspondence public 与 materialize+ordered public；correctness equivalence 通过；QEMU Doctor 0/0/0；production correspondence boundary historical；Phase 107 20-run board 256K 有 `4/20` 低于 1，Doctor 0/4/0。 | rolled back / not adopted |
| correspondence staged-dual profile | `estimatePublicCorrespondenceStagedDualIndexed2D` vs current public correspondence RVV | `run_test_compare`、`record_qemu_correspondence_staging_profile_state`、`run_board_bench_correspondence_staging_profile_repeated` | staged query/match indices + dual-indexed public RVV 与 direct correspondence public 对拍；current Std/RVV 84/84；QEMU Doctor 0/0/0；dual-indexed production boundary 50 lines、correspondence boundary 47 lines；20-run board D/S 0.833x / 0.729x / 0.848x，Doctor 3/2/0。 | attempted / profile-only / rejected for production switch |
| correspondence locality/order profile | direct / staged-dual / materialize public RVV profiles | `run_test_compare`、`record_qemu_correspondence_locality_order_profile_state`、`run_board_bench_correspondence_locality_order_profile_repeated` | identity、reverse、shuffled、strided query/match 分布下三条 public RVV family 对拍；current Std/RVV 84/84；QEMU Doctor 0/0/0；20-run board D/S identity `0.850x / 0.621x / 0.756x`、reverse `0.851x / 0.690x / 0.759x`、shuffled `0.928x / 0.974x / 0.978x`、strided `0.893x / 0.887x / 0.927x`，Doctor 12/5/0。 | attempted / profile-only / negative / no dispatch switch |
| correspondence generic PointXYZ-like diagnostic | `estimateFused2DCorrespondenceDirectGatherCandidate<PointSource,PointTarget>` | `run_test_compare`、`record_qemu_correspondence_generic_state`、`run_board_bench_correspondence_generic_xyz_point_types_repeated` | 四类 same-type、四组 mixed pair、extra-field ignored 和 fallback 通过；QEMU Doctor 0/0/0；asm `correspondence_generic_candidate_lambda_boundary` 860 RVV lines；board negative，Doctor 11/19/2。 | attempted / negative / no production dispatch change |
| correspondence component ablation | `runCorrespondenceComponentAblationCase` | `record_qemu_correspondence_component_ablation_state`、`run_board_bench_correspondence_component_ablation_repeated` | scan / extract / gather / materialize no-solve 与 prematerialized / direct / staged / materialize full anchors 分开输出；QEMU Doctor 0/0/0；component boundary 14 RVV lines；10-run board full-anchor D/S 为 `0.834x / 0.761x / 0.841x`，Doctor 3/2/0。 | attempted / component-ablation / negative / no dispatch switch |
| materialize-to-ordered fused candidate | source-indexed-cloud-pair | `estimateFused2DSourceIndexedCandidate`、`run_board_bench_row_source_repeated` | 同输入语义刷新；board median 1.096x / 1.019x / 1.033x；Doctor 1/1/6。 | attempted / diagnostic-only |
| materialize-to-ordered fused candidate | dual-indexed-cloud-pair | `estimateFused2DDualIndexedCandidate`、`run_board_bench_row_source_repeated` | 同输入语义刷新；board median 1.085x / 1.050x / 1.042x，64K 有 1/5 退化；Doctor 1/1/6。 | attempted / diagnostic-only |
| materialize-to-ordered fused candidate | correspondence-pair | `estimateFused2DCorrespondenceCandidate`、`run_board_bench_row_source_repeated` | 同输入语义刷新；board median 1.076x / 1.025x / 1.025x，64K 有 2/5 退化；Doctor 1/1/6。 | attempted / diagnostic-only |
| direct gather fused candidate | source-indexed-cloud-pair | `estimateFused2DSourceIndexedDirectGatherCandidate`、`run_board_bench_row_source_direct_gather_repeated` | correctness/fallback pass；QEMU Doctor 0/0/0；asm 361 RVV lines；board 1.199x / 1.135x / 1.130x，`B/A<1=0/5` each。 | production-ready-for-PI1, not adopted |
| direct gather fused candidate | dual-indexed-cloud-pair | `estimateFused2DDualIndexedDirectGatherCandidate`、`run_board_bench_row_source_direct_gather_repeated` | correctness/fallback pass；QEMU Doctor 0/0/0；board 1.073x / 1.023x / 0.994x，64K `2/5`、256K `3/5` 退化。 | attempted / diagnostic-only; guarded probe allowed |
| direct gather fused candidate | correspondence-pair | `estimateFused2DCorrespondenceDirectGatherCandidate`、`run_board_bench_row_source_direct_gather_repeated` | correctness/fallback pass；QEMU Doctor 0/0/0；board 1.101x / 0.965x / 0.922x，三个规模均触发退化 Error。 | attempted / diagnostic-only; guarded probe allowed |
| production dispatch | current traits-gated production patch | Phase 080 result | PI5 证据支持保留当前 production patch；用户已确认采纳。 | adopted-by-user |

## 标量路径与 RVV 路径差异

| 阶段 | 当前 production | test-only candidate / probe |
| --- | --- | --- |
| centroid | `compute3DCentroid(ConstCloudIterator&)`，逐点 `pcl::isFinite`。 | dense finite gate 通过后，顺序扫描 x/y 两遍求质心。 |
| demean | 写出 source / target 两份 4xN dynamic matrix。 | 不写出矩阵，第二遍直接累加中心化后的 `H00/H01/H10/H11`。 |
| correlation | Eigen matrix multiply。 | RVV 构建用 `vlsseg3e32` load、`vfsub` 中心化、`vfmacc` 累加、`vfredosum` 规约。 |
| solve | `atan2`、`cos/sin` 和 translation 标量计算。 | 保持标量计算。 |
| fallback | 不满足 production gate 时继续当前公开入口标量路径。 | test-only candidate 和 production patch 均保留 fallback。 |

## 代码级证据索引

| 代码 | 证据角色 | 说明 |
| --- | --- | --- |
| `estimatePublic2D` | public semantic anchor（公开语义锚点） | 调用真实 `TransformationEstimation2D`。 |
| `estimateFused2DStd` | same-chain scalar reference（同构标量参考链路） | 两遍中心化累加，不使用 RVV。 |
| `include/te2d.h` | stable test support aggregator（稳定测试支撑聚合入口） | Phase 114 后测试和 bench 继续只 include 该入口，内部实现按职责拆分。 |
| `estimateFused2DCandidate` | test-only RVV candidate | RVV 构建且 dense finite、规模不少于 16 时尝试 RVV。 |
| `estimateFused2DSourceIndexedDirectGatherCandidate` | test-only row-source RVV candidate | source 侧按 index 生成 byte offset 并 gather x/y/z，target 顺序 strided load；不物化整点云。 |
| `tryTransformationEstimation2DSourceIndexedCloudPairRVV` | adopted narrow production dispatch | Phase 091 真实 source-indexed public overload helper；只覆盖 `PointXYZ -> PointXYZ`、`Scalar=float`、valid indices、dense finite、size >= 16。 |
| `estimatePublicSourceIndexedMaterializedOrdered2D` | production-detail A/B helper | materialize selected source rows，再调用真实 ordered-cloud-pair public overload；Phase 092 用于与 direct source-indexed public RVV 做 family selection。 |
| `tryTransformationEstimation2DDualIndexedCloudPairRVV` | retained exact production dispatch | Phase 107/109 真实 dual-indexed public overload helper；只覆盖 `PointXYZ -> PointXYZ`、`Scalar=float`、valid source/target indices、dense finite、size >= 16。 |
| `estimatePublicDualIndexedMaterializedOrdered2D` | production-detail A/B helper | 物化 selected source/target rows 后调用真实 ordered-cloud-pair public overload；Phase 093 用于与 direct dual-indexed public RVV 做 family selection。 |
| `tryTransformationEstimation2DCorrespondencePairRVV` | pending-user guarded production candidate | Phase 094 真实 correspondence public overload helper；只覆盖 `PointXYZ -> PointXYZ`、`Scalar=float`、valid query/match correspondences、dense finite、size >= 16。 |
| `estimatePublicCorrespondenceMaterializedOrdered2D` | production-detail A/B helper | 物化 selected query/match rows 后调用真实 ordered-cloud-pair public overload；Phase 094 用于与 direct correspondence public RVV 做 family selection。 |
| `estimatePublicCorrespondenceStagedDualIndexed2D` | production-detail staging/profile helper | 将 query/match 拷成连续 source/target indices 后调用真实 dual-indexed public overload；Phase 095 用于判断 staged-dual 是否能替换 direct correspondence public。 |
| `runCorrespondenceComponentAblationCase` | bench-local component ablation helper | 输出 scan / extract / gather / materialize no-solve 和四类 full anchor label；Phase 097 只用作 bottleneck profile，不作为 production adoption。 |
| `estimateFused2DDualIndexedDirectGatherCandidate` | test-only row-source RVV candidate | source / target 两侧都按 index stream gather x/y/z；valid-index-only。 |
| `estimateFused2DCorrespondenceDirectGatherCandidate` | test-only row-source RVV candidate | 从 correspondence query / match 读取两条 index stream，再分别 gather source / target。 |
| `accumulateFused2DRVV` | RVV hotspot candidate | 两遍 RVV：第一遍求质心，第二遍求中心化 2x2 correlation。 |
| `matrixChecksum` | bench smoke checksum | 检查输出路径和矩阵明显变化。 |

## 细粒度 target 字典

| target | 证明点 | 输出 |
| --- | --- | --- |
| `run_test_compare` | Std/RVV correctness 全量。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`。 |
| `run_test_public_semantics` | 公开入口语义子集。 | 覆盖 `TransformationEstimation2D.Public*`。 |
| `run_test_candidates` | fused candidate 子集。 | 覆盖 `TransformationEstimation2D.Fused*`。 |
| `run_bench_ordered_cloud_pair_smoke` | RVV diagnostic bench 可运行和日志形状。 | `log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log`。 |
| `run_bench_ordered_cloud_pair_public_smoke` | RVV public probe bench 可运行和日志形状。 | `log/qemu/production_public/run_bench_ordered_cloud_pair_public_rvv.log`。 |
| `run_bench_source_indexed_public_smoke` / `record_qemu_source_indexed_public_state` | RVV source-indexed public probe bench、asm、manifest 和 Doctor。 | `log/qemu/source_indexed_public/`；只证明真实 public path 和证据形状。 |
| `run_bench_source_indexed_family_ab_smoke` / `record_qemu_source_indexed_family_ab_state` | RVV source-indexed family A/B bench、asm、manifest 和 Doctor。 | `log/qemu/source_indexed_family_ab/`；只证明同边界 A/B 的路径和证据形状。 |
| `run_bench_source_indexed_generic_xyz_point_types_smoke` / `record_qemu_source_indexed_generic_state` | RVV source-indexed generic point-type bench、asm、manifest 和 Doctor。 | `log/qemu/source_indexed_generic_xyz_point_types/`；只证明 test-rvv generic row-source 路径和证据形状。 |
| `run_bench_source_indexed_generic_xyz_point_types_public_variance_smoke` / `record_qemu_source_indexed_generic_public_variance_state` | RVV source-indexed generic public variance bench、asm、manifest 和 Doctor。 | `log/qemu/source_indexed_generic_xyz_point_types_public_variance/`；只证明 Phase 106 public variance 路径和证据形状。 |
| `run_bench_dual_indexed_family_ab_smoke` / `record_qemu_dual_indexed_family_ab_state` | RVV dual-indexed family A/B bench、asm、manifest 和 Doctor。 | `log/qemu/dual_indexed_family_ab/`；只证明同边界 A/B 的路径和证据形状。 |
| `run_bench_dual_indexed_generic_xyz_point_types_smoke` / `record_qemu_dual_indexed_generic_state` | RVV dual-indexed generic point-type bench、asm、manifest 和 Doctor。 | `log/qemu/dual_indexed_generic_xyz_point_types/`；只证明 test-rvv generic row-source 路径和证据形状。 |
| `run_bench_correspondence_generic_xyz_point_types_smoke` / `record_qemu_correspondence_generic_state` | RVV correspondence generic point-type bench、asm、manifest 和 Doctor。 | `log/qemu/correspondence_generic_xyz_point_types/`；只证明 test-rvv generic row-source 路径和证据形状。 |
| `run_bench_correspondence_public_smoke` / `record_qemu_correspondence_public_state` | RVV correspondence public probe bench、asm、manifest 和 Doctor。 | `log/qemu/correspondence_public/`；只证明真实 public path 和证据形状。 |
| `run_bench_correspondence_family_ab_smoke` / `record_qemu_correspondence_family_ab_state` | RVV correspondence family A/B bench、asm、manifest 和 Doctor。 | `log/qemu/correspondence_family_ab/`；只证明同边界 A/B 的路径和证据形状。 |
| `run_bench_correspondence_staging_profile_smoke` / `record_qemu_correspondence_staging_profile_state` | RVV correspondence staging/profile bench、asm、manifest 和 Doctor。 | `log/qemu/correspondence_staging_profile/`；只证明 direct / staged-dual / materialize 三组路径和证据形状。 |
| `run_bench_correspondence_locality_order_profile_smoke` / `record_qemu_correspondence_locality_order_profile_state` | RVV correspondence locality/order profile bench、asm、manifest 和 Doctor。 | `log/qemu/correspondence_locality_order_profile/`；只证明 identity / reverse / shuffled / strided profile 的路径和证据形状。 |
| `run_bench_correspondence_component_ablation_smoke` / `record_qemu_correspondence_component_ablation_state` | RVV correspondence component ablation bench、asm、manifest 和 Doctor。 | `log/qemu/correspondence_component_ablation/`；只证明 component label、checksum、asm boundary 和证据形状。 |
| `generate_asm_attribution_summary` | 反汇编归因摘要，区分 candidate、production scalar、Eigen / stdlib 和 bench 边界。 | `log/qemu/asm_attribution.md`、`log/qemu/asm_attribution.json`。 |
| `run_qemu_production_public_evidence_doctor` | production-public QEMU manifest 和 Evidence Doctor。 | `log/qemu/production_public/evidence_manifest.json`、`log/qemu/production_public/evidence_doctor.md`。 |
| `run_board_bench_ordered_cloud_pair_repeated` | 5-run diagnostic board repeated summary 和 Evidence Doctor。 | `log/board/ordered_cloud_pair_repeated/summary.md`、manifest、doctor。 |
| `run_board_bench_ordered_cloud_pair_public_repeated` | 5-run production-public board repeated summary 和 Evidence Doctor。 | `log/board/ordered_cloud_pair_public_repeated/summary.md`、manifest、doctor。 |
| `run_board_bench_source_indexed_public_repeated` / `record_board_source_indexed_public_state` | 5-run source-indexed production-public board summary 和 Evidence Doctor。 | `log/board/source_indexed_public_repeated/`；Phase 091 adopted narrow production evidence。 |
| `run_board_bench_source_indexed_family_ab_repeated` / `record_board_source_indexed_family_ab_state` | 5-run source-indexed direct-vs-materialize family A/B board summary 和 Evidence Doctor。 | `log/board/source_indexed_family_ab_repeated/`；Phase 092 weak-positive production-detail evidence。 |
| `run_board_bench_source_indexed_generic_xyz_point_types_repeated` / `record_board_source_indexed_generic_xyz_point_types_state` | 5-run source-indexed generic point-type board summary 和 Evidence Doctor。 | `log/board/source_indexed_generic_xyz_point_types_repeated/`；Phase 099 mixed-negative diagnostic evidence，不接 production。 |
| `run_board_bench_source_indexed_generic_xyz_point_types_public_variance_repeated` / `record_board_source_indexed_generic_xyz_point_types_public_variance_state` | 20-run source-indexed generic public representative variance board summary 和 Evidence Doctor。 | `log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/`；Phase 106 negative for full widening，不 clean-adopt。 |
| `run_board_bench_source_indexed_pointxyzi_public_phase110_repeated` / `record_board_source_indexed_pointxyzi_public_state` | 20-run source-indexed exact `PointXYZI -> PointXYZI` public board summary 和 Evidence Doctor。 | `log/board/source_indexed_pointxyzi_public_phase110_repeated/`；Phase 110/112 adopted exact production evidence，不接 generic widening。 |
| `run_board_bench_dual_indexed_family_ab_phase109_repeated` / `record_board_dual_indexed_family_ab_phase109_state` | 20-run dual-indexed direct-vs-materialize family A/B board summary 和 Evidence Doctor。 | `log/board/dual_indexed_family_ab_phase109_variance_repeated/`；Phase 109 retained exact production evidence，保留 4K caveat。 |
| `run_board_bench_dual_indexed_generic_xyz_point_types_repeated` / `record_board_dual_indexed_generic_xyz_point_types_state` | 5-run dual-indexed generic point-type board summary 和 Evidence Doctor。 | `log/board/dual_indexed_generic_xyz_point_types_repeated/`；Phase 100 negative diagnostic evidence，不接 production。 |
| `run_board_bench_correspondence_generic_xyz_point_types_repeated` / `record_board_correspondence_generic_xyz_point_types_state` | 5-run correspondence generic point-type board summary 和 Evidence Doctor。 | `log/board/correspondence_generic_xyz_point_types_repeated/`；Phase 101 negative diagnostic evidence，不接 production。 |
| `run_board_bench_correspondence_public_repeated` / `record_board_correspondence_public_state` | 5-run correspondence production-public board summary 和 Evidence Doctor。 | `log/board/correspondence_public_repeated/`；Phase 094 public probe positive。 |
| `run_board_bench_correspondence_family_ab_repeated` / `record_board_correspondence_family_ab_state` | correspondence direct-vs-materialize family A/B board summary 和 Evidence Doctor。 | `log/board/correspondence_family_ab_repeated/`；Phase 094 20-run 结果显示 256K unstable / negative。 |
| `run_board_bench_correspondence_staging_profile_repeated` / `record_board_correspondence_staging_profile_state` | correspondence direct-vs-staged-dual/materialize profile board summary 和 Evidence Doctor。 | `log/board/correspondence_staging_profile_repeated/`；Phase 095 20-run 结果显示 staged-dual negative。 |
| `run_board_bench_correspondence_locality_order_profile_repeated` / `record_board_correspondence_locality_order_profile_state` | correspondence locality/order profile board summary 和 Evidence Doctor。 | `log/board/correspondence_locality_order_profile_repeated/`；Phase 096 20-run 结果显示 staged-dual negative。 |
| `run_board_bench_correspondence_component_ablation_repeated` / `record_board_correspondence_component_ablation_state` | correspondence component ablation board summary 和 Evidence Doctor。 | `log/board/correspondence_component_ablation_repeated/`；Phase 097 10-run 结果显示 full-anchor negative，只保留 bottleneck 线索。 |
| `run_bench_row_source_smoke` / `record_qemu_row_source_state` | 3 类 row source × 3 个规模的 QEMU smoke、asm、manifest 和 Doctor。 | `log/qemu/row_source/` 下的 summary-only 证据。 |
| `run_board_bench_row_source_repeated` / `record_board_row_source_state` | 5-run row-source materialize board repeated summary 和 Evidence Doctor。 | 已按 Phase 090 同输入语义刷新；Doctor 1/1/6，用作 direct gather / staging 对照。 |
| `run_bench_row_source_direct_gather_smoke` / `record_qemu_row_source_direct_gather_state` | 3 类 row source direct gather × 3 个规模的 QEMU smoke、asm、manifest 和 Doctor。 | `log/qemu/row_source_direct_gather/`；QEMU 只证明路径和日志形状。 |
| `run_board_bench_row_source_direct_gather_repeated` / `record_board_row_source_direct_gather_state` | 5-run direct gather board repeated summary 和 Evidence Doctor。 | `log/board/row_source_direct_gather_repeated/`；source-indexed 可进入 PI1，dual/correspondence 降级。 |
| `run_bench_generic_xyz_point_types_smoke` / `record_qemu_generic_state` | 四类 same-type、四组 mixed pair 的 16-case QEMU smoke、generic asm、manifest 和 Doctor。 | `log/qemu/generic_xyz_point_types/`；只证明 test-only candidate 路径。 |
| `run_board_bench_generic_xyz_point_types_repeated` / `record_board_generic_xyz_point_types_state` | 5-run generic point-type diagnostic board repeated summary 和 Evidence Doctor。 | `log/board/generic_xyz_point_types_repeated/`；按 point type/size 分桶，支持历史 PI1 诊断。 |
| `run_bench_generic_xyz_point_types_public_smoke` / `record_qemu_generic_public_state` | 16-case generic production-public QEMU smoke、asm、manifest 和 Doctor。 | `log/qemu/generic_xyz_point_types_public/`；只证明真实 public path 和证据形状。 |
| `run_board_bench_generic_xyz_point_types_public_repeated` / `record_board_generic_xyz_point_types_public_state` | 5-run generic production-public board summary 和 Evidence Doctor。 | `log/board/generic_xyz_point_types_public_repeated/`；15 个 case 正向，PointNormal->PointNormal 64K 独立 negative bucket。 |
| `evidence_status` | registry freshness。 | 检查本地生成的 registry 和文档引用。 |

## 当前可提交证据

可提交候选：`Makefile`、`board.mk`、`include/**`、`src/**`、`script/**`、`doc/**`、`README.zh.md`。

默认不提交：`build/**`、`log/**`。本阶段文档引用这些路径作为本地 evidence pointers（证据指针），不是提交白名单。

## 结论边界

当前只有 ordered-cloud-pair traits-gated production patch 和 Phase 091 source-indexed
`PointXYZ -> PointXYZ` narrow patch 被采纳；其它 row source 不继承：

- `log/qemu/production_public/asm_attribution.md` 显示 public overload 或 `runPublicCase` 内联边界有 `vlsseg3e32.v`、`vfmacc` 和 `vfredosum`。
- `log/board/ordered_cloud_pair_public_repeated/summary.md` 显示 public 4K / 64K / 256K median 分别为 `4.222x`、`5.310x`、`4.947x`，均为 `positive`。
- `log/board/ordered_cloud_pair_public_repeated/evidence_doctor.md` 为 Errors=0、Warnings=0、Suggestions=0。

因此当前 traits-gated ordered-cloud-pair production patch 保留并已获用户确认采纳；是否提交仍需
用户另行授权。Phase 080 已建立独立 production evidence chain，但 board 的
`PointNormal -> PointNormal 64K` case 为 negative，不能用 15 个 positive case 覆盖。
Phase 091 已补 source-indexed `PointXYZ -> PointXYZ` production-public probe：QEMU/board
Doctor 均为 `0/0/0`，board 三个规模均为 positive；用户已确认采纳该窄范围 patch。Phase 092
同边界 family A/B 显示 direct public RVV 相对 materialize+ordered public RVV 为弱正向，
因此当前 direct gather family 保留，但不作为 dual-indexed / correspondence 或 generic row-source 结论。
Phase 110/112 已补 source-indexed exact `PointXYZI -> PointXYZI` production-public path：
QEMU Doctor `0/0/0`，board 为 `3.979x / 3.547x / 3.602x`，`B/A<1=0/20`，Doctor
`0/3/0`。该证据支持 exact gate adoption，但不能扩大到泛型点型或 Normal 类。
Phase 093/107/109 已补 dual-indexed `PointXYZ -> PointXYZ` production-detail family A/B：QEMU
Doctor `0/0/0`，Phase 109 board 为 `1.080x / 1.661x / 1.646x`，overall `positive`，
Doctor `0/3/0`。该证据支持 exact narrow dual-indexed direct family，保留 4K caveat，
但不能扩大到泛型点型或 correspondence。
Phase 094 已补 correspondence `PointXYZ -> PointXYZ` public probe 和 production-detail
family A/B：public probe positive，但 family A/B 在 256K 有 5/20 低于 1 并触发 Doctor
Error。该证据支持保留 guarded candidate，但不能 clean-adopt，也不能扩大到泛型点型。
Phase 095 继续补 staged-dual profile，但 direct/staged D/S 为 `0.833x / 0.729x / 0.848x`
且 Doctor `3/2/0`，因此 staged-dual 不能作为 production dispatch switch。Phase 096
locality/order profile 进一步显示四类 query/match 分布都不支持 staged-dual switch；Phase 097
component ablation 的 full-anchor D/S 为 `0.834x / 0.761x / 0.841x`，Doctor `3/2/0`，
只保留 materialize 写回和 gather 成本线索。Phase 098 又完成 chunked xyz staging candidate，
20-run board D/C 为 `0.813x / 0.941x / 0.983x`，Doctor `3/3/0`，仍不支持
correspondence production dispatch switch，也不作为生产接入或回滚依据。
未逐类型上板的自定义点型、RGB/RGBA 和 `Scalar=double` 仍保持未覆盖生产结论。Phase 090
已补三类 row-source direct gather 诊断：source-indexed 三个规模无退化且已进入 Phase 091；
dual-indexed 通过 Phase 093 在真实 production-detail 边界重新证据化；correspondence 通过
Phase 094 证明 public path 正向，但 Phase 095 / 096 / 097 / 098 未支持替代路线。Phase 099
已完成 source-indexed generic point-type diagnostic，结论为 mixed-negative；Phase 103/104
source-indexed generic public probe 仍是 guarded，Phase 106 20-run representative variance
为 negative for full widening，不能 clean-adopt；Phase 100
已完成 dual-indexed generic point-type diagnostic，结论为 negative；Phase 101
已完成 correspondence generic point-type diagnostic，结论为 negative。默认恢复动作转为
Phase 093 / Phase 094 用户检查点，或另开有界生产探针 phase；任何新生产接入都必须重新补
接入后的 correctness、性能、asm 和 Evidence Doctor。
