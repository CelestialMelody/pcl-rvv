# ISS 3D production public repeated board summary

- run_label: `iss_3d_phase010_production_public_repeated`
- expected_runs: `5`
- collected_runs: `5`
- iterations: `5`
- warmup_iterations: `1`
- evidence_role: `production_public`
- A/B boundary: `test_helper`
- asm_rvv_line_count: `609`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `public_iss_3d_grid_4096` | 5 | 1.005x | 0.996x | 1.021x | 0.996x | 1.016x | 2/5 | 1.021x, 1.009x, 0.996x, 1.005x, 0.996x |
