# GICP component diagnostic board repeated summary

- run_label: `dfddf_loop_dense_repeated`
- expected_runs: `5`
- collected_runs: `5`
- device: `Milkv-Jupiter`
- evidence_role: `pre_production_diagnostic`
- A/B boundary: `test helper`
- decision_question: `RVV-vs-scalar pre-production component diagnostic`

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `dfddf-loop-dense` | 5 | 1.410x | 1.396x | 1.427x | 0/5 | `positive` |

## Evidence boundary

This summary is pre-production diagnostic evidence. It can support a bounded
production probe only after public-entry profile and fallback / dispatch planning.
