# Hough3D Phase 030 Default Distance Weight Board Summary

## Context

- run_label: `hough_3d_phase030_default_distance_weight_repeated`
- dataset: `synthetic Hough3D production direct`
- vote_count: `65536`
- iterations: `30`
- warmup_iterations: `3`
- evidence_role: `production_direct`

## Results

| case | runs | median | min | max | p10 | p90 | B/A < 1 | decision_bucket | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `hough_3d_default_distance_weight_hough_voting` | 5 | 1.005x | 0.997x | 1.008x | 0.999x | 1.007x | 1/5 | neutral | 1.002x, 1.008x, 1.005x, 0.997x, 1.006x |

## Evidence Doctor Input Notes

- asm_path: `test-rvv/recognition/hough_3d/build/asm/riscv/bench_hough_3d_rvv.full.asm`
- asm_rvv_instr_count: `15557`
- checksum: `8587619939972507824`
- notes: Default-distance-weight row checks the class default use_distance_weight=false boundary so phase 010's non-default weighted result is not over-generalized.
