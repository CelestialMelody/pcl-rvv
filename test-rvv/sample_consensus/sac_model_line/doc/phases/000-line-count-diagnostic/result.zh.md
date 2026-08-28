# Phase 000: line count diagnostic result

## 实际执行范围

本阶段按 `plan.zh.md` 执行 `countWithinDistance` 的 production-shaped diagnostic（生产形态诊断）。范围保持为 direct indexed `indices_`、`PointXYZ`、float xyz AoS、`Eigen::VectorXf` model coefficients 和 65536 点 synthetic line-distance cloud。production 源码未修改。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED test | done | `run_test_compare` 曾因缺少 `countWithinDistanceCandidate` 编译失败。 | 测试能卡住候选入口缺失。 |
| GREEN candidate | done | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | Phase 000 当时 Std/RVV 各跑 1 个 count gtest 并通过；当前 aggregate correctness 见 Phase 010 result。 |
| bench scaffold | done | `make -C test-rvv/sample_consensus/sac_model_line dump_bench_rvv` | 反汇编生成成功，candidate symbol 可归属。 |
| board repeated | done | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_evidence` | 5-run board repeated 完成，gtest 每轮通过。 |
| manifest / doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_evidence_state` | manifest、doctor Markdown / JSON 和 registry 已生成。 |
| docs | done | README、evaluation、role docs、roadmap、matrix、本 result。 | 下一轮短 prompt 可从 phase index 和 roadmap 恢复。 |

## Board Evidence

| case | role | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | --- | ---: | ---: | --- | --- | --- |
| public countWithinDistance | summary_only_unknown | 1.770945 | 1.770613 | not_applicable | companion only | production unchanged |
| diagnostic candidate countWithinDistance | production_shaped_diagnostic | 1.770477 | 0.396724 | `4.4920x, 4.4350x, 4.4569x, 4.4390x, 4.4913x` | `4.4569x / 4.4350x / 4.4920x` | positive-stable |

Raw repeated board logs are under `log/board/repeated-20260828-phase000/run-*/` and remain local-only by default. Summary artifacts are:

- `test-rvv/sample_consensus/sac_model_line/doc/phases/000-line-count-diagnostic/repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/000-line-count-diagnostic/repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/000-line-count-diagnostic/repeated-evidence-doctor.json`

## Evidence Doctor

`repeated-evidence-doctor.md` reports Errors=0, Warnings=0, Suggestions=0. Public count is modeled as `summary_only_unknown` companion and has no `ba_values`, because production source has no line RVV dispatch. This prevents the unchanged public path from being interpreted as a failed RVV candidate.

Board output repeatedly emitted clock skew warning for `script/rvv-board-run.mk`. The warning is an environment timestamp signal; it did not change command exit status, gtest result, checksum, manifest parse, or doctor result.

## ASM Attribution

`build/asm/riscv/bench_sac_model_line_rvv.full.asm` contains `pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ>::countWithinDistanceCandidateRVV(...)` with `vluxseg3ei32.v`, `vfmacc.vv`, `vmflt.vf` and `vcpop.m`. The manifest records 24 RVV instructions for the candidate boundary. The public companion row records no production RVV asm boundary.

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic for the candidate; public row is companion only. |
| A/B boundary | test helper vs public scalar entry. |
| 当前决策问题 | RVV-vs-scalar 是否值得进入 PI1 production integration plan。 |
| diagnostic 是否可外推到 production | unknown。它复用真实输入状态和 indices，但没有 production dispatch / fallback。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 在测试派生类中，inline 和 helper 边界可能不同于 production。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮结果不是弱 / 负 / 中性 / 不稳定。若后续复跑变弱，仍需先完成 mismatch audit，不得直接 no-production。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有既有 line RVV family；若 PI1 引入多个实现族，需要 production boundary 内 A/B。 |

## Continue / Stop Decision

EvidenceDecision：`partial-production-candidate`。本阶段完成了 count diagnostic 矩阵条目，但仍有 unblocked next actions。Phase 000 当时的默认下一阶段是 `010-line-select-diagnostic`；该阶段现在已经完成。当前 production 方向见 phase index 和 Phase 010 result。

当前恢复状态：`010-line-select-diagnostic` 已完成并记录在 `../010-line-select-diagnostic/result.zh.md`。因此本段的 select 后续动作已经关闭；默认 production 方向改为 count/select 合并 PI1，未授权 production 时继续 `020-line-get-distances-diagnostic`。

## doc_suite parity audit

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已新增 README。 | `doc-suite-quality-bar.zh.md` topic_navigation。 | adopted | `README.zh.md`。 | none |
| testing overview | 已新增 target 粒度审计。 | testing_overview role。 | adopted | `doc/testing-overview.zh.md`。 | none |
| correctness tests | 已解释 gtest。 | correctness_tests role。 | adopted | `doc/correctness-tests.zh.md`。 | none |
| benchmark/evidence | 已解释 bench label、manifest、doctor、registry。 | benchmark_and_evidence role。 | adopted | `doc/benchmark-and-evidence.zh.md`。 | none |
| optimization evidence | 已记录 candidate 状态。 | optimization_evidence role。 | adopted | `doc/optimization-evidence.zh.md`。 | none |
| test support code map | 已列当前文件职责和 layout audit。 | test_support_code_map role。 | adopted | `doc/test-support-code-map.zh.md`。 | none |
| evaluation | 已新增 diagnostic evaluation。 | evaluation_diagnostic role。 | adopted | `doc/sac_model_line-evaluation.zh.md`。 | none |
| production topic doc | 当前没有 production 行为。 | production_topic_doc 只适用于 adopted production。 | not_applicable with evidence | 未修改 production。 | PI5 后再创建 |
| phase suite | phase index、matrix、plan/result 已存在。 | phase_suite role。 | adopted | `doc/phases/`。 | none |
| artifact tracking | 新增文件需在 path-limited status 中出现。 | Artifact Tracking gate。 | adopted pending final scan | final validation。 | run final status |
