# Phase 030 Plan: rollback/no-production closeout

## 阶段意图和边界

本阶段只做 closeout（收尾）和提交前一致性检查，不再尝试新的 RVV（RISC-V Vector，可变长向量扩展）实现族。目标是把 Phase 010 留在工作区的 `selectWithinDistance` production patch（生产补丁）回滚，把 topic-local 文档、optimization matrix（优化矩阵）和筛选表刷新为 `rollback/no-production`，并确认当前源码没有 adopted production behavior（已采纳生产行为）。

本阶段不修改其它 sample_consensus topic，不创建正式 `doc-rvv/sample_consensus/sac_model_circle3d-RVV.zh.md`，不提交 raw board logs（原始板卡日志）或本地 work log（工作日志）。

## 当前状态清单

| item | 状态 |
| --- | --- |
| Phase 000 | count candidate B/A mean `0.5713`，5/5 退化；select test-only candidate B/A mean `1.1294`，只能作为生产探针输入。 |
| Phase 010 | `PointXYZ` production-public 10-run B/A mean `0.9748`，median `0.9991`，5/10 退化；Evidence Doctor 为 Errors=1、Warnings=1、Suggestions=0。 |
| Phase 020 | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 均负向并拒绝扩展。 |
| production source | `sac_model_circle3d.hpp` 中的 RVV helper 和 dispatch 需要回滚到标量公开入口。 |

## 动作计划

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| rollback production patch | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | `git diff -- <file>` 为空；生产源码没有本 topic RVV helper、dispatch 或 noinline 宏。 |
| update tests | `src/test_sac_model_circle3d.cpp` | correctness 不再直接调用已回滚 production helper；Std/RVV 构建仍能对拍公开入口和测试专用 reference。 |
| update docs | README、evaluation、roadmap、matrix、phase result、筛选表和 Handoff | 所有当前状态统一为 `rollback/no-production`，历史 production evidence 明确标为 historical evidence（历史证据）。 |
| validation | `run_test_compare`、`check_projection_asm`、evidence status、`git diff --check` | 全部通过；生产 asm gate 不再适用。 |

## 继续 / 停止条件

若验证通过且 roadmap / matrix 没有当前 topic 授权范围内、未阻塞且值得推进的同边界方向，本 topic 结束。`getDistancesToModel` 的 lambda sign audit（符号语义审计）和新实现族探索属于未来新 phase / 新 topic，不作为当前 closeout 的阻塞项。
