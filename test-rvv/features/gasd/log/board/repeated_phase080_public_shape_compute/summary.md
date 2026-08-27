# GASD public shape compute profile repeated board summary

- run_label: `gasd_phase080_public_shape_compute_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production-public profile`
- A/B boundary: `public compute benchmark`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `public_gasd_shape_compute` | 5 | 1.220x | 1.210x | 1.220x | 1.214x | 1.220x | 0/5 | 1.210x, 1.220x, 1.220x, 1.220x, 1.220x |

## Evidence boundary

这是未接 production（生产源码）的性能证据。Std/RVV 两侧共享同一个 bench wrapper；若 case 是 public compute profile，生产源码相同，结果只能作为 build-level profile signal（构建层级性能信号）。计时只覆盖 manifest 中的 timer boundary。
