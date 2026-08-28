# CGM full-chain production-shaped diagnostic repeated board summary

- run_label: `cgm_phase020_full_chain_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_shaped_diagnostic`
- A/B boundary: `test_helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `full_chain_320x240` | 5 | 2.320x | 2.280x | 2.340x | 2.280x | 2.332x | 0/5 | 2.320x, 2.280x, 2.320x, 2.340x, 2.280x |
| `full_chain_641x481_tail` | 5 | 2.480x | 2.460x | 2.490x | 2.464x | 2.486x | 0/5 | 2.480x, 2.460x, 2.490x, 2.470x, 2.480x |

## Evidence boundary

本 summary 是 Phase 020 未接 production 的 production-shaped diagnostic 证据。Std/RVV 两侧共享同一个 bench wrapper，计时边界覆盖 manifest 中的 Sobel+quantize+dominant filter 串接 helper，不能替代 `ColorGradientModality::processInputData()` 的 production evidence。
