# GICP production public PointXYZ board repeated summary

- run_label: `production_public_align_pointxyz_repeated`
- expected_runs: `5`
- collected_runs: `5`
- device: `Milkv-Jupiter`
- evidence_role: `production_public`
- A/B boundary: `public GICP entry`
- decision_question: `RVV-vs-scalar production public GICP PointXYZ align`

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `production-public-align-pointxyz` | 5 | 1.019x | 1.014x | 1.030x | 0/5 | `neutral` |

## Evidence boundary

This summary is production-public evidence for the explicit benchmarked
PointXYZ entry only. It does not prove generic point-type coverage or
clean adoption without PI5 user confirmation.
