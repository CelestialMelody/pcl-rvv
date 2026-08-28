# sac_model_normal_sphere Phase 030 RGB/RGBA point-type expansion summary

- run_label: `normal-sphere-phase030-rgb-rgba-smoke`
- evidence_role: `production_shaped_diagnostic`.
- A/B boundary: board Std build fallback vs board RVV build test-only candidates for RGB/RGBA source layouts.
- rerun_budget: `1/5 used`; current buckets are far from threshold for candidate rows, so no automatic rerun was required.
- manifest: `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/030-rgb-rgba-point-type-expansion/board-evidence-manifest.json`

| case | point type | baseline ms | candidate ms | B/A speedup | bucket | evidence role |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `PointXYZRGB diagnostic candidate selectWithinDistance` | `PointXYZRGB + Normal` | 9.7830 | 3.9895 | 2.452x | `positive` | `production_shaped_diagnostic` |
| `PointXYZRGB diagnostic candidate vcompress selectWithinDistance` | `PointXYZRGB + Normal` | 9.7836 | 3.3626 | 2.910x | `positive` | `component_ablation` |
| `PointXYZRGB diagnostic candidate countWithinDistance` | `PointXYZRGB + Normal` | 14.6652 | 2.7522 | 5.329x | `positive` | `production_shaped_diagnostic` |
| `PointXYZRGB diagnostic candidate getDistancesToModel` | `PointXYZRGB + Normal` | 14.2316 | 3.1502 | 4.518x | `positive` | `production_shaped_diagnostic` |
| `PointXYZRGBA diagnostic candidate selectWithinDistance` | `PointXYZRGBA + Normal` | 9.7423 | 4.1918 | 2.324x | `positive` | `production_shaped_diagnostic` |
| `PointXYZRGBA diagnostic candidate vcompress selectWithinDistance` | `PointXYZRGBA + Normal` | 9.7479 | 3.2767 | 2.975x | `positive` | `component_ablation` |
| `PointXYZRGBA diagnostic candidate countWithinDistance` | `PointXYZRGBA + Normal` | 14.6551 | 2.7018 | 5.424x | `positive` | `production_shaped_diagnostic` |
| `PointXYZRGBA diagnostic candidate getDistancesToModel` | `PointXYZRGBA + Normal` | 14.2459 | 3.1290 | 4.553x | `positive` | `production_shaped_diagnostic` |

## Public 入口上下文

公开入口当前没有 production RVV dispatch（生产 RVV 分流）。它们的 board 行只用于确认现有 public overload 仍是混合边界上下文，不进入 Evidence Doctor 的候选 B/A 分布检查。

| case | PointXYZRGB speedup | PointXYZRGBA speedup | role |
| --- | ---: | ---: | --- |
| `public selectWithinDistance` | 0.995x | 0.983x | `mixed_boundary_cross_check` |
| `public countWithinDistance` | 0.998x | 0.995x | `mixed_boundary_cross_check` |
| `public getDistancesToModel` | 0.992x | 1.010x | `mixed_boundary_cross_check` |

## Evidence Doctor 输入边界

这份 summary 来自单次 board smoke（板卡小型验证），不是 repeated board（重复板卡测试）稳定性结论。
Phase 030 只判断 RGB/RGBA source layout 是否能加入测试专用候选范围；即使正向，也不能替代 production direct（真实生产路径证据）。
