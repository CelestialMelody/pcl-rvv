# Phase 060: stick doc-suite structure plan

## 阶段意图和边界

本阶段只补齐 `test-rvv/sample_consensus/sac_model_stick/` 的 topic-local doc suite（主题本地文档套件）。目标是让下一轮 worker 或 reviewer 不依赖聊天上下文，也能从 README、evaluation、phase suite、测试总览、正确性测试说明、bench/evidence 说明、优化证据索引和测试支撑代码地图恢复当前 topic。

本阶段不修改 production（生产源码）`sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`，不新增 RVV candidate（候选实现），不重跑板卡 bench（性能测试），不改变 Phase 000 / 020 / 040 的性能数字和 EvidenceDecision（证据决策）。若文档引用新增 summary evidence（摘要证据）路径，只同步 topic Makefile 的 `EVIDENCE_DOC_REFS`，让 evidence registry（证据登记表）检查能覆盖新文档。

## 当前状态清单

| area | 当前状态 | 本阶段动作 |
| --- | --- | --- |
| topic navigation | `README.zh.md` 已有阅读路径、常用命令、证据白名单和 production 边界。 | 增加五个 role 文档和 Phase 060 入口。 |
| evaluation | `doc/sac_model_stick-evaluation.zh.md` 承载 S2、Traceability Map、诊断证据链和文档归属矩阵。 | 增加 doc-suite role inventory（文档职责清单）和 Phase 060 归属。 |
| phase suite | Phase 000 / 020 / 040 diagnostic result 已闭合，Phase 010 / 030 / 050 PI1 plan 已写。 | 新增 Phase 060 plan/result，并更新 phase index。 |
| role docs | 复杂职责目前合并在 README、evaluation、phase result 和源码注释中。 | 拆出 `testing-overview`、`correctness-tests`、`benchmark-and-evidence`、`optimization-evidence`、`test-support-code-map`。 |
| production topic doc | `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` 不存在。 | 保持 not_applicable；无 adopted production behavior（已采用生产行为）。 |
| evidence registry | `log/evidence_registry.json` 已登记 Phase 000 / 020 / 040 summary evidence。 | 不覆盖 registry；运行 `repeated_evidence_status` 验证 freshness（新鲜度）。 |

## doc_suite_role_inventory

| role | phase 前状态 | phase 目标 |
| --- | --- | --- |
| topic_navigation | `standalone:README.zh.md` | adopted，补齐 role 文档入口。 |
| testing_overview | `phase_deferred + unblocked` | `standalone:doc/testing-overview.zh.md`。 |
| correctness_tests | `phase_deferred + unblocked` | `standalone:doc/correctness-tests.zh.md`。 |
| benchmark_and_evidence | `phase_deferred + unblocked` | `standalone:doc/benchmark-and-evidence.zh.md`。 |
| optimization_evidence | `phase_deferred + unblocked` | `standalone:doc/optimization-evidence.zh.md`。 |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` | adopted，增加 doc-suite structure route。 |
| test_support_code_map | `phase_deferred + unblocked` | `standalone:doc/test-support-code-map.zh.md`。 |
| phase_index | `standalone:doc/phases/README.zh.md` | adopted，列出 Phase 060。 |
| evaluation_diagnostic | `standalone:doc/sac_model_stick-evaluation.zh.md` | adopted，补文档归属和 inventory。 |
| production_topic_doc | `not_applicable with evidence` | 保持 not_applicable；当前没有 production patch 或 PI5 确认。 |

## Target 粒度审计计划

| target 类别 | 当前工程事实 | 本阶段写法 |
| --- | --- | --- |
| correctness aggregate | 共享 Make target `run_test_compare` 运行 Std/RVV 两个 gtest binary。 | 写入 testing overview 和 correctness tests。 |
| correctness aliases | topic target `run_stick_count_tests`、`run_stick_select_tests` 存在；getDistances 没有单独 alias。 | 写清已存在 alias 与缺口；getDistances alias 作为可选后续，不作为本阶段阻塞。 |
| bench diagnostic aliases | bench binary 一次输出 count/select/getDistances 六行；manifest script 用 `--focus` 抽取 count/select/getdistances。 | 写清 case label 和 focus 边界。 |
| QEMU smoke aliases | `dump_bench_rvv` 用于 bench binary build / asm；`run_bench_compare` 有 QEMU guard。 | 写清 QEMU 不支撑性能结论。 |
| board smoke aliases | 共享 `board_smoke` 运行 board test + bench compare + fetch logs。 | 写清它不是 repeated evidence。 |
| board repeated aliases | `collect_repeated_board_evidence` + `record_repeated_board_evidence_state`。 | 写清 5-run budget、run label 和 summary evidence。 |
| doctor / registry aliases | `generate_board_evidence_manifest`、`run_repeated_board_evidence_doctor`、`repeated_evidence_status`。 | 写清 manifest、doctor、registry 关系。 |
| historical probe guarded aliases | 当前 topic 没有历史 production probe target。 | 写 `not_applicable with evidence`。 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 拆出 testing overview | `doc/testing-overview.zh.md` | 运行入口分类、target 粒度审计、覆盖矩阵和提交边界可从文档恢复。 |
| 拆出 correctness tests | `doc/correctness-tests.zh.md` | 6 个 gtest 的输入、断言、证明范围和不能证明的范围清楚。 |
| 拆出 benchmark/evidence | `doc/benchmark-and-evidence.zh.md` | bench 输出、case label、board repeated、Evidence Doctor、registry 和 raw log 排除边界清楚。 |
| 拆出 optimization evidence | `doc/optimization-evidence.zh.md` | count/select/getDistances 三个 candidate family 与 PI1 计划、证据和 production gate 对齐。 |
| 拆出 code map | `doc/test-support-code-map.zh.md` | production、test-only helper、test/bench source、script 和 evidence output 可定位。 |
| 同步导航和矩阵 | README、evaluation、roadmap、phase index、optimization matrix、Makefile | 新文档在阅读路径和 evidence doc refs 中可见。 |
| Phase result | `doc/phases/060-stick-doc-suite-structure/result.zh.md` | 回填 doc-suite audit、artifact tracking、验证命令和继续 / 停止判断。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新 board manifest，也不覆盖 Phase 000 / 020 / 040 的 summary evidence。验证只运行 `make -C test-rvv/sample_consensus/sac_model_stick repeated_evidence_status`，确保现有 registry 中登记的 manifest、doctor Markdown 和 doctor JSON 仍 fresh，且 README / evaluation / phase / role docs 对这些路径的引用足以通过 `--require-doc-ref`。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段本身是 documentation structure（文档结构）动作；它引用的性能证据仍是 Phase 000 / 020 / 040 的 `production-shaped diagnostic`。 |
| A/B boundary | 不新增 A/B；已有 A/B 仍是 public baseline 或 test helper 边界。 |
| 当前决策问题 | 文档套件是否足以支持 reviewer 复核三个 diagnostic candidate 和 PI2 授权门禁。 |
| diagnostic 是否可外推到 production | 不改变。诊断证据仍不能外推成 production direct evidence。 |
| comparison-boundary / baseline mismatch 风险 | Phase 040 public 行 Error、Phase 000 / 020 public weak cross-check 都继续作为已解释的诊断边界记录。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不在本阶段扩大。count/select/getDistances 的 PI2 仍需要用户授权。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有 adopted RVV family；若 PI2-PI5 引入多 family，另按 production boundary 做 A/B。 |

## 完成条件和停止条件

完成条件：五个 role 文档存在，README / evaluation / roadmap / phase index / matrix 都能定位它们，Makefile evidence doc refs 覆盖引用 summary evidence 的新文档，registry freshness 和 QEMU correctness 验证通过，production source diff 为空。

本阶段完成后，topic-local doc-suite structure 不再是 `phase_deferred + unblocked`。默认下一阶段仍是 authorization-gated PI2：用户若授权，按 Phase 010 / 030 / 050 中某一个 PI1 plan 进入对应 production patch；没有授权时保持 `partial-production-candidate`。
