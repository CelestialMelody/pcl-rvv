# CGM production direct repeated board summary

- run_label: `cgm_phase030_production_direct_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_direct`
- A/B boundary: `public_overload`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `production_process_320x240` | 5 | 1.490x | 1.450x | 1.510x | 1.454x | 1.502x | 0/5 | 1.490x, 1.490x, 1.510x, 1.450x, 1.460x |
| `production_process_641x481_tail` | 5 | 1.540x | 1.530x | 1.570x | 1.534x | 1.562x | 0/5 | 1.540x, 1.550x, 1.570x, 1.540x, 1.530x |

## Evidence boundary

本 summary 是 Phase 030 production direct（真实生产路径）证据。Std/RVV 两侧都通过 `ColorGradientModality<PointXYZRGB>::processInputData()` 公开入口计时；计时边界包含 RGB copy、Gaussian convolution、Gaussian 后 color-gradient Std/RVV 链路和 spread，不包含 feature extraction。该证据只证明当前 public RVV path 是否快于当前 public scalar path；PI5 仍需用户确认是否采纳或回滚 production patch。
