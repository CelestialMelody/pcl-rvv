# Hough3D Phase 020 No-Interpolation Board Summary

## Context

- run_label: `hough_3d_phase020_no_interpolation_repeated`
- dataset: `synthetic Hough3D production direct`
- vote_count: `65536`
- iterations: `30`
- warmup_iterations: `3`
- evidence_role: `production_direct`

## Results

| case | runs | median | min | max | p10 | p90 | B/A < 1 | decision_bucket | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `hough_3d_no_interpolation_hough_voting` | 3 | 1.002x | 1.000x | 1.049x | 1.001x | 1.040x | 0/3 | neutral | 1.000x, 1.049x, 1.002x |

## Evidence Doctor Input Notes

- asm_path: `test-rvv/recognition/hough_3d/build/asm/riscv/bench_hough_3d_rvv.full.asm`
- asm_rvv_instr_count: `15557`
- checksum: `8587619939972507824`
- notes: No-interpolation row checks whether the simpler HoughSpace::vote path exposes vote-generation RVV benefit once voteInt's 27-neighbor interpolation is removed.
