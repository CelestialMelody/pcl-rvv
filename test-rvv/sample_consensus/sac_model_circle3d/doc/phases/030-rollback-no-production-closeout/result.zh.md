# Phase 030 Result: rollback/no-production closeout

## 执行范围

本阶段只做收尾：确认 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` 已回滚到标量公开入口，刷新 topic-local 文档和交接包到 `rollback/no-production`，并验证当前 topic 没有可继续推进的同边界 RVV（RISC-V Vector，可变长向量扩展）方向。

本阶段不新增生产 helper，不新增新的 board（板卡）跑分，不创建正式 `doc-rvv/sample_consensus/sac_model_circle3d-RVV.zh.md`。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| production rollback | done | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | `selectWithinDistance` 的未采纳 production RVV patch 已移除，源码回到标量路径。 |
| test fallback refresh | done | `src/test_sac_model_circle3d.cpp` | correctness 仍使用 test-only reference（测试专用参考实现）对拍公开入口；不再直连已回滚 production helper。 |
| doc closeout refresh | done | `README.zh.md`、evaluation、roadmap、matrix、phase 010/020 result、benchmark-and-evidence、testing-overview、optimization-evidence、筛选表、Handoff | 所有当前状态统一为 `rollback/no-production`，历史证据保留但不再是活跃门禁。 |
| validation | done | `run_test_compare`、`check_projection_asm`、`projection_evidence_status`、`select_production_evidence_status`、`select_xyzi_evidence_status`、`select_xyzrgb_evidence_status`、`select_xyzrgba_evidence_status` | 全部通过；生产 asm gate 已不适用当前 topic。 |

## 结论

- `countWithinDistance`：继续保持标量；Phase 000 已 `rejected with evidence`。
- `selectWithinDistance`：生产 RVV 补丁已回滚；当前 topic 结论为 `rollback/no-production`。
- `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA`：只保留历史负向证据，当前公开入口回退为标量。
- `getDistancesToModel`：入口不同，且当前 topic 已收口，不再作为同边界继续推进项。

## Continue / Stop Decision

`continue_stop_decision = phase_closed_with_no_unblocked_next_action`

本 topic 已没有值得继续推进的同边界优化方向。若未来重启，只能从新的 phase plan 开始，不能复用本轮已回滚的 production patch。
