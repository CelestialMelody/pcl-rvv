# Phase 010: line select diagnostic result

## 实际执行范围

本阶段按 `plan.zh.md` 执行 `selectWithinDistance` 的 production-shaped diagnostic（生产形态诊断，测试专用代码模拟真实公开入口的数据形态）。范围保持为 direct indexed `indices_`、`PointXYZ`、float xyz AoS（结构数组布局）、`Eigen::VectorXf` model coefficients、乱序索引和 65536 点 synthetic line-distance cloud（合成直线距离点云）。production（生产源码）未修改。

本阶段只关闭 `PointXYZ + direct indexed select` 的诊断矩阵条目，不关闭 `getDistancesToModel`、泛型点型、`Scalar=double`、identity-index fast path（恒等索引快速路径）或真实 production dispatch（生产分流）。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED test | done | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` 曾因缺少 `selectWithinDistanceCandidate` 编译失败。 | 测试能卡住 select candidate 入口缺失。 |
| GREEN candidate | done | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | Std/RVV 两个 QEMU 构建各 3 个 gtest 通过。 |
| bench scaffold | done | `make -C test-rvv/sample_consensus/sac_model_line dump_bench_rvv` | bench 输出 public / diagnostic candidate select 两行，反汇编可归属到 candidate helper。 |
| board repeated | done | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_select_evidence` | 5-run board repeated 完成，每轮 board gtest 均通过。 |
| manifest / doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_select_evidence_state` | manifest、doctor Markdown / JSON 和 registry 已生成。 |
| docs | done | README、evaluation、role docs、roadmap、matrix、本 result。 | 下一轮短 prompt 可从 phase index 和 roadmap 恢复。 |

## Board Evidence

| case | role | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | --- | ---: | ---: | --- | --- | --- |
| public selectWithinDistance | summary_only_unknown | 2.455160 | 2.448876 | not_applicable | companion only | production unchanged |
| diagnostic candidate selectWithinDistance | production_shaped_diagnostic | 2.466660 | 0.795046 | `3.0647x, 3.1409x, 3.0622x, 3.1010x, 3.1434x` | `3.1010x / 3.0622x / 3.1434x` | positive-stable |

Raw repeated board logs are under `log/board/repeated-20260828-phase010/run-*/` and remain local-only by default. Summary artifacts are:

- `test-rvv/sample_consensus/sac_model_line/doc/phases/010-line-select-diagnostic/repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/010-line-select-diagnostic/repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/010-line-select-diagnostic/repeated-evidence-doctor.json`

## Evidence Doctor

`repeated-evidence-doctor.md` reports Errors=0, Warnings=0, Suggestions=0. The earlier checksum mismatch was an evidence checksum policy issue: the initial checksum included exact floating-point error values, while the RVV helper is allowed to differ from the public Eigen scalar chain within the `1e-6` correctness tolerance. The bench checksum now fingerprints the inlier sequence and error vector size, and the dedicated gtest `SelectCandidateMatchesPublicEntryOnBenchScaleInput` keeps numerical correctness as the real gate（会失败的验收条件）。

Public select is modeled as `summary_only_unknown` companion and has no `ba_values`, because production source has no line RVV dispatch. This prevents the unchanged public path from being interpreted as a failed RVV candidate.

Board output emitted clock skew warning for `script/rvv-board-run.mk`. The warning is an environment timestamp signal; it did not change command exit status, gtest result, checksum, manifest parse, or doctor result.

## ASM Attribution

`build/asm/riscv/bench_sac_model_line_rvv.full.asm` contains `pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ>::selectWithinDistanceCandidateRVV(...)` with `vluxseg3ei32.v`, `vfmacc.vv`, `vmflt.vf`, two `vcompress.vm` instructions and `vse32.v`. The manifest records 33 RVV instructions for the candidate boundary. The public companion row records no production RVV asm boundary.

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic for the candidate; public row is companion only. |
| A/B boundary | test helper vs public scalar entry. |
| 当前决策问题 | RVV-vs-scalar 是否值得进入 production probe（生产探针）或 PI1 production integration plan（生产接入计划）。 |
| diagnostic 是否可外推到 production | unknown。它复用真实 `input_`、`indices_`、`inliers` 顺序和 `error_sqr_dists_` 语义，但没有 production dispatch、fallback 和 protected helper 边界。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 在测试派生类中，inline、buffer 组织和 helper 边界可能不同于 production。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮结果不是弱 / 负 / 中性 / 不稳定。若后续复跑变弱，仍需先完成 mismatch audit，不得直接 no-production 或拒绝有界 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有既有 line select RVV family；若 PI1 引入多个实现族，需要 production boundary 内 A/B。 |

## Scope and Matrix Update

`select-vcompress-f32m2` 进入 `partial-production-candidate`。该结论只覆盖测试专用 `PointXYZ` direct indexed select helper；它不能证明 production-ready（可接入生产）、不能证明泛型点型成立，也不能替代 `getDistancesToModel` 的 sqrt / dense store 诊断。

`getDistances-sqrt-store` 和 `point-type-expansion` 仍是 `phase_deferred + unblocked`。由于 production 源码修改需要用户授权，当前默认恢复动作是先由用户选择：授权 count/select 的 PI1 production integration plan，或继续 `020-line-get-distances-diagnostic`。

## Continue / Stop Decision

EvidenceDecision：`partial-production-candidate`。本阶段完成了 select diagnostic 矩阵条目；性能结论来自 board repeated（板卡重复性能测试），QEMU timing（QEMU 计时）没有进入性能判断。

Stop condition 命中的是 production 权限边界：count 和 select 都已形成正向 production-shaped diagnostic，但进入 PI1 / PI2 需要用户明确授权修改 production 或至少写生产接入计划。若不授权 production，下一阶段默认可继续 `020-line-get-distances-diagnostic`，它仍在 topic-local 测试资产范围内。

## doc_suite parity audit

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | README 已同步 Phase 000 / 010 当前结论和命令。 | `doc-suite-quality-bar.zh.md` topic_navigation。 | adopted | `README.zh.md`。 | none |
| testing overview | target 粒度已加入 select aliases、board repeated 和 registry。 | testing_overview role。 | adopted | `doc/testing-overview.zh.md`。 | none |
| correctness tests | 已解释 3 个 gtest，包括 bench-scale select 对拍。 | correctness_tests role。 | adopted | `doc/correctness-tests.zh.md`。 | none |
| benchmark/evidence | 已解释 count/select label、manifest、doctor、registry 和 checksum policy。 | benchmark_and_evidence role。 | adopted | `doc/benchmark-and-evidence.zh.md`。 | none |
| optimization evidence | 已记录 `count-indexed-gather-f32m2` 和 `select-vcompress-f32m2` 状态。 | optimization_evidence role。 | adopted | `doc/optimization-evidence.zh.md`。 | none |
| test support code map | 当前文件形态已更新到 count/select 两个 diagnostic helper。 | test_support_code_map role。 | adopted | `doc/test-support-code-map.zh.md`。 | none |
| evaluation | 已同步 select EvidenceDecision 和 Traceability Map。 | evaluation_diagnostic role。 | adopted | `doc/sac_model_line-evaluation.zh.md`。 | none |
| production topic doc | 当前没有 production 行为。 | production_topic_doc 只适用于 adopted production。 | not_applicable with evidence | 未修改 production。 | PI5 后再创建 |
| phase suite | phase index、matrix、plan/result 已存在。 | phase_suite role。 | adopted | `doc/phases/`。 | none |
| artifact tracking | 新增文件需在 path-limited status 中出现。 | Artifact Tracking gate。 | adopted pending final scan | run final status |
