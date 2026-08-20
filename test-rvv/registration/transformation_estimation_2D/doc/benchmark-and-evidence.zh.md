# Benchmark 与证据说明

## 本文职责

本文记录 `transformation_estimation_2D` 的 bench（性能测试）入口、case-filter（用例过滤参数）、计时边界、QEMU / board 证据分层、asm attribution（反汇编归因）口径和提交边界。当前 adopted production patch 保留 traits-gated ordered-cloud-pair public overload、Phase 091 用户已采纳的 source-indexed `PointXYZ -> PointXYZ` narrow production dispatch、Phase 112 用户已采纳的 source-indexed exact `PointXYZI -> PointXYZI` dispatch，以及 Phase 107/109 保留的 dual-indexed `PointXYZ -> PointXYZ` exact dispatch。Phase 099 已完成 source-indexed generic PointXYZ-like diagnostic，结果为 mixed-negative，不修改 production dispatch。Phase 103/104 把 source-indexed generic widening 放到真实 public boundary 后仍是 guarded；Phase 106 独立 20-run public variance 为 12 positive、1 weak_positive、3 negative，board Doctor `1/27/0`，不支持 clean-adopt。Phase 100 已完成 dual-indexed generic PointXYZ-like diagnostic，结果为 negative，同样不修改 production dispatch。Phase 101 已完成 correspondence generic PointXYZ-like diagnostic，结果为 negative，同样不修改 production dispatch。Phase 094 已完成 correspondence `PointXYZ -> PointXYZ` public probe 和 same-boundary family A/B；public probe positive，但 Phase 107 family A/B 在 256K 有退化频率，production dispatch 已退回。Phase 095 staged-dual profile、Phase 096 locality/order profile、Phase 097 component ablation 和 Phase 098 chunked xyz staging 均为 negative / profile-only，不支持 correspondence production dispatch switch。Phase 030 / 090 的 row-source bench 仍作为 test-only materialize-to-ordered 或 direct gather 诊断保留；Phase 070 generic diagnostic、Phase 080 generic public bench、Phase 099 source-indexed generic bench、Phase 100 dual-indexed generic bench、Phase 101 correspondence generic bench 和 Phase 106 source-indexed generic public variance 分开记录。

## Bench 输出格式

`src/bench_te2d.cpp` 输出：

- topic banner 和 build 标记：Std 或 RVV。
- dataset：synthetic dense `PointXYZ` ordered-cloud-pair，或 synthetic dense row-source pairs。
- iterations 和 warmup iterations。
- 每个 case 的 `ms/iter`、`Total Time` 和 checksum（校验和）。

这些字段用于 smoke（小规模可运行性检查）和 board repeated summary 解析。QEMU 计时不进入性能排序。

## CLI 参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `--case-filter` | all | `ordered-cloud-pair-public`、`source-indexed-public`、`source-indexed-family-ab`、`source-indexed-generic-xyz-point-types`、`source-indexed-generic-xyz-point-types-public-variance`、`dual-indexed-family-ab`、`dual-indexed-generic-xyz-point-types`、`correspondence-generic-xyz-point-types`、`correspondence-public`、`correspondence-family-ab`、`correspondence-staging-profile`、`correspondence-locality-order-profile`、`correspondence-component-ablation`、`correspondence-chunked-xyz-staging`、`ordered-cloud-pair-fused`、`row-source-fused`、`row-source-direct-gather`、`generic-xyz-point-types`、`generic-xyz-point-types-public` 或 `all`。 |
| `--iterations` | 20 | 测量循环次数。 |
| `--warmup-iterations` | 3 | warm-up（预热）次数。 |

## Bench Label / case-filter 字典

| case-filter | label | 边界 |
| --- | --- | --- |
| `ordered-cloud-pair-public` | `public 2D ordered-cloud-pair 4K/64K/256K` | 真实 public overload；当前 production patch 的直接性能入口。 |
| `source-indexed-public` | `public 2D source-indexed-cloud-pair 4K/64K/256K` | 真实 source-indexed public overload；Phase 091 adopted narrow production dispatch 的直接性能入口。 |
| `source-indexed-family-ab` | `family source-indexed direct public RVV`、`family source-indexed materialized ordered public RVV`，各 4K/64K/256K | Phase 092 同一 source-indexed production boundary 内的 RVV-vs-RVV family A/B；materialize selected source rows 的成本计入 timer。 |
| `source-indexed-generic-xyz-point-types` | `source-indexed generic 2D <PointSource>-><PointTarget>`，四类 same-type 各 4K/64K/256K，加四组 64K mixed pair | Phase 099 test-rvv generic direct-gather diagnostic；source/target 各自使用 traits layout，QEMU/board 不直接导出 production 结论。 |
| `source-indexed-generic-xyz-point-types-public-variance` | `public source-indexed generic 2D <PointSource>-><PointTarget>`，四类 same-type 各 4K/64K/256K，加四组 64K mixed pair | Phase 106 真实 public source-indexed generic guarded probe 的独立 20-run variance 证据；不覆盖 Phase 103/104，不能自动 adopted。 |
| `source-indexed-pointxyzi-public` | `public source-indexed generic 2D PointXYZI->PointXYZI`，4K/64K/256K | Phase 110/112 真实 public source-indexed exact evidence；只支持 exact gate adoption，不支持 generic widening。 |
| `dual-indexed-family-ab` | `family dual-indexed direct public RVV`、`family dual-indexed materialized ordered public RVV`，各 4K/64K/256K | Phase 093 同一 dual-indexed production boundary 内的 RVV-vs-RVV family A/B；materialize selected source/target rows 的成本计入 timer。 |
| `dual-indexed-generic-xyz-point-types` | `dual-indexed generic 2D <PointSource>-><PointTarget>`，四类 same-type 各 4K/64K/256K，加四组 64K mixed pair | Phase 100 test-rvv generic direct-gather diagnostic；source/target 两侧各自使用 traits layout 和 index stream，QEMU/board 不直接导出 production 结论。 |
| `correspondence-generic-xyz-point-types` | `correspondence generic 2D <PointSource>-><PointTarget>`，四类 same-type 各 4K/64K/256K，加四组 64K mixed pair | Phase 101 test-rvv generic direct-gather diagnostic；query/source 与 match/target 两侧各自使用 traits layout 和 correspondence index stream，QEMU/board 不直接导出 production 结论。 |
| `correspondence-public` | `public 2D correspondence-pair 4K/64K/256K` | Phase 094 真实 correspondence public overload 的 Std/RVV probe；只证明 public RVV path 是否快于 public scalar path。 |
| `correspondence-family-ab` | `family correspondence direct public RVV`、`family correspondence materialized ordered public RVV`，各 4K/64K/256K | Phase 094 同一 correspondence production boundary 内的 RVV-vs-RVV family A/B；materialize selected query/match rows 的成本计入 timer。 |
| `correspondence-staging-profile` | `family correspondence direct public RVV`、`staged-dual-indexed public 2D correspondence-pair`、`family correspondence materialized ordered public RVV`，各 4K/64K/256K | Phase 095 staging/profile；staged-dual 先拷贝 query/match 为连续 indices，再调用 dual-indexed public RVV；copy 成本计入 timer，不是 production adoption。 |
| `correspondence-locality-order-profile` | `profile <identity/reverse/shuffled/strided> correspondence direct public RVV`、`profile ... staged-dual-indexed public RVV`、`profile ... materialized ordered public RVV`，各 4K/64K/256K | Phase 096 locality/order profile；只改变 query/match 分布，比较 direct、staged-dual 和 materialize 三条 public RVV 路径，不是 production adoption。 |
| `correspondence-component-ablation` | `component scan-correspondence-structs`、`component extract-indices`、`component gather-xyz-no-solve`、`component materialize-rows-no-solve`、`prematerialized ordered public full`、`direct correspondence public full`、`staged-dual-indexed public full`、`materialize-ordered public full`，各 4K/64K/256K | Phase 097 component ablation；只解释 strided correspondence 的 component 成本和 full-anchor 方向，不是 production adoption。 |
| `correspondence-chunked-xyz-staging` | `direct correspondence public 2D correspondence-pair`、`chunked-xyz-staging 2D correspondence-pair`、`materialize-ordered public 2D correspondence-pair`、`staged-dual-indexed public 2D correspondence-pair`，各 4K/64K/256K | Phase 098 chunked xyz staging profile；chunked candidate 是 test-rvv helper，anchors 是真实 public RVV 路径。不是 production adoption。 |
| `ordered-cloud-pair-fused` | `fused 2D correlation ordered-cloud-pair 4K/64K/256K` | test-only fused candidate；RVV 构建下尝试 RVV 累加。 |
| `row-source-fused` | `fused 2D correlation source-indexed-cloud-pair`、`dual-indexed-cloud-pair`、`correspondence-pair`，各 4K/64K/256K | test-only materialize-to-ordered candidate；source / target 展开成本计入每次 case 计时。 |
| `row-source-direct-gather` | `direct-gather 2D correlation source-indexed-cloud-pair`、`dual-indexed-cloud-pair`、`correspondence-pair`，各 4K/64K/256K | test-only direct gather candidate；index / correspondence offset 生成和 gather 成本计入每次 case 计时，不物化整点云。 |
| `generic-xyz-point-types` | `generic 2D PointXYZ->PointXYZ`、`PointXYZI->PointXYZI`、`PointNormal->PointNormal`、`PointXYZINormal->PointXYZINormal` 各 4K/64K/256K，加四组 64K mixed pair | test-only traits-gated ordered-cloud-pair candidate；source/target 各自 offset/stride，输入构造不计入 case timer。 |
| `generic-xyz-point-types-public` | 与 generic diagnostic 相同的 16 个 representative point-type / mixed cases | 真实 production public ordered-cloud-pair dispatch；source/target gate 和 fallback 进入 case timer，输入构造不计入 case timer。 |

## 推荐 Target

| target | 作用 | 证据等级 |
| --- | --- | --- |
| `run_bench_ordered_cloud_pair_smoke` | 只运行 RVV bench 的 fused ordered-cloud-pair 小规模 smoke。 | QEMU log shape only。 |
| `run_bench_ordered_cloud_pair_public_smoke` | 只运行 RVV bench 的 public ordered-cloud-pair 小规模 smoke。 | QEMU production-public probe log shape only。 |
| `run_bench_source_indexed_public_smoke` | 只运行 RVV bench 的 public source-indexed 小规模 smoke。 | QEMU source-indexed production probe log shape only。 |
| `run_bench_source_indexed_family_ab_smoke` | 只运行 RVV bench 的 source-indexed family A/B 小规模 smoke。 | QEMU production-detail A/B log shape only；不证明性能。 |
| `run_bench_source_indexed_generic_xyz_point_types_smoke` | 只运行 RVV bench 的 source-indexed generic point-type smoke，共 16 个 case。 | QEMU source-indexed generic diagnostic log shape only；不证明性能。 |
| `run_bench_source_indexed_generic_xyz_point_types_public_variance_smoke` | 只运行 RVV bench 的 source-indexed generic public variance smoke，共 16 个 case。 | QEMU source-indexed generic public variance log shape only；不证明性能。 |
| `run_bench_source_indexed_pointxyzi_public_smoke` | 只运行 RVV bench 的 source-indexed exact `PointXYZI -> PointXYZI` public smoke，共 3 个 case。 | QEMU source-indexed PointXYZI public log shape only；不证明性能。 |
| `run_bench_dual_indexed_family_ab_smoke` | 只运行 RVV bench 的 dual-indexed family A/B 小规模 smoke。 | QEMU production-detail A/B log shape only；不证明性能。 |
| `run_bench_dual_indexed_generic_xyz_point_types_smoke` | 只运行 RVV bench 的 dual-indexed generic point-type smoke，共 16 个 case。 | QEMU dual-indexed generic diagnostic log shape only；不证明性能。 |
| `run_bench_correspondence_generic_xyz_point_types_smoke` | 只运行 RVV bench 的 correspondence generic point-type smoke，共 16 个 case。 | QEMU correspondence generic diagnostic log shape only；不证明性能。 |
| `run_bench_correspondence_public_smoke` | 只运行 RVV bench 的 correspondence public 小规模 smoke。 | QEMU correspondence production-public probe log shape only。 |
| `run_bench_correspondence_family_ab_smoke` | 只运行 RVV bench 的 correspondence family A/B 小规模 smoke。 | QEMU production-detail A/B log shape only；不证明性能。 |
| `run_bench_correspondence_staging_profile_smoke` | 只运行 RVV bench 的 correspondence staging/profile 小规模 smoke。 | QEMU production-detail staging/profile log shape only；不证明性能。 |
| `run_bench_correspondence_locality_order_profile_smoke` | 只运行 RVV bench 的 correspondence locality/order profile 小规模 smoke。 | QEMU production-detail profile log shape only；不证明性能。 |
| `run_bench_correspondence_chunked_xyz_staging_smoke` | 只运行 RVV bench 的 correspondence chunked xyz staging profile 小规模 smoke。 | QEMU chunked-staging profile log shape only；不证明性能。 |
| `run_bench_row_source_smoke` | 只运行 RVV bench 的三类 row-source candidate smoke，共 9 个 case。 | QEMU log shape only；不证明性能。 |
| `run_bench_row_source_direct_gather_smoke` | 只运行 RVV bench 的三类 row-source direct gather smoke，共 9 个 case。 | QEMU log shape only；不证明性能。 |
| `run_bench_generic_xyz_point_types_smoke` | 只运行 RVV bench 的 generic point-type smoke，共 16 个 case。 | QEMU log shape only；不证明性能。 |
| `run_bench_generic_xyz_point_types_public_smoke` | 只运行 RVV bench 的 generic production-public smoke，共 16 个 case。 | QEMU production path / log shape only；不证明性能。 |
| `dump_bench_std` / `dump_bench_rvv` | 构建 Std / RVV bench 并导出 objdump。 | asm attribution input。 |
| `generate_asm_attribution_summary` | 生成 diagnostic asm summary。 | 反汇编归因摘要；不证明性能。 |
| `generate_production_public_asm_attribution_summary` | 生成 production-public probe asm summary。 | 路径命中和指令归属；不证明性能。 |
| `generate_source_indexed_public_asm_attribution_summary` | 生成 source-indexed production-public probe asm summary。 | 路径命中和指令归属；不证明性能或 family selection。 |
| `generate_source_indexed_family_ab_asm_attribution_summary` | 生成 source-indexed family A/B asm summary。 | materialize-family wrapper 路径归属；不替代 production source-indexed boundary。 |
| `generate_source_indexed_generic_asm_attribution_summary` | 生成 source-indexed generic point-type asm summary。 | 聚焦 `source_indexed_generic_candidate_lambda_boundary`；不替代 production source-indexed boundary。 |
| `generate_source_indexed_generic_public_variance_asm_attribution_summary` | 生成 Phase 106 source-indexed generic public variance asm summary。 | 聚焦 `production_public_source_indexed_generic_boundary`；不证明 board 性能或 clean adoption。 |
| `generate_source_indexed_pointxyzi_public_asm_attribution_summary` | 生成 Phase 110/112 source-indexed PointXYZI exact public asm summary。 | 聚焦 `production_public_source_indexed_generic_boundary`，用于 exact gate 路径归属。 |
| `generate_dual_indexed_family_ab_asm_attribution_summary` | 生成 dual-indexed family A/B asm summary。 | materialize-family wrapper 路径归属；不替代 production dual-indexed boundary。 |
| `generate_dual_indexed_generic_asm_attribution_summary` | 生成 dual-indexed generic point-type asm summary。 | 聚焦 `dual_indexed_generic_candidate_lambda_boundary`；不替代 production dual-indexed boundary。 |
| `generate_correspondence_generic_asm_attribution_summary` | 生成 correspondence generic point-type asm summary。 | 聚焦 `correspondence_generic_candidate_lambda_boundary`；不替代 production correspondence boundary。 |
| `generate_correspondence_public_asm_attribution_summary` | 生成 correspondence production-public asm summary。 | 路径命中和指令归属；不证明性能或 family selection。 |
| `generate_correspondence_family_ab_asm_attribution_summary` | 生成 correspondence family A/B asm summary。 | materialize-family wrapper 路径归属；不替代 production correspondence boundary。 |
| `generate_correspondence_staging_profile_asm_attribution_summary` | 生成 correspondence staging/profile asm summary。 | staged wrapper 可能内联；重点记录 dual-indexed public boundary 和 correspondence public boundary。 |
| `generate_correspondence_locality_order_profile_asm_attribution_summary` | 生成 correspondence locality/order profile asm summary。 | profile lambda 和既有 production public boundaries 的归属；不证明性能或采纳。 |
| `generate_correspondence_component_ablation_asm_attribution_summary` | 生成 correspondence component ablation asm summary。 | component lambda 和既有 production public boundaries 的归属；不证明性能或采纳。 |
| `generate_correspondence_chunked_xyz_staging_asm_attribution_summary` | 生成 correspondence chunked xyz staging asm summary。 | chunked staging lambda 和既有 production public boundaries 的归属；不证明性能或采纳。 |
| `generate_row_source_direct_gather_asm_attribution_summary` | 聚焦 `row_source_lambda_boundary` 生成 direct gather row-source asm summary。 | test-only direct gather 归属；不证明 production dispatch。 |
| `generate_generic_asm_attribution_summary` | 聚焦 `generic_candidate_lambda_boundary` 生成 generic asm summary。 | test-only candidate 归属；不证明 production dispatch。 |
| `run_qemu_smoke_evidence_doctor` | 生成 QEMU diagnostic manifest 并运行 Evidence Doctor。 | QEMU 证据合同检查；不证明性能。 |
| `run_qemu_production_public_evidence_doctor` | 生成 QEMU production-public manifest 并运行 Evidence Doctor。 | QEMU probe 证据合同检查；不证明性能。 |
| `run_qemu_source_indexed_public_evidence_doctor` | 生成 QEMU source-indexed public manifest 并运行 Evidence Doctor。 | QEMU probe 证据合同检查；不证明性能。 |
| `run_qemu_source_indexed_family_ab_evidence_doctor` | 生成 QEMU source-indexed family A/B manifest 并运行 Evidence Doctor。 | QEMU production-detail A/B 合同检查；不证明性能。 |
| `run_qemu_source_indexed_generic_evidence_doctor` | 生成 QEMU source-indexed generic point-type manifest 并运行 Evidence Doctor。 | QEMU generic row-source diagnostic 合同检查；不证明性能。 |
| `run_qemu_source_indexed_generic_public_variance_evidence_doctor` | 生成 QEMU source-indexed generic public variance manifest 并运行 Evidence Doctor。 | Phase 106 QEMU 合同检查；不证明性能。 |
| `run_qemu_source_indexed_pointxyzi_public_evidence_doctor` | 生成 QEMU source-indexed PointXYZI exact public manifest 并运行 Evidence Doctor。 | Phase 110/112 exact gate QEMU 合同检查；不证明性能。 |
| `run_qemu_dual_indexed_family_ab_evidence_doctor` | 生成 QEMU dual-indexed family A/B manifest 并运行 Evidence Doctor。 | QEMU production-detail A/B 合同检查；不证明性能。 |
| `run_qemu_dual_indexed_generic_evidence_doctor` | 生成 QEMU dual-indexed generic point-type manifest 并运行 Evidence Doctor。 | QEMU generic row-source diagnostic 合同检查；不证明性能。 |
| `run_qemu_correspondence_generic_evidence_doctor` | 生成 QEMU correspondence generic point-type manifest 并运行 Evidence Doctor。 | QEMU generic row-source diagnostic 合同检查；不证明性能。 |
| `run_qemu_correspondence_public_evidence_doctor` | 生成 QEMU correspondence public manifest 并运行 Evidence Doctor。 | QEMU production-public probe 合同检查；不证明性能。 |
| `run_qemu_correspondence_family_ab_evidence_doctor` | 生成 QEMU correspondence family A/B manifest 并运行 Evidence Doctor。 | QEMU production-detail A/B 合同检查；不证明性能。 |
| `run_qemu_correspondence_staging_profile_evidence_doctor` | 生成 QEMU correspondence staging/profile manifest 并运行 Evidence Doctor。 | QEMU staging/profile 合同检查；不证明性能。 |
| `run_qemu_correspondence_locality_order_profile_evidence_doctor` | 生成 QEMU correspondence locality/order profile manifest 并运行 Evidence Doctor。 | QEMU locality/order profile 合同检查；不证明性能。 |
| `run_qemu_correspondence_component_ablation_evidence_doctor` | 生成 QEMU correspondence component ablation manifest 并运行 Evidence Doctor。 | QEMU component ablation 合同检查；不证明性能。 |
| `run_qemu_correspondence_chunked_xyz_staging_evidence_doctor` | 生成 QEMU correspondence chunked xyz staging manifest 并运行 Evidence Doctor。 | QEMU chunked-staging 合同检查；不证明性能。 |
| `run_qemu_row_source_evidence_doctor` | 生成 row-source manifest 并运行 Evidence Doctor。 | 当前 9-case QEMU 证据合同检查；不证明性能。 |
| `run_qemu_row_source_direct_gather_evidence_doctor` | 生成 row-source direct gather manifest 并运行 Evidence Doctor。 | 当前 9-case direct gather QEMU 证据合同检查；不证明性能。 |
| `run_qemu_generic_evidence_doctor` | 生成 generic point-type manifest 并运行 Evidence Doctor。 | 当前 16-case QEMU 证据合同检查；不证明性能。 |
| `run_qemu_generic_public_evidence_doctor` | 生成 generic production-public manifest 并运行 Evidence Doctor。 | 当前 16-case production public QEMU 证据合同检查；不证明性能。 |
| `run_board_bench_ordered_cloud_pair_repeated` | 部署 Std/RVV bench 到板卡并按 5-run 预算重复运行。 | pre-production board diagnostic summary。 |
| `run_board_bench_ordered_cloud_pair_public_repeated` | 部署当前 production patch 的 Std/RVV bench 到板卡并按 5-run 预算运行 public probe。 | production candidate board evidence。 |
| `run_board_bench_source_indexed_public_repeated` | 部署当前 source-indexed narrow dispatch 的 Std/RVV bench 到板卡并按同一预算运行 public probe。 | Phase 091 production-public board evidence；已由用户采纳。 |
| `run_board_bench_source_indexed_family_ab_repeated` | 部署 RVV bench 到板卡并按同一预算运行 source-indexed direct-vs-materialize family A/B。 | Phase 092 production-detail family selection evidence；当前弱正向保留 direct gather。 |
| `run_board_bench_source_indexed_generic_xyz_point_types_repeated` | 部署 Std/RVV bench 到板卡并运行 source-indexed generic point-type representative cases。 | Phase 099 diagnostic evidence；当前 mixed-negative，不接 production dispatch。 |
| `run_board_bench_source_indexed_generic_xyz_point_types_public_variance_repeated` | 部署 Std/RVV bench 到板卡并按 20-run 预算运行 source-indexed generic public representative variance。 | Phase 106 production-public variance evidence；当前 negative for full widening，不 clean-adopt。 |
| `run_board_bench_source_indexed_pointxyzi_public_phase110_repeated` | 部署 Std/RVV bench 到板卡并按 20-run 预算运行 source-indexed exact `PointXYZI -> PointXYZI` public path。 | Phase 110/112 production-public evidence；当前 adopted exact gate，不扩大 generic widening。 |
| `run_board_bench_dual_indexed_family_ab_repeated` | 部署 RVV bench 到板卡并按同一预算运行 dual-indexed direct-vs-materialize family A/B。 | Phase 109 production-detail variance evidence；当前 retained exact dispatch，保留 4K caveat。 |
| `run_board_bench_dual_indexed_generic_xyz_point_types_repeated` | 部署 Std/RVV bench 到板卡并运行 dual-indexed generic point-type representative cases。 | Phase 100 diagnostic evidence；当前 negative，不接 production dispatch。 |
| `run_board_bench_correspondence_generic_xyz_point_types_repeated` | 部署 Std/RVV bench 到板卡并运行 correspondence generic point-type representative cases。 | Phase 101 diagnostic evidence；当前 negative，不接 production dispatch。 |
| `run_board_bench_correspondence_public_repeated` | 部署 Std/RVV bench 到板卡并按同一预算运行 correspondence public probe。 | Phase 094 production-public evidence；当前 public positive。 |
| `run_board_bench_correspondence_family_ab_repeated` | 部署 RVV bench 到板卡并运行 correspondence direct-vs-materialize family A/B。 | Phase 094 production-detail family selection evidence；当前 256K unstable / negative。 |
| `run_board_bench_correspondence_staging_profile_repeated` | 部署 RVV bench 到板卡并运行 correspondence direct-vs-staged-dual/materialize profile。 | Phase 095 production-detail staging/profile evidence；当前 staged-dual negative，不切 dispatch。 |
| `run_board_bench_correspondence_locality_order_profile_repeated` | 部署 RVV bench 到板卡并运行 correspondence identity/reverse/shuffled/strided locality-order profile。 | Phase 096 production-detail profile evidence；当前 overall negative，不切 dispatch。 |
| `run_board_bench_correspondence_component_ablation_repeated` | 部署 RVV bench 到板卡并运行 correspondence strided component ablation。 | Phase 097 component ablation profile evidence；当前 full-anchor negative，不切 dispatch。 |
| `run_board_bench_correspondence_chunked_xyz_staging_repeated` | 部署 RVV bench 到板卡并运行 correspondence direct/chunked/materialize/staged profile。 | Phase 098 chunked-staging profile evidence；当前 20-run negative，不切 dispatch。 |
| `run_board_bench_row_source_repeated` | 部署 Std/RVV bench 到板卡并按 5-run、20 iterations、5 warmup 预算运行 `row-source-fused`。 | row-source diagnostic board evidence；已完成，Doctor 异常使结论降级。 |
| `run_board_bench_row_source_direct_gather_repeated` | 部署 Std/RVV bench 到板卡并按同一预算运行 `row-source-direct-gather`。 | row-source direct gather diagnostic board evidence；source-indexed 支持 PI1，dual/correspondence 降级。 |
| `run_board_bench_generic_xyz_point_types_repeated` | 部署 Std/RVV bench 到板卡并按 5-run、20 iterations、5 warmup 预算运行 `generic-xyz-point-types`。 | generic point-type test-only representative board evidence；本轮 16 cases，Doctor 0/3/2。 |
| `run_board_bench_generic_xyz_point_types_public_repeated` | 部署 Std/RVV bench 到板卡并按同一预算运行 `generic-xyz-point-types-public`。 | generic production-public board evidence；本轮 16 cases，15 个 case 为 `B/A<1=0/5`，PointNormal->PointNormal 64K 为 `1/5`，Doctor 0/6/0。 |

默认不在 QEMU 上运行完整 Std/RVV compare。QEMU 只用于 correctness、构建、路径命中和日志形状。

## 计时边界

bench case 在每次迭代内包含：

- 调用 public estimator 或 test-only fused candidate。
- 对 `row-source-fused` candidate，包含 source / target index 或 correspondence 的 materialize-to-ordered 展开。
- 对 `row-source-direct-gather` candidate，包含 index / correspondence offset 生成和 source/target xyz gather，不包含整点云 materialize。
- 对 `source-indexed-public` production probe，包含 public overload runtime gate、source index validity scan、selected-row finite scan、source gather、target strided load 和 fallback 判定。
- 对 `source-indexed-family-ab` production-detail A/B，candidate A 计入真实 source-indexed public direct RVV timer；candidate B 计入 selected source rows materialize 成 ordered cloud 后再调用真实 ordered-cloud-pair public RVV 的完整 timer。
- 对 `dual-indexed-family-ab` production-detail A/B，candidate A 计入真实 dual-indexed public direct RVV timer；candidate B 计入 selected source/target rows materialize 成 ordered cloud 后再调用真实 ordered-cloud-pair public RVV 的完整 timer。
- 对 `correspondence-public` production-public probe，包含 public overload runtime gate、valid correspondence scan、selected-row finite scan、query/match gather、2D solve 和 fallback 判定。
- 对 `correspondence-family-ab` production-detail A/B，candidate A 计入真实 correspondence public direct RVV timer；candidate B 计入 selected query/match rows materialize 成 ordered cloud 后再调用真实 ordered-cloud-pair public RVV 的完整 timer。
- 对 `correspondence-staging-profile` production-detail profile，direct 计入真实 correspondence public RVV timer；staged-dual 计入 query/match 拷贝成连续 source/target indices 后调用真实 dual-indexed public RVV 的完整 timer；materialize reference 计入 query/match rows 物化成 ordered cloud 后调用真实 ordered-cloud-pair public RVV 的完整 timer。
- 对 `correspondence-locality-order-profile` production-detail profile，按 identity、reverse、shuffled、strided 四类 query/match 分布分别计时 direct correspondence public RVV、staged-dual public RVV 和 materialize+ordered public RVV；索引构造不在 timer 内，每次调用内的 public gate、copy、gather、materialize 和 2D solve 按各路径真实成本计入。
- 对 `correspondence-component-ablation` component profile，component no-solve label 只计时 scan/extract/gather/materialize 局部 helper 和 checksum sink；full anchor label 计时 prematerialized ordered public、direct correspondence public、staged-dual public 和 materialize+ordered public 的完整 estimate。no-solve 与 full anchor 不是 strict A/B。
- 对 `correspondence-chunked-xyz-staging` profile，direct anchor 计入真实 correspondence public RVV timer；chunked label 计入 test-rvv chunk-local x/y/z staging helper 的完整 estimate；materialize 和 staged anchors 分别计入真实 ordered / dual-indexed public RVV 路径。chunked label 不是 production dispatch。
- 对 generic candidate，包含 traits-gated x/y/z strided load、两遍中心化累加、2D solve 和 checksum；不包含 generic cloud 构造。
- 对 `source-indexed-generic-xyz-point-types`，包含 source index offset 生成和 source gather、target prefix strided load、两遍中心化累加、2D solve 和 checksum；不包含 generic cloud 构造。
- 对 `dual-indexed-generic-xyz-point-types`，包含 source / target 两侧 index offset 生成和 gather、两遍中心化累加、2D solve 和 checksum；不包含 generic cloud 构造。
- 对 `correspondence-generic-xyz-point-types`，包含 correspondence query / match 两列 index stream 的 offset 生成、source / target 两侧 gather、两遍中心化累加、2D solve 和 checksum；不包含 generic cloud 或 correspondence 构造。
- 2D angle、`cos/sin` 和 4x4 matrix 写回。
- checksum 计算。

bench case 不包含：

- 输入点云构造。
- `makeRigid2DTransform` 和 `transformCloud2D`。
- 日志解析、Evidence Doctor 或 registry 更新。

## Checksum 来源

`matrixChecksum` 把 4x4 matrix 逐元素乘以 `1e6` 后转换为整数并做 FNV-like 混合。production-public bench log 的 checksum 还包含 RVV gate flag（路径标记），因此 Std / RVV log checksum 不要求相同。checksum 用于发现路径或输出明显变化，不替代逐元素 correctness 断言。

## 当前 QEMU 证据

Diagnostic smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_doctor.md`

Production-public smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/run_bench_ordered_cloud_pair_public_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/evidence_doctor.md`

Production-public QEMU Doctor 为 Errors=0、Warnings=0、Suggestions=0；manifest 中 `rvv_instr_count=52`。该结果只证明 probe binary 的路径和日志形状。

Row-source smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/run_bench_row_source_fused_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/asm_attribution.md`

该 manifest 覆盖 source-indexed、dual-indexed、correspondence 三类 row source，各 4K/64K/256K，共 9 个 comparison；Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。它只证明可运行、日志字段和 wrapper / candidate 的归属形状。

Row-source direct gather smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source_direct_gather/run_bench_row_source_direct_gather_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source_direct_gather/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source_direct_gather/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source_direct_gather/asm_attribution.md`

该 manifest 覆盖三类 row source direct gather，各 4K/64K/256K，共 9 个 comparison；Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。asm summary 中 `row_source_lambda_boundary` 归属 361 条 RVV lines，包含 `vluxseg3ei32.v=10`、`vlsseg3e32.v=2`、`vfmacc=12`、`vfredosum=48`。

Generic point-type smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/generic_xyz_point_types/run_bench_generic_xyz_point_types_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/generic_xyz_point_types/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/generic_xyz_point_types/evidence_doctor.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/generic_xyz_point_types/asm_attribution.md`

该 manifest 覆盖四类 same-type 和四组 mixed pair，共 16 个 comparison；Evidence Doctor 为
Errors=0、Warnings=0、Suggestions=0。asm summary 聚焦
`generic_candidate_lambda_boundary`，不能替代 production-symbol attribution。

Generic production-public smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/generic_xyz_point_types_public/run_bench_generic_xyz_point_types_public_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/generic_xyz_point_types_public/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/generic_xyz_point_types_public/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/generic_xyz_point_types_public/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/generic_xyz_point_types_public/evidence_doctor.md`

该 manifest 覆盖真实 production public generic dispatch 的 16 个 comparison；Evidence
Doctor 为 Errors=0、Warnings=0、Suggestions=0。该 smoke 只证明 public path、manifest 和
production-symbol asm attribution 可解析，不替代 Milkv-Jupiter board performance。

Source-indexed production-public smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_public/run_bench_source_indexed_public_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_public/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_public/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_public/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_public/evidence_doctor.md`

该 manifest 覆盖真实 source-indexed public dispatch 的 3 个 comparison；Evidence Doctor 为
Errors=0、Warnings=0、Suggestions=0。asm summary 中
`production_public_source_indexed_boundary` 归属 46 条 RVV lines，包含
`vluxseg3ei32.v=2`、`vlsseg3e32.v=2`、`vfmacc=4`、`vfredosum=8`。

Source-indexed family A/B smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_family_ab/run_bench_source_indexed_family_ab_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_family_ab/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_family_ab/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_family_ab/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_family_ab/evidence_doctor.md`

该 manifest 覆盖同一 RVV binary 内 direct source-indexed public 与 materialize+ordered
public 两个 family，各 4K/64K/256K，共 6 个 comparison；Evidence Doctor 为
Errors=0、Warnings=0、Suggestions=0。asm summary 中
`source_indexed_family_ab_lambda_boundary` 归属 9 条 RVV lines；真实 production helper
仍由 `production_public_source_indexed_boundary` 归属。QEMU 只证明路径和证据合同。

Source-indexed generic point-type smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types/run_bench_source_indexed_generic_xyz_point_types_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types/evidence_doctor.md`

该 manifest 覆盖 source-indexed generic 16 个代表性 point-type / mixed comparison；
Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。asm summary 中
`source_indexed_generic_candidate_lambda_boundary` 归属 546 条 RVV lines，包含
`vlsseg3e32.v=14`、`vluxseg3ei32.v=14`、`vfmacc=36`、`vfredosum=72`。
QEMU 只证明路径、checksum 和证据合同，不证明性能或 production dispatch。

Source-indexed generic public variance smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/run_bench_source_indexed_generic_xyz_point_types_public_variance_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/evidence_doctor.md`

该 manifest 覆盖 Phase 106 source-indexed generic public variance 的 16 个代表性
point-type / mixed comparison；Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。
asm summary 聚焦 `production_public_source_indexed_generic_boundary`。QEMU 只证明路径、
checksum 和证据合同，不证明性能或 clean adoption。

Dual-indexed family A/B smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_family_ab/run_bench_dual_indexed_family_ab_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_family_ab/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_family_ab/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_family_ab/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_family_ab/evidence_doctor.md`

该 manifest 覆盖同一 RVV binary 内 direct dual-indexed public 与 materialize+ordered
public 两个 family，各 4K/64K/256K，共 6 个 comparison；Evidence Doctor 为
Errors=0、Warnings=0、Suggestions=0。asm summary 中
`dual_indexed_family_ab_lambda_boundary` 归属 13 条 RVV lines；真实 production helper
由 `production_public_dual_indexed_boundary` 归属。QEMU 只证明路径和证据合同。

Dual-indexed generic point-type smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_generic_xyz_point_types/run_bench_dual_indexed_generic_xyz_point_types_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_generic_xyz_point_types/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_generic_xyz_point_types/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_generic_xyz_point_types/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_generic_xyz_point_types/evidence_doctor.md`

该 manifest 覆盖 dual-indexed generic 16 个代表性 point-type / mixed comparison；
Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。asm summary 中
`dual_indexed_generic_candidate_lambda_boundary` 归属 880 条 RVV lines，包含
`vluxseg3ei32.v=32`、`vfmacc=32`、`vfredosum=128`。QEMU 只证明路径、
checksum 和证据合同，不证明性能或 production dispatch。

Correspondence generic point-type smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_generic_xyz_point_types/run_bench_correspondence_generic_xyz_point_types_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_generic_xyz_point_types/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_generic_xyz_point_types/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_generic_xyz_point_types/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_generic_xyz_point_types/evidence_doctor.md`

该 manifest 覆盖 correspondence generic 16 个代表性 point-type / mixed comparison；
Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。asm summary 中
`correspondence_generic_candidate_lambda_boundary` 归属 860 条 RVV lines，包含
`vluxseg3ei32.v=32`、`vfmacc=32`、`vfredosum=128`。QEMU 只证明路径、
checksum 和证据合同，不证明性能或 production dispatch。

Correspondence production-public smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_public/run_bench_correspondence_public_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_public/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_public/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_public/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_public/evidence_doctor.md`

该 manifest 覆盖真实 correspondence public dispatch 的 3 个 comparison；Evidence Doctor 为
Errors=0、Warnings=0、Suggestions=0。asm summary 中
`production_public_correspondence_boundary` 归属 47 条 RVV lines，包含
`vluxseg3ei32.v=4`、`vfmacc=4`、`vfredosum=8`。

Correspondence family A/B smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_family_ab/run_bench_correspondence_family_ab_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_family_ab/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_family_ab/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_family_ab/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_family_ab/evidence_doctor.md`

该 manifest 覆盖同一 RVV binary 内 direct correspondence public 与 materialize+ordered
public 两个 family，各 4K/64K/256K，共 6 个 comparison；Evidence Doctor 为
Errors=0、Warnings=0、Suggestions=0。asm summary 中
`correspondence_family_ab_lambda_boundary` 归属 12 条 RVV lines；真实 production helper
由 `production_public_correspondence_boundary` 归属。QEMU 只证明路径和证据合同。

Correspondence staging profile smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_staging_profile/run_bench_correspondence_staging_profile_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_staging_profile/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_staging_profile/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_staging_profile/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_staging_profile/evidence_doctor.md`

该 manifest 覆盖 direct correspondence public、staged-dual public 和 materialize+ordered
public 三组 RVV label，各 4K/64K/256K，共 9 个 comparison；Evidence Doctor 为
Errors=0、Warnings=0、Suggestions=0。asm summary 优先记录
`production_public_dual_indexed_boundary` 和 `production_public_correspondence_boundary`；
QEMU 只证明路径和证据合同。

Correspondence locality/order profile smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_locality_order_profile/run_bench_correspondence_locality_order_profile_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_locality_order_profile/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_locality_order_profile/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_locality_order_profile/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_locality_order_profile/evidence_doctor.md`

该 manifest 覆盖 identity、reverse、shuffled、strided 四类 query/match profile 下的
direct correspondence public、staged-dual public 和 materialize+ordered public 三组 RVV
label，共 36 个 comparison；Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。
QEMU 只证明路径、checksum 和日志形状。

Correspondence component ablation smoke：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_component_ablation/run_bench_correspondence_component_ablation_rvv.log`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_component_ablation/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_component_ablation/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_component_ablation/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_component_ablation/evidence_doctor.md`

该 manifest 覆盖 strided query/match profile 下的 scan/extract/gather/materialize no-solve
component label 和四个 full anchor label；Evidence Doctor 为 Errors=0、Warnings=0、
Suggestions=0。asm summary 聚焦 `correspondence_component_ablation_lambda_boundary`，
QEMU 只证明路径、checksum 和日志形状。

## 当前 Board 证据

Diagnostic repeated summary：

| case | median B/A | min | max | bucket |
| --- | ---: | ---: | ---: | --- |
| `fused 2D correlation ordered-cloud-pair 4K` | 1.176x | 1.166x | 1.191x | `positive` |
| `fused 2D correlation ordered-cloud-pair 64K` | 1.096x | 1.068x | 1.145x | `weak_positive` |
| `fused 2D correlation ordered-cloud-pair 256K` | 1.096x | 1.080x | 1.121x | `weak_positive` |

该结果只覆盖 test-only fused candidate，证据角色是 pre-production diagnostic（接入生产前诊断）。它曾支持进入 PI1/PI2，不证明最终 production dispatch 成立。

Production-public repeated summary：

| case | median B/A | min | max | B/A<1 | bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `public 2D ordered-cloud-pair 4K` | 4.222x | 4.110x | 4.231x | 0/5 | `positive` |
| `public 2D ordered-cloud-pair 64K` | 5.310x | 5.199x | 5.362x | 0/5 | `positive` |
| `public 2D ordered-cloud-pair 256K` | 4.947x | 4.864x | 5.068x | 0/5 | `positive` |

Production-public summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_doctor.md`

该结果是当前 Phase 050 PI5 主证据，来源是最新真实 `Milkv-Jupiter` 板卡复跑。三个规模均为稳定 `positive`，支持保留窄范围 production patch。

Row-source materialize-to-ordered board repeated 已使用仓库内 `test-rvv/config.mk`、`REMOTE_USER`、`REMOTE_IP` 和 `BOARD_LABEL=Milkv-Jupiter` 完成。Phase 090 修正 dual-indexed / correspondence 的 source 与 target index stream 后已重跑，当前 summary 见 `log/board/row_source_fused_repeated/summary.md`：

| row source | median B/A | B/A<1 | bucket |
| --- | ---: | ---: | --- |
| source-indexed 4K / 64K / 256K | 1.096x / 1.019x / 1.033x | 0/5 each | weak-positive / unstable / weak-positive |
| dual-indexed 4K / 64K / 256K | 1.085x / 1.050x / 1.042x | 0/5, 1/5, 0/5 | weak-positive |
| correspondence 4K / 64K / 256K | 1.076x / 1.025x / 1.025x | 0/5, 2/5, 0/5 | weak-positive / negative / unstable |

Materialize Doctor 为 Errors=1、Warnings=1、Suggestions=6，仍只能作为 test-only 诊断。

Row-source direct gather board repeated：

| row source | median B/A | B/A<1 | bucket | decision |
| --- | ---: | ---: | --- | --- |
| source-indexed 4K / 64K / 256K | 1.199x / 1.135x / 1.130x | 0/5 each | positive / weak-positive / weak-positive | production-ready-for-PI1，不是 adopted |
| dual-indexed 4K / 64K / 256K | 1.073x / 1.023x / 0.994x | 0/5, 2/5, 3/5 | weak-positive / unstable / negative | diagnostic-only；可做有界 probe 或 staging/profile |
| correspondence 4K / 64K / 256K | 1.101x / 0.965x / 0.922x | 2/5, 4/5, 5/5 | negative | diagnostic-only；不能直接拒绝 production probe |

direct gather summary 路径为 `log/board/row_source_direct_gather_repeated/summary.md`；Doctor 为 Errors=5、Warnings=6、Suggestions=1。Errors 全部来自 dual-indexed / correspondence，source-indexed 三个 case 没有 Error。

Generic point-type diagnostic board repeated：

| case group | median B/A | B/A<1 | bucket |
| --- | ---: | ---: | --- |
| `PointXYZ` same-type | `1.060x-1.091x` | 0/15 | weak_positive |
| `PointXYZI` same-type | `1.247x-1.496x` | 0/15 | positive |
| `PointNormal` same-type | `1.040x-1.142x` | 0/15 | weak_positive |
| `PointXYZINormal` same-type | `1.040x-1.179x` | 0/15 | per-case weak_positive/positive |
| mixed 64K | `1.113x-1.328x` | 0/20 | per-case weak_positive/positive |

summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_repeated/evidence_doctor.md`

该板卡证据是代表性点型 test-only diagnostic。PointXYZINormal 256K 的长尾、PointXYZI
4K/64K 的组内离群和两个 64K near-threshold case 已保留在 Doctor 与 Phase 070 result；
不能用 group median 覆盖单 case。

Generic production-public board repeated：

| case group | 结果 | bucket |
| --- | --- | --- |
| 15 个 case | 每 case `B/A<1=0/5` | 按 point type / size 分桶的 positive 或 weak-positive |
| `PointNormal -> PointNormal 64K` | median `3.298x`，values 含 `0.553x`，`B/A<1=1/5` | `negative`，独立保留 |

production-public summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_public_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_public_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_public_repeated/evidence_doctor.md`

该 evidence role 是 `post_production_performance`。用户已确认当前 patch 可以采纳，但负向
case 必须作为 adopted production behavior 的代表性性能边界保留，不能写成 generic stable
positive；5-run board budget 已耗尽，不自动扩大复跑。

Source-indexed production-public board repeated：

| case | median B/A | min | max | B/A<1 | bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `public 2D source-indexed-cloud-pair 4K` | 4.103x | 4.085x | 4.175x | 0/5 | `positive` |
| `public 2D source-indexed-cloud-pair 64K` | 4.818x | 4.796x | 4.844x | 0/5 | `positive` |
| `public 2D source-indexed-cloud-pair 256K` | 4.575x | 4.553x | 4.638x | 0/5 | `positive` |

source-indexed production-public summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_public_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_public_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_public_repeated/evidence_doctor.md`

该 evidence role 是 Phase 091 `post_production_performance`。它证明当前 source-indexed
public RVV path 快于 source-indexed public scalar path；它不证明 direct gather family
优于 materialize 或 staging，也不覆盖 dual-indexed / correspondence。

Source-indexed family A/B board repeated：

| size | B/A values | median | bucket | Doctor |
| --- | --- | ---: | --- | --- |
| 4K | 1.069x, 1.065x, 1.089x, 1.092x, 1.090x | 1.089x | `weak_positive` | 0/0/2 overall |
| 64K | 1.051x, 1.041x, 1.044x, 1.051x, 1.040x | 1.044x | `weak_positive` | near-threshold suggestion |
| 256K | 1.036x, 1.038x, 1.037x, 1.036x, 1.037x | 1.037x | `weak_positive` | near-threshold suggestion |

这里的 `B/A = materialize ordered public RVV ms / direct source-indexed public RVV ms`，
大于 1 表示 direct gather 更快。summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_family_ab_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_family_ab_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_family_ab_repeated/evidence_doctor.md`

该 evidence role 是 Phase 092 `production_detail`。它支持保留当前 source-indexed direct
gather family，但只是弱正向；不修改 production dispatch，也不覆盖 source-indexed generic
point types、dual-indexed 或 correspondence。

Source-indexed generic point-type board repeated：

| point type / pair | size | median B/A | B/A<1 | bucket |
| --- | ---: | ---: | ---: | --- |
| `PointXYZ -> PointXYZ` | 4K | 0.672x | 5/5 | `negative` |
| `PointXYZ -> PointXYZ` | 64K | 0.650x | 5/5 | `negative` |
| `PointXYZ -> PointXYZ` | 256K | 0.648x | 5/5 | `negative` |
| `PointXYZI -> PointXYZI` | 4K | 1.307x | 0/5 | `positive` |
| `PointXYZI -> PointXYZI` | 64K | 1.120x | 0/5 | `weak_positive` |
| `PointXYZI -> PointXYZI` | 256K | 0.747x | 4/5 | `negative` |
| `PointNormal -> PointNormal` | 4K | 1.219x | 0/5 | `positive` |
| `PointNormal -> PointNormal` | 64K | 1.133x | 0/5 | `weak_positive` |
| `PointNormal -> PointNormal` | 256K | 0.419x | 5/5 | `negative` |
| `PointXYZINormal -> PointXYZINormal` | 4K | 1.230x | 0/5 | `positive` |
| `PointXYZINormal -> PointXYZINormal` | 64K | 1.346x | 0/5 | `positive` |
| `PointXYZINormal -> PointXYZINormal` | 256K | 1.101x | 0/5 | `weak_positive` |
| `PointXYZI -> PointXYZ` | 64K | 1.050x | 0/5 | `weak_positive` |
| `PointXYZ -> PointXYZI` | 64K | 1.196x | 0/5 | `positive` |
| `PointNormal -> PointXYZINormal` | 64K | 1.333x | 0/5 | `positive` |
| `PointXYZINormal -> PointNormal` | 64K | 1.306x | 0/5 | `positive` |

summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_repeated/evidence_doctor.md`

该 evidence role 是 Phase 099 `source-indexed generic point-type diagnostic`。Board
Doctor 为 `Errors=5, Warnings=10, Suggestions=1`，结论是 mixed-negative；
因此不修改 production dispatch，也不把 Phase 091 exact source-indexed gate 泛化。

Source-indexed generic public variance board repeated：

| bucket | case 数 | 说明 |
| --- | ---: | --- |
| `positive` | 12 | 多数 `PointXYZ` / `PointXYZI` / mixed 64K 和部分 normal case 中位数正向且无 `B/A<1`。 |
| `weak_positive` | 1 | `PointNormal->PointXYZINormal 64K` median `2.441x`，min `1.123x`，长尾明显。 |
| `negative` | 3 | `PointNormal->PointNormal 256K`、`PointXYZINormal->PointNormal 64K`、`PointXYZINormal->PointXYZINormal 256K`。 |

summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/evidence_doctor.md`

该 evidence role 是 Phase 106 `source-indexed generic production public variance`。
Board Doctor 为 `Errors=1, Warnings=27, Suggestions=0`；`PointNormal->PointNormal 256K`
为 median `1.574x`，但 `B/A<1=7/20`，触发退化频率 Error。该证据不支持 full
source-indexed generic widening clean adoption。

Dual-indexed family A/B board repeated：

| size | B/A values | median | bucket | Doctor |
| --- | --- | ---: | --- | --- |
| 4K | 1.018x, 1.015x, 1.034x, 0.854x, 1.088x | 1.018x | `negative` | 0/3/1 overall |
| 64K | 1.636x, 1.719x, 1.671x, 1.676x, 1.555x | 1.671x | `positive` | Warning 全不在主决策规模 |
| 256K | 1.566x, 1.554x, 1.491x, 1.528x, 1.592x | 1.554x | `positive` | Warning 全不在主决策规模 |

这里的 `B/A = materialize ordered public RVV ms / direct dual-indexed public RVV ms`，
大于 1 表示 direct gather 更快。summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_repeated/evidence_doctor.md`

该 evidence role 已由 Phase 109 独立 20-run variance 刷新。64K / 256K 是主决策规模，overall
bucket 为 `positive`；4K 仍有 `1/20` below-1 和长尾 Warning。该证据支持保留 exact
dual-indexed direct gather family 和 4K caveat，但不覆盖 correspondence 或泛型点型。

Dual-indexed generic point-type board repeated：

| point type / pair | size | median B/A | B/A<1 | bucket |
| --- | ---: | ---: | ---: | --- |
| `PointXYZ -> PointXYZ` | 4K | 1.171x | 0/5 | `weak_positive` |
| `PointXYZ -> PointXYZ` | 64K | 1.012x | 2/5 | `negative` |
| `PointXYZ -> PointXYZ` | 256K | 0.853x | 5/5 | `negative` |
| `PointXYZI -> PointXYZI` | 4K | 0.921x | 5/5 | `negative` |
| `PointXYZI -> PointXYZI` | 64K | 0.892x | 5/5 | `negative` |
| `PointXYZI -> PointXYZI` | 256K | 0.817x | 5/5 | `negative` |
| `PointNormal -> PointNormal` | 4K | 1.077x | 0/5 | `weak_positive` |
| `PointNormal -> PointNormal` | 64K | 0.446x | 5/5 | `negative` |
| `PointNormal -> PointNormal` | 256K | 0.783x | 5/5 | `negative` |
| `PointXYZINormal -> PointXYZINormal` | 4K | 1.088x | 0/5 | `weak_positive` |
| `PointXYZINormal -> PointXYZINormal` | 64K | 0.865x | 5/5 | `negative` |
| `PointXYZINormal -> PointXYZINormal` | 256K | 0.449x | 5/5 | `negative` |
| mixed pairs | 64K | 0.359x-0.951x | all degraded | `negative` |

summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_generic_xyz_point_types_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_generic_xyz_point_types_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_generic_xyz_point_types_repeated/evidence_doctor.md`

该 evidence role 是 Phase 100 `dual-indexed generic point-type diagnostic`。Board Doctor
为 `Errors=13, Warnings=17, Suggestions=1`，结论是 negative；因此不修改
production dispatch，也不把 Phase 093 exact dual-indexed gate 泛化。

Correspondence generic point-type board repeated：

| point type / pair | size | median B/A | B/A<1 | bucket |
| --- | ---: | ---: | ---: | --- |
| `PointXYZ -> PointXYZ` | 4K | 1.157x | 0/5 | `weak_positive` |
| `PointXYZ -> PointXYZ` | 64K | 1.047x | 1/5 | `weak_positive` |
| `PointXYZ -> PointXYZ` | 256K | 0.966x | 3/5 | `negative` |
| `PointXYZI -> PointXYZI` | 4K | 1.079x | 0/5 | `weak_positive` |
| `PointXYZI -> PointXYZI` | 64K | 0.977x | 3/5 | `negative` |
| `PointXYZI -> PointXYZI` | 256K | 0.373x | 5/5 | `negative` |
| `PointNormal -> PointNormal` | 4K | 1.076x | 0/5 | `weak_positive` |
| `PointNormal -> PointNormal` | 64K | 0.329x | 5/5 | `negative` |
| `PointNormal -> PointNormal` | 256K | 0.480x | 5/5 | `negative` |
| `PointXYZINormal -> PointXYZINormal` | 4K | 1.109x | 0/5 | `weak_positive` |
| `PointXYZINormal -> PointXYZINormal` | 64K | 0.951x | 5/5 | `negative` |
| `PointXYZINormal -> PointXYZINormal` | 256K | 0.863x | 5/5 | `negative` |
| mixed pairs | 64K | 0.857x-1.018x | mostly degraded | `negative` |

summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_generic_xyz_point_types_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_generic_xyz_point_types_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_generic_xyz_point_types_repeated/evidence_doctor.md`

该 evidence role 是 Phase 101 `correspondence generic point-type diagnostic`。Board Doctor
为 `Errors=11, Warnings=19, Suggestions=2`，结论是 negative；因此不修改
production dispatch，也不把 Phase 094 guarded correspondence patch 泛化。

Correspondence production-public board repeated：

| case | median B/A | min | max | B/A<1 | bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `public 2D correspondence-pair 4K` | 4.517x | 4.098x | 4.647x | 0/5 | `positive` |
| `public 2D correspondence-pair 64K` | 3.705x | 3.559x | 3.904x | 0/5 | `positive` |
| `public 2D correspondence-pair 256K` | 3.149x | 1.720x | 3.329x | 0/5 | `positive` |

summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_public_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_public_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_public_repeated/evidence_doctor.md`

该 evidence role 是 Phase 094 `production_public`。它证明 correspondence public RVV path
快于 correspondence public scalar path；它不证明 direct gather family 优于 materialize 或 staging。

Correspondence family A/B board repeated：

| size | median | min | max | bucket | Doctor |
| --- | ---: | ---: | ---: | --- | --- |
| 4K | 1.081x | 1.039x | 1.136x | `weak_positive` | 1/3/0 overall |
| 64K | 1.639x | 1.486x | 1.803x | `positive` | long-tail warning |
| 256K | 1.406x | 0.633x | 1.556x | `negative` | degradation-frequency Error |

这里的 `B/A = materialize ordered public RVV ms / direct correspondence public RVV ms`，
大于 1 表示 direct gather 更快。summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_family_ab_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_family_ab_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_family_ab_repeated/evidence_doctor.md`

该 evidence role 是 Phase 094 `production_detail`。虽然 median 多数正向，256K 仍有
5/20 低于 1；当前 direct correspondence family 只能写成 guarded candidate，不 clean-adopt。

Correspondence staging profile board repeated：

| size | D/S median | D/S min | D/S max | D/S<1 | bucket | Doctor |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| 4K | 0.833x | 0.822x | 0.876x | 20/20 | `negative` | 3/2/0 overall |
| 64K | 0.729x | 0.687x | 0.917x | 20/20 | `negative` | degradation-frequency Error |
| 256K | 0.848x | 0.780x | 1.620x | 15/20 | `negative` | degradation-frequency Error |

这里的 `D/S = direct correspondence public RVV ms / staged-dual-indexed public RVV ms`，
大于 1 表示 staged-dual 更快。summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_staging_profile_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_staging_profile_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_staging_profile_repeated/evidence_doctor.md`

该 evidence role 是 Phase 095 `production_detail_staging_profile`。staged-dual 三个规模均
negative，当前不切换 correspondence production dispatch。

Correspondence locality/order profile board repeated：

| profile | D/S 4K | D/S 64K | D/S 256K | 主要观察 |
| --- | ---: | ---: | ---: | --- |
| identity | 0.850x | 0.621x | 0.756x | direct 三个规模 20/20 快于 staged-dual。 |
| reverse | 0.851x | 0.690x | 0.759x | 逆序访问仍不支持 staged-dual。 |
| shuffled | 0.928x | 0.974x | 0.978x | direct 略快于 staged-dual；materialize 在该分布下明显更快，但本阶段不切 production。 |
| strided | 0.893x | 0.887x | 0.927x | 中位数负向，256K 有长尾和 12/20 D/S<1。 |

这里的 `D/S = direct correspondence public RVV ms / staged-dual-indexed public RVV ms`，
大于 1 表示 staged-dual 更快。summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_locality_order_profile_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_locality_order_profile_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_locality_order_profile_repeated/evidence_doctor.md`

该 evidence role 是 Phase 096 `production_detail_profile`。overall bucket 为 `negative`，
board Doctor 为 `12/5/0`；它只说明 locality/order profile 不支持 staged-dual switch，
不自动回滚 Phase 094 guarded patch。

Correspondence component ablation board repeated：

| size | D/S median | D/S<1 | bucket | materialize/direct |
| --- | ---: | ---: | --- | ---: |
| 4K | 0.834x | 10/10 | `negative` | 1.079x |
| 64K | 0.761x | 10/10 | `negative` | 1.577x |
| 256K | 0.841x | 8/10 | `negative` | 1.395x |

这里的 `D/S = direct correspondence public RVV ms / staged-dual-indexed public RVV ms`，
大于 1 表示 staged-dual 更快；`materialize/direct` 来自 full-anchor label。summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_component_ablation_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_component_ablation_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_component_ablation_repeated/evidence_doctor.md`

该 evidence role 是 Phase 097 `component_ablation_profile`。full-anchor 三个规模均为
negative，board Doctor 为 `3/2/0`；component no-solve 只提供 bottleneck 线索，不能直接推出
chunked staging 或 production dispatch switch。

Correspondence chunked xyz staging board repeated：

| size | D/C median | D/C<1 | bucket | M/C median | S/C median |
| --- | ---: | ---: | --- | ---: | ---: |
| 4K | 0.813x | 20/20 | `negative` | 0.878x | 0.962x |
| 64K | 0.941x | 16/20 | `negative` | 1.503x | 1.199x |
| 256K | 0.983x | 11/20 | `negative` | 1.354x | 1.044x |

这里的 `D/C = direct correspondence public RVV ms / chunked-xyz-staging RVV ms`，
大于 1 表示 chunked 更快；`M/C` 和 `S/C` 分别表示 materialize-ordered public RVV
与 staged-dual public RVV 相对 chunked 的比值。summary 路径：

- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_chunked_xyz_staging_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_chunked_xyz_staging_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_chunked_xyz_staging_repeated/evidence_doctor.md`

该 evidence role 是 Phase 098 `chunked_xyz_staging_profile`。overall bucket 为
`negative`，board Doctor 为 `3/3/0`；它只说明 chunked xyz staging candidate
不支持 correspondence dispatch switch，不自动回滚 Phase 094 guarded patch。

## Evidence Doctor / Manifest 边界

| evidence | Errors | Warnings | Suggestions | 处理 |
| --- | ---: | ---: | ---: | --- |
| diagnostic QEMU manifest | 0 | 0 | 0 | 只作为 QEMU smoke contract。 |
| diagnostic board manifest | 0 | 0 | 0 | 只作为 pre-production diagnostic。 |
| production-public QEMU manifest | 0 | 0 | 0 | 只作为 QEMU probe contract。 |
| source-indexed production-public QEMU manifest | 0 | 0 | 0 | 只作为 Phase 091 QEMU probe contract。 |
| source-indexed family A/B QEMU manifest | 0 | 0 | 0 | 只作为 Phase 092 production-detail A/B smoke contract。 |
| source-indexed generic QEMU manifest | 0 | 0 | 0 | 只作为 Phase 099 source-indexed generic diagnostic smoke contract。 |
| source-indexed generic public variance QEMU manifest | 0 | 0 | 0 | 只作为 Phase 106 source-indexed generic public variance smoke contract；QEMU timing 不作性能结论。 |
| source-indexed PointXYZI exact QEMU manifest | 0 | 0 | 0 | 只作为 Phase 110/112 exact `PointXYZI -> PointXYZI` public path smoke contract；QEMU timing 不作性能结论。 |
| dual-indexed family A/B QEMU manifest | 0 | 0 | 0 | 只作为 Phase 093 production-detail A/B smoke contract。 |
| row-source QEMU manifest | 0 | 0 | 0 | 只作为 9-case QEMU smoke contract。 |
| generic QEMU manifest | 0 | 0 | 0 | 只作为 16-case generic QEMU smoke contract。 |
| generic production-public QEMU manifest | 0 | 0 | 0 | 只作为 16-case public dispatch smoke contract。 |
| production-public board manifest | 0 | 0 | 0 | 支持历史 exact production candidate；当前源码已由 generic gate 覆盖。 |
| source-indexed production-public board manifest | 0 | 0 | 0 | 支持 Phase 091 adopted narrow production dispatch；不覆盖其它 row source。 |
| source-indexed family A/B board manifest | 0 | 0 | 2 | 64K / 256K near-threshold；只支持 weak-positive family retention。 |
| source-indexed generic board manifest | 5 | 10 | 1 | mixed-negative；阻止把 Phase 091 exact gate 直接扩大为泛型 production dispatch。 |
| source-indexed generic public variance board manifest | 1 | 27 | 0 | Phase 106 独立 20-run public variance；12 positive、1 weak_positive、3 negative，`PointNormal->PointNormal 256K` 为 `7/20` below-1，阻止 full source-indexed generic clean adoption。 |
| source-indexed PointXYZI exact board manifest | 0 | 3 | 0 | Phase 110/112 独立 20-run public evidence；4K/64K/256K 均 positive 且 `B/A<1=0/20`，支持 exact gate adoption。 |
| dual-indexed family A/B board manifest | 0 | 3 | 0 | Phase 109 20-run variance；64K / 256K 支持 positive 主决策，4K 保留 `1/20` below-1 caveat。 |
| dual-indexed generic QEMU manifest | 0 | 0 | 0 | 只作为 Phase 100 dual-indexed generic diagnostic smoke contract。 |
| dual-indexed generic board manifest | 13 | 17 | 1 | same-type 大规模和 mixed pair 多数 negative；阻止把 Phase 093 exact gate 扩大为泛型 production dispatch。 |
| correspondence generic QEMU manifest | 0 | 0 | 0 | 只作为 Phase 101 correspondence generic diagnostic smoke contract。 |
| correspondence generic board manifest | 11 | 19 | 2 | 多数 64K/256K same-type 和 mixed pair negative；阻止把 Phase 094 guarded exact gate 扩大为泛型 production dispatch。 |
| correspondence production-public QEMU manifest | 0 | 0 | 0 | 只作为 Phase 094 QEMU probe contract。 |
| correspondence family A/B QEMU manifest | 0 | 0 | 0 | 只作为 Phase 094 production-detail A/B smoke contract。 |
| correspondence staging profile QEMU manifest | 0 | 0 | 0 | 只作为 Phase 095 staging/profile smoke contract。 |
| correspondence locality/order profile QEMU manifest | 0 | 0 | 0 | 只作为 Phase 096 locality/order profile smoke contract。 |
| correspondence component ablation QEMU manifest | 0 | 0 | 0 | 只作为 Phase 097 component ablation smoke contract。 |
| correspondence chunked xyz staging QEMU manifest | 0 | 0 | 0 | 只作为 Phase 098 chunked-staging smoke contract。 |
| correspondence production-public board manifest | 0 | 2 | 0 | public probe positive，但 256K 长尾和 4K group outlier 必须保留。 |
| correspondence family A/B board manifest | 1 | 3 | 0 | 256K 退化频率阻止 clean-adopt；只能写 guarded candidate。 |
| correspondence staging profile board manifest | 3 | 2 | 0 | staged-dual 三个规模均为 negative；不能切换 production dispatch。 |
| correspondence locality/order profile board manifest | 12 | 5 | 0 | 四类 profile 均不支持 staged-dual switch；只能作为负向 profile 证据。 |
| correspondence component ablation board manifest | 3 | 2 | 0 | full-anchor 三个规模均为 negative；component no-solve 只作 bottleneck 线索。 |
| correspondence chunked xyz staging board manifest | 3 | 3 | 0 | chunked staging 三个规模均为 negative；不切换 production dispatch。 |
| row-source materialize board manifest | 1 | 1 | 6 | 保留为 test-only 诊断；Phase 090 已按同输入语义刷新。 |
| row-source direct gather board manifest | 5 | 6 | 1 | source-indexed 可进 PI1；dual/correspondence 降级为 diagnostic-only 或 guarded probe 输入。 |
| generic board manifest | 0 | 3 | 2 | Warning/Suggestion 已按具体 point type/size 解释；支持创建 PI1，不等于 production direct。 |
| generic production-public board manifest | 0 | 6 | 0 | 15 个 case 正向，但 PointNormal->PointNormal 64K 为 negative bucket；用户采纳不消除该边界。 |

QEMU Doctor 的 `0/0/0` 不等于性能通过；性能结论只来自 board summary。Row-source Doctor 的 Error/Warning 必须按 row source 分桶解释；direct gather 的 dual-indexed / correspondence 负向不能覆盖 source-indexed 正向，也不能直接推出拒绝 production probe。

## ASM Attribution 口径

Diagnostic asm 输出：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.json`

Production-public asm 输出：

- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source_direct_gather/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source_direct_gather/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_public/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_public/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_family_ab/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_family_ab/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_family_ab/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_family_ab/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_generic_xyz_point_types/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_generic_xyz_point_types/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_generic_xyz_point_types/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_generic_xyz_point_types/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_staging_profile/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_staging_profile/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_locality_order_profile/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_locality_order_profile/asm_attribution.json`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_component_ablation/asm_attribution.md`
- `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_component_ablation/asm_attribution.json`

Production-public summary 结论为 `production_public_inline_boundary_present_with_candidate_boundary_with_other_rvv_boundaries`。它能证明 probe binary 中 public overload 或 `runPublicCase` 内联边界出现关键 RVV 指令；它不能证明目标硬件性能成立。

Row-source summary 聚焦 `row_source_lambda_boundary`，当前结论为 `row_source_lambda_boundary_present_with_candidate_boundary_with_other_rvv_boundaries`。它能证明三类 wrapper lambda 的路径归属，但共享数学 RVV 仍归属于 test-support fixture，不能替代 production asm 或 board evidence。

Row-source direct gather summary 也聚焦 `row_source_lambda_boundary`，当前归属 361 条 RVV
lines，含 `vluxseg3ei32.v=10`、`vlsseg3e32.v=2`、`vfmacc=12`、`vfredosum=48`。它证明
direct gather test helper 的 RVV 指令形状，不证明 public overload 已接入。

Generic summary 聚焦 `generic_candidate_lambda_boundary`，当前归属 540 条 RVV 指令，含
`vlsseg3e32.v`、`vfmacc` 和 `vfredosum`。它能证明 source/target generic candidate 的
traits-aware RVV 路径进入 test binary，不能替代 production public overload 的 asm attribution。

Generic production-public summary 聚焦 `production_public_generic_boundary`，当前归属
406 条 RVV 指令，含 `vlsseg3e32.v=28`、`vfmacc=28`、`vfredosum=56`；它能证明代表性
production template symbols 的指令归属，但不能抹去 board negative bucket。

Source-indexed production-public summary 聚焦 `production_public_source_indexed_boundary`，
当前归属 46 条 RVV 指令，含 `vluxseg3ei32.v=2`、`vlsseg3e32.v=2`、`vfmacc=4`、
`vfredosum=8`；它能证明 source-indexed public overload 中的 production helper 命中，
但不能替代 board performance 或同边界 RVV-vs-RVV family A/B。

Source-indexed family A/B summary 聚焦 `source_indexed_family_ab_lambda_boundary`，
当前归属 9 条 RVV 指令；它证明 materialize-family wrapper 可以归属。current direct
source-indexed production helper 的 RVV 指令仍以 `production_public_source_indexed_boundary`
为准，family A/B 性能排序只引用板卡 repeated summary。

Source-indexed generic summary 聚焦 `source_indexed_generic_candidate_lambda_boundary`，
当前归属 546 条 RVV 指令，含 `vlsseg3e32.v=14`、`vluxseg3ei32.v=14`、
`vfmacc=36`、`vfredosum=72`。它证明 test-rvv generic source-indexed candidate 的
RVV 指令形状，不替代 production public asm 或板卡性能结论。

Dual-indexed family A/B summary 聚焦 `dual_indexed_family_ab_lambda_boundary`，
当前归属 13 条 RVV 指令；它证明 materialize-family wrapper 可以归属。current direct
dual-indexed production helper 的 RVV 指令以 `production_public_dual_indexed_boundary`
为准，当前归属 50 条 RVV lines，含 `vluxseg3ei32.v=4`、`vfmacc=4` 和
`vfredosum=8`。family A/B 性能排序只引用板卡 repeated summary。

Dual-indexed generic summary 聚焦 `dual_indexed_generic_candidate_lambda_boundary`，
当前归属 880 条 RVV 指令，含 `vluxseg3ei32.v=32`、`vfmacc=32` 和
`vfredosum=128`。它证明 test-rvv generic dual-indexed candidate 的 RVV 指令形状，
不替代 production public asm 或板卡性能结论。

Correspondence generic summary 聚焦 `correspondence_generic_candidate_lambda_boundary`，
当前归属 860 条 RVV 指令，含 `vluxseg3ei32.v=32`、`vfmacc=32` 和
`vfredosum=128`。它证明 test-rvv generic correspondence candidate 的 RVV 指令形状，
不替代 production public asm 或板卡性能结论。

Correspondence production-public summary 聚焦 `production_public_correspondence_boundary`，
当前归属 47 条 RVV 指令，含 `vluxseg3ei32.v=4`、`vfmacc=4` 和 `vfredosum=8`；
它能证明 correspondence public overload 中的 guarded helper 命中，但不能替代 board
performance 或同边界 RVV-vs-RVV family A/B。

Correspondence family A/B summary 聚焦 `correspondence_family_ab_lambda_boundary`，
当前归属 12 条 RVV 指令；它证明 materialize-family wrapper 可以归属。current direct
correspondence production helper 的 RVV 指令仍以 `production_public_correspondence_boundary`
为准。family A/B 性能排序只引用板卡 repeated summary。

Correspondence staging profile summary 的 attribution decision 为
`production_public_dual_indexed_boundary_present_with_candidate_boundary_with_other_rvv_boundaries`。
staged-dual wrapper 可能被内联，关键归属看 `production_public_dual_indexed_boundary`
50 条 RVV lines 和 `production_public_correspondence_boundary` 47 条 RVV lines；这只证明
路径形状，不支持 staged-dual production dispatch switch。

Correspondence locality/order profile summary 聚焦
`correspondence_locality_order_profile_lambda_boundary`，同时保留
`production_public_correspondence_boundary` 和 `production_public_dual_indexed_boundary`
等既有边界。它证明 locality/order profile bench 的 RVV 路径和归属形状，不支持
production dispatch switch。

Correspondence component ablation summary 聚焦
`correspondence_component_ablation_lambda_boundary`，同时保留 direct correspondence、
dual-indexed、source-indexed 和 ordered/generic public production boundaries。它只证明
component bench 与生产公开边界可区分，不支持 production dispatch switch。

## 复现命令

```bash
make -C test-rvv/registration/transformation_estimation_2D run_test_compare
make -C test-rvv/registration/transformation_estimation_2D run_bench_ordered_cloud_pair_smoke
make -C test-rvv/registration/transformation_estimation_2D generate_asm_attribution_summary
make -C test-rvv/registration/transformation_estimation_2D run_qemu_smoke_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D run_qemu_production_public_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D run_bench_source_indexed_public_smoke
make -C test-rvv/registration/transformation_estimation_2D run_qemu_source_indexed_public_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D record_qemu_source_indexed_family_ab_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_source_indexed_generic_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_source_indexed_generic_public_variance_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_dual_indexed_family_ab_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_dual_indexed_generic_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_correspondence_generic_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_correspondence_public_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_correspondence_family_ab_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_correspondence_staging_profile_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_correspondence_locality_order_profile_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_correspondence_component_ablation_state
make -C test-rvv/registration/transformation_estimation_2D run_bench_row_source_smoke
make -C test-rvv/registration/transformation_estimation_2D run_qemu_row_source_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D run_bench_row_source_direct_gather_smoke
make -C test-rvv/registration/transformation_estimation_2D run_qemu_row_source_direct_gather_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D run_bench_generic_xyz_point_types_smoke
make -C test-rvv/registration/transformation_estimation_2D run_qemu_generic_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D run_bench_generic_xyz_point_types_public_smoke
make -C test-rvv/registration/transformation_estimation_2D run_qemu_generic_public_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D record_board_ordered_cloud_pair_public_state
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_source_indexed_public_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_source_indexed_family_ab_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_source_indexed_generic_xyz_point_types_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_source_indexed_generic_xyz_point_types_public_variance_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_dual_indexed_family_ab_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_dual_indexed_generic_xyz_point_types_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_correspondence_generic_xyz_point_types_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_correspondence_public_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_correspondence_family_ab_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_correspondence_staging_profile_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_correspondence_locality_order_profile_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_correspondence_component_ablation_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_row_source_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_row_source_direct_gather_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_generic_xyz_point_types_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_generic_xyz_point_types_public_repeated
make -C test-rvv/registration/transformation_estimation_2D evidence_status
```

`record_board_ordered_cloud_pair_public_state` 只解析并登记已抓回的 repeated board run；正式板卡采集入口是 `run_board_bench_ordered_cloud_pair_public_repeated`。
`run_board_bench_row_source_repeated` 使用 `test-rvv/config.mk` 中的板卡配置；配置存在且板卡可达时必须运行，不得以 QEMU timing 代替。

## 提交边界

默认提交源码 scaffold、Makefile、board.mk、topic-local script 和文档。`build/`、`log/qemu/*.log`、`log/board/**/run-*`、`log/evidence_registry.json` 和板卡 raw logs 都是本地生成产物，除非用户明确要求提交脱敏 evidence logs，否则不提交。

当前 summary-only 证据路径已被本文和 phase result 引用，用于 registry freshness（证据新鲜度）检查；它们是证据指针，不是 raw log 提交授权。
