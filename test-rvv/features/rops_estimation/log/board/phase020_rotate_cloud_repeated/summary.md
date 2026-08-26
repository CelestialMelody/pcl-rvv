# ROPS rotateCloud + AABB diagnostic repeated board summary

- run_label: `phase020_rotate_cloud_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `diagnostic`
- A/B boundary: `test helper`
- timer_boundary: `rotate_cloud_aos_3x3_store_and_aabb_reduction`
- decision_bucket: `positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `rops_rotate_cloud_aabb,points=65536,repeat=8` | 5 | 1.220x | 1.200x | 1.220x | 1.208x | 1.220x | 0/5 | 1.220x, 1.220x, 1.200x, 1.220x, 1.220x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | checksum match |
| --- | ---: | ---: | ---: | --- |
| `run-01` | 117749.4646 | 96202.1604 | 1.220x | yes |
| `run-02` | 117199.0876 | 95869.0063 | 1.220x | yes |
| `run-03` | 116953.2167 | 97397.2584 | 1.200x | yes |
| `run-04` | 118371.5522 | 97202.9938 | 1.220x | yes |
| `run-05` | 116423.0229 | 95315.1396 | 1.220x | yes |

## Evidence boundary

这是未接 production 的 component diagnostic（组件诊断）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 走 scalar fallback，RVV build 走 `rotateCloudRVV()` 候选。计时覆盖 AoS stride load/store、3x3 rotation 和 AABB min/max reduction，不包含 LRF、distribution matrix、central moments、mesh local surface 或完整 descriptor normalization。
