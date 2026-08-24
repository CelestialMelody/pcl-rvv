# Phase 040 Result: structure parity doc suite

> 更新说明：phase 050 已覆盖 `log/board/component_repeat_5/` 的 summary / manifest / Evidence Doctor / registry。本文中的 phase 020 数值只保留为 phase 040 完成时的 historical evidence（历史证据）；当前证据事实见 `../050-decode-implementation-shape-audit/result.zh.md`。

## 执行摘要

本阶段按 `doc-suite-quality-bar.zh.md` 补齐 `test-rvv/io/color_coding` 的 topic-local doc suite（主题本地文档套件）。本阶段只修改 topic-local docs 和后续 Handoff，不修改 `io/include/pcl/compression/color_coding.h`，不进入 PI2 production patch（生产补丁），不刷新板卡 repeated evidence。

phase 040 完成时的 EvidenceDecision（证据决策）不变：`partial-production-candidate`。encode average、encode points average pass 和 default color 保留 PI1 候选；decode 当时因 `ps_decode_points_leaf4096` Evidence Doctor Error 不进入 production candidate。phase 050 后，decode 结论更新为 direct path weak / unstable、staged-store rejected。

## 计划动作回填

| action | status | artifact / evidence |
| --- | --- | --- |
| 新建 testing overview | done | `doc/testing-overview.zh.md` |
| 新建 correctness 字典 | done | `doc/correctness-tests.zh.md` |
| 新建 benchmark/evidence 字典 | done | `doc/benchmark-and-evidence.zh.md` |
| 新建 optimization evidence | done | `doc/optimization-evidence.zh.md` |
| 新建 test support code map | done | `doc/test-support-code-map.zh.md` |
| 同步 README | done | `README.zh.md` 增加 role 导航、证据白名单和默认排除项。 |
| 同步 evaluation | done | `doc/color_coding-evaluation.zh.md` 增加 doc role Traceability Map 和 role inventory。 |
| 同步 roadmap / matrix / phase index | done | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/phases/README.zh.md`。 |
| 更新 Handoff | done after result | `tmp/rvv-work-logs/io/color_coding/current-handoff/*`。 |

## Structure Parity 审计表

| area | current shape scan | config / quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test/bench source layout | 已使用 `src/test_color_coding.cpp` 和 `src/bench_color_coding.cpp`。 | source split 符合当前 topic 复杂度。 | adopted | 路径存在且 Makefile 指向 `src/`。 | 保持。 |
| aggregator and internal helpers | `include/color_coding.h` 聚合到 `include/impl/color_coding_support.hpp`。 | 默认聚合入口 `include/`，内部 helper `include/impl/`。 | adopted | 无旧 `test_support/` 目录。 | 若 PI2 后 helper 激增，再按职责细拆。 |
| script and bench registry | summary / manifest 两个 topic-local script；bench label 在 `src/bench_color_coding.cpp`。 | 需要能定位 case-filter、checksum、Doctor input。 | adopted | `doc/benchmark-and-evidence.zh.md` 和 `doc/test-support-code-map.zh.md` 已记录。 | 保持。 |
| target granularity | aggregate correctness、QEMU smoke、board smoke、board repeated、Doctor / registry 均有入口；无 correctness alias 和 historical probe。 | 缺失类别需 `not_applicable with evidence` 或下一 phase。 | adopted / not_applicable with evidence | production direct / fallback alias 需要生产补丁后才存在；historical probe 不存在。 | PI2 后新增 production direct / fallback alias。 |
| topic-local docs | README、evaluation、roadmap、phase suite 已存在；本阶段新增 5 个 role docs。 | 复杂 topic 默认拆出独立 role docs。 | adopted | 文档路径都在 `test-rvv/io/color_coding/doc/`。 | 保持。 |
| long-term docs | 未创建 `doc-rvv/io/color_coding-RVV.zh.md`。 | diagnostic / partial-production-candidate 默认不创建 production 长期文档。 | not_applicable with evidence | 无 adopted production behavior、production patch 或 PI5 证据闭环。 | PI5 通过且用户确认采纳后再创建。 |
| legacy compatibility | 无旧 pointer、compatibility alias 或 `test_support/` 旧目录。 | 默认不保留 legacy pointer。 | not_applicable with evidence | shape scan 未发现旧入口。 | 无。 |
| evidence freshness | phase 020 registry 记录 summary / manifest / Doctor fresh。 | closeout 前检查 registry。 | adopted | `make check_evidence_freshness` passed。 | 只在重跑板卡后刷新。 |

## doc_suite_role_inventory

| role | status | path / evidence |
| --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` |
| testing_overview | standalone | `doc/testing-overview.zh.md` |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` |
| phase_index | standalone | `doc/phases/README.zh.md` |
| evaluation_diagnostic | standalone | `doc/color_coding-evaluation.zh.md` |
| evaluation_production | not_applicable with evidence | 没有 production patch、production direct tests 或 PI5 生产证据闭环。 |
| production_topic_doc | not_applicable with evidence | 没有 adopted production behavior；不创建 `doc-rvv/io/color_coding-RVV.zh.md`。 |

## Target 粒度审计结果

| target 类别 | decision | evidence / reason |
| --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` 运行 Std/RVV 两边各 6 个 TEST。 |
| correctness aliases | not_applicable with evidence | 当前没有 production direct / fallback TEST；gtest filter 可手动筛选。 |
| bench diagnostic aliases | adopted | `--case-filter <label>` 可隔离 21 个 bench label。 |
| QEMU smoke aliases | adopted | `run_qemu_smoke` 只用于 correctness / build / log shape。 |
| board smoke aliases | adopted | `run_board_color_coding_smoke` 只用于可运行 smoke。 |
| board repeated aliases | adopted | `run_board_color_coding_repeated` 生成 summary / manifest / Doctor / registry。 |
| doctor / registry aliases | adopted | repeated target 生成，`check_evidence_freshness` 检查。 |
| historical probe guarded aliases | not_applicable with evidence | 没有 production patch、回滚探针或历史 production target。 |

## Evidence 与生产边界

本阶段没有新增性能数据。phase 040 完成时沿用的证据是 phase 020；phase 050 后这些数字已降级为 historical evidence：

- Board summary: `log/board/component_repeat_5/summary.md`
- Manifest: `log/board/component_repeat_5/evidence_manifest.json`
- Evidence Doctor: `log/board/component_repeat_5/evidence_doctor.md`
- Registry: `log/evidence_registry.json`

phase 040 完成时 Evidence Doctor 为 `Errors=1, Warnings=13, Suggestions=2`。该记录现在只作为 historical evidence；phase 050 的当前 Doctor 为 `Errors=2, Warnings=11, Suggestions=6`，并拒绝 staged-store decode shape。

## 验证命令

```bash
make -C test-rvv/io/color_coding run_test_compare
make -C test-rvv/io/color_coding check_evidence_freshness
git diff --check -- test-rvv/io/color_coding tmp/rvv-work-logs/io/color_coding
```

结果：

- `run_test_compare` passed：Std / RVV 各 6 个 TEST passed。
- `check_evidence_freshness` passed：`evidence registry check: fresh`。
- `git diff --check` passed：无 whitespace error。

## artifact tracking

本阶段新增或同步的 topic-local docs 都在 `test-rvv/io/color_coding/**` 授权边界内。整个 topic 目录当前在本 checkout 仍是 untracked topic artifact；`build/` 和 raw board run subdirs 仍是生成产物 / 默认不提交对象。

## roadmap / matrix 更新

- `doc/optimization-roadmap.zh.md` 将 `doc-suite parity essentials` 标为 phase 040 adopted。
- `doc/phases/optimization-matrix.zh.md` 新增 doc-suite parity 行。
- 默认恢复队列现在首先停在 `production_patch_authorization_required`：PI2 会修改 `io/include/pcl/compression/color_coding.h`，需要用户明确确认。

## continue_stop_decision

`turn_stop_deferred with stop_condition_hit=production_patch_authorization_required`。

当前授权范围内的 doc-suite 缺口已闭合；继续推进默认会进入 PI2 production patch，触碰 production source。除非用户明确确认 PI2 范围，否则不能继续修改 `io/include/pcl/compression/color_coding.h`。若用户不想接生产补丁，可选择另开 decode implementation-shape audit，但 decode 不应被当前 phase 020 Error 直接写入生产候选。
