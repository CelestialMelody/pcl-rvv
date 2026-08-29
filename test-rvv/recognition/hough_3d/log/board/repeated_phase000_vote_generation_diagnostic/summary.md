# Hough3D Phase 000 Repeated Board Summary

## Context

- run_label: `hough_3d_phase000_vote_generation_repeated`
- dataset: `synthetic Hough3D vote generation`
- vote_count: `65536`
- iterations: `200`
- warmup_iterations: `5`
- evidence_role: `production_shaped_diagnostic`

## Results

| case | runs | median | min | max | p10 | p90 | B/A < 1 | decision_bucket | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `hough_3d_vote_generation_candidate` | 5 | 1.076x | 1.053x | 1.093x | 1.061x | 1.092x | 0/5 | weak_positive | 1.091x, 1.072x, 1.053x, 1.093x, 1.076x |

## Evidence Doctor Input Notes

- asm_path: `test-rvv/recognition/hough_3d/build/asm/riscv/bench_hough_3d_rvv.full.asm`
- asm_rvv_instr_count: `137`
- checksum: `8587619939972507824`
