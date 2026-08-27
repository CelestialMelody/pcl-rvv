# GASD trilinear interpolation diagnostic repeated board summary

- run_label: `gasd_phase030_trilinear_interpolation_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `diagnostic`
- A/B boundary: `test helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `candidate_trilinear_interpolation_rvv` | 5 | 1.950x | 1.560x | 2.030x | 1.588x | 2.006x | 0/5 | 1.950x, 1.560x, 1.630x, 1.970x, 2.030x |

## Evidence boundary

这是未接 production（生产源码）的 component diagnostic（组件诊断）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 走 scalar fallback，RVV build 走 test-only RVV candidate。计时只覆盖 manifest 中的 timer boundary，不代表完整 public compute。
