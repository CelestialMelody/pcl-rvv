# SNM production direct repeated board summary

- run_label: `snm_phase010_production_direct_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_direct`
- A/B boundary: `public_overload`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `production_process_320x240` | 5 | 1.060x | 1.060x | 1.080x | 1.060x | 1.076x | 0/5 | 1.070x, 1.060x, 1.060x, 1.060x, 1.080x |
| `production_process_641x481_tail` | 5 | 1.060x | 1.050x | 1.060x | 1.054x | 1.060x | 0/5 | 1.060x, 1.050x, 1.060x, 1.060x, 1.060x |

## Evidence boundary

本 summary 是 Phase 010 production direct（真实生产路径）证据。Std/RVV 两侧都通过 `SurfaceNormalModality<PointXYZRGBA>::processInputData()` 公开入口计时；计时边界包含 depth-to-normal / quantize、5x5 filter 和 spread，不包含 `extractFeatures()`。本轮用户已授权接入后板卡有收益即可采纳，因此该 summary 可作为 adopted production behavior（已采纳生产行为）的生产性能依据。
