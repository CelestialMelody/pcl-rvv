# Phase 020: doc-suite closeout plan

## 阶段意图和边界

本阶段不新增 RVV candidate（候选实现），也不修改 production（生产源码）。目标是把 Phase 000/010 已经收集的 diagnostic（诊断）和 production-shaped diagnostic（生产形态诊断）证据整理到 topic-local doc suite（主题本地文档套件），让 reviewer 能从 README、evaluation、phase result、summary、Evidence Doctor（证据体检）和 test support（测试支撑代码）之间稳定跳转。

范围只覆盖 `test-rvv/features/moment_invariants/` 下的文档、Makefile 证据元数据和 evidence registry（证据登记表），以及 features 筛选/队列表中当前 topic 的状态行。`features/include/pcl/features/impl/moment_invariants.hpp` 保持未修改；没有 adopted production behavior（已采用生产行为）前，不创建 `doc-rvv/features/moment_invariants-RVV.zh.md`。

## 当前状态清单

| 对象 | 当前状态 | 本阶段处理 |
| --- | --- | --- |
| Phase 000 helper-only evidence | board repeated median 1.141x，decision bucket 为 weak-positive；Evidence Doctor 为 0 Error / 0 Warning / 2 Suggestion。 | 写 `result.zh.md`，说明这是 helper-only diagnostic，不能外推到 production。 |
| Phase 010 public-search-shaped evidence | board repeated median 1.031x，min 1.026x；Evidence Doctor 为 0 Error / 0 Warning / 3 Suggestion，包含 near-threshold。 | 写 `result.zh.md`，说明真实 KdTree search 外层稀释后只剩近阈值弱正向。 |
| topic-local docs | 已有 README、evaluation、roadmap、phase index 和 optimization matrix；缺少 testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map 的独立 role 文档。 | 补齐独立 role 文档，并在 README/evaluation/phase index 中加入阅读路径。 |
| evidence registry | 已登记 Phase 000/010 summary、manifest 和 doctor。 | 运行 freshness check，记录当前 summary-only 提交边界。 |

## 执行动作

1. 补写 Phase 000 和 Phase 010 的 `result.zh.md`，逐项回填计划、命令、summary、Evidence Doctor、registry 和 production 判断。
2. 新增 `testing-overview.zh.md`、`correctness-tests.zh.md`、`benchmark-and-evidence.zh.md`、`optimization-evidence.zh.md`、`test-support-code-map.zh.md`。
3. 更新 README、evaluation、optimization roadmap、optimization matrix 和 phase index，让默认恢复入口变成 closeout / ready-for-review 检查，而不是继续跑已经完成的 Phase 010。
4. 同步 `doc-rvv/library-screening/features/` 中当前 topic 的状态，保持队列表只写状态和入口，不复制 topic-local 证据长文。
5. 运行 `make run_test_compare`、`make dump_bench_rvv`、evidence registry check 和 `git diff --check`。

## doc-suite role inventory

| role | 计划状态 | 计划路径 |
| --- | --- | --- |
| topic_navigation | update existing | `README.zh.md` |
| testing_overview | standalone | `doc/testing-overview.zh.md` |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` |
| optimization_roadmap | update existing | `doc/optimization-roadmap.zh.md` |
| phase_index / phase_result / optimization_matrix | update existing | `doc/phases/` |
| evaluation_diagnostic | update existing | `doc/moment_invariants-evaluation.zh.md` |
| production_topic_doc | not_applicable with evidence | 没有 production patch 或 adopted production behavior。 |

## 完成条件

本阶段完成时，doc suite 审计表中当前 topic 授权范围内的文档结构项应为 adopted 或 not_applicable with evidence；证据链应能解释为什么不建议直接接入 production。若验证失败、registry 显示 stale evidence、或需要修改 production 才能继续，则停止并输出 blocked / turn-stop Handoff。

## 继续 / 停止条件

若 Phase 020 完成且 verification 通过，默认进入 `ready_for_review`。继续做 production integration loop 需要新的用户确认，因为当前 production-shaped diagnostic 只有近阈值 weak-positive，且没有 production direct、fallback、泛型点类型、`Scalar=double` 或真实 `computeFeature` RVV dispatch 证据。
