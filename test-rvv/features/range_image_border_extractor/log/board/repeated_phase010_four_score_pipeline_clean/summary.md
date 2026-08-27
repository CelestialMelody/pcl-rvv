# RangeImageBorderExtractor four-score-pipeline diagnostic repeated board summary

- run_label: `range_image_border_extractor_phase010_four_score_pipeline_repeated_clean`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `diagnostic`
- A/B boundary: `test helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `score_pipeline_641x481_four_images` | 5 | 2.210x | 2.160x | 2.230x | 2.176x | 2.226x | 0/5 | 2.200x, 2.160x, 2.220x, 2.230x, 2.210x |

## Evidence boundary

这是未接 production（生产源码）的 score-update component diagnostic（组件诊断）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 走 scalar reference，RVV build 走 test-only RVV candidate。计时只覆盖连续 float 分数图像的 3x3 邻域传播，不代表完整 public computeFeature。
