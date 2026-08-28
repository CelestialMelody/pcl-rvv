# CGM Sobel+quantize production-shaped diagnostic repeated board summary

- run_label: `cgm_phase000_sobel_quantize_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_shaped_diagnostic`
- A/B boundary: `test_helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `sobel_quantize_320x240` | 5 | 2.230x | 2.130x | 2.280x | 2.166x | 2.264x | 0/5 | 2.240x, 2.230x, 2.220x, 2.280x, 2.130x |
| `sobel_quantize_641x481_tail` | 5 | 2.430x | 2.400x | 2.450x | 2.408x | 2.446x | 0/5 | 2.450x, 2.430x, 2.420x, 2.440x, 2.400x |

## Evidence boundary

本 summary 是 Phase 000 未接 production 的 production-shaped diagnostic 证据。Std/RVV 两侧共享同一个 bench wrapper，计时边界只覆盖 manifest 中的 Sobel+quantize helper，不能替代 `ColorGradientModality::processInputData()` 的 production evidence。
