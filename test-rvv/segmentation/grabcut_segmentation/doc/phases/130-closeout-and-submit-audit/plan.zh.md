# Phase 130 closeout and submit audit 计划

## 阶段意图和边界

本阶段只做 topic closeout（主题收尾）和 commit preparation（提交准备）审计。阶段目标是确认
Phase 120 的停止条件仍成立，检查已采纳 production patch（生产补丁）是否符合 RVV dispatch /
fallback（分流 / 回退）规范，检查 topic-local doc suite（主题本地文档套件）和
`doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` 是否足够支撑结束当前 topic，并按
summary-only（只提交摘要）证据策略准备提交。

本阶段不新增 RVV helper，不扩大到 n-link、color staging、non-organized KNN、GaussianFitter
accumulation、LMUL / ILP variants 或 max-flow solver。若发现文档或提交边界缺口，只修正当前
topic 的文档、Handoff 或提交候选集；不修改 `.agents/` 指令资产。

## 当前状态清单

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| production behavior | `initGraph()` unknown terminal helper 和 `learnGMMs()` assignment helper 已由用户确认采纳。 | `doc/phases/060-production-initgraph-terminal-evidence/result.zh.md`、`doc/phases/110-learn-gmms-production-integration-plan/result.zh.md` |
| post-adoption profile | Phase 120 显示 `public_extract_profile` median B/A `1.290653x`，但 `learn_gmms` 剩余占比 `4.592618%`，不触发新 helper。 | `doc/phases/120-post-learn-gmms-adoption-profile/result.zh.md` |
| correctness | Std 8/8、RVV 10/10 passed。 | `make -C test-rvv/segmentation/grabcut_segmentation run_test_compare` |
| evidence registry | `fresh`。 | `make -C test-rvv/segmentation/grabcut_segmentation evidence_status` |
| dirty isolation | 当前 diff 限定在 GrabCut production、topic-local test-rvv、`doc-rvv` 主题文档和 segmentation 复筛表。 | `git status --short --untracked-files=all -- <topic paths>` |

## 审计动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| production dispatch review | 读取 `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` 和 `segmentation/src/grabcut_segmentation.cpp` diff。 | public API 不变；RVV helper 在 `__RVV10__` 内；公开入口是短路 RVV 尝试加 Std fallback；小输入和固定标签 fallback 有测试。 |
| doc-suite closeout review | 检查 README、evaluation、roadmap、phase index、matrix、长期 `doc-rvv`。 | 每个文档 role 有主归属或合并归属；长期文档只保存 adopted production behavior 和证据链。 |
| freshness review | 运行 `run_test_compare`、`evidence_status`、`git diff --check`。 | correctness、registry 和 whitespace 均通过。 |
| artifact tracking review | 路径限定扫描 staged / untracked 文件。 | 提交候选明确排除 build、`log/board`、`log/qemu`、`repeated-board-*/*.log` 和本地 Handoff。 |
| commit preparation | path-limited staging 和 commit。 | commit 只包含 production 源码、topic 测试资产、文档、manifest / Doctor / registry 摘要证据和复筛状态。 |

## Evidence Doctor 和 Registry 规则

本阶段不生成新的板卡数据。生产性能结论只引用已登记的 Phase 060、Phase 070、Phase 110 和 Phase 120
manifest / Evidence Doctor（证据体检）摘要。提交前必须重新运行 `evidence_status`，确保 summary
artifact 没有未登记覆盖或 stale doc（过期文档）风险。

## 继续 / 停止条件

若审计发现当前授权范围内仍有未阻塞的文档结构、生产分发、证据登记或提交边界缺口，则先补齐再提交。
若只剩新输入形态、其它点类型、`Scalar=double`、n-link、color staging、non-organized KNN、max-flow
或算法级优化方向，则结束当前 topic；这些方向需要新 profile、新 scope 或另开 topic。

## 提交策略

commit preference（提交偏好）冻结为 `topic-only + summary evidence`：提交 production 源码、topic
测试/bench 支撑、phase/evaluation/长期文档、manifest / Doctor / registry 摘要证据和复筛状态；不提交
raw logs（原始日志）、build 输出、私有板卡参数、本地 Handoff 或 `.agents/` 指令资产。
