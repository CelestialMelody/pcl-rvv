# GICP component diagnostic board repeated summary

- run_label: `component_diagnostic_repeated`
- expected_runs: `5`
- collected_runs: `5`
- device: `Milkv-Jupiter`
- evidence_role: `pre_production_diagnostic`
- A/B boundary: `test helper`
- timer_boundary: excludes KdTree search, correspondence search, Eigen SVD/Newton solver and production dispatch.

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `covariance-post-knn-default-k` | 5 | 1.151x | 1.027x | 1.259x | 0/5 | `weak_positive` |
| `residual-indexed-gather` | 5 | 1.255x | 1.140x | 1.322x | 0/5 | `positive` |
| `residual-mahalanobis-dense` | 5 | 1.347x | 1.317x | 1.411x | 0/5 | `positive` |

## Evidence boundary

This summary is pre-production diagnostic evidence. It can support a bounded
production probe only after public-entry profile and fallback / dispatch planning.
