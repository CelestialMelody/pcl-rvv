# 测试支撑代码地图

## 本文职责

本文说明 `transformation_estimation_2D` 的 test support（测试支撑代码）如何按 `artifact_layout` 和 `test_support` 配置组织，帮助 reviewer 从文档定位到源码、target 和本地证据。

## 总调用图

```text
src/test_te2d.cpp
  -> include/te2d.h
    -> include/impl/te2d_candidates.hpp
      -> estimatePublic2D()
      -> estimatePublicSourceIndexed2D()
	      -> estimatePublicSourceIndexedMaterializedOrdered2D()
	      -> estimatePublicDualIndexed2D()
	      -> estimatePublicDualIndexedMaterializedOrdered2D()
	      -> estimatePublicCorrespondence2D()
	      -> estimatePublicCorrespondenceMaterializedOrdered2D()
	      -> estimatePublicCorrespondenceStagedDualIndexed2D()
	      -> estimateFused2DStd()
	      -> estimateFused2DCandidate()
	      -> estimateFused2DSourceIndexedCandidate()
	      -> estimateFused2DDualIndexedCandidate()
	      -> estimateFused2DCorrespondenceCandidate()
	      -> estimateFused2DSourceIndexedDirectGatherCandidate()
	      -> estimateFused2DDualIndexedDirectGatherCandidate()
	      -> estimateFused2DCorrespondenceDirectGatherCandidate()
	      -> estimateFused2DCandidate<PointSource,PointTarget>()
	      -> estimateFused2DSourceIndexedDirectGatherCandidate<PointSource,PointTarget>()
	      -> estimateFused2DDualIndexedDirectGatherCandidate<PointSource,PointTarget>()
	      -> estimateFused2DCorrespondenceDirectGatherCandidate<PointSource,PointTarget>()

src/bench_te2d.cpp
  -> include/te2d.h
	    -> ordered-cloud-pair public case
		    -> source-indexed public case
		    -> source-indexed family A/B case
		    -> dual-indexed family A/B case
		    -> correspondence public case
		    -> correspondence family A/B case
			    -> correspondence staging/profile case
			    -> correspondence locality/order profile case
			    -> correspondence component ablation case
		    -> ordered-cloud-pair fused candidate case
	    -> row-source materialize-to-ordered candidate cases
	    -> row-source direct gather candidate cases
	    -> generic point-type candidate / public cases
	    -> source-indexed generic point-type candidate cases
	    -> dual-indexed generic point-type candidate cases
	    -> correspondence generic point-type candidate cases
```

## 稳定聚合入口

| 文件 | 作用 | 边界 |
| --- | --- | --- |
| `include/te2d.h` | 测试和 bench 的唯一稳定 include 入口。 | 不暴露给 production，不承载候选实现正文。 |

## Fixtures 与输入构造

| helper | 作用 | 证据角色 |
| --- | --- | --- |
| `makePointXYZCloud` | 构造 deterministic dense `PointXYZ` corpus。 | correctness / bench input。 |
| `makeNearCancellationCloud` | 构造大公共偏移和小扰动输入。 | 数值压力样本。 |
| `makeRigid2DTransform` | 固定 2D rotation + translation。 | public semantic anchor。 |
| `transformCloud2D` | 用固定矩阵生成 target。 | 输入构造。 |
| `makeXYZLikeCloud<PointT>` / `transformCloud2DTo<Source,Target>` | 构造四类代表性 PointXYZ-like 点型和 source/target mixed pair；额外字段保持非零有限。 | generic traits gate、AoS stride/offset 和额外字段 correctness。 |
| `makeStridedIndices` / `makeStridedCorrespondences` | 构造 source 与 target 不同 stride / offset 的合法索引流。 | direct gather source/target stream 分离 correctness。 |
| `makeReverseIndices` / `makeShuffledIndices` | 构造逆序和固定伪随机 query/match index stream。 | Phase 096 locality/order profile；用于解释访问顺序和局部性，不改变 row pairing 语义。 |

## 标量 Reference

| helper | 作用 | 证据角色 |
| --- | --- | --- |
| `estimatePublic2D` | 调用当前 production public overload。 | scalar truth / public semantics anchor。 |
| `estimatePublicSourceIndexed2D` | 调用当前 production source-indexed public overload。 | Phase 091 source-indexed public probe / fallback anchor。 |
| `estimatePublicSourceIndexedMaterializedOrdered2D` | 将 selected source rows 物化为 ordered source cloud，再调用当前 production ordered-cloud-pair public overload。 | Phase 092 source-indexed direct-vs-materialize family A/B anchor。 |
| `estimatePublicDualIndexed2D` | 调用当前 production dual-indexed public overload。 | Phase 093 dual-indexed public probe / fallback anchor。 |
| `estimatePublicDualIndexedMaterializedOrdered2D` | 将 selected source/target rows 物化为 ordered source/target cloud，再调用当前 production ordered-cloud-pair public overload。 | Phase 093 dual-indexed direct-vs-materialize family A/B anchor。 |
| `estimatePublicCorrespondence2D` | 调用当前 production correspondence public overload。 | Phase 094 correspondence public probe / fallback anchor。 |
| `estimatePublicCorrespondenceMaterializedOrdered2D` | 将 correspondence query/match rows 物化为 ordered source/target cloud，再调用当前 production ordered-cloud-pair public overload。 | Phase 094 correspondence direct-vs-materialize family A/B anchor。 |
| `estimatePublicCorrespondenceStagedDualIndexed2D` | 将 correspondence query/match 拷成连续 source/target indices，再调用当前 production dual-indexed public overload。 | Phase 095 correspondence staged-dual profile anchor；结果为 negative，不接 production。 |
| `runCorrespondenceComponentAblationCase` | bench-local component driver，输出 scan、extract、gather、materialize no-solve 和 full public anchor label。 | Phase 097 component ablation；只提供 bottleneck 线索，不作为 production adoption。 |
| `estimateFused2DStd` | 标量两遍中心化 fused reference。 | RVV candidate 的 same-chain reference。 |
| `solveTransform2DFromAccumulation` | 从 2D 质心和中心化 `H` 写出 4x4 transform。 | 公式 reference。 |

## Candidate / Diagnostic Helper

| helper | 作用 | 证据角色 |
| --- | --- | --- |
| `estimateFused2DCandidate` | RVV 构建下尝试 dense finite ordered-cloud-pair candidate；其它情况 fallback。 | diagnostic candidate。 |
| `RVVXYZAoSFloatLayout<PointT>` | 使用 PCL traits 分别检查 x/y/z datatype、offset、POD、sizeof、standard-layout 和 alignment。 | generic source/target layout gate；不等于 production dispatch。 |
| `estimateFused2DSourceIndexedCandidate` | 读取 source indices，将 source row materialize 成顺序点云对，再复用 fused candidate；展开成本计入 bench。 | source-indexed row-source diagnostic；不实现 production gather。 |
| `estimateFused2DDualIndexedCandidate` | 读取 source / target 两侧 indices，物化成顺序点云对，再复用 fused candidate；展开成本计入 bench。 | dual-indexed row-source diagnostic；不实现 production gather。 |
| `estimateFused2DCorrespondenceCandidate` | 读取 correspondence 的 query / match，物化成顺序点云对，再复用 fused candidate；展开成本计入 bench。 | correspondence row-source diagnostic；不实现 production gather。 |
| `estimateFused2DSourceIndexedDirectGatherCandidate` | source 侧按 index 生成 byte offset 并 gather x/y/z，target 侧顺序 strided load；不物化整点云；模板形态可分别使用 source/target PointXYZ-like traits gate。 | Phase 090 source-indexed direct gather diagnostic；Phase 099 source-indexed generic diagnostic；不是 production。 |
| `estimateFused2DDualIndexedDirectGatherCandidate` | source / target 两侧分别按 index stream gather x/y/z；模板形态可分别使用 source/target PointXYZ-like traits gate。 | Phase 090 dual-indexed direct gather diagnostic；Phase 100 dual-indexed generic diagnostic；不是 production。 |
| `estimateFused2DCorrespondenceDirectGatherCandidate` | 从 correspondence 读取 query / match index stream，再 gather source/target x/y/z；模板形态可分别使用 source/target PointXYZ-like traits gate。 | Phase 090 correspondence direct gather diagnostic；Phase 101 correspondence generic diagnostic；不是 production。 |
| `accumulateFused2DRVV` | 第一遍 RVV 求 x/y 质心，第二遍 RVV 累加中心化 2x2 correlation。 | asm attribution input。 |
| `isDenseFiniteOrderedPair` | 检查 source/target 数量、dense 标记和 `pcl::isFinite`。 | test-only gate，避免改变非有限 production 语义。 |
| `countFiniteIndexedRows` / `countFiniteCorrespondence*Rows` | 对 selected row 而非整云计数。 | Phase 090 fallback gate；避免只信 `is_dense` 而漏掉被索引选中的非有限点。 |

## Bench Harness 与 Case Registry

| 文件 | case-filter | 说明 |
| --- | --- | --- |
| `src/bench_te2d.cpp` | `ordered-cloud-pair-public` | 真实 public overload probe；当前对应 traits-gated production dispatch。 |
| `src/bench_te2d.cpp` | `source-indexed-public` | 真实 source-indexed public overload probe；Phase 091 adopted narrow production dispatch。 |
| `src/bench_te2d.cpp` | `source-indexed-family-ab` | 同一 RVV bench binary 内比较 source-indexed direct public 与 materialize+ordered public；Phase 092 production-detail family selection。 |
| `src/bench_te2d.cpp` | `source-indexed-generic-xyz-point-types` | 四类 source-indexed generic same-type 各 4K/64K/256K，加四组 64K mixed pair；Phase 099 diagnostic，mixed-negative，不接 production。 |
| `src/bench_te2d.cpp` | `dual-indexed-family-ab` | 同一 RVV bench binary 内比较 dual-indexed direct public 与 materialize+ordered public；Phase 093 production-detail family selection。 |
| `src/bench_te2d.cpp` | `dual-indexed-generic-xyz-point-types` | 四类 dual-indexed generic same-type 各 4K/64K/256K，加四组 64K mixed pair；Phase 100 diagnostic，negative，不接 production。 |
| `src/bench_te2d.cpp` | `correspondence-generic-xyz-point-types` | 四类 correspondence generic same-type 各 4K/64K/256K，加四组 64K mixed pair；Phase 101 diagnostic，negative，不接 production。 |
| `src/bench_te2d.cpp` | `correspondence-public` | 真实 correspondence public overload probe；Phase 094 production-public evidence，当前 public Std/RVV positive。 |
| `src/bench_te2d.cpp` | `correspondence-family-ab` | 同一 RVV bench binary 内比较 correspondence direct public 与 materialize+ordered public；Phase 094 production-detail family selection，当前 256K 退化频率阻止 clean-adopt。 |
| `src/bench_te2d.cpp` | `correspondence-staging-profile` | 同一 RVV bench binary 内比较 correspondence direct public、staged-dual public 和 materialize+ordered public；Phase 095 staging/profile，当前 staged-dual negative。 |
| `src/bench_te2d.cpp` | `correspondence-locality-order-profile` | 按 identity、reverse、shuffled、strided query/match 分布比较 direct、staged-dual 和 materialize 三条 public RVV 路径；Phase 096 locality/order profile，overall negative。 |
| `src/bench_te2d.cpp` | `correspondence-component-ablation` | 在 strided query/match 分布下拆分 scan/extract/gather/materialize no-solve，并输出 direct/staged/materialize/prematerialized full anchors；Phase 097 profile-only negative。 |
| `src/bench_te2d.cpp` | `ordered-cloud-pair-fused` | test-only fused candidate。 |
| `src/bench_te2d.cpp` | `row-source-fused` | source-indexed、dual-indexed、correspondence 三类 materialize-to-ordered candidate，各覆盖 4K/64K/256K。 |
| `src/bench_te2d.cpp` | `row-source-direct-gather` | source-indexed、dual-indexed、correspondence 三类 direct gather candidate，各覆盖 4K/64K/256K。 |
| `src/bench_te2d.cpp` | `generic-xyz-point-types` | 四类 same-type 各 4K/64K/256K，加四组 64K mixed pair；generic cloud 只在该 filter 下构造。 |
| `src/bench_te2d.cpp` | `generic-xyz-point-types-public` | 真实 production public generic dispatch 的同一 16-case representative registry；generic cloud 只在该 filter 下构造。 |

## Scripts 与 Evidence Output

| 路径 | 作用 | 提交边界 |
| --- | --- | --- |
| `script/generate_te2d_asm_summary.py` | 解析 Std / RVV objdump，生成 diagnostic 和 production-public asm attribution summary。 | topic-local script；可提交。 |
| `script/generate_te2d_qemu_evidence_manifest.py` | 把 QEMU smoke log 和 asm summary 转成 Evidence Doctor manifest。 | topic-local script；可提交。 |
| `script/generate_te2d_board_repeated_summary.py` | 解析 board repeated `run-*` 日志，生成 diagnostic、production-public、source-indexed generic、dual-indexed generic 或 correspondence generic summary 和 manifest。 | topic-local script；可提交。 |
| `script/generate_te2d_source_indexed_family_ab_summary.py` | 解析 source-indexed family A/B repeated logs，生成 direct-vs-materialize summary 和 manifest。 | topic-local script；可提交。 |
| `script/generate_te2d_dual_indexed_family_ab_summary.py` | 解析 dual-indexed family A/B repeated logs，生成 direct-vs-materialize summary 和 manifest。 | topic-local script；可提交。 |
| `script/generate_te2d_correspondence_staging_profile_summary.py` | 解析 correspondence staging/profile repeated logs，生成 direct-vs-staged / materialize-vs-staged summary 和 manifest。 | topic-local script；可提交。 |
| `script/generate_te2d_correspondence_locality_order_profile_summary.py` | 解析 correspondence locality/order repeated logs，按 profile 生成 direct-vs-staged / materialize comparison summary 和 manifest。 | topic-local script；可提交。 |
| `script/generate_te2d_correspondence_component_ablation_summary.py` | 解析 correspondence component ablation repeated logs，生成 component ratios、full-anchor summary 和 manifest。 | topic-local script；可提交。 |
| `log/qemu/run_test_std.log` | Std correctness 输出。 | 本地生成；默认不提交。 |
| `log/qemu/run_test_rvv.log` | RVV correctness 输出。 | 本地生成；默认不提交。 |
| `log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log` | QEMU diagnostic bench smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/asm_attribution.md` / `.json` | diagnostic asm attribution 摘要。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/evidence_manifest.json` / `evidence_doctor.md` | diagnostic QEMU Evidence Doctor manifest / report。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/production_public/run_bench_ordered_cloud_pair_public_rvv.log` | QEMU production-public smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/production_public/asm_attribution.md` / `.json` | production-public asm attribution 摘要。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/production_public/evidence_manifest.json` / `evidence_doctor.md` | production-public QEMU Evidence Doctor manifest / report。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/source_indexed_public/run_bench_source_indexed_public_rvv.log` | source-indexed public QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/source_indexed_public/asm_attribution.md` / `.json` | source-indexed production-public asm attribution；聚焦 `production_public_source_indexed_boundary`，46 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/source_indexed_public/evidence_manifest.json` / `evidence_doctor.md` | source-indexed production-public QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/source_indexed_family_ab/run_bench_source_indexed_family_ab_rvv.log` | source-indexed family A/B QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/source_indexed_family_ab/asm_attribution.md` / `.json` | source-indexed family A/B asm attribution；聚焦 `source_indexed_family_ab_lambda_boundary`，9 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/source_indexed_family_ab/evidence_manifest.json` / `evidence_doctor.md` | source-indexed family A/B QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/source_indexed_generic_xyz_point_types/run_bench_source_indexed_generic_xyz_point_types_rvv.log` | source-indexed generic point-type QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/source_indexed_generic_xyz_point_types/asm_attribution.md` / `.json` | source-indexed generic asm attribution；聚焦 `source_indexed_generic_candidate_lambda_boundary`，546 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/source_indexed_generic_xyz_point_types/evidence_manifest.json` / `evidence_doctor.md` | source-indexed generic QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/dual_indexed_generic_xyz_point_types/run_bench_dual_indexed_generic_xyz_point_types_rvv.log` | dual-indexed generic point-type QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/dual_indexed_generic_xyz_point_types/asm_attribution.md` / `.json` | dual-indexed generic asm attribution；聚焦 `dual_indexed_generic_candidate_lambda_boundary`，880 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/dual_indexed_generic_xyz_point_types/evidence_manifest.json` / `evidence_doctor.md` | dual-indexed generic QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_generic_xyz_point_types/run_bench_correspondence_generic_xyz_point_types_rvv.log` | correspondence generic point-type QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/correspondence_generic_xyz_point_types/asm_attribution.md` / `.json` | correspondence generic asm attribution；聚焦 `correspondence_generic_candidate_lambda_boundary`，860 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_generic_xyz_point_types/evidence_manifest.json` / `evidence_doctor.md` | correspondence generic QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/dual_indexed_family_ab/run_bench_dual_indexed_family_ab_rvv.log` | dual-indexed family A/B QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/dual_indexed_family_ab/asm_attribution.md` / `.json` | dual-indexed family A/B asm attribution；聚焦 `dual_indexed_family_ab_lambda_boundary`，并记录 `production_public_dual_indexed_boundary` 50 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/dual_indexed_family_ab/evidence_manifest.json` / `evidence_doctor.md` | dual-indexed family A/B QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_public/run_bench_correspondence_public_rvv.log` | correspondence public QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/correspondence_public/asm_attribution.md` / `.json` | correspondence production-public asm attribution；聚焦 `production_public_correspondence_boundary`，47 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_public/evidence_manifest.json` / `evidence_doctor.md` | correspondence production-public QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_family_ab/run_bench_correspondence_family_ab_rvv.log` | correspondence family A/B QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/correspondence_family_ab/asm_attribution.md` / `.json` | correspondence family A/B asm attribution；聚焦 `correspondence_family_ab_lambda_boundary`，并记录 `production_public_correspondence_boundary` 47 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_family_ab/evidence_manifest.json` / `evidence_doctor.md` | correspondence family A/B QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_staging_profile/run_bench_correspondence_staging_profile_rvv.log` | correspondence staging/profile QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/correspondence_staging_profile/asm_attribution.md` / `.json` | correspondence staging/profile asm attribution；记录 `production_public_dual_indexed_boundary` 50 RVV lines 和 `production_public_correspondence_boundary` 47 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_staging_profile/evidence_manifest.json` / `evidence_doctor.md` | correspondence staging/profile QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_locality_order_profile/run_bench_correspondence_locality_order_profile_rvv.log` | correspondence locality/order QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/correspondence_locality_order_profile/asm_attribution.md` / `.json` | correspondence locality/order asm attribution；聚焦 `correspondence_locality_order_profile_lambda_boundary` 并保留 production public boundaries。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_locality_order_profile/evidence_manifest.json` / `evidence_doctor.md` | correspondence locality/order QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_component_ablation/run_bench_correspondence_component_ablation_rvv.log` | correspondence component ablation QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/correspondence_component_ablation/asm_attribution.md` / `.json` | correspondence component ablation asm attribution；聚焦 `correspondence_component_ablation_lambda_boundary` 并保留 production public boundaries。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/correspondence_component_ablation/evidence_manifest.json` / `evidence_doctor.md` | correspondence component ablation QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/row_source/run_bench_row_source_fused_rvv.log` | row-source QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/row_source/asm_attribution.md` / `.json` | 聚焦 `row_source_lambda_boundary` 的 row-source asm attribution。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/row_source/evidence_manifest.json` / `evidence_doctor.md` | 9-case row-source QEMU manifest / report。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/row_source_direct_gather/run_bench_row_source_direct_gather_rvv.log` | row-source direct gather QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/row_source_direct_gather/asm_attribution.md` / `.json` | 聚焦 `row_source_lambda_boundary` 的 direct gather asm attribution，当前 361 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/row_source_direct_gather/evidence_manifest.json` / `evidence_doctor.md` | 9-case direct gather QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/generic_xyz_point_types/run_bench_generic_xyz_point_types_rvv.log` | 16-case generic QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/generic_xyz_point_types/asm_attribution.md` / `.json` | 聚焦 `generic_candidate_lambda_boundary` 的 generic asm attribution。 | 本地生成；summary-only 证据指针；不等于 production symbol。 |
| `log/qemu/generic_xyz_point_types/evidence_manifest.json` / `evidence_doctor.md` | generic QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/generic_xyz_point_types_public/asm_attribution.md` / `.json` | production generic public asm attribution；聚焦 `production_public_generic_boundary`，406 RVV lines。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/generic_xyz_point_types_public/evidence_manifest.json` / `evidence_doctor.md` | generic production-public QEMU manifest / report；Doctor 0/0/0。 | 本地生成；summary-only 证据指针。 |
| `log/board/ordered_cloud_pair_repeated/summary.md` | diagnostic board repeated summary。 | 本地生成；summary-only 证据指针。 |
| `log/board/ordered_cloud_pair_repeated/evidence_manifest.json` / `evidence_doctor.md` | diagnostic board manifest / report。 | 本地生成；summary-only 证据指针。 |
| `log/board/ordered_cloud_pair_public_repeated/summary.md` | production-public board repeated summary。 | 本地生成；summary-only 证据指针。 |
| `log/board/ordered_cloud_pair_public_repeated/evidence_manifest.json` / `evidence_doctor.md` | production-public board manifest / report。 | 本地生成；summary-only 证据指针。 |
| `log/board/source_indexed_public_repeated/summary.md` | source-indexed production-public board repeated summary；3 cases，均 `B/A<1=0/5`。 | summary-only 证据指针；Phase 091 adopted narrow evidence。 |
| `log/board/source_indexed_public_repeated/evidence_manifest.json` / `evidence_doctor.md` | source-indexed production-public board manifest / report；Doctor 0/0/0。 | summary-only 证据指针。 |
| `log/board/source_indexed_family_ab_repeated/summary.md` | source-indexed family A/B board repeated summary；direct 相对 materialize 为 1.089x / 1.044x / 1.037x。 | summary-only 证据指针；Phase 092 weak-positive production-detail evidence。 |
| `log/board/source_indexed_family_ab_repeated/evidence_manifest.json` / `evidence_doctor.md` | source-indexed family A/B board manifest / report；Doctor 0/0/2。 | summary-only 证据指针。 |
| `log/board/source_indexed_generic_xyz_point_types_repeated/summary.md` | source-indexed generic point-type board repeated summary；16 cases，mixed-negative，多个 same-type case 为 negative。 | summary-only 证据指针；Phase 099 diagnostic evidence，不接 production。 |
| `log/board/source_indexed_generic_xyz_point_types_repeated/evidence_manifest.json` / `evidence_doctor.md` | source-indexed generic board manifest / report；Doctor 5/10/1。 | summary-only 证据指针。 |
| `log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/summary.md` | source-indexed generic public representative variance board summary；20 runs，12 positive、1 weak_positive、3 negative。 | summary-only 证据指针；Phase 106 public variance evidence，不支持 clean-adopt。 |
| `log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/evidence_manifest.json` / `evidence_doctor.md` | source-indexed generic public variance board manifest / report；Doctor 1/27/0。 | summary-only 证据指针；`PointNormal->PointNormal 256K` 的 `7/20` below-1 阻止 full widening adoption。 |
| `log/board/source_indexed_pointxyzi_public_phase110_repeated/summary.md` | source-indexed exact `PointXYZI -> PointXYZI` public board summary；20 runs，4K/64K/256K 为 3.979x / 3.547x / 3.602x，`B/A<1=0/20`。 | summary-only 证据指针；Phase 110/112 adopted exact production evidence，不支持 generic widening。 |
| `log/board/source_indexed_pointxyzi_public_phase110_repeated/evidence_manifest.json` / `evidence_doctor.md` | source-indexed exact `PointXYZI -> PointXYZI` board manifest / report；Doctor 0/3/0。 | summary-only 证据指针；Warning 保留为长尾 / 方差 caveat。 |
| `log/board/dual_indexed_generic_xyz_point_types_repeated/summary.md` | dual-indexed generic point-type board repeated summary；16 cases，negative，多数 same-type 大规模和 mixed pair 为 negative。 | summary-only 证据指针；Phase 100 diagnostic evidence，不接 production。 |
| `log/board/dual_indexed_generic_xyz_point_types_repeated/evidence_manifest.json` / `evidence_doctor.md` | dual-indexed generic board manifest / report；Doctor 13/17/1。 | summary-only 证据指针。 |
| `log/board/correspondence_generic_xyz_point_types_repeated/summary.md` | correspondence generic point-type board repeated summary；16 cases，negative，多数 same-type 64K/256K 和 mixed pair 为 negative。 | summary-only 证据指针；Phase 101 diagnostic evidence，不接 production。 |
| `log/board/correspondence_generic_xyz_point_types_repeated/evidence_manifest.json` / `evidence_doctor.md` | correspondence generic board manifest / report；Doctor 11/19/2。 | summary-only 证据指针。 |
| `log/board/dual_indexed_family_ab_phase109_variance_repeated/summary.md` | dual-indexed family A/B 20-run variance summary；direct 相对 materialize 为 1.080x / 1.661x / 1.646x。 | summary-only 证据指针；Phase 109 retained exact production evidence，保留 4K caveat。 |
| `log/board/dual_indexed_family_ab_phase109_variance_repeated/evidence_manifest.json` / `evidence_doctor.md` | dual-indexed family A/B Phase 109 board manifest / report；Doctor 0/3/0，Warning 全在 4K。 | summary-only 证据指针。 |
| `log/board/correspondence_public_repeated/summary.md` | correspondence production-public board repeated summary；public Std/RVV 为 4.517x / 3.705x / 3.149x。 | summary-only 证据指针；Phase 094 public probe positive，不等于 family clean-adopt。 |
| `log/board/correspondence_public_repeated/evidence_manifest.json` / `evidence_doctor.md` | correspondence production-public board manifest / report；Doctor 0/2/0。 | summary-only 证据指针。 |
| `log/board/correspondence_family_ab_repeated/summary.md` | correspondence family A/B board repeated summary；direct 相对 materialize median 为 1.081x / 1.639x / 1.406x，但 256K 有 5/20 低于 1。 | summary-only 证据指针；Phase 094 guarded candidate，不 clean-adopt。 |
| `log/board/correspondence_family_ab_repeated/evidence_manifest.json` / `evidence_doctor.md` | correspondence family A/B board manifest / report；Doctor 1/3/0。 | summary-only 证据指针；Error 必须阻塞 clean adoption。 |
| `log/board/correspondence_staging_profile_repeated/summary.md` | correspondence staging/profile board repeated summary；direct/staged D/S 为 0.833x / 0.729x / 0.848x。 | summary-only 证据指针；Phase 095 staged-dual negative，不切 production dispatch。 |
| `log/board/correspondence_staging_profile_repeated/evidence_manifest.json` / `evidence_doctor.md` | correspondence staging/profile board manifest / report；Doctor 3/2/0。 | summary-only 证据指针；Error 阻止 staged-dual switch。 |
| `log/board/correspondence_locality_order_profile_repeated/summary.md` | correspondence locality/order board repeated summary；identity、reverse、shuffled、strided D/S 均不支持 staged-dual switch。 | summary-only 证据指针；Phase 096 overall negative，不切 production dispatch。 |
| `log/board/correspondence_locality_order_profile_repeated/evidence_manifest.json` / `evidence_doctor.md` | correspondence locality/order board manifest / report；Doctor 12/5/0。 | summary-only 证据指针；Error 阻止 staged-dual switch，不能自动回滚 guarded patch。 |
| `log/board/correspondence_component_ablation_repeated/summary.md` | correspondence component ablation board repeated summary；strided full-anchor D/S 为 0.834x / 0.761x / 0.841x。 | summary-only 证据指针；Phase 097 profile-only negative，不切 production dispatch。 |
| `log/board/correspondence_component_ablation_repeated/evidence_manifest.json` / `evidence_doctor.md` | correspondence component ablation board manifest / report；Doctor 3/2/0。 | summary-only 证据指针；component no-solve 只作 bottleneck 线索。 |
| `log/board/row_source_fused_repeated/summary.md` | row-source materialize board repeated summary。 | 已按 Phase 090 同输入语义刷新；摘要包含 9 个 case，Doctor 1/1/6。 |
| `log/board/row_source_direct_gather_repeated/summary.md` | row-source direct gather board repeated summary。 | 已生成；source-indexed 可进 PI1，dual/correspondence 降级，Doctor 5/6/1。 |
| `log/board/row_source_direct_gather_repeated/evidence_manifest.json` / `evidence_doctor.md` | direct gather board manifest / report。 | summary-only 证据指针；不证明 production dispatch。 |
| `log/board/generic_xyz_point_types_repeated/summary.md` | generic point-type board repeated summary。 | 已生成；摘要包含 16 个 case，按 source/target point type 和 size 分桶。 |
| `log/board/generic_xyz_point_types_repeated/evidence_manifest.json` / `evidence_doctor.md` | generic board manifest / report；Doctor 0/3/2。 | summary-only 证据指针。 |
| `log/board/generic_xyz_point_types_public_repeated/summary.md` | generic production-public board repeated summary；16 cases，15 个 `B/A<1=0/5`，PointNormal->PointNormal 64K 为 `1/5`。 | summary-only 证据指针；保留 negative bucket。 |
| `log/board/generic_xyz_point_types_public_repeated/evidence_manifest.json` / `evidence_doctor.md` | generic production-public board manifest / report；Doctor 0/6/0。 | summary-only 证据指针。 |
| `log/evidence_registry.json` | 本地 evidence registry。 | 本地生成；默认不提交。 |
| `build/asm/riscv/bench_transformation_estimation_2D_rvv.asm` | RVV 指令 grep 结果。 | 本地生成；默认不提交。 |

Topic-local scripts 已由 Phase 050 接入。全局 `test-rvv/script/evidence_doctor.py` 只读取规范 manifest，不直接理解 `te2d` 的 case label 或 asm 符号。

## Production 与 Test Support 边界

当前 production 文件保留 traits-gated ordered-cloud-pair RVV patch；production intrinsic 在
ordered-cloud-pair、`Scalar=float`、两侧 PointXYZ-like AoS xyz gate、dense finite、点数不少于
16 时命中。Phase 091 另有 source-indexed `PointXYZ -> PointXYZ` adopted narrow production
dispatch，只覆盖 valid source indices、dense finite selected rows 和 size >= 16。test-rvv 中的
generic diagnostic intrinsic 和 row-source materialize / direct gather intrinsic 仍是诊断实现；
Phase 080 的 generic public path 已有独立接入后证据，但只覆盖代表性点型；Phase 091 的
source-indexed public path 不能外推到自定义点型、dual-indexed 或 correspondence dispatch。
Phase 092 只为该 source-indexed `PointXYZ -> PointXYZ` production boundary 补 direct-vs-materialize
family A/B，支持保留 current direct gather；不修改 production dispatch。
Phase 099 只为 source-indexed generic PointXYZ-like test-rvv candidate 补 diagnostic evidence，
结果 mixed-negative，不修改 production dispatch。
Phase 103/104/106 只为 source-indexed generic PointXYZ-like public guarded probe 补生产公开入口证据；
Phase 106 独立 20-run 方差为 `1/27/0` Doctor，结论为 full widening 不 clean-adopt。
Phase 100 只为 dual-indexed generic PointXYZ-like test-rvv candidate 补 diagnostic evidence，
结果 negative，不修改 production dispatch。
Phase 101 只为 correspondence generic PointXYZ-like test-rvv candidate 补 diagnostic evidence，
结果 negative，不修改 production dispatch。
Phase 110/112 只为 source-indexed exact `PointXYZI -> PointXYZI` production boundary 补
public Std/RVV、QEMU/asm、20-run board 和 Doctor，并已按用户确认采纳；仍不能覆盖 Normal、
mixed pair 或 generic row-source。
Phase 093/107/109 只为 dual-indexed `PointXYZ -> PointXYZ` production boundary 补
direct-vs-materialize family A/B，当前为 retained / positive with 4K caveat；仍不能覆盖
correspondence 或 generic row-source。
Phase 094/107 只为 correspondence `PointXYZ -> PointXYZ` production boundary 补 public probe 和
direct-vs-materialize family A/B；public Std/RVV positive，但 256K family A/B 退化频率阻止
production adoption，因此当前是 rolled back / not adopted，不能 clean-adopt。
Phase 095 / 096 分别完成 staged-dual profile 和 locality/order profile，结论均为 negative，
只阻止 staged-dual switch，不自动回滚 Phase 094 guarded patch。

## 拆分审计

| shape | present | paths | roles found | risk if unchanged | decision | next action |
| --- | --- | --- | --- | --- | --- | --- |
| root test source | no | not_present | none | none | not_applicable with evidence | none |
| root bench source | no | not_present | none | none | not_applicable with evidence | none |
| `src/` source | yes | `src/test_te2d.cpp`、`src/bench_te2d.cpp` | gtest、bench thin entry、row-source case registry | 当前规模可审查。 | adopted | board evidence 完成后复审 case 统计。 |
| aggregator header | yes | `include/te2d.h` | stable include | none | adopted | none |
| internal helpers | yes | `include/impl/te2d_candidates.hpp` | fixtures、reference、traits gate、generic candidate、row-source materialize/direct candidate、assertions-adjacent helper | 仍低于 soft limit；generic gate 已使用公共 traits，direct gather gate 已按 selected rows 计数。 | adopted | 若 PI2-PI5 或 row-source production probe 后继续增长，再拆 fixtures、references、candidates。 |
| legacy `test_support/` directory | no | not_present | none | none | not_applicable with evidence | none |
| topic-local script | yes | `script/generate_te2d_asm_summary.py`、`script/generate_te2d_qemu_evidence_manifest.py`、`script/generate_te2d_board_repeated_summary.py`、`script/generate_te2d_source_indexed_family_ab_summary.py`、`script/generate_te2d_dual_indexed_family_ab_summary.py`、`script/generate_te2d_correspondence_staging_profile_summary.py`、`script/generate_te2d_correspondence_locality_order_profile_summary.py`、`script/generate_te2d_correspondence_component_ablation_summary.py`、`script/generate_te2d_correspondence_chunked_xyz_staging_summary.py` | asm attribution、QEMU manifest、board repeated summary、source-indexed / dual-indexed / correspondence generic summary、source-indexed / dual-indexed / correspondence family A/B summary、staging/locality profile summary、component ablation summary 和 chunked staging summary | 已覆盖 row-source label、row-source boundary、source-indexed generic label、dual-indexed generic label、correspondence generic label、family A/B boundary、profile/component/chunked-staging boundary 和 manifest 字段。 | adopted | 若下一 phase 增加新的 bounded candidate，再复审 parser。 |
| bench case registry | structured | `src/bench_te2d.cpp` | 包含 generic point type / source / target labels、source-indexed public、source-indexed family A/B、source-indexed generic point-type、dual-indexed family A/B、dual-indexed generic point-type、correspondence generic point-type、correspondence public、correspondence family A/B、correspondence staging/profile、locality/order、component ablation 和 chunked staging labels | 当前足够区分 diagnostic、production-public、production-detail family A/B、source-indexed generic、dual-indexed generic、correspondence generic、staging/profile、locality/order、component ablation、chunked staging、row-source 和 generic candidate；generic cloud 只在专用 filter 下构造。 | adopted | 若下一 phase 增加新的 bounded candidate，再复审是否拆 internal bench cases。 |
| Makefile / board target | yes | `Makefile`、`board.mk` | build、QEMU、registry、board repeated target | none | adopted | none |
| topic-local docs | yes | `doc/*.zh.md`、`doc/phases/**` | doc suite | Phase 030、050、070、080、090、091、092、093、094、095、096、097、098、099、100、101 已补入 board 数值、Doctor、generic gate、source-indexed / dual-indexed production public boundary、source-indexed / dual-indexed / correspondence generic diagnostic、direct gather、family A/B、staging/profile、locality/order、component ablation、chunked staging 和 QEMU/board 分层。 | adopted | 继续保持 negative bucket、fallback 边界和用户检查点恢复入口。 |
| `doc-rvv` long-term doc | planned | `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` | production long-term doc | 只记录窄范围 production patch，不写 row-source 诊断为已采用。 | applicable with narrow scope | 用户 review/adoption 后继续更新。 |
| evidence registry | local generated | `log/evidence_registry.json` | freshness pointer | 默认不提交，提交前需说明。 | adopted as local-only | 若用户要提交 evidence logs，再脱敏并重新审计。 |
