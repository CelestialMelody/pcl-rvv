# GICP Phase Index

| phase | status | scope | plan | result | next |
| --- | --- | --- | --- | --- | --- |
| `000-current-state-and-gaps` | done | residual / covariance pre-production diagnostic | `000-current-state-and-gaps/plan.zh.md` | `000-current-state-and-gaps/result.zh.md` | dense residual positive、indexed residual positive、covariance weak_positive；建议 PI1 前置 profile |
| `001-production-profile-prerequisite` | done / stop-for-user-checkpoint | cost-only production probe + Hessian follow-up diagnostic | `001-production-profile-prerequisite/plan.zh.md` | `001-production-profile-prerequisite/result.zh.md` | cost-only public entry neutral；`dfddf-loop-dense` positive，建议转向 `dfddf()` 有界生产探针 |
| `002-dfddf-production-probe` | done / PI5-user-checkpoint | `dfddf()` production probe | `002-dfddf-production-probe/plan.zh.md` | `002-dfddf-production-probe/result.zh.md` | public GICP PointXYZ weak_positive；等待用户确认采纳 / 回滚 / 提交 |
| `003-clean-adoption` | done / production-closeout-ready | 只保留 `dfddfLoopRVV()`，移除 cost-only helper | `003-clean-adoption/plan.zh.md` | `003-clean-adoption/result.zh.md` | clean production public 1024 median `1.080x`、4096 median `1.058x`，均 weak_positive |
| `004-dfddf-gather-width` | done / not-adopted | 尝试 `dfddfLoopRVV()` matrix gather 从 `vluxei64` 改为 `vluxei32` | `004-dfddf-gather-width/plan.zh.md` | `004-dfddf-gather-width/result.zh.md` | 1024 median `1.073x`，未优于 Phase 003；已回退本阶段小改动 |
| `005-rollback-no-production-closeout` | done / no-production | 回退全部 GICP 生产源码 RVV 接入，删除长期生产文档 | `005-rollback-no-production-closeout/plan.zh.md` | `005-rollback-no-production-closeout/result.zh.md` | 生产源码零 diff；topic 可按 no-production closeout 提交 |

默认恢复入口：先检查 `log/evidence_registry.json`、Phase 005 result 和当前 production diff。
当前源码应保持无 GICP RVV 生产补丁；topic 状态为 no-production closeout。
