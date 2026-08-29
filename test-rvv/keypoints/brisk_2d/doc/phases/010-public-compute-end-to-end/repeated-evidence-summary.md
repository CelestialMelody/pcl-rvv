# brisk_2d Phase 010 public compute repeated board summary

- repeat_root: `log/board/repeated-phase010-public-compute`
- evidence_role: `production_mixed_detail_and_public`
- A/B boundary: `production detail helper / ScaleSpace helper chain / public BriskKeypoint2D::compute`
- collected_runs: `5`
- asm_rvv_line_count: `4717`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `brisk_halfsample_640x480` | 5 | 1.047x | 1.032x | 1.054x | 1.037x | 1.051x | 0/5 | 1.047x, 1.047x, 1.054x, 1.043x, 1.032x |
| `brisk_halfsample_641x481_tail` | 5 | 1.039x | 1.035x | 1.053x | 1.036x | 1.048x | 0/5 | 1.040x, 1.039x, 1.053x, 1.037x, 1.035x |
| `brisk_twothirdsample_640x480` | 5 | 1.108x | 1.099x | 1.112x | 1.099x | 1.111x | 0/5 | 1.099x, 1.110x, 1.112x, 1.100x, 1.108x |
| `brisk_construct_pyramid_640x480` | 5 | 1.136x | 1.122x | 1.140x | 1.127x | 1.140x | 0/5 | 1.134x, 1.140x, 1.140x, 1.136x, 1.122x |
| `brisk_public_compute_320x240` | 5 | 1.017x | 1.014x | 1.023x | 1.014x | 1.023x | 0/5 | 1.023x, 1.022x, 1.015x, 1.014x, 1.017x |
