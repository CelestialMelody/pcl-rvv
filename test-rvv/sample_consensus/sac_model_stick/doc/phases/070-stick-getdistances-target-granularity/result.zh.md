# Phase 070: getDistances target 粒度补齐结果

## 阶段结论

Phase 070 已完成 `run_stick_getdistances_tests` correctness alias（正确性细分入口）补齐。该 alias 只运行 RVV build 下的两个 getDistances gtest（GoogleTest 测试用例）：

- `SampleConsensusModelStick.GetDistancesCandidateMatchesPublicDirectionCoefficientSemantics`
- `SampleConsensusModelStick.GetDistancesCandidatePreservesPenaltyAndDenseIndexedOrder`

本阶段没有修改 production（生产源码）`sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`，没有新增 RVV candidate（候选实现），没有重跑 board bench（板卡性能测试），也没有改变 Phase 000 / 020 / 040 的 EvidenceDecision（证据决策）。当前 topic 仍是 count/select/getDistances 三个入口的 `partial-production-candidate`；PI2 production patch（生产补丁）仍需要用户明确授权。

## 计划回填

| action | 状态 | 证据 / 文件 | 结论 |
| --- | --- | --- | --- |
| RED 检查 | done | `make -C test-rvv/sample_consensus/sac_model_stick run_stick_getdistances_tests` | 新增前失败为 `No rule to make target 'run_stick_getdistances_tests'`，证明缺的是 Makefile 可运行入口。 |
| 新增 getDistances filter | done | `Makefile` 的 `STICK_GETDISTANCES_FILTER` | filter 精确覆盖两个 getDistances case。 |
| 新增 alias target | done | `Makefile` 的 `run_stick_getdistances_tests` | target 调用 `run_test_rvv` 并传入 getDistances filter。 |
| GREEN 检查 | done | `make -C test-rvv/sample_consensus/sac_model_stick run_stick_getdistances_tests` | RVV build 下运行 2 个 getDistances gtest，2/2 通过。 |
| 同步文档入口 | done | `README.zh.md`、`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` | 文档不再把 getDistances alias 写成当前缺口。 |
| 同步 registry 文档引用 | done | `Makefile` 的 `EVIDENCE_DOC_REFS` | Phase 070 plan/result 纳入 `repeated_evidence_status` 的 doc refs。 |

## Target 粒度审计

| target 类别 | current shape scan | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` 运行 Std/RVV 两个构建。 | adopted | 仍作为完整 correctness gate（正确性验收）。 | none |
| correctness aliases | `run_stick_count_tests`、`run_stick_select_tests`、`run_stick_getdistances_tests` 均存在。 | adopted | getDistances alias 已通过 GREEN 检查，2/2 gtest pass。 | none |
| bench diagnostic aliases | `run_bench_std` / `run_bench_rvv` 和 board repeated 仍复用现有 bench wrapper。 | adopted | 本阶段不改变 bench label 或 manifest focus。 | none |
| QEMU smoke aliases | `run_stick_getdistances_tests` 在 QEMU 中只证明 correctness 和日志形状。 | adopted | QEMU timing（仿真计时）不进入性能结论。 | none |
| board repeated aliases | `collect_repeated_board_evidence` / `record_repeated_board_evidence_state` 保持 Phase 040 focus。 | adopted | 本阶段不新增 board evidence。 | none |
| doctor / registry aliases | `repeated_evidence_status` 将检查 Phase 070 文档引用。 | adopted | Phase 070 plan/result 已加入 `EVIDENCE_DOC_REFS`。 | none |
| historical probe guarded aliases | 当前 topic 没有历史 production probe target。 | not_applicable with evidence | production 源码未改，也没有 rollback/probe target。 | none |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段是 target granularity（测试入口粒度）动作；它引用的 getDistances 证据仍是 Phase 040 的 `production-shaped diagnostic`。 |
| A/B boundary | 没有新增 A/B；alias 只单跑已有 getDistances correctness case。 |
| 当前决策问题 | 是否把 getDistances correctness 细分入口补齐到和 count/select 一致的 reviewer 可运行粒度。 |
| diagnostic 是否可外推到 production | 不改变。诊断证据仍不能外推成 production direct evidence（真实生产路径证据）。 |
| comparison-boundary / baseline mismatch 风险 | 不改变。Phase 040 public 行的 negative cross-check 仍只说明未接 RVV 的 public getDistances 边界。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不扩大 probe。getDistances PI2 仍需要用户授权。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有 adopted RVV family；若后续 PI2-PI5 引入多 family，必须另做同边界 A/B。 |

## Evidence Registry 与新鲜度

本阶段不覆盖 `log/evidence_registry.json` 登记的 evidence files，也不生成新 manifest。Phase 070 plan/result 已加入 `Makefile` 的 `EVIDENCE_DOC_REFS`，阶段结束前用 `repeated_evidence_status` 检查既有 summary evidence（摘要证据）和新增文档引用仍为 fresh。

验证结果：

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| `make -C test-rvv/sample_consensus/sac_model_stick run_stick_getdistances_tests` | pass: 2/2 | RVV build 下只运行两个 getDistances gtest。 |
| `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | pass: Std 6/6, RVV 6/6 | 完整 correctness aggregate（正确性汇总入口）仍通过。 |
| `make -C test-rvv/sample_consensus/sac_model_stick repeated_evidence_status` | pass: `evidence registry check: fresh` | Phase 070 plan/result 已进入 doc refs 后，既有 summary evidence 仍新鲜。 |
| `git diff --check -- test-rvv/sample_consensus/sac_model_stick doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md` | pass | 当前 topic 和队列表 diff 无 whitespace error。 |
| `git diff -- sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` | pass: empty | 本阶段未修改 production 源码。 |

## Artifact Tracking

本阶段新增或更新的可审查 topic 产物：

| 类别 | 路径 | 提交边界 |
| --- | --- | --- |
| Makefile alias | `Makefile` | topic test asset；新增 correctness alias，不改变 bench 或 production 行为。 |
| phase docs | `doc/phases/070-stick-getdistances-target-granularity/plan.zh.md`、`doc/phases/070-stick-getdistances-target-granularity/result.zh.md` | phase docs，review 后可随 topic test asset 提交。 |
| navigation / role docs | `README.zh.md`、`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md` | topic-local docs。 |
| roadmap / matrix / phase index | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/phases/README.zh.md` | phase loop recovery docs。 |

## 继续 / 停止判断

Phase 070 内没有未完成的 target-granularity action。当前 roadmap 和 optimization matrix 中仍有 count/select/getDistances 三个 production probe，但它们都需要用户授权修改 production，因此是 `authorization-gated`，不是本轮可自行继续的 topic-local 动作。

`next_phase_default`：等待用户选择并授权某一个 PI2 production patch，入口为：

- `010-stick-count-production-integration-plan`
- `030-stick-select-production-integration-plan`
- `050-stick-getdistances-production-integration-plan`

没有 PI2 授权时，生产源码必须保持不动，当前 topic 保持 `partial-production-candidate`。
