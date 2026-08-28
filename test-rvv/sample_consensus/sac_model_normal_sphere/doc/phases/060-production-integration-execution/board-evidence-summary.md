# sac_model_normal_sphere Phase 060 production public integration summary

- run_label: `normal-sphere-phase060-production-public`
- evidence_role: `production_public`.
- A/B boundary: board Std build public overload vs board RVV build public overload after production dispatch integration.
- rerun_budget: `5/5 used`; repeated board buckets are stable, so no extra rerun was required.
- manifest: `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/060-production-integration-execution/board-evidence-manifest.json`

| case | point type | baseline ms | candidate ms | B/A speedup | bucket | evidence role |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `PointXYZ public selectWithinDistance` | `PointXYZ + Normal` | 8.2819 | 2.7093 | 3.039x | `positive` | `production_public` |
| `PointXYZ public countWithinDistance` | `PointXYZ + Normal` | 7.5855 | 2.2804 | 3.332x | `positive` | `production_public` |
| `PointXYZ public getDistancesToModel` | `PointXYZ + Normal` | 11.0847 | 2.4861 | 4.456x | `positive` | `production_public` |
| `PointXYZI public selectWithinDistance` | `PointXYZI + Normal` | 8.2453 | 2.8430 | 2.900x | `positive` | `production_public` |
| `PointXYZI public countWithinDistance` | `PointXYZI + Normal` | 7.6386 | 2.2700 | 3.358x | `positive` | `production_public` |
| `PointXYZI public getDistancesToModel` | `PointXYZI + Normal` | 11.1768 | 2.6948 | 4.157x | `positive` | `production_public` |
| `PointXYZRGB public selectWithinDistance` | `PointXYZRGB + Normal` | 8.2390 | 2.7747 | 2.973x | `positive` | `production_public` |
| `PointXYZRGB public countWithinDistance` | `PointXYZRGB + Normal` | 7.5953 | 2.2915 | 3.308x | `positive` | `production_public` |
| `PointXYZRGB public getDistancesToModel` | `PointXYZRGB + Normal` | 11.1085 | 2.5904 | 4.290x | `positive` | `production_public` |
| `PointXYZRGBA public selectWithinDistance` | `PointXYZRGBA + Normal` | 8.2208 | 2.8293 | 2.906x | `positive` | `production_public` |
| `PointXYZRGBA public countWithinDistance` | `PointXYZRGBA + Normal` | 7.5989 | 2.2367 | 3.411x | `positive` | `production_public` |
| `PointXYZRGBA public getDistancesToModel` | `PointXYZRGBA + Normal` | 11.0953 | 2.6681 | 4.161x | `positive` | `production_public` |

## Evidence Doctor 输入边界

这份 summary 来自 5-run repeated board（重复板卡测试）中位数，用于支撑当前 production-public（真实公开入口）性能结论。
Phase 060 使用接入后的真实 public entry Std/RVV 板卡对比；若 correctness、asm 和 Evidence Doctor 无 Error 且速度桶为正向，可支撑本阶段生产采纳判断。
