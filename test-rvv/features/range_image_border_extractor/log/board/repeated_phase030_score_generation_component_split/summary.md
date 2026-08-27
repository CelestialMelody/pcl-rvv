# RangeImageBorderExtractor score-generation component split production-shaped diagnostic repeated board summary

- run_label: `range_image_border_extractor_phase030_score_generation_component_split_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production-shaped diagnostic`
- A/B boundary: `test helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `range_image_border_scores_after_surface_160x120` | 5 | 1.000x | 0.990x | 1.010x | 0.994x | 1.006x | 1/5 | 1.000x, 1.000x, 1.010x, 0.990x, 1.000x |
| `range_image_local_surface_160x120` | 5 | 1.000x | 0.990x | 1.010x | 0.994x | 1.006x | 1/5 | 0.990x, 1.000x, 1.010x, 1.000x, 1.000x |

## Evidence boundary

这是未接 production（生产源码）的 score-update diagnostic（诊断）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 走 scalar reference，RVV build 走 test-only RVV candidate。RangeImage case 会把真实 extractBorderScoreImages() 计入边界，但仍不代表 production dispatch 已接入，也不覆盖完整 public computeFeature 的后续 shadow/veil 和输出构造。
