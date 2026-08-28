# Phase 030 Result: structure parity doc suite

## 执行摘要

本阶段补齐了 `sac_model_circle` topic-local doc suite（主题本地文档套件）和 current handoff（当前交接包）。没有修改 production（生产源码），没有重跑板卡 bench（性能测试），也没有改变 Phase 000 / Phase 020 的 EvidenceDecision（证据决策）。

本阶段结束时，topic 的默认停止条件仍是 `blocked_by_PI5_user_confirmation`：Phase 000 的 select/count production patch 证据为正向，但是否采纳或回滚必须由用户明确确认。后续 Phase 040 已按用户确认采纳该 patch，并创建长期 production 文档。

## Doc Suite Role Inventory

| role | 状态 | 证据 |
| --- | --- | --- |
| topic_navigation | `standalone:test-rvv/sample_consensus/sac_model_circle/README.zh.md` | 已写当前结论、阅读顺序、常用命令、可提交证据和默认排除项。 |
| testing_overview | `standalone:test-rvv/sample_consensus/sac_model_circle/doc/testing-overview.zh.md` | 已按 Makefile / board.mk / gtest / bench / script 抽取 target 粒度。 |
| correctness_tests | `standalone:test-rvv/sample_consensus/sac_model_circle/doc/correctness-tests.zh.md` | 已解释 5 个 gtest 的输入、断言、证明范围和不覆盖范围。 |
| benchmark_and_evidence | `standalone:test-rvv/sample_consensus/sac_model_circle/doc/benchmark-and-evidence.zh.md` | 已解释 bench row、board repeated、manifest、Evidence Doctor、asm 和提交边界。 |
| optimization_evidence | `standalone:test-rvv/sample_consensus/sac_model_circle/doc/optimization-evidence.zh.md` | 后续 Phase 040 已刷新为 select/count adopted、getDistances rejected、identity rejected 和剩余 scope 边界。 |
| optimization_roadmap | `standalone:test-rvv/sample_consensus/sac_model_circle/doc/optimization-roadmap.zh.md` | 已存在并在本阶段刷新默认恢复动作。 |
| test_support_code_map | `standalone:test-rvv/sample_consensus/sac_model_circle/doc/test-support-code-map.zh.md` | 已定位 production helper、test-only helper、bench wrapper、script 和 output。 |
| phase_index | `standalone:test-rvv/sample_consensus/sac_model_circle/doc/phases/README.zh.md` | 已加入 Phase 030 和默认恢复说明。 |
| evaluation | `standalone:test-rvv/sample_consensus/sac_model_circle/doc/sac_model_circle-evaluation.zh.md` | 已承载 Traceability Map（可追踪性地图）、证据链和 PI5 边界。 |
| production_topic_doc | Phase 030 结束时为 `not_applicable with evidence`；后续 Phase 040 已转为 `standalone:doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md`。 | 用户已确认板卡收益可采纳，长期文档只记录当前 adopted production behavior。 |

## Structure Parity 审计表

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test/bench source layout | test / bench 已在 `src/`，文件为 283 / 245 行。 | 配置要求 `artifact_layout.source_subdir=src`。 | adopted | 无旧根目录长源文件。 | none |
| aggregator and internal helpers | 当前没有共享 helper header；测试派生类和 bench 派生类各自承载候选。 | 当前规模未触发必须拆 `include/impl`。 | adopted | 没有旧 `test_support/`，单文件未超过阈值。 | 新候选族增加时复查。 |
| script and bench registry | topic-local script 存在，Makefile 有 production / getDistances 两种 mode。 | topic-specific manifest wrapper 应位于 topic-local `script/`。 | adopted | registry 已记录 Phase 000 / 020 summary evidence。 | none |
| target granularity | correctness、board smoke、board repeated、doctor / registry target 均存在。 | 复杂 topic 需要 target 粒度审计。 | adopted | 详见 `doc/testing-overview.zh.md`。 | none |
| topic-local docs | 本阶段创建 README、evaluation 和 5 个 role docs。 | doc-suite quality bar 要求稳定主归属。 | adopted | 本 result 和 README 均列出路径。 | none |
| long-term docs | Phase 030 未创建 `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md`；Phase 040 已创建。 | 只有 adopted production behavior 后适用。 | adopted by Phase 040 | 用户已确认板卡收益可采纳，长期文档采用接入后 board production direct 数据。 | none |
| legacy compatibility | 当前无旧 evaluation pointer 或兼容 alias。 | 默认不保留 legacy pointer。 | not_applicable with evidence | 文件扫描未发现旧入口。 | none |
| evidence freshness | Phase 000 / 020 registry 已存在。 | closeout 前必须跑 status target。 | adopted | 本阶段验证 `production_evidence_status` 和 `getdistances_evidence_status` 后更新。 | 若 status stale，刷新 evidence。 |

## Target 粒度审计

| target 类别 | decision | 证据 |
| --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` |
| correctness aliases | adopted | `run_circle_public_tests`、`run_circle_getdistances_candidate_test` |
| bench diagnostic aliases | adopted | `--mode production` / `--mode getdistances` manifest 拆分 |
| QEMU smoke aliases | adopted | QEMU 只作为 correctness，不写性能结论 |
| board smoke aliases | adopted | `run_board_circle_public_tests`、`run_board_circle_getdistances_candidate_test` |
| board repeated aliases | adopted | `collect_production_repeated_board_evidence`、`collect_getdistances_repeated_board_evidence` |
| doctor / registry aliases | adopted | production 与 getDistances 各有 doctor / record / status target |
| historical probe guarded aliases | not_applicable with evidence | 当前无历史 production probe 或 rollback probe target |

## 证据和验证

本阶段不新增性能数据。沿用证据：

- Phase 000 production direct：select median 1.6702x，count median 1.4012x，Evidence Doctor 0/0/0。
- Phase 020 production-shaped diagnostic：getDistances candidate B/A median 0.6590x，Evidence Doctor Errors=1 / Warnings=1 / Suggestions=0。

验证命令在阶段末执行并记录到最终 Handoff：`production_evidence_status`、`getdistances_evidence_status`、`run_test_compare`、`git diff --check` 和 topic 路径 untracked scan。

## 文档同步

已同步：

- `README.zh.md`
- `doc/sac_model_circle-evaluation.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `tmp/rvv-work-logs/sample_consensus/sac_model_circle/current-handoff/current-handoff.zh.md`

## Continue / Stop Decision

`continue_stop_decision`: Phase 030 当时为 `turn_stop_deferred with stop_condition_hit`；后续 Phase 040 已解除该停止点。

`stop_condition_hit`: Phase 030 当时的阻塞是 PI5 用户确认边界；当前已由用户确认采纳条件解除。

`next_phase_default`: 已由 Phase 040 执行；当前默认恢复入口是 Phase 040 result、optimization matrix 和 current handoff。
