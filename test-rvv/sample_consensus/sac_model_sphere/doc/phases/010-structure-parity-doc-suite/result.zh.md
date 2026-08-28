# Phase 010: structure parity doc suite 结果

## 执行摘要

本阶段补齐了 `sac_model_sphere` topic-local doc suite（主题本地文档套件），没有修改 C++ 测试逻辑、bench 逻辑或 production（生产源码）。Phase 000 的 repeated board、Evidence Doctor（证据体检）和候选取舍现在分别有稳定主归属：README 做导航，testing overview 做 target 粒度审计，correctness role 解释 gtest，benchmark/evidence role 解释 bench 和证据边界，optimization evidence role 解释 candidate 取舍，code map 解释测试支撑代码定位。

## 产物变化

| role | 路径 | 状态 |
| --- | --- | --- |
| topic_navigation | `test-rvv/sample_consensus/sac_model_sphere/README.zh.md` | adopted |
| testing_overview | `doc/testing-overview.zh.md` | adopted |
| correctness_tests | `doc/correctness-tests.zh.md` | adopted |
| benchmark_and_evidence | `doc/benchmark-and-evidence.zh.md` | adopted |
| optimization_evidence | `doc/optimization-evidence.zh.md` | adopted |
| test_support_code_map | `doc/test-support-code-map.zh.md` | adopted |
| phase_plan / phase_result | `doc/phases/010-structure-parity-doc-suite/` | adopted |

## Target 粒度审计结果

| 类别 | 当前 shape scan | decision | next action |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` 同时跑 Std/RVV gtest。 | adopted | 后续 production direct 时继续复用。 |
| correctness aliases | `run_sphere_public_tests` / `run_board_sphere_public_tests` 用 gtest filter 隔离三条核心用例。 | adopted | PI1 后若新增 production direct select fallback，再补细分 filter。 |
| bench diagnostic aliases | `bench_sac_model_sphere` 输出 public 和 candidate 五行；没有单独 case-filter。 | adopted for Phase 000 | 后续 production bench 若需要隔离 select direct，应新增 case metadata，而不是复用 diagnostic 行。 |
| QEMU smoke aliases | `run_test_compare`、`dump_bench_rvv`。 | adopted | QEMU 不写性能结论。 |
| board smoke aliases | `board_smoke` 跑 board test + 单次 bench。 | adopted | 单次 board 只做可运行和输出形状。 |
| board repeated aliases | 5-run repeated 目前是手工目录 `log/board/repeated-20260827-phase000/run-*`；Phase 015 已补正式 doctor / registry target。 | adopted | 后续 production direct 仍需新的 repeated run 和 case metadata。 |
| doctor / registry aliases | `record_repeated_board_evidence_state` 和 `repeated_evidence_status` 已能生成 doctor JSON、登记 registry 并检查 freshness。 | adopted | Phase 020 PI1 应要求 production evidence registry 或等价 freshness check。 |
| historical probe guarded aliases | 当前 topic 没有历史 guarded probe。 | not_applicable with evidence | 无。 |

## doc_suite_role_inventory

| role | decision | 证据 |
| --- | --- | --- |
| topic_navigation | adopted | README 列出当前结论、先读路径、命令、可提交证据和默认排除项。 |
| testing_overview | adopted | `doc/testing-overview.zh.md` 覆盖 Makefile、board.mk、test、bench、script 和证据输出。 |
| correctness_tests | adopted | `doc/correctness-tests.zh.md` 逐项解释 3 个 gtest。 |
| benchmark_and_evidence | adopted | `doc/benchmark-and-evidence.zh.md` 记录 bench label、计时边界、board repeated、doctor 和 asm 口径。 |
| optimization_evidence | adopted | `doc/optimization-evidence.zh.md` 区分 count adopted、select partial、getDistances rejected 和 deferred 路线。 |
| test_support_code_map | adopted | `doc/test-support-code-map.zh.md` 能定位 production、test helper、bench wrapper、script 和 output。 |
| optimization_roadmap | adopted | `doc/optimization-roadmap.zh.md` 已把默认恢复动作更新到 PI1 计划。 |
| phase_index / matrix | adopted | `doc/phases/README.zh.md` 与 `optimization-matrix.zh.md` 已可恢复。 |
| production_topic_doc | not_applicable with evidence | select/getDistances 尚无 PI5 通过并由用户确认采纳的 production 行为。 |

## Artifact tracking

本阶段新增文档均位于 `test-rvv/sample_consensus/sac_model_sphere/` topic 边界内，属于 topic-local test asset。`log/board/**` raw logs 和 `build/**` 仍默认排除；Phase 000 manifest / doctor 是 summary evidence，可作为 review 候选。

## Continue / stop decision

Phase 010 当前完成；Phase 015 随后关闭了 registry alias 缺口。下一阶段默认进入：

```text
next_phase_default: 020-select-production-integration-plan
```

Phase 020 只写 `selectWithinDistance` 的 PI1 production integration plan。进入 PI2 production patch 会修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp`，需要用户明确授权，不能由本阶段自动推进。
