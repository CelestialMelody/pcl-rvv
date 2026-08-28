# Phase 030 Plan: topic closeout and submit readiness

## 阶段意图和边界

本阶段判断 `sac_model_plane` topic 是否可以结束优化工作并进入提交。范围只覆盖当前 topic 的
production（生产源码）分发、fallback（回退路径）、topic-local doc suite（主题本地文档套件）、
长期 `doc-rvv` 文档和提交边界。

本阶段不新增 RVV performance candidate（性能候选），不修改其它 sample_consensus topic，不提交 raw logs
（原始日志）或 build（构建）输出。

## 审计动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| production dispatch audit | `sac_model_plane.h`、`impl/sac_model_plane.hpp` | 公开入口保持“语义检查 -> RVV gate -> Standard fallback”结构；非 RVV、layout 不匹配和 32-bit offset 超界均回退。 |
| doc suite closeout audit | README、testing overview、correctness、benchmark/evidence、optimization evidence、roadmap、matrix、evaluation、`doc-rvv` | 每个 role 有独立文档或明确归属，当前结论、测试数量、板卡收益和边界一致。 |
| queue refresh | `doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md` | 当前 topic row 记录 Phase 025 后 7 个 gtest 和结束判断。 |
| verification | `run_test_compare`、`run_board_base_plane_public_tests`、`dump_bench_rvv`、`git diff --check`、私有信息扫描 | correctness、板卡 smoke、asm、格式和脱敏均通过。 |
| commit boundary | 精确 `git add` 当前 topic 路径 | 只提交 `sac_model_plane` 源码、测试、bench、topic docs、长期 doc 和必要队列表；排除其它 topic、raw logs、build 输出和本地 Handoff。 |

## 完成条件

如果审计没有发现未阻塞性能方向或提交阻塞项，本阶段关闭为
`ready_for_topic_commit`，并使用 `topic-only` 提交策略创建单个提交。
