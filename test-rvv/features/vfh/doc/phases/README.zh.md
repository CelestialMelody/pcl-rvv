# VFH Phase Index

| phase | status | plan | result | default recovery |
| --- | --- | --- | --- | --- |
| `000-current-state-and-vfh-scaffold` | completed | `000-current-state-and-vfh-scaffold/plan.zh.md` | `000-current-state-and-vfh-scaffold/result.zh.md` | 历史 diagnostic（诊断）阶段已闭合，曾支持进入 PI1 生产接入计划；最终采用以 Phase 060 为准。 |
| `010-production-integration-plan` | completed | `010-production-integration-plan/plan.zh.md` | `010-production-integration-plan/result.zh.md` | PI1 边界已被冻结，后续 Phase 040 已进入 PI2-PI5。 |
| `020-viewpoint-histogram-rvv-diagnostic` | completed | `020-viewpoint-histogram-rvv-diagnostic/plan.zh.md` | `020-viewpoint-histogram-rvv-diagnostic/result.zh.md` | combined SPFH + viewpoint candidate 为 positive diagnostic。 |
| `030-centroid-normal-reduction-rvv-diagnostic` | completed | `030-centroid-normal-reduction-rvv-diagnostic/plan.zh.md` | `030-centroid-normal-reduction-rvv-diagnostic/result.zh.md` | normal centroid reduction combined candidate 为 positive diagnostic。 |
| `040-production-rvv-probe` | completed / superseded | `040-production-rvv-probe/plan.zh.md` | `040-production-rvv-probe/result.zh.md` | production-public（公开生产入口）mean `1.36195x`，但 whole-cloud staging 后续被 Phase 050 取代。 |
| `050-chunk-local-staging` | completed / superseded | `050-chunk-local-staging/plan.zh.md` | `050-chunk-local-staging/result.zh.md` | chunk-local staging mean `1.44115x`，后续被 Phase 060 取代。 |
| `060-rvv-bin-index-precompute` | adopted | `060-rvv-bin-index-precompute/plan.zh.md` | `060-rvv-bin-index-precompute/result.zh.md` | 当前采用形态；post-review production-public 5-run mean `1.63906x`，Evidence Doctor `0E/0W/11S`。 |

当前默认恢复入口：review / commit 准备。当前同一默认 VFH production boundary（生产边界）内没有未阻塞且值得继续的
高优先级优化；histogram scatter（直方图离散累加）并行化、point type expansion（点类型扩展）、
`size_component`、`normalize_distances`、非 dense / subset indices 和 CVFH / OUR-CVFH 调用路径均需另建窄
phase 或 follow-up topic，并重新闭合 correctness、asm、board 和 Evidence Doctor。
