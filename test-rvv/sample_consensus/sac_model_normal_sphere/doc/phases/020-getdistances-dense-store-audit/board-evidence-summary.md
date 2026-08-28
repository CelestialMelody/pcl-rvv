# sac_model_normal_sphere Phase 020 getDistances dense-store audit summary

- run_label: `normal-sphere-phase020-getdistances-board-smoke`
- evidence_role: `production_shaped_diagnostic`.
- A/B boundary: board Std build fallback vs board RVV build getDistances test-only helper for dense double output.
- rerun_budget: `1/5 used`; current buckets are far from threshold for candidate rows, so no automatic rerun was required.
- manifest: `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/020-getdistances-dense-store-audit/board-evidence-manifest.json`

| case | point type | baseline ms | candidate ms | B/A speedup | bucket | evidence role |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `PointXYZ diagnostic candidate getDistancesToModel` | `PointXYZ + Normal` | 14.2753 | 2.7685 | 5.156x | `positive` | `production_shaped_diagnostic` |
| `PointXYZI diagnostic candidate getDistancesToModel` | `PointXYZI + Normal` | 14.1455 | 3.1257 | 4.526x | `positive` | `production_shaped_diagnostic` |

## Public 入口上下文

公开入口当前没有 production RVV dispatch（生产 RVV 分流）。它们的 board 行只用于确认现有 public overload 仍是混合边界上下文，不进入 Evidence Doctor 的候选 B/A 分布检查。

| case | PointXYZ speedup | PointXYZI speedup | role |
| --- | ---: | ---: | --- |
| `public selectWithinDistance` | n/a | n/a | `mixed_boundary_cross_check` |
| `public countWithinDistance` | n/a | n/a | `mixed_boundary_cross_check` |
| `public getDistancesToModel` | 1.220x | 1.225x | `mixed_boundary_cross_check` |

## Evidence Doctor 输入边界

这份 summary 来自单次 board smoke（板卡小型验证），不是 repeated board（重复板卡测试）稳定性结论。
Phase 020 只判断 dense double store 诊断候选是否仍值得保留；即使正向，也不能替代 production direct（真实生产路径证据）。
