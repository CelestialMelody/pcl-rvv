# GASD production-shaped shape combined diagnostic repeated board summary

- run_label: `gasd_phase060_shape_combined_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production-shaped diagnostic`
- A/B boundary: `test helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `candidate_shape_combined_rvv` | 5 | 0.590x | 0.590x | 0.600x | 0.590x | 0.600x | 5/5 | 0.600x, 0.590x, 0.590x, 0.590x, 0.600x |

## Evidence boundary

这是未接 production（生产源码）的诊断性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 走 scalar fallback，RVV build 走 test-only RVV candidate。计时只覆盖 manifest 中的 timer boundary，不代表完整 public compute。
