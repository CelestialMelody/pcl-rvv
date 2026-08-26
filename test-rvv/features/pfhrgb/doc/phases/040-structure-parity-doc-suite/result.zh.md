# Phase 040 Structure Parity Doc Suite Result

## 执行摘要

Phase 040 是 production patch（生产补丁）之前完成的 topic-local doc suite（主题本地文档套件）
对齐阶段。它的结构审计仍有效，但证据结论已经被 Phase 020 production integration result（生产接入结果）
刷新：`features/include/pcl/features/impl/pfhrgb.hpp` 现在已有 adopted production behavior（已采用生产行为），
production 长期主题文档 `doc-rvv/features/pfhrgb-RVV.zh.md` 现在适用。

本阶段当时把 Phase 030/040 rerun 写成结构阶段的 truth（事实）：`public_pfhrgb_k_with_candidate_reuse`
是 positive production-shaped diagnostic（正向生产形态诊断），非复用 `public_pfhrgb_k_with_candidate`
也是 positive；helper-only `candidate_pfhrgb_pair_batch_rvv` 和 `component_pfhrgb_signature` 被降级。
当前 truth 以后续 Phase 020 为准：接入后的 `public_pfhrgb_k` 为 production-public（生产公开入口）
positive，median `1.27x`。

## 计划回填

| action | 状态 | 产物 / 证据 | 结论 |
| --- | --- | --- | --- |
| REFRESH-040 | done | README、evaluation、Phase 000/010/020/030、matrix、roadmap、current handoff | 旧 `board-blocked`、旧 `Errors=0` 和 helper-only positive 不再作为当前 truth。 |
| DOCS-040 | done | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | 复杂 topic 的测试、bench、证据和代码地图已有独立主归属。 |
| AUDIT-040 | done | 本文件的 inventory 和 target granularity audit | doc-suite role 均为 adopted 或 not_applicable with evidence。 |
| VERIFY-040 | done | `run_test_compare`、`dump_bench_rvv`、`evidence_doctor_repeated`、JSON 格式检查、`git diff --check` | 当时 Std/RVV 各 4 项 diagnostic gtest 通过；ASM dump 生成并含 RVV 指令；Doctor 为 2E/0W/6S historical baseline；格式检查通过。 |

## Doc Suite Role Inventory

| role | status | evidence / path | next action |
| --- | --- | --- | --- |
| topic_navigation | standalone:`README.zh.md` | 当前状态、阅读路径、常用命令和证据提交边界已记录。 | 已在 Phase 020 后刷新为 adopted production 状态。 |
| testing_overview | standalone:`doc/testing-overview.zh.md` | target 类型、运行流程、输入范围和粒度审计已拆出。 | 已在 Phase 020 后刷新 production-public 证据和 6 项 gtest 口径。 |
| correctness_tests | standalone:`doc/correctness-tests.zh.md` | 当时四项 diagnostic gtest 的输入、断言和证明范围已记录。 | 已在 Phase 020 后补 production exact path 与非 exact fallback 测试说明。 |
| benchmark_and_evidence | standalone:`doc/benchmark-and-evidence.zh.md` | 五个 bench case、board repeated、Doctor、ASM 和提交边界已记录。 | 已在 Phase 020 后刷新 production-public 证据。 |
| optimization_evidence | standalone:`doc/optimization-evidence.zh.md` | candidate family 到测试、bench、Doctor 和 decision 的索引已拆出。 | 已在 Phase 020 后新增 production direct / adopted row。 |
| optimization_roadmap | standalone:`doc/optimization-roadmap.zh.md` | 搜索空间和默认恢复动作已更新。 | Phase 020 已完成；当前默认无同边界续作。 |
| test_support_code_map | standalone:`doc/test-support-code-map.zh.md` | 聚合头、internal helper、bench wrapper、script 和 output 关系已记录。 | 若 PI1 扩大 helper 面，刷新拆分审计。 |
| phase_index | standalone:`doc/phases/README.zh.md` | Phase 000-040 状态已更新。 | PI1 后新增 result。 |
| evaluation_diagnostic | standalone:`doc/pfhrgb-evaluation.zh.md` | 当时记录 production checkpoint 和 Traceability Map。 | 已在 Phase 020 后转入 adopted production 分层。 |
| production_topic_doc | standalone:`doc-rvv/features/pfhrgb-RVV.zh.md` | Phase 020 后已创建正式 production 长期主题文档。 | 后续仅在 point-type expansion 或新证据改变 production truth 时刷新。 |

## Target Granularity Audit

| target 类别 | decision | evidence | next action |
| --- | --- | --- | --- |
| correctness aggregate | adopted | Phase 040 当时 `run_test_compare` 覆盖 Std/RVV 两侧 4 项 diagnostic gtest。 | Phase 020 后已扩展到 Std/RVV 两侧 6 项 gtest。 |
| correctness aliases | adopted for current scope | `run_test_std`、`run_test_rvv` 可分侧运行；当前 gtest 名称可定位。 | fallback gate 增多时再拆。 |
| bench diagnostic aliases | adopted | `--case-filter` 隔离五个 case。 | Phase 020 后 `public_pfhrgb_k` 已重标为 production-public case。 |
| QEMU smoke aliases | adopted | QEMU 只用于 correctness、构建和 log-shape。 | 继续禁止 QEMU timing 性能结论。 |
| board smoke aliases | adopted | `check_board_ssh` 本轮通过，`board_repeated` 完整运行。 | PI1 前短探测。 |
| board repeated aliases | adopted | `REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated`。 | Phase 020 后已重跑 production-public evidence。 |
| doctor / registry aliases | adopted | `evidence_doctor_repeated`、`script/generate_pfhrgb_evidence_manifest.py`、`log/evidence_registry.json`。 | 若 manifest 字段扩展，更新脚本和 registry。 |
| historical probe guarded aliases | not_applicable with evidence | 当前没有 production probe 或 rollback target。 | PI5 需要回滚时再建 guarded target。 |

## Evidence Freshness

当前 evidence summary 是 `log/board/repeated/evidence_manifest.json`，包含五个 comparisons（对比项）。
Phase 040 当时的 Doctor baseline 为 2E/0W/6S；Phase 020 后当前 Doctor 为 1E/1W/6S。Phase 000、
Phase 010 和 Phase 040 的旧数值作为 historical baseline（历史基线）保留；凡引用当前结果时，以
Phase 020 result、optimization matrix、evaluation、benchmark/evidence 和正式 `doc-rvv` 为准。

## Artifact Tracking

本阶段新增的 topic-local doc-suite 文件位于 `test-rvv/features/pfhrgb/doc/`，属于 topic test asset
提交候选但仍需 review。raw logs（原始日志）和 `build/` 默认 local-only（仅本机保留）。路径限定扫描显示
README / evaluation / roadmap / Handoff 引用的新增文档都存在，并处于 untracked / to-be-staged artifact set
（待纳入 topic 产物集合）。

## Verification

| 命令 | 结果 | 备注 |
| --- | --- | --- |
| `make -B -C test-rvv/features/pfhrgb run_test_compare` | passed | 当时 Std/RVV 两侧各 4 项 diagnostic gtest 通过；Phase 020 后当前口径为 6 项。 |
| `make -B -C test-rvv/features/pfhrgb dump_bench_rvv` | passed | `build/asm/riscv/bench_pfhrgb_rvv.asm` 生成，包含 `vsetvli`、`vle32.v` 等 RVV 指令。 |
| `make -C test-rvv/features/pfhrgb evidence_doctor_repeated` | passed | 当时 Doctor baseline 为 2E/0W/6S；当前结论以 Phase 020 的 1E/1W/6S 为准。 |
| `python3 -m json.tool ...evidence_manifest.json` / `...evidence_registry.json` | passed | JSON 可解析。 |
| stale wording scan（旧措辞扫描） | passed | 无旧阻塞、旧四项 manifest 或 Phase 000 旧 truth 句残留；完整命令见 worker 验证记录，避免文档自引用命中。 |
| `git diff --check -- test-rvv/features/pfhrgb tmp/rvv-work-logs/features/pfhrgb/current-handoff/current-handoff.zh.md` | passed | 无 whitespace error 输出。 |

## Continue / Stop Decision

`continue_stop_decision`: historical stop at production checkpoint; superseded by Phase 020 adoption。

`stop_condition_hit`: Phase 040 当时的停止原因是继续推进真实 production integration loop 需要修改
`features/include/pcl/features/impl/pfhrgb.hpp`。用户随后授权“接入后板卡有收益即可采纳”，Phase 020
已经完成接入、板卡复跑和 adopted closeout，因此该停止原因已经解除。

`next_phase_default`: 当前不再回到 PI1。若继续，应新建 `050-point-type-expansion`，范围是更宽点型 /
fallback / 数据布局扩展。
