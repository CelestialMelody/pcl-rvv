# RangeImageBorderExtractor score-update diagnostic repeated board summary

- run_label: `range_image_border_extractor_phase000_score_update_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `diagnostic`
- A/B boundary: `test helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `score_update_641x481_tail` | 5 | 2.560x | 2.520x | 2.590x | 2.536x | 2.582x | 0/5 | 2.560x, 2.590x, 2.520x, 2.560x, 2.570x |

## Evidence boundary

这是未接 production（生产源码）的 score-update component diagnostic（组件诊断）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 走 scalar reference，RVV build 走 test-only RVV candidate。计时只覆盖连续 float 分数图像的 3x3 邻域传播，不代表完整 public computeFeature。
