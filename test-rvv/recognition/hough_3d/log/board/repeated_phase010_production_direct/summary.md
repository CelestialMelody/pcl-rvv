# Hough3D Phase 010 Production Direct Board Summary

## Context

- run_label: `hough_3d_phase010_production_direct_repeated`
- dataset: `synthetic Hough3D production direct`
- vote_count: `65536`
- iterations: `30`
- warmup_iterations: `3`
- evidence_role: `production_direct`

## Results

| case | runs | median | min | max | p10 | p90 | B/A < 1 | decision_bucket | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `hough_3d_production_hough_voting` | 5 | 0.995x | 0.988x | 0.996x | 0.990x | 0.996x | 5/5 | neutral | 0.988x, 0.993x, 0.996x, 0.995x, 0.996x |

## Evidence Doctor Input Notes

- asm_path: `test-rvv/recognition/hough_3d/build/asm/riscv/bench_hough_3d_rvv.full.asm`
- asm_rvv_instr_count: `15770`
- checksum: `8587619939972507824`
- notes: Production row compares Std build scalar houghVoting with RVV build houghVoting after the vote-generation RVV patch; accumulator scatter remains scalar in both builds.
