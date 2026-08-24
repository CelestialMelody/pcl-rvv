# Phase 032 计划草案：rollback / no-production after PI5 rejection

## Phase 目标和授权边界

本文件是 PI5 之后的 rollback/no-production（回滚 / 不接入生产）历史备选计划草案。用户已确认当前有收益实现可以接入，Phase 031 已完成 adoption closeout；因此本计划当前为 archived_not_applicable_after_adoption，不主动回滚、不删除 topic-local tests / docs / evidence，也不把当前 topic 写成 no-production。

回滚授权只覆盖当前 topic 的 production patch：

- `registration/include/pcl/registration/transformation_estimation_svd_scale.h`
- `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp`

不得回滚其它 topic、其它 registration production patch、agent instruction commits、`test-rvv/registration/transformation_estimation_svd_scale/**` 的测试资产，或 unrelated dirty worktree。

## 触发条件

必须有明确用户输入，例如“不采纳”“回滚当前 production patch”“不要保留这个源码接入”或等价表达。用户只说“继续”“看看别的优化”或“暂不确定”不构成回滚授权。

## 执行步骤

| step | action | completion evidence |
| --- | --- | --- |
| 1 | 冻结当前 production diff。 | 保存 / 引用 Phase 030 确认包和当前 `git diff -- registration/include/pcl/registration/transformation_estimation_svd_scale.h registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp`。 |
| 2 | 只回滚当前 production patch。 | 两个 production 源文件回到进入 PI2 前状态；不碰其它 dirty 文件。 |
| 3 | 保留 topic-local probe 资产。 | `test-rvv/registration/transformation_estimation_svd_scale/**` 保留为 diagnostic / production-probe record。 |
| 4 | 更新 EvidenceDecision。 | README、evaluation、optimization matrix、roadmap、phase index/result 从 pending 改为 rollback/no-production，并说明 positive evidence 未被采纳的原因来自用户决策或维护边界。 |
| 5 | 确认 production 长期主题文档不适用。 | 不创建 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档；若已有遗留文档，标记 / 删除策略另审。 |
| 6 | 运行验证。 | 至少 `run_test_compare` 证明 test-only diagnostic 仍可运行；`evidence_status`、Markdown relative link check、`git diff --check` 通过。 |
| 7 | 更新后续 diagnostic 队列。 | `matrix-local-scale-simplification` 可作为较小 patch 备选；generic point type / row source expansion 只有在有新的 production candidate 时恢复。 |

## 回滚后文档口径

回滚后不能把 Phase 010 的 positive production direct evidence 写成 adopted，也不能把回滚写成“证据失败”。推荐口径：

- `direct-fused-scale-accum`：`rollback_no_production_by_user_decision` 或等价状态。
- production evidence role：historical production probe（历史生产探针）。
- diagnostic evidence role：Phase 000 / Phase 020 继续保留。
- production_topic_doc：`not_applicable with evidence`。
- next candidates：若用户仍想要较小 patch，可恢复 `matrix-local-production-probe`；否则 topic 停在 no-production closeout。

## 不得执行的动作

- 不得用 `git reset --hard` 或仓库级 checkout 回滚。
- 不得删除 `test-rvv/registration/transformation_estimation_svd_scale/doc`、`src`、`include`、`script` 或 summary evidence。
- 不得把 unrelated `transformation_estimation_2D`、`gicp`、`ndt` 脏状态纳入当前回滚。
- 不得在没有用户明确回滚授权时撤销当前 production patch。

## 默认下一动作

等待用户 PI5 选择。若用户确认采纳，执行 Phase 031；若用户确认不采纳 / 回滚，执行本 Phase 032；若用户授权 pending 下继续 diagnostic，另建独立 phase，不改变本计划状态。
