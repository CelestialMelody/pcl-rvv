# ROPS production rotate + distribution detail repeated board summary

- run_label: `phase040_production_direct_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_detail`
- A/B boundary: `production private helper`
- timer_boundary: `production_rotate_cloud_aabb_then_three_distribution_matrices`
- decision_bucket: `positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `rops_production_rotate_distribution_pipeline,points=65536,repeat=8` | 5 | 1.700x | 1.690x | 1.700x | 1.694x | 1.700x | 0/5 | 1.700x, 1.700x, 1.700x, 1.690x, 1.700x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | checksum match |
| --- | ---: | ---: | ---: | --- |
| `run-01` | 434232.8168 | 255784.4272 | 1.700x | yes |
| `run-02` | 435384.6961 | 256002.3439 | 1.700x | yes |
| `run-03` | 435904.1356 | 255993.5481 | 1.700x | yes |
| `run-04` | 432565.8586 | 255994.4314 | 1.690x | yes |
| `run-05` | 434744.7336 | 255995.8022 | 1.700x | yes |

## Evidence boundary

这是 production-detail（生产私有 helper 直连）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 因未定义 `__RVV10__` 走 production scalar body，RVV build 通过真实 `ROPSEstimation::rotateCloud()` 和 `getDistributionMatrix()` 的 production dispatch 命中 RVV helper。计时覆盖 production private helper 的 rotateCloud + AABB、rotated cloud buffer 写回、3 个 projection 的 distribution matrix row/col staging 和标量 scatter；不包含 LRF、central moments、mesh local surface、descriptor normalization 或完整 public `computeFeature()`。
