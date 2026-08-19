# GICP production public PointXYZ board repeated summary

- run_label: `production_public_align_pointxyz_dfddf_4096_repeated`
- expected_runs: `3`
- collected_runs: `3`
- device: `Milkv-Jupiter`
- evidence_role: `production_public`
- A/B boundary: `public GICP entry`
- decision_question: `RVV-vs-scalar production public GICP PointXYZ align`

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `production-public-align-pointxyz` | 3 | 1.070x | 1.068x | 1.073x | 0/3 | `weak_positive` |

## Evidence boundary

This summary is production-public evidence for the explicit benchmarked
PointXYZ entry only. It does not prove generic point-type coverage or
clean adoption without PI5 user confirmation.
