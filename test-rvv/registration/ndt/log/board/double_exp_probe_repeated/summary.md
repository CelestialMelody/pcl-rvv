# NDT double exp probe board repeated summary

- run_label: `double_exp_probe_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `pre_production_diagnostic`
- A/B boundary: `test helper with topic-local double exp prototype`
- timer_boundary: excludes voxel neighbor search, point derivative precompute, line search, Eigen solver and production dispatch.

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `derivative-gradient-staged` | 5 | 0.405x | 0.404x | 0.408x | 5/5 | `negative` |
| `derivative-hessian-staged` | 5 | 0.321x | 0.318x | 0.323x | 5/5 | `negative` |
| `production-public-align-pointxyz` | 5 | 1.007x | 1.006x | 1.014x | 0/5 | `neutral` |

## Evidence boundary

This is pre-production diagnostic evidence. It supports only a bounded
production probe discussion after staging, fallback and public-entry costs are audited.
