# SNM depth-to-normal quantize production-shaped diagnostic repeated board summary

- run_label: `snm_phase000_depth_quantize_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_shaped_diagnostic`
- A/B boundary: `test_helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `depth_quantize_320x240` | 5 | 1.100x | 1.100x | 1.120x | 1.100x | 1.116x | 0/5 | 1.110x, 1.100x, 1.120x, 1.100x, 1.100x |
| `depth_quantize_641x481_tail` | 5 | 1.110x | 1.110x | 1.120x | 1.110x | 1.116x | 0/5 | 1.110x, 1.110x, 1.120x, 1.110x, 1.110x |

## Evidence boundary

本 summary 是 Phase 000 未接 production（生产源码）的 production-shaped diagnostic （生产形态诊断）证据。Std/RVV 两侧共享同一个 bench wrapper（性能测试包装入口），计时边界只覆盖 manifest 中的 depth-to-normal / quantize helper，不能替代 `SurfaceNormalModality::processInputData()` 的 production direct（真实生产路径）证据。
