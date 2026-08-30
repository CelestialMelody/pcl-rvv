# ISS 3D scatter f64 diagnostic repeated board summary

- run_label: `iss_3d_phase000_scatter_f64_repeated`
- expected_runs: `5`
- collected_runs: `5`
- iterations: `20`
- warmup_iterations: `3`
- evidence_role: `diagnostic`
- A/B boundary: `test_helper`
- asm_rvv_line_count: `20`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `scatter_contiguous_256` | 5 | 1.868x | 1.849x | 1.891x | 1.856x | 1.888x | 0/5 | 1.868x, 1.884x, 1.865x, 1.891x, 1.849x |
| `scatter_indexed_256` | 5 | 1.629x | 0.691x | 1.684x | 1.063x | 1.678x | 1/5 | 0.691x, 1.629x, 1.684x, 1.670x, 1.621x |
| `scatter_indexed_tail_73` | 5 | 1.228x | 1.214x | 1.241x | 1.220x | 1.236x | 0/5 | 1.228x, 1.228x, 1.241x, 1.214x, 1.230x |
