# ROPS PointNormal production rotate + distribution detail repeated board summary

- run_label: `phase050_pointnormal_production_detail_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_detail`
- A/B boundary: `production private helper`
- timer_boundary: `production_rotate_cloud_aabb_then_three_distribution_matrices`
- decision_bucket: `positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `rops_production_pointnormal_rotate_distribution_pipeline,points=65536,repeat=8` | 5 | 1.590x | 1.570x | 1.610x | 1.570x | 1.606x | 0/5 | 1.600x, 1.570x, 1.570x, 1.610x, 1.590x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | checksum match |
| --- | ---: | ---: | ---: | --- |
| `run-01` | 485782.2336 | 304294.5543 | 1.600x | yes |
| `run-02` | 488747.0190 | 311067.9293 | 1.570x | yes |
| `run-03` | 485155.7356 | 309668.2355 | 1.570x | yes |
| `run-04` | 484887.5106 | 300371.0314 | 1.610x | yes |
| `run-05` | 486875.2252 | 305926.7064 | 1.590x | yes |

## Evidence boundary

这是 PointNormal production-detail（生产私有 helper 直连）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 因未定义 `__RVV10__` 走 production scalar body，RVV build 通过真实 `ROPSEstimation::rotateCloud()` 和 `getDistributionMatrix()` 的 production dispatch 命中 RVV helper。计时覆盖 production private helper 的 rotateCloud + AABB、rotated cloud buffer 写回、3 个 projection 的 distribution matrix row/col staging 和标量 scatter；不包含 LRF、central moments、mesh local surface、descriptor normalization 或完整 public `computeFeature()`。额外字段不参与当前 RVV stage，本结果不证明这些字段的输出语义。
