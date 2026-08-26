# ROPS combined rotate + distribution diagnostic repeated board summary

- run_label: `phase030_combined_rotate_distribution_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_shaped_diagnostic`
- A/B boundary: `test helper`
- timer_boundary: `rotate_cloud_aabb_then_three_distribution_matrices`
- decision_bucket: `positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `rops_rotate_distribution_pipeline,points=65536,repeat=8` | 5 | 1.630x | 1.630x | 1.640x | 1.630x | 1.640x | 0/5 | 1.630x, 1.640x, 1.640x, 1.630x, 1.630x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | checksum match |
| --- | ---: | ---: | ---: | --- |
| `run-01` | 418699.7877 | 256802.0751 | 1.630x | yes |
| `run-02` | 419116.6148 | 255999.2147 | 1.640x | yes |
| `run-03` | 420811.8440 | 255981.3564 | 1.640x | yes |
| `run-04` | 417540.3002 | 255995.7272 | 1.630x | yes |
| `run-05` | 418000.2689 | 255981.2939 | 1.630x | yes |

## Evidence boundary

这是未接 production 的 production-shaped diagnostic（生产形态诊断）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 走 scalar fallback，RVV build 走 `rotateCloudAndDistributionMatricesRVV()` 组合候选。计时覆盖 rotateCloud + AABB、rotated cloud buffer 写回、3 个 projection 的 distribution matrix row/col staging 和标量 scatter；不包含 LRF、central moments、mesh local surface、descriptor normalization 或 public dispatch。
