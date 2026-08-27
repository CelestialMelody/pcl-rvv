# min_cut_segmentation RVV 诊断入口

本目录保存 `MinCutSegmentation<PointT>` 的 RVV topic-local 测试资产。Phase 000 已完成 component ablation（组件消融）：把 `calculateUnaryPotential` 的 foreground 最小距离规约和 `calculateBinaryPotential` 的三维距离 / `exp` 权重拆成可对拍、可计时的 test-only helper（测试专用辅助函数）。Phase 010 已完成 production-shaped buildGraph timing（生产形态图构建计时），结论是当前 potential batch 不建议接入 production。

当前没有修改 production（生产源码），也没有创建 `doc-rvv/segmentation/min_cut_segmentation-RVV.zh.md`。长期生产主题文档只有在真实生产补丁被用户确认采纳后才适用。

## 先读路径

| 读者问题 | 路径 |
| --- | --- |
| 当前函数级评估和诊断边界是什么 | `doc/min_cut_segmentation-evaluation.zh.md` |
| 测试入口和证据边界是什么 | `doc/testing-overview.zh.md` |
| 每个 correctness 测试证明什么 | `doc/correctness-tests.zh.md` |
| bench、manifest 和 Evidence Doctor 如何复核 | `doc/benchmark-and-evidence.zh.md` |
| 已尝试候选如何取舍 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码如何定位 | `doc/test-support-code-map.zh.md` |
| 当前 phase 从哪里恢复 | `doc/phases/README.zh.md` |
| 当前候选搜索空间 | `doc/optimization-roadmap.zh.md` |
| 跨阶段证据矩阵 | `doc/phases/optimization-matrix.zh.md` |
| Phase 000 结果 | `doc/phases/000-current-state-and-component-ablation/result.zh.md` |
| Phase 010 结果 | `doc/phases/010-production-shaped-buildgraph-timing/result.zh.md` |

## 常用命令

| 命令 | 作用 | 证据边界 |
| --- | --- | --- |
| `make run_test_compare` | 分别编译并运行 Std / RVV correctness（正确性）测试 | QEMU correctness；不证明性能 |
| `make run_test_rvv` | 只运行 RVV correctness 测试 | RED/GREEN 或局部调试 |
| `make dump_bench_rvv` | 生成 bench RVV binary 的反汇编摘录 | 只证明指令存在性，需要人工归属 |
| `make run_board_min_cut_repeated` | 板卡 repeated benchmark（重复性能采集）和 Evidence Doctor（证据体检） | 板卡性能证据 |
| `make run_board_min_cut_buildgraph_repeated` | Phase 010 buildGraph-shaped repeated benchmark 和 Evidence Doctor | 板卡 production-shaped diagnostic |

QEMU（仿真器）默认不跑完整 benchmark compare（性能对比）。如果需要 bench 数值结论，使用板卡 target。

## doc_suite_role_inventory

| role | 状态 |
| --- | --- |
| topic_navigation | standalone:`README.zh.md` |
| testing_overview | standalone:`doc/testing-overview.zh.md` |
| correctness_tests | standalone:`doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone:`doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone:`doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone:`doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone:`doc/test-support-code-map.zh.md` |
| phase_index | standalone:`doc/phases/README.zh.md` |
| phase_plan / phase_result / optimization_matrix | standalone:`doc/phases/*/plan.zh.md`、`doc/phases/*/result.zh.md`、`doc/phases/optimization-matrix.zh.md` |
| evaluation_diagnostic | standalone:`doc/min_cut_segmentation-evaluation.zh.md` |
| evaluation_production | not_applicable with evidence：本 topic 未接 production |
| production_topic_doc | not_applicable with evidence：没有 adopted production behavior、production patch 或 PI5 production evidence |

## 提交边界

当前建议 `topic-only` 提交：包含 topic-local 源码、脚本和文档；不提交 `build/`、raw logs 或完整 `log/` 树。summary / manifest / Evidence Doctor 可在用户要求 evidence commit 时单独 `git add -f`。
