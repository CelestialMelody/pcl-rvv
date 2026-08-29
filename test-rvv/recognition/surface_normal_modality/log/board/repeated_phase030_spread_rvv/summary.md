# SNM spread RVV production direct repeated board summary

- run_label: `snm_phase030_spread_rvv_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_direct`
- A/B boundary: `public_overload`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `production_process_320x240` | 5 | 1.620x | 1.620x | 1.640x | 1.620x | 1.632x | 0/5 | 1.640x, 1.620x, 1.620x, 1.620x, 1.620x |
| `production_process_641x481_tail` | 5 | 1.640x | 1.630x | 1.650x | 1.630x | 1.646x | 0/5 | 1.650x, 1.640x, 1.630x, 1.640x, 1.630x |

## Evidence boundary

本 summary 是 surface_normal_modality 当前 production direct（真实生产路径）证据。Std/RVV 两侧都通过 `SurfaceNormalModality<PointXYZRGBA>::processInputData()` 公开入口计时；计时边界包含 depth-to-normal / quantize、5x5 filter 和 spread，不包含 `extractFeatures()`。本轮用户已授权接入后板卡有收益即可采纳，因此该 summary 可作为 adopted production behavior（已采纳生产行为）的生产性能依据。
