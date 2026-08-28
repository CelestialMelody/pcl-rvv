# CGM dominant filter production-shaped diagnostic repeated board summary

- run_label: `cgm_phase010_dominant_filter_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_shaped_diagnostic`
- A/B boundary: `test_helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `filter_dominant_320x240` | 5 | 3.480x | 3.470x | 3.510x | 3.470x | 3.502x | 0/5 | 3.490x, 3.480x, 3.470x, 3.510x, 3.470x |

## Evidence boundary

本 summary 是 Phase 010 未接 production 的 production-shaped diagnostic 证据。Std/RVV 两侧共享同一个 bench wrapper，计时边界只覆盖 manifest 中的 dominant filter helper，不能替代 `ColorGradientModality::processInputData()` 的 production evidence。
