# GASD trilinear histogram write probe diagnostic repeated board summary

- run_label: `gasd_phase040_histogram_write_probe_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `diagnostic`
- A/B boundary: `test helper`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `candidate_trilinear_histogram_write_rvv` | 5 | 1.060x | 1.050x | 1.060x | 1.054x | 1.060x | 0/5 | 1.060x, 1.050x, 1.060x, 1.060x, 1.060x |

## Evidence boundary

这是未接 production（生产源码）的 component diagnostic（组件诊断）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 走 scalar fallback，RVV build 走 test-only RVV candidate。计时只覆盖 manifest 中的 timer boundary，不代表完整 public compute。
