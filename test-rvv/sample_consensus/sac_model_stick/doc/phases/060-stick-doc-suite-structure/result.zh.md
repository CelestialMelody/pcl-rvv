# Phase 060: stick doc-suite structure result

## 阶段结论

Phase 060 已完成 topic-local doc suite（主题本地文档套件）结构补齐。新增的独立 role 文档包括：

- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`

本阶段没有修改 production（生产源码）`sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`，没有新增 RVV candidate（候选实现），没有重跑板卡 bench（性能测试），也没有改变 Phase 000 / 020 / 040 的 speedup（加速比）或 EvidenceDecision（证据决策）。当前 topic 仍是 count/select/getDistances 三个入口的 `partial-production-candidate`；PI2 production patch（生产补丁）仍需要用户明确授权。

## 计划回填

| action | 状态 | 证据 / 文件 | 结论 |
| --- | --- | --- | --- |
| 拆出 testing overview | done | `doc/testing-overview.zh.md` | 测试入口分类、target（Make 目标）粒度审计、QEMU / board 边界和覆盖矩阵已独立可读。 |
| 拆出 correctness tests | done | `doc/correctness-tests.zh.md` | 6 个 gtest 的输入、断言、证明范围和不能证明的范围已逐项记录。 |
| 拆出 benchmark/evidence | done | `doc/benchmark-and-evidence.zh.md` | bench 输出、case label（用例标签）、manifest、Evidence Doctor（证据体检）、registry 和 raw log 排除边界已独立记录。 |
| 拆出 optimization evidence | done | `doc/optimization-evidence.zh.md` | 三个 candidate family 与 PI1 计划、summary evidence、asm 和 production gate 已建立索引。 |
| 拆出 code map | done | `doc/test-support-code-map.zh.md` | production public entry、test-only helper、test/bench source、script 和 evidence output 可定位。 |
| 同步导航和矩阵 | done | `README.zh.md`、`doc/sac_model_stick-evaluation.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` | 新 role 文档已进入阅读路径、Traceability Map（可追踪性地图）、roadmap、matrix 和 phase index。 |
| 同步 registry 文档引用 | done | `Makefile` 的 `EVIDENCE_DOC_REFS` | 新增 role 文档和 Phase 060 plan/result 纳入 `repeated_evidence_status` 的 doc refs。 |

## doc_suite_role_inventory

| role | 状态 | 路径 / 证据 | 说明 |
| --- | --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` | 已包含当前结论、阅读路径、常用命令、证据白名单和 production 边界。 |
| testing_overview | standalone | `doc/testing-overview.zh.md` | 已覆盖运行入口分类、target 粒度审计、覆盖矩阵和 QEMU / board 边界。 |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` | 已覆盖 6 个 gtest 的输入、被测路径、断言和不能证明的范围。 |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` | 已覆盖 bench 输出、manifest focus、summary evidence、Evidence Doctor、asm 和提交边界。 |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` | 已覆盖 candidate family 到代码、测试、bench、board、asm 和决策的映射。 |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` | 已新增 `doc-suite-structure` completed 行；Phase 070 已关闭当时记录的 getDistances alias 后续。 |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` | 已覆盖 helper、fixtures、bench harness、script、output 和拆分审计。 |
| phase_index | standalone | `doc/phases/README.zh.md` | 已新增 Phase 060 状态和当前 `next_phase_default`。 |
| evaluation_diagnostic | standalone | `doc/sac_model_stick-evaluation.zh.md` | 已新增 role 文档到 Traceability Map 和 inventory。 |
| production_topic_doc | not_applicable with evidence | `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` 不存在 | 当前没有 adopted production behavior（已采用生产行为）、production patch 或 PI5 用户确认。 |

## Target 粒度审计

| target 类别 | current shape scan | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` 运行 Std/RVV 两个构建。 | adopted | `doc/testing-overview.zh.md` 和 `doc/correctness-tests.zh.md` 已说明。 | none |
| correctness aliases | Phase 060 完成时 `run_stick_count_tests`、`run_stick_select_tests` 存在，getDistances 暂无独立 alias。Phase 070 已补 `run_stick_getdistances_tests`。 | adopted | aggregate target 覆盖 getDistances 两个 gtest；Phase 070 GREEN 检查证明细分 alias 可运行。 | none |
| bench diagnostic aliases | bench binary 输出六条 public/candidate 行，manifest script 用 `--focus` 隔离 phase。 | adopted | `doc/benchmark-and-evidence.zh.md` 已写 focus 字典。 | none |
| QEMU smoke aliases | `dump_bench_rvv` 用于构建 / 反汇编，`run_bench_compare` 默认 guard。 | adopted | QEMU timing 不进入性能结论。 | none |
| board smoke aliases | `board_smoke` 部署并运行一次 board test + bench compare。 | adopted | 文档已说明单次 smoke 不是 repeated performance。 | none |
| board repeated aliases | `collect_repeated_board_evidence` 和 `record_repeated_board_evidence_state`。 | adopted | Phase 000 / 020 / 040 已有 5-run summary evidence。 | none |
| doctor / registry aliases | `generate_board_evidence_manifest`、`run_repeated_board_evidence_doctor`、`repeated_evidence_status`。 | adopted | Makefile `EVIDENCE_DOC_REFS` 已纳入新增 role docs。 | none |
| historical probe guarded aliases | 当前 topic 没有历史 production probe target。 | not_applicable with evidence | production 源码未改，也没有 rollback/probe target。 | none |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段是 documentation structure（文档结构）动作；它引用的 Phase 000 / 020 / 040 仍是 `production-shaped diagnostic`。 |
| A/B boundary | 没有新增 A/B。已有 candidate 行是 test helper 边界，public 行只是未接 RVV 的 cross-check（交叉检查）。 |
| 当前决策问题 | doc suite 是否足以让 reviewer 复核三个 diagnostic candidate 和 PI2 授权门禁。 |
| diagnostic 是否可外推到 production | 不改变。诊断证据仍不能外推成 production direct evidence（真实生产路径证据）。 |
| comparison-boundary / baseline mismatch 风险 | Phase 000 / 020 的 public weak cross-check 和 Phase 040 的 public negative cross-check 已在 role 文档中保留降级解释。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不扩大 probe。count/select/getDistances 的 PI2 仍需要用户授权。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有 adopted RVV family；若后续 PI2-PI5 引入多 family，必须另做同边界 A/B。 |

## Evidence Registry 与新鲜度

本阶段不覆盖 `log/evidence_registry.json` 登记的 evidence files，也不生成新 manifest。`Makefile` 的 `EVIDENCE_DOC_REFS` 已新增五个 role 文档和 Phase 060 plan/result，因此 `repeated_evidence_status` 会把新增文档纳入 doc ref（文档引用）扫描。

可提交 summary evidence 仍是 Phase 000 / 020 / 040 的 repeated manifest、doctor Markdown、doctor JSON 和 `log/evidence_registry.json`。raw board logs、QEMU logs、build output 和本机配置仍为 local-only。

## Artifact Tracking

本阶段新增或更新的可审查 topic 产物：

| 类别 | 路径 | 提交边界 |
| --- | --- | --- |
| role docs | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | topic-local docs，review 后可随 topic test asset 提交。 |
| phase docs | `doc/phases/060-stick-doc-suite-structure/plan.zh.md`、`doc/phases/060-stick-doc-suite-structure/result.zh.md` | phase docs，review 后可随 topic test asset 提交。 |
| navigation / evaluation | `README.zh.md`、`doc/sac_model_stick-evaluation.zh.md`、`doc/phases/README.zh.md` | topic-local docs。 |
| roadmap / matrix | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | phase loop recovery docs。 |
| Makefile refs | `Makefile` | topic test asset；只新增 doc refs，不改变 build or bench behavior。 |

路径限定扫描使用：

```bash
git status --short --untracked-files=all -- test-rvv/sample_consensus/sac_model_stick doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp
```

当前 topic 文件多数仍是 untracked（未跟踪）状态；它们属于 topic artifact tracking / commit boundary。没有提交授权时不创建 commit。

## 继续 / 停止判断

Phase 060 内没有未完成的 doc-suite structure action。当前 roadmap 和 optimization matrix 中仍有 count/select/getDistances 三个 production probe，但它们都需要用户授权修改 production，因此是 `authorization-gated`，不是本轮可自行继续的 topic-local 文档动作。

`next_phase_default`：等待用户选择并授权某一个 PI2 production patch，入口为：

- `010-stick-count-production-integration-plan`
- `030-stick-select-production-integration-plan`
- `050-stick-getdistances-production-integration-plan`

没有 PI2 授权时，生产源码必须保持不动，当前 topic 保持 `partial-production-candidate`。
