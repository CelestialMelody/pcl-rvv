# GICP production public PointXYZ board repeated summary

- run_label: `production_public_align_pointxyz_dfddf_gather32_repeated`
- expected_runs: `5`
- collected_runs: `5`
- device: `Milkv-Jupiter`
- evidence_role: `production_public`
- A/B boundary: `public GICP entry`
- decision_question: `RVV-vs-scalar production public GICP PointXYZ align`

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `production-public-align-pointxyz` | 5 | 1.073x | 1.061x | 1.096x | 0/5 | `weak_positive` |

## Evidence boundary

This summary is production-public evidence for the explicit benchmarked
PointXYZ entry only. It does not prove generic point-type coverage or
clean adoption without PI5 user confirmation.
