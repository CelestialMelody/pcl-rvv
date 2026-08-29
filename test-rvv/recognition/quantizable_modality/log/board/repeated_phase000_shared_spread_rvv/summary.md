# QuantizableModality shared spread RVV production-detail repeated board summary

- run_label: `qm_phase000_shared_spread_rvv_repeated`
- expected_runs: `5`
- collected_runs: `5`
- iterations: `50`
- warmup_iterations: `5`
- evidence_role: `production_detail`
- A/B boundary: `production_detail_helper`
- asm_rvv_line_count: `82`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `shared_spread_320x240` | 5 | 4.670x | 4.510x | 5.070x | 4.550x | 5.030x | 0/5 | 4.670x, 4.610x, 4.510x, 4.970x, 5.070x |
| `shared_spread_641x481_tail` | 5 | 4.770x | 4.340x | 4.790x | 4.464x | 4.786x | 0/5 | 4.780x, 4.650x, 4.340x, 4.790x, 4.770x |

## Evidence boundary

本 summary 是公共 `QuantizedMap::spreadQuantizedMap()` 的 production-detail（生产细节 helper）证据。它可以证明接入后 helper 本身的 Std/RVV 性能差异，但不能单独外推为三个 modality 公开入口的端到端收益。
