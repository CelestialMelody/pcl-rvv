# sac_model_normal_sphere Phase 000 board smoke summary

- run_label: `normal-sphere-phase000-board-smoke`
- evidence_role: `production_shaped_diagnostic`.
- A/B boundary: board Std build fallback vs board RVV build candidate for the same test-only helper.
- rerun_budget: `1/5 used`; current buckets are far from threshold for candidate rows, so no automatic rerun was required.
- manifest: `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/000-normal-sphere-count-select-diagnostic/board-evidence-manifest.json`

| case | point type | baseline ms | candidate ms | B/A speedup | bucket | evidence role |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `PointXYZ diagnostic candidate selectWithinDistance` | `PointXYZ + Normal` | 9.8632 | 3.4534 | 2.856x | `positive` | `production_shaped_diagnostic` |
| `PointXYZ diagnostic candidate countWithinDistance` | `PointXYZ + Normal` | 14.6506 | 2.3717 | 6.177x | `positive` | `production_shaped_diagnostic` |
| `PointXYZ diagnostic candidate getDistancesToModel` | `PointXYZ + Normal` | 14.1026 | 2.7212 | 5.182x | `positive` | `production_shaped_diagnostic` |
| `PointXYZI diagnostic candidate selectWithinDistance` | `PointXYZI + Normal` | 9.9449 | 4.1866 | 2.375x | `positive` | `production_shaped_diagnostic` |
| `PointXYZI diagnostic candidate countWithinDistance` | `PointXYZI + Normal` | 14.6162 | 2.8514 | 5.126x | `positive` | `production_shaped_diagnostic` |
| `PointXYZI diagnostic candidate getDistancesToModel` | `PointXYZI + Normal` | 14.2398 | 3.1615 | 4.504x | `positive` | `production_shaped_diagnostic` |

## Public 入口上下文

公开入口当前没有 production RVV dispatch（生产 RVV 分流）。它们的 board 行只用于确认现有 public overload 仍是混合边界上下文，不进入 Evidence Doctor 的候选 B/A 分布检查。

| case | PointXYZ speedup | PointXYZI speedup | role |
| --- | ---: | ---: | --- |
| `public selectWithinDistance` | 1.001x | 0.989x | `mixed_boundary_cross_check` |
| `public countWithinDistance` | 1.002x | 1.001x | `mixed_boundary_cross_check` |
| `public getDistancesToModel` | 1.205x | 1.216x | `mixed_boundary_cross_check` |

## Evidence Doctor 输入边界

这份 summary 来自单次 board smoke（板卡小型验证），不是 repeated board（重复板卡测试）稳定性结论。
候选行的速度信号足够远离 1.0 阈值，可用于 Phase 000 是否进入下一阶段的诊断判断；production 接入仍需要 PI1-PI5。
