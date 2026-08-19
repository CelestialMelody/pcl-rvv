# NDT derivative accumulation board repeated summary

- run_label: `derivative_accumulation_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `pre_production_diagnostic`
- A/B boundary: `test helper`
- timer_boundary: excludes voxel neighbor search, point derivative precompute, line search, Eigen solver and production dispatch.

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `derivative-gradient-staged` | 5 | 0.401x | 0.398x | 0.402x | 5/5 | `negative` |
| `derivative-hessian-staged` | 5 | 0.320x | 0.317x | 0.321x | 5/5 | `negative` |

## Evidence boundary

This is pre-production diagnostic evidence. It supports only a bounded
production probe discussion after staging, fallback and public-entry costs are audited.
