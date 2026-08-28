# Phase 020: line getDistancesToModel diagnostic result

## 实际执行范围

本阶段按 `plan.zh.md` 为 `getDistancesToModel` 建立 production-shaped diagnostic（生产形态诊断，测试专用代码复用真实对象状态和入口数据流）。范围保持为 direct indexed `indices_`、`PointXYZ`、float xyz AoS（结构数组布局）、`Eigen::VectorXf` model coefficients、dense `std::vector<double>` distance output（连续距离输出）和 65536 点 synthetic line-distance cloud（合成直线距离点云）。production（生产源码）未修改。

本阶段只关闭 `getDistances-sqr-rvv-scalar-sqrt-store` 这一候选族的诊断条目。它不关闭 `getDistancesToModel` 的所有 RVV 可能形状，也不影响 Phase 000 count 和 Phase 010 select 的 positive diagnostic。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED test | done | `make -C test-rvv/sample_consensus/sac_model_line run_line_get_distances_tests` 先因缺少 target 失败，补 target 后因缺少 `getDistancesToModelCandidate` 编译失败。 | 测试能卡住 getDistances candidate 入口缺失；Makefile 也补齐了细分 correctness target。 |
| GREEN candidate | done | `make -C test-rvv/sample_consensus/sac_model_line run_line_get_distances_tests` | QEMU RVV 构建下 3 个 getDistances gtest 通过。 |
| aggregate correctness | done | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | Std/RVV 两个 QEMU 构建各 6 个 gtest 通过。 |
| bench / manifest | done | `src/bench_sac_model_line.cpp`、`script/generate_line_board_evidence_manifest.py`、`Makefile` | bench 输出 public / diagnostic candidate getDistances 两行；manifest 支持 `--items getdist`。 |
| asm | done | `make -C test-rvv/sample_consensus/sac_model_line dump_bench_rvv` | `getDistancesToModelCandidateRVV` 可归属 `vluxseg3ei32.v`、`vfmacc.vv` 和 `vse32.v`；当前形状没有 `vfsqrt`。 |
| board repeated | done | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_get_distances_evidence` | 5-run board repeated 完成，每轮 board gtest 均通过。 |
| manifest / doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_get_distances_evidence_state`、`make -C test-rvv/sample_consensus/sac_model_line repeated_get_distances_evidence_status` | manifest、doctor Markdown / JSON 和 registry 已生成，registry check 为 fresh。 |
| docs | partial | 本 result 已写，topic docs 待同步。 | 本阶段先记录负向诊断，再继续 Phase 030 vfsqrt 候选。 |

## Board Evidence

| case | role | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | --- | ---: | ---: | --- | --- | --- |
| public getDistancesToModel | summary_only_unknown | 2.230201 | 2.267723 | not_applicable | companion only | production unchanged |
| diagnostic candidate getDistancesToModel | production_shaped_diagnostic | 2.233099 | 2.374752 | `0.9289x, 0.9732x, 0.9356x, 0.9340x, 0.9300x` | `0.9340x / 0.9289x / 0.9732x` | negative |

Raw repeated board logs are under `log/board/repeated-20260828-phase020/run-*/` and remain local-only by default. Summary artifacts are:

- `test-rvv/sample_consensus/sac_model_line/doc/phases/020-line-get-distances-diagnostic/repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/020-line-get-distances-diagnostic/repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/020-line-get-distances-diagnostic/repeated-evidence-doctor.json`

## Evidence Doctor

`repeated-evidence-doctor.md` reports Errors=1, Warnings=0, Suggestions=0. The Error is `ba_degradation_frequency` for `diagnostic candidate getDistancesToModel PointXYZ 65536`: all 5 B/A values are below 1. The handling action is to reject this exact diagnostic implementation shape for production probe. This is not a correctness failure, because QEMU and board gtest compare public and candidate dense distances within tolerance.

The bench checksum for getDistances is size-only. It verifies that public and candidate wrote the same dense output length; gtest owns numerical correctness and checks each distance with tolerance. This split avoids treating allowed float rounding differences as a performance-log checksum failure.

Board output emitted clock skew warning for `script/rvv-board-run.mk`. The warning is an environment timestamp signal; it did not change command exit status, gtest result, checksum, manifest parse, or registry freshness.

## ASM Attribution

`build/asm/riscv/bench_sac_model_line_rvv.full.asm` contains `pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ>::getDistancesToModelCandidateRVV(...)` with `vluxseg3ei32.v`, `vfmacc.vv` and `vse32.v`. The manifest records 21 RVV instructions for the candidate boundary. Since sqrt and double store stay scalar in this Phase 020 shape, no `vfsqrt` instruction appears in this helper.

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic for the candidate; public row is companion only. |
| A/B boundary | test helper. Baseline is Std build `getDistancesToModelCandidateScalar`; candidate is RVV build `getDistancesToModelCandidateRVV` with scalar sqrt / double store. |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape。 |
| diagnostic 是否可外推到 production | no for this implementation shape. 它复用真实 `input_` / `indices_` 状态和公开入口公式，但 5-run board 负向，且没有 production dispatch / fallback 证据。 |
| comparison-boundary / baseline mismatch 风险 | yes。public companion timing 只说明 production 未改；严格 B/A 只使用 diagnostic candidate 的 Std/RVV 同 wrapper 对比。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 scalar-sqrt store 形状不建议进入 production probe。若后续 Phase 030 vfsqrt 候选 positive，仍需用户授权 PI1 并补 production direct / fallback / asm / board。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。diagnostic positive 也不能直接写成 clean adopted。 |

## Scope and Matrix Update

`getDistances-sqr-rvv-scalar-sqrt-store` 进入 `attempted / rejected for current diagnostic boundary`。拒绝理由是 repeated board negative 且 Evidence Doctor Error 指向 5/5 退化。该结论只覆盖当前 `PointXYZ + direct indexed + RVV sqr distance + scalar sqrt + dense double store` 形状。

`getDistances-vfsqrt-store` 是新的 `phase_deferred + unblocked` 候选。现有 `common/include/pcl/common/impl/rvv_math.hpp` 没有通用 sqrt helper，但仓库内有 `__riscv_vfsqrt_v_f32m2` 使用先例；下一阶段可以直接验证把 sqrt 移入 RVV chunk 后是否改变 board decision bucket。

## Continue / Stop Decision

EvidenceDecision：`negative diagnostic for current getDistances scalar-sqrt shape`。Stop condition 未命中，因为 Phase 030 vfsqrt 候选仍在当前 topic-local 测试资产授权范围内，不需要修改 production 源码，也不需要新板卡权限。下一阶段默认入口是 `030-line-get-distances-vfsqrt-diagnostic`。

count 和 select 仍保持 `partial-production-candidate`。Phase 020 的负向结果不能推出 line topic 的 no-production，也不能拒绝 count/select 的 PI1 production integration plan。

## doc_suite parity audit

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 需要同步 Phase 020 负向和 Phase 030 恢复入口。 | `doc-suite-quality-bar.zh.md` topic_navigation。 | phase_deferred + unblocked | 本 result。 | 本轮同步 README。 |
| testing overview | 新增 getDistances target、board repeated、doctor / registry aliases。 | testing_overview role。 | phase_deferred + unblocked | Makefile 已新增 target。 | 本轮同步测试总览。 |
| correctness tests | 新增 3 个 getDistances gtest。 | correctness_tests role。 | phase_deferred + unblocked | `src/test_sac_model_line.cpp`。 | 本轮同步 correctness 文档。 |
| benchmark/evidence | 新增 getDistances bench label 和 Phase 020 negative summary。 | benchmark_and_evidence role。 | phase_deferred + unblocked | manifest / doctor。 | 本轮同步 bench 文档。 |
| optimization evidence | 需要记录 rejected scalar-sqrt 形状和 planned vfsqrt 形状。 | optimization_evidence role。 | phase_deferred + unblocked | 本 result。 | 本轮同步优化证据。 |
| test support code map | helper、bench 和 script 已增长但仍低于 soft line limit；职责仍是当前 topic diagnostic。 | test_support_code_map role。 | adopted | `wc -l` 显示 helper 465 行、test 254 行、bench 198 行、script 430 行。 | Phase 030 后再复查是否拆分。 |
| production topic doc | 当前没有 production 行为。 | production_topic_doc 只适用于 adopted production。 | not_applicable with evidence | 未修改 production。 | PI5 后再创建。 |
| artifact tracking | 新增 Phase 020 result 和 summary artifacts 需在 path-limited status 中出现。 | Artifact Tracking gate。 | adopted pending final scan | 待 final status。 | final validation。 |
