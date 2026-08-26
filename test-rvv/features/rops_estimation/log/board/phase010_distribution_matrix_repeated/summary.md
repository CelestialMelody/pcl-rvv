# ROPS distribution matrix diagnostic repeated board summary

- run_label: `phase010_distribution_matrix_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `diagnostic`
- A/B boundary: `test helper`
- timer_boundary: `distribution_matrix_binning_with_row_col_staging_and_scalar_scatter`
- decision_bucket: `positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `rops_distribution_matrix_binning,points=65536,bins=5` | 5 | 1.440x | 1.420x | 1.460x | 1.420x | 1.452x | 0/5 | 1.440x, 1.440x, 1.420x, 1.460x, 1.420x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | checksum match |
| --- | ---: | ---: | ---: | --- |
| `run-01` | 71718.1813 | 49869.1750 | 1.440x | yes |
| `run-02` | 71863.9188 | 49759.6084 | 1.440x | yes |
| `run-03` | 70863.8208 | 49926.1688 | 1.420x | yes |
| `run-04` | 71743.1042 | 48981.0459 | 1.460x | yes |
| `run-05` | 70876.8000 | 49944.0396 | 1.420x | yes |

## Evidence boundary

这是未接 production 的 component diagnostic（组件诊断）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 走 scalar fallback，RVV build 走 `getDistributionMatrixRVV()` 候选。计时包含 row/col staging 和标量 scatter，不包含 LRF、rotateCloud、mesh local surface 或完整 descriptor normalization。
