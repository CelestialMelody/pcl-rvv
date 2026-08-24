# ONI Grabber Phase Index

| phase | 状态 | 入口 | 结果 | 默认恢复动作 |
| --- | --- | --- | --- | --- |
| `000-current-state-and-diagnostic-scaffold` | complete / positive diagnostic | `000-current-state-and-diagnostic-scaffold/plan.zh.md` | `000-current-state-and-diagnostic-scaffold/result.zh.md` | 已进入 Phase 010 production-depth-probe。 |
| `010-production-depth-probe` | adopted production behavior / production-detail positive | `010-production-depth-probe/plan.zh.md` | `010-production-depth-probe/result.zh.md` | 当前 depth-only production patch 已采纳；无值得继续同轮推进的未阻塞优化方向。 |

当前没有 `ready_for_review` 停止位。production-detail（生产内部边界）证据已支持 depth-only `PointXYZ` adopted production behavior（已采纳生产行为）；RGB/RGBA/IR 和完整 ONI replay public-entry 仍需要新 profile、ONI 场景或新候选后才能恢复。
