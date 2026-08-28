# CGM RGB stencil production direct repeated board summary

- run_label: `cgm_phase050_rgb_stencil_production_direct_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_direct`
- A/B boundary: `public_overload`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `production_process_320x240` | 5 | 1.620x | 1.610x | 1.680x | 1.614x | 1.664x | 0/5 | 1.620x, 1.620x, 1.640x, 1.610x, 1.680x |
| `production_process_641x481_tail` | 5 | 1.590x | 1.580x | 1.660x | 1.584x | 1.636x | 0/5 | 1.590x, 1.600x, 1.590x, 1.580x, 1.660x |

## Evidence boundary

本 summary 是 Phase 050 production direct（真实生产路径）证据。Std/RVV 两侧都通过 `ColorGradientModality<PointXYZRGB>::processInputData()` 公开入口计时；计时边界包含 RGB copy、Gaussian convolution、Gaussian 后 color-gradient Std/RVV 链路和 spread，不包含 feature extraction。该证据只证明当前 public RVV path 是否快于当前 public scalar path；本轮用户已授权接入后板卡有收益即可采纳，因此该 summary 可作为 Phase 050 adopted production behavior 的生产性能依据。
