# CGM RGB stencil RVV ablation repeated board summary

- run_label: `cgm_phase040_rgb_stencil_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_shaped_diagnostic`
- A/B boundary: `test_helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `full_chain_320x240` | 5 | 2.340x | 2.110x | 2.360x | 2.194x | 2.356x | 0/5 | 2.350x, 2.340x, 2.110x, 2.320x, 2.360x |
| `full_chain_641x481_tail` | 5 | 2.470x | 2.420x | 2.490x | 2.440x | 2.486x | 0/5 | 2.470x, 2.490x, 2.420x, 2.470x, 2.480x |
| `full_chain_stencil_320x240` | 5 | 5.040x | 4.980x | 5.100x | 5.000x | 5.080x | 0/5 | 5.050x, 5.040x, 4.980x, 5.100x, 5.030x |
| `full_chain_stencil_641x481_tail` | 5 | 4.680x | 4.660x | 4.840x | 4.664x | 4.828x | 0/5 | 4.660x, 4.670x, 4.810x, 4.680x, 4.840x |

## Evidence boundary

本 summary 是 Phase 040 未接 production 的 production-shaped diagnostic 证据。Std/RVV 两侧共享同一个 bench wrapper，计时边界覆盖当前 full-chain helper 和 RGB Sobel stencil RVV helper。它用于判断新 candidate family 是否值得进入后续 production integration，不能替代已采纳 production direct evidence。
