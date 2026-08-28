# sac_model_normal_sphere Phase 010 vcompress select ablation summary

- run_label: `normal-sphere-phase010-vcompress-board-smoke`
- evidence_role: `component_ablation`.
- A/B boundary: board RVV scalar-writeback select helper vs board RVV vcompress select helper for Phase 010 family comparison.
- rerun_budget: `1/5 used`; current buckets are far from threshold for candidate rows, so no automatic rerun was required.
- manifest: `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/010-vcompress-select-ablation/board-evidence-manifest.json`

| case | point type | baseline ms | candidate ms | B/A speedup | bucket | evidence role |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `PointXYZ diagnostic candidate selectWithinDistance` | `PointXYZ + Normal` | 9.7318 | 3.4133 | 2.851x | `positive` | `production_shaped_diagnostic` |
| `PointXYZ diagnostic candidate vcompress selectWithinDistance` | `PointXYZ + Normal` | 9.7316 | 2.7178 | 3.581x | `positive` | `component_ablation` |
| `PointXYZ diagnostic candidate countWithinDistance` | `PointXYZ + Normal` | 14.5984 | 2.3462 | 6.222x | `positive` | `production_shaped_diagnostic` |
| `PointXYZ diagnostic candidate getDistancesToModel` | `PointXYZ + Normal` | 14.1068 | 2.6772 | 5.269x | `positive` | `production_shaped_diagnostic` |
| `PointXYZ vcompress select vs scalar-writeback select` | `PointXYZ + Normal` | 3.4133 | 2.7178 | 1.256x | `positive` | `component_ablation` |
| `PointXYZI diagnostic candidate selectWithinDistance` | `PointXYZI + Normal` | 9.7171 | 3.7140 | 2.616x | `positive` | `production_shaped_diagnostic` |
| `PointXYZI diagnostic candidate vcompress selectWithinDistance` | `PointXYZI + Normal` | 9.7057 | 3.1470 | 3.084x | `positive` | `component_ablation` |
| `PointXYZI diagnostic candidate countWithinDistance` | `PointXYZI + Normal` | 14.5858 | 2.8169 | 5.178x | `positive` | `production_shaped_diagnostic` |
| `PointXYZI diagnostic candidate getDistancesToModel` | `PointXYZI + Normal` | 14.0765 | 2.9815 | 4.721x | `positive` | `production_shaped_diagnostic` |
| `PointXYZI vcompress select vs scalar-writeback select` | `PointXYZI + Normal` | 3.7140 | 3.1470 | 1.180x | `weak_positive` | `component_ablation` |

## Public 入口上下文

公开入口当前没有 production RVV dispatch（生产 RVV 分流）。它们的 board 行只用于确认现有 public overload 仍是混合边界上下文，不进入 Evidence Doctor 的候选 B/A 分布检查。

| case | PointXYZ speedup | PointXYZI speedup | role |
| --- | ---: | ---: | --- |
| `public selectWithinDistance` | 1.003x | 0.997x | `mixed_boundary_cross_check` |
| `public countWithinDistance` | 1.004x | 0.998x | `mixed_boundary_cross_check` |
| `public getDistancesToModel` | 1.210x | 1.228x | `mixed_boundary_cross_check` |

## Evidence Doctor 输入边界

这份 summary 来自单次 board smoke（板卡小型验证），不是 repeated board（重复板卡测试）稳定性结论。
Phase 010 的写回实现族 B/A 可用于判断 `vcompress` 是否进入 PI1 审计；production 接入仍需要 PI1-PI5。
