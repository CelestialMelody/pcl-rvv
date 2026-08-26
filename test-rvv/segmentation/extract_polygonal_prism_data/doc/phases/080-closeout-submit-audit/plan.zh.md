# Phase 080 Plan: closeout-submit-audit

## 阶段意图和边界

本阶段只做提交前收尾审计：确认当前 topic 是否应结束、production dispatch / fallback 是否符合 RVV implementation（实现）规范、topic-local doc suite（主题本地文档套件）是否满足 closeout 结构要求，并准备 topic-only 提交边界。本阶段不新增新的 RVV 性能候选，不继续 wide-stride、`projectPoints`、custom point type 或 `Scalar=double` 扩展。

## 当前状态

Phase 070 已采纳 `indices_->size() >= 32` 阈值，32 点 production public confirm5 median 1.19x、min 1.18x、Evidence Doctor 0 / 0 / 0。Production patch 当前为 adopted production behavior，覆盖 `RVVXYZAoSFloatLayout<PointT>`、`sizeof(PointT) <= 32`、合法 dense / indexed indices、合法 single / nested polygon。剩余方向需要扩大 production boundary 或另开 topic，因此当前 phase 的目标是结束本 topic 的自动优化循环。

## 动作清单

| action | 产物 | 完成判据 |
| --- | --- | --- |
| production dispatch audit | `impl/extract_polygonal_prism_data.hpp` | public entry 短路 RVV，失败自然 fallback；无未用 include；indexed load 避免别名风险 |
| doc-suite parity audit | topic-local role 文档和本 result | testing、correctness、benchmark/evidence、optimization evidence、code map、evaluation、doc-rvv 均有稳定归属 |
| freshness / artifact tracking | evidence registry check 和 `git status --short --untracked-files=all -- <topic paths>` | 文档引用的提交候选存在；raw/build/local-only 文件不进入 topic commit |
| verification | `make run_test_compare`、`make run_board_test`、`make dump_bench_rvv`、`git diff --check` | 全部通过后进入 commit flow |

## 继续 / 停止条件

若审计发现只属于当前 topic 的未阻塞结构缺口，则在本阶段补齐。若测试、板卡、Evidence Doctor 或 registry 失败，则暂停并报告。若审计和验证通过，当前 topic 结束，提交边界采用 `topic-only` 加可选 summary evidence commit；raw logs 不提交。
