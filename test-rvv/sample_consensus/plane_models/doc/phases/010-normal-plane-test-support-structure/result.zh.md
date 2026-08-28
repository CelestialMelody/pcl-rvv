# Normal-plane 测试支撑结构 Phase Result

## 执行摘要

本阶段关闭为 `positive / structure closed`。`plane_models` topic-owned 测试和 bench 源码已迁入 `src/`，Makefile 的 board test / bench 默认参数已改为板卡侧 PCD fixture，normal-plane 公开入口测试新增本地和板卡 alias，topic-local doc suite（主题本地文档套件）已按 role 拆分。

本阶段不改变 normal-plane RVV 生产算法，也不新增性能结论。phase 000 的 board compare、asm 和 Evidence Doctor 仍是当前生产补丁保留判断的证据来源。

## 计划动作回填

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| A1 phase plan | done | `010-normal-plane-test-support-structure/plan.zh.md` 先于结构编辑存在。 | phase plan gate 满足。 |
| A2 source move | done | `src/test_sample_consensus_plane_models.cpp`、`src/bench_sac_normal_plane.cpp`、`src/bench_sac_normal_plane_load_compare.cpp`；`make -C test-rvv/sample_consensus/plane_models run_test_compare`。 | Std/RVV 各 22 tests passed。 |
| A3 board args | done | `make -C test-rvv/sample_consensus/plane_models run_board_bench_compare fetch_board_logs`；`make -C test-rvv/sample_consensus/plane_models run_board_test fetch_board_logs`。 | 默认 board-side PCD path 生效，不再需要命令行 override。 |
| A4 alias targets | done | `make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests`。 | phase 000 三个 public/fallback/helper 测试 passed。 |
| A5 doc suite | done | 新增 `README.zh.md` 和 `doc/*.zh.md` role docs。 | role 分工可从 README、evaluation 和 phase index 恢复。 |
| A6 closeout | done | 本 result、matrix、roadmap、evaluation、长期 `doc-rvv` 和队列表同步。 | phase 010 可关闭；默认下一 phase 是 evidence registry。 |

## 源码 / 测试 / bench / 文档变化

| 类型 | 路径 | 内容 |
| --- | --- | --- |
| test source | `test-rvv/sample_consensus/plane_models/src/test_sample_consensus_plane_models.cpp` | 从 topic 根目录迁入 `src/`；测试内容保持。 |
| bench source | `test-rvv/sample_consensus/plane_models/src/bench_sac_normal_plane.cpp` | 从 topic 根目录迁入 `src/`；helper bench 内容保持。 |
| bench probe | `test-rvv/sample_consensus/plane_models/src/bench_sac_normal_plane_load_compare.cpp` | 从 topic 根目录迁入 `src/`；历史 load strategy probe 内容保持。 |
| Makefile | `test-rvv/sample_consensus/plane_models/Makefile` | `SRCS_*` 指向 `src/`；新增 board-side PCD 参数和 public-test alias。 |
| topic docs | `test-rvv/sample_consensus/plane_models/README.zh.md`、`doc/*.zh.md` | 新增 navigation、testing、correctness、benchmark/evidence、optimization evidence 和 code map role。 |

## 结果

### Correctness / QEMU

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| `make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests` | 3 tests passed | 本地 RVV 构建只跑 phase 000 新增公开入口 / fallback / helper buffer contract 用例。 |
| `make -C test-rvv/sample_consensus/plane_models run_test_compare` | Std 22 tests passed；RVV 22 tests passed | source move 后完整 QEMU correctness 通过；QEMU 不作为性能证据。 |

### Board

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| `make -C test-rvv/sample_consensus/plane_models run_board_bench_compare fetch_board_logs` | `selectWithinDistance` 9.79x；`countWithinDistance` 11.52x；`getDistancesToModel` 10.52x | 默认 board-side PCD 参数生效，日志位于 `log/board/analyze_bench_compare.log`。 |
| `make -C test-rvv/sample_consensus/plane_models run_board_test fetch_board_logs` | 22 tests passed | 默认 board-side PCD 参数生效，日志位于 `log/board/run_test.log`。 |

### Evidence Doctor

| 输入 | 输出 | 结果 |
| --- | --- | --- |
| `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json` | `evidence-doctor.md`、`evidence-doctor.json` | Errors=0、Warnings=0、Suggestions=0。 |

本阶段未新增性能结论，因此复用 phase 000 manifest。manifest 已与当前 `log/board/analyze_bench_compare.log` 的 9.79x / 11.52x / 10.52x 数值一致。

## doc_suite_role_inventory

| role | status | evidence | closeout |
| --- | --- | --- | --- |
| topic_navigation | `standalone:test-rvv/sample_consensus/plane_models/README.zh.md` | README 已列阅读顺序、常用命令、证据白名单和 production topic doc 适用性。 | adopted |
| testing_overview | `standalone:test-rvv/sample_consensus/plane_models/doc/testing-overview.zh.md` | target 分类、target 粒度审计和覆盖矩阵已拆出。 | adopted |
| correctness_tests | `standalone:test-rvv/sample_consensus/plane_models/doc/correctness-tests.zh.md` | phase 000 新增测试和 normal-plane helper 测试字典已拆出。 | adopted |
| benchmark_and_evidence | `standalone:test-rvv/sample_consensus/plane_models/doc/benchmark-and-evidence.zh.md` | bench wrapper、board summary、asm、manifest、doctor 和提交边界已拆出。 | adopted |
| optimization_evidence | `standalone:test-rvv/sample_consensus/plane_models/doc/optimization-evidence.zh.md` | adopted/deferred candidate 到证据路径的映射已拆出。 | adopted |
| optimization_roadmap | `standalone:test-rvv/sample_consensus/plane_models/doc/optimization-roadmap.zh.md` | roadmap 已存在并将默认恢复动作更新到 phase 020。 | adopted |
| test_support_code_map | `standalone:test-rvv/sample_consensus/plane_models/doc/test-support-code-map.zh.md` | source、proxy、bench wrapper、production helper 和 evidence output 关系已拆出。 | adopted |
| phase_index | `standalone:test-rvv/sample_consensus/plane_models/doc/phases/README.zh.md` | phase 000 / 010 状态和当时的 phase 020 默认恢复入口已同步；当前入口以后续 phase index 为准。 | adopted |
| evaluation_diagnostic / evaluation_production | `standalone:test-rvv/sample_consensus/plane_models/doc/sac_model_normal_plane-evaluation.zh.md` | 函数级评估、Traceability Map、EvidenceDecision 和 closeout 已刷新到 `src/` 路径与当前 board 数值。 | adopted |
| production_topic_doc | `standalone:doc-rvv/sample_consensus/selectWithinDistance_getDistancesToModel_RVV.zh.md` | 当前已有 retained production patch，因此长期 doc-rvv 适用；测试工程细节已转由 topic-local docs 承载。 | adopted |
| artifact_tracking | `to-be-staged topic artifacts` | 新增 role docs 均位于 `test-rvv/sample_consensus/plane_models` topic boundary；raw logs/build/output 默认排除。 | adopted |

## Structure Parity 审计

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test/bench source layout | 根目录三份 `.cpp` 已迁入 `src/`，Makefile `SRCS_*` 已更新。 | `artifact_layout.source_subdir=src`。 | adopted | `run_test_compare`、board test / bench passed。 | none |
| aggregator and internal helpers | 当前没有共享 helper header 或旧 `test_support/` 目录。 | 无共享 helper 时可 `not_applicable with evidence`。 | not_applicable with evidence | helper 仍只由单一 test / bench source 使用。 | point-type expansion 时重新审计。 |
| target granularity | 已有 aggregate、public alias、board alias、bench compare、asm dump。 | 复杂 topic 应列 target 字典。 | adopted | `testing-overview.zh.md` 已列。 | phase 020 补 doctor / registry alias。 |
| topic-local docs | README、testing、correctness、benchmark/evidence、optimization evidence、code map 已补齐。 | doc-suite quality bar。 | adopted | 本 result 的 inventory。 | none |
| long-term docs | 已有 production doc-rvv，现用于 retained production patch 说明。 | production doc 只写长期生产行为。 | adopted | 长期 doc 已刷新测试路径和 phase 000 证据。 | 后续 production 证据变化时刷新。 |
| legacy compatibility | 不保留 root `.cpp` compatibility alias。 | 默认更新引用并删除旧入口。 | adopted | Makefile 和文档引用已改到 `src/`。 | none |
| evidence freshness | phase-local manifest / doctor 存在；registry 未自动化。 | registry 缺口应显式进入下一 phase。 | phase_deferred + unblocked | 当前无 `log/evidence_registry.json`。 | `020-normal-plane-evidence-registry-target-alias` |

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | phase 010 是 test-support / doc-suite 结构证据；board compare 仍沿用 phase 000 的 production-shaped diagnostic。 |
| A/B boundary | helper bench 是 protected helper hot path；公开入口 dispatch / fallback 由 GTest 覆盖。 |
| 当前决策问题 | harness correctness、board fixture 参数和文档可恢复性，不做 RVV family selection。 |
| diagnostic 是否可外推到 production | 不外推；phase 000 已把 helper bench 与公开入口测试分层。 |
| comparison-boundary / baseline mismatch 风险 | host PCD path 误传到 board 的风险已由 Makefile target-specific args 修正。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；本阶段未新增 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；未选择新 RVV family。 |

## Evidence registry 状态

`evidence_registry_status = not_available / phase_deferred + unblocked`

当前 topic 已有 phase-local manifest 和 Evidence Doctor 报告，但还没有 topic-local wrapper 或 `log/evidence_registry.json`。这不改变 phase 000 的 positive bucket，因为当前 board summary、manifest 和 doctor 已人工对齐；它仍是下一阶段默认动作，用于降低后续覆盖日志或手工复跑导致 stale evidence 的风险。

后续 phase 020 已关闭该缺口；恢复当前 topic 时以 `doc/phases/020-normal-plane-evidence-registry-target-alias/result.zh.md` 和 `doc/phases/README.zh.md` 为准。

## 继续 / 停止决定

phase 010 本身关闭。仍存在当前 topic 授权范围内的未阻塞动作：

```text
020-normal-plane-evidence-registry-target-alias
```

继续该 phase 不需要扩大 production 或 public API，但需要新增 topic-local script / Make alias，属于测试资产和证据自动化范围。若本轮不继续，合法停止条件只能是用户要求先交审、dirty isolation 变化、工具失败或 reviewer 要求先检查当前结构 diff。
