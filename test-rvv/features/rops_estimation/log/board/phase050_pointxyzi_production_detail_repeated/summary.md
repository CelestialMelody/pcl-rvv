# ROPS PointXYZI production rotate + distribution detail repeated board summary

- run_label: `phase050_pointxyzi_production_detail_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_detail`
- A/B boundary: `production private helper`
- timer_boundary: `production_rotate_cloud_aabb_then_three_distribution_matrices`
- decision_bucket: `positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `rops_production_pointxyzi_rotate_distribution_pipeline,points=65536,repeat=8` | 5 | 1.520x | 1.470x | 1.570x | 1.470x | 1.570x | 0/5 | 1.570x, 1.520x, 1.470x, 1.570x, 1.470x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | checksum match |
| --- | ---: | ---: | ---: | --- |
| `run-01` | 471124.0481 | 299995.5877 | 1.570x | yes |
| `run-02` | 471796.2190 | 310394.8377 | 1.520x | yes |
| `run-03` | 470794.7044 | 319997.2169 | 1.470x | yes |
| `run-04` | 471132.9377 | 300641.7731 | 1.570x | yes |
| `run-05` | 470643.3461 | 320312.1064 | 1.470x | yes |

## Evidence boundary

这是 PointXYZI production-detail（生产私有 helper 直连）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 因未定义 `__RVV10__` 走 production scalar body，RVV build 通过真实 `ROPSEstimation::rotateCloud()` 和 `getDistributionMatrix()` 的 production dispatch 命中 RVV helper。计时覆盖 production private helper 的 rotateCloud + AABB、rotated cloud buffer 写回、3 个 projection 的 distribution matrix row/col staging 和标量 scatter；不包含 LRF、central moments、mesh local surface、descriptor normalization 或完整 public `computeFeature()`。额外字段不参与当前 RVV stage，本结果不证明这些字段的输出语义。
