# Optimization Evidence

| candidate family | test support path | test target | bench / board target | asm evidence | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `plane_d_dot_rvv` | `include/impl/omps_components.hpp` | `run_test_compare` | `run_board_omps_repeated` case `plane_d_dot` | `computePlaneDValuesRVV` has `vlse32.v` / `vfmacc.vv` | `log/board/repeated/evidence_doctor.md` reports 5/5 degradation Error | rejected |
| `boundary_gather_rvv` | 同上 | `run_test_compare` | case `boundary_gather` | `gatherBoundaryCloudRVV` has `vluxei32.v` / `vsse32.v` | near-threshold and variance warnings | attempted, neutral |
| `viewpoint_projection_rvv` | 同上 | `run_test_compare` | case `projection` | `projectBoundaryFromViewpointRVV` has `vfdiv.vv` / `vfmacc.vv` / `vsse32.v` | group-outlier warning handled by Phase 010 | component positive only |
| `region_boundary_projection_rvv` | 同上 | `run_test_compare` | case `region_projected` with Phase 010 output dir | `assembleRegionBoundariesRVV` calls gather/projection RVV helpers | `phase010-region_projected/evidence_doctor.md` reports 5/5 degradation Error | rejected |
| `region_boundary_gather_only_rvv` | 同上 | `run_test_compare` | case `region_gather_only` with Phase 010 output dir | `assembleRegionBoundariesRVV` calls gather RVV helper | `phase010-region_gather_only/evidence_doctor.md` reports 5/5 degradation Error | rejected |

## EvidenceDecision

当前 EvidenceDecision 是 `bench-only/no-production`。局部 projection positive 不足以接入 production；生产形态 boundary/projection 两个 case 在板卡上均稳定退化，且 Evidence Doctor 把退化频率列为 Error。当前 topic 不建议进入 PI1。
