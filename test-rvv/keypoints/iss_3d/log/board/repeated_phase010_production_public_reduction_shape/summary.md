# ISS 3D production public repeated board summary

- run_label: `iss_3d_phase010_production_public_reduction_shape`
- expected_runs: `5`
- collected_runs: `5`
- iterations: `5`
- warmup_iterations: `1`
- evidence_role: `production_public`
- A/B boundary: `public_compute`
- asm_rvv_line_count: `609`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `public_iss_3d_grid_4096` | 5 | 1.014x | 1.005x | 1.022x | 1.007x | 1.021x | 0/5 | 1.010x, 1.005x, 1.022x, 1.014x, 1.020x |
