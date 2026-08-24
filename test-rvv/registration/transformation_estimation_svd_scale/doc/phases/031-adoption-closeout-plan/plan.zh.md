# Phase 031 计划草案：adoption closeout after PI5 confirmation

## Phase 目标和授权边界

本文件是 PI5 之后的 adoption closeout（采纳收尾）执行计划。该计划已在 Phase 031 result 中完成；当前状态见 `result.zh.md`，不再是 `production_patch_positive_pending_user_confirmation`。

历史上若用户选择不采纳或回滚，本计划会作废并改走 rollback/no-production phase；当前用户已确认可接入，Phase 032 不再是默认路径。

## 触发条件

必须同时满足：

| gate | required state |
| --- | --- |
| user confirmation | 用户已明确允许当前有收益实现接入。 |
| production patch | `registration/include/pcl/registration/transformation_estimation_svd_scale.h` 和 `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` 中的 pending patch 仍与 Phase 030 确认包一致。 |
| correctness | 当前或重新执行的 `run_test_compare` Std/RVV 7 tests passed。 |
| registry | `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` fresh。 |
| board evidence | Phase 010 production direct repeated summary 和 Evidence Doctor 仍作为采纳证据；若被覆盖或 stale，先重跑或降级。 |

## 执行步骤

| step | action | completion evidence |
| --- | --- | --- |
| 1 | 重新确认 production diff 和 Phase 030 范围一致。 | `git diff -- registration/include/pcl/registration/transformation_estimation_svd_scale.h registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` 与确认包范围一致。 |
| 2 | 更新 EvidenceDecision。 | README、evaluation、optimization matrix、roadmap、phase index/result 从 `production_patch_positive_pending_user_confirmation` 改为 adopted 的有界状态。 |
| 3 | 创建 / 更新 production 长期主题文档。 | `artifact_layout.topic_doc_template` 解析出的 `doc-rvv` 文档存在，且只写当前 production behavior、dispatch/fallback、证据链和未覆盖范围。 |
| 4 | 同步 topic-local doc suite。 | README、testing overview、benchmark/evidence、optimization evidence、test-support code map、evaluation、phase README、matrix 和 roadmap 均指向 adopted 状态。 |
| 5 | 运行验证。 | `run_test_compare`、`evidence_status`、Markdown relative link check、`git diff --check` 通过。若重跑 board，刷新 summary / doctor / registry。 |
| 6 | 准备提交边界。 | 明确 topic / production / summary evidence / raw logs / unrelated dirty worktree 的拆分；不默认提交 raw logs、build output 或其它 topic。 |

## Production 长期主题文档内容要求

确认采纳后，`artifact_layout.topic_doc_template` 解析出的 production 长期主题文档至少包含：

- 当前采用的优化方式：ordered cloud-pair public overload 下的 `direct-fused-scale-accum`。
- 公开入口和 dispatch：哪些条件命中 RVV，哪些条件回父类 scale path。
- 标量路径对照：父类 false branch 的 centroid、demean matrix、correlation、scale 计算。
- RVV 数据流：source / target xyz sum、cross sum、source square sum、3x3 SVD、scale 和 translation。
- 正确性与高效性证据链：QEMU correctness、public-scale smoke、ASM attribution、board production direct summary、Evidence Doctor warning 解释和 registry freshness。
- 不覆盖范围：source-indexed、dual-indexed、correspondence、`Scalar=double`、非 dense、小规模、退化 source variance、未验证泛型点型 direct evidence。
- 后续扩展队列：generic point type、row source expansion、matrix-local smaller patch alternative。

## 暂停 / 回滚条件

| condition | action |
| --- | --- |
| 用户未确认采纳 | 停在 Phase 030 / Phase 031，不改 adopted 状态。 |
| 用户确认回滚 | 新建 rollback/no-production phase，回滚只限当前 production patch。 |
| correctness 或 registry 失败 | 先修正证据或降级状态，不进入长期文档 closeout。 |
| board evidence stale 或被新结果反向 | 刷新 board evidence / Evidence Doctor，再重新判断是否仍建议采纳。 |
| production diff 被其它改动污染 | 先隔离脏工作区；不得把其它 topic 或 unrelated production patch 混入 closeout。 |

## 默认下一动作

本计划已执行完毕。默认下一动作是 Phase 040 / generic point type 后续 board + ASM phase，或另建 row-source expansion phase。
