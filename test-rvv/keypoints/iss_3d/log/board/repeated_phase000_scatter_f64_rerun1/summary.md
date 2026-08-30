# ISS 3D scatter f64 diagnostic repeated board summary rerun1

- run_label: `iss_3d_phase000_scatter_f64_rerun1`
- expected_runs: `5`
- collected_runs: `5`
- iterations: `20`
- warmup_iterations: `3`
- evidence_role: `diagnostic`
- A/B boundary: `test_helper`
- asm_rvv_line_count: `20`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `scatter_contiguous_256` | 5 | 1.859x | 1.845x | 1.865x | 1.849x | 1.864x | 0/5 | 1.865x, 1.859x, 1.862x, 1.855x, 1.845x |
| `scatter_indexed_256` | 5 | 1.662x | 1.616x | 1.674x | 1.623x | 1.672x | 0/5 | 1.674x, 1.633x, 1.670x, 1.662x, 1.616x |
| `scatter_indexed_tail_73` | 5 | 1.213x | 1.209x | 1.252x | 1.210x | 1.251x | 0/5 | 1.251x, 1.209x, 1.213x, 1.213x, 1.252x |
