# brisk_2d Phase 000 repeated board summary

- repeat_root: `log/board/repeated-phase000-downsample`
- evidence_role: `production_direct`
- A/B boundary: `production detail helper / ScaleSpace helper chain`
- collected_runs: `5`
- asm_rvv_line_count: `4717`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `brisk_halfsample_640x480` | 5 | 1.048x | 1.017x | 1.088x | 1.022x | 1.082x | 0/5 | 1.030x, 1.072x, 1.048x, 1.088x, 1.017x |
| `brisk_halfsample_641x481_tail` | 5 | 1.041x | 1.000x | 1.051x | 1.008x | 1.047x | 0/5 | 1.051x, 1.000x, 1.041x, 1.042x, 1.021x |
| `brisk_twothirdsample_640x480` | 5 | 1.115x | 1.105x | 1.140x | 1.106x | 1.138x | 0/5 | 1.133x, 1.105x, 1.107x, 1.140x, 1.115x |
| `brisk_construct_pyramid_640x480` | 5 | 1.175x | 1.125x | 1.200x | 1.129x | 1.193x | 0/5 | 1.175x, 1.182x, 1.135x, 1.200x, 1.125x |
