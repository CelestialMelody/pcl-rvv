# Phase 020 Result: production-shaped-color-coder-precheck

> 更新说明：phase 050 已覆盖 `log/board/component_repeat_5/` 的 summary / manifest / Evidence Doctor / registry。本文中的 phase 020 run label 只保留为 historical evidence（历史证据）；当前证据事实见 `../050-decode-implementation-shape-audit/result.zh.md`。

## 执行范围

本阶段按 plan 增加了 production-shaped diagnostic（生产形态诊断），使用真实 `pcl::PointXYZRGBA` 点类型验证 RGBA 字段 offset 与 AoS（结构数组）stride 下的 candidate helper。没有修改 `io/include/pcl/compression/color_coding.h`，没有创建 production patch（生产补丁）。

本阶段曾覆盖 `log/board/component_repeat_5/`，因此 phase 010 的同路径 summary 在 phase 020 时降级为 historical evidence（历史证据）；phase 050 后本文数字本身也降级为 historical evidence。

## 动作结果

| action | status | command / evidence | conclusion |
| --- | --- | --- | --- |
| production-shaped smoke | done | `make -C test-rvv/io/color_coding run_test_compare` | Std/RVV 各 6 个测试通过；新增 `ColorCodingProductionShaped.PointXYZRGBAMatchesComponentSemantics` |
| point-vector helper | done | `include/impl/color_coding_support.hpp` | test-only helper 支持 `pcl::PointXYZRGBA` 的 average / encode / decode / default 诊断链路 |
| production-shaped bench labels | done | `src/bench_color_coding.cpp` | 新增 `ps_encode_average_*`、`ps_encode_points_*`、`ps_decode_points_*`、`ps_set_default_color_*` |
| QEMU bench smoke | done | `make -C test-rvv/io/color_coding run_bench_rvv BENCH_ARGS="--case-filter ps_encode_points_leaf257 --iterations 2 --warmup-iterations 1 --batch-repeats 2"` | 只验证 RVV bench 输出 label 和日志形状 |
| board repeated bench | done | `make -C test-rvv/io/color_coding run_board_color_coding_repeated` | 5-run summary / manifest / Doctor / registry 已刷新 |
| asm attribution | done | `make -C test-rvv/io/color_coding dump_bench_rvv` | candidate binary 中仍有 RVV 指令；归属是测试候选二进制，不是 production hot symbol |
| evidence registry | done | `log/evidence_registry.json` | current run label 为 `board-color-coding-component-repeat-phase020` |
| doc-suite essentials | partial | 本 result / evaluation / roadmap / matrix 更新；独立 testing / benchmark / code-map role docs 未拆出 | deferred；不阻塞本阶段证据，但仍是下一轮 closeout 前结构动作 |

## Board Summary

当前板卡证据路径：

- summary: `test-rvv/io/color_coding/log/board/component_repeat_5/summary.md`
- manifest: `test-rvv/io/color_coding/log/board/component_repeat_5/evidence_manifest.json`
- Evidence Doctor（证据体检）: `test-rvv/io/color_coding/log/board/component_repeat_5/evidence_doctor.md`
- registry: `test-rvv/io/color_coding/log/evidence_registry.json`

| case family | scope | median speedup | min | max | decision bucket |
| --- | --- | ---: | ---: | ---: | --- |
| `encode_average` | component, leaf 31/257/1024/4096 | 1.58x / 2.53x / 1.88x / 1.89x | all > 1.52x | up to 2.86x | positive, separate reporting required |
| `encode_points` | component, leaf 31/257/1024/4096 | 1.20x / 1.29x / 1.25x / 1.25x | all > 1.18x | up to 1.30x | positive |
| `decode_points` | component, leaf 31/257/1024/4096 | 1.04x / 1.08x / 1.06x / 1.07x | all > 1.01x | up to 1.16x | weak-positive |
| `set_default_color` | component, 4096/16384 | 1.23x / 1.27x | all > 1.10x | up to 1.38x | positive with long-tail warning on 16384 |
| `ps_encode_average` | `PointXYZRGBA`, leaf 257/4096 | 2.49x / 1.94x | 1.50x / 2.47x lower bound | up to 2.91x | positive, separate reporting required |
| `ps_encode_points` | `PointXYZRGBA`, leaf 257/4096 | 1.30x / 1.22x | 1.29x / 0.81x | up to 1.32x | positive but large leaf has degradation warning |
| `ps_decode_points` | `PointXYZRGBA`, leaf 257/4096 | 1.06x / 1.00x | 1.01x / 0.82x | up to 1.12x | leaf257 weak-positive; leaf4096 unstable / blocked |
| `ps_set_default_color` | `PointXYZRGBA`, 4096 | 1.16x | 1.07x | 1.19x | positive |

## Evidence Doctor 回填

Evidence Doctor 结果为 `Errors=1, Warnings=13, Suggestions=2`。

| severity | signal | case | handling |
| --- | --- | --- | --- |
| Error | `ba_degradation_frequency` | `ps_decode_points_leaf4096` | decode production-shaped large case 不能进入 PI1；降级为 implementation-shape audit |
| Warning | `ba_degradation_frequency` | `ps_encode_points_leaf4096` | encode points large case 5-run 中 1 次退化；可以作为 partial candidate，但 PI1 必须独立保留 fallback / size gate |
| Warning | `long_tail_or_variance` | `ps_decode_points_leaf4096` | 与 Error 同源；不能用 median=1.00x 写正向 |
| Warning | `long_tail_or_variance` | `ps_encode_average_leaf257` / `ps_encode_average_leaf4096` | average 仍必须单独报告；高收益不能 group-wide 外推 |
| Warning | `group_outlier` | component / production-shaped average cases | average 是独立 candidate family |
| Suggestion | `near_threshold_ba` | `decode_points_leaf31` / `ps_decode_points_leaf4096` | weak / near-threshold decode 不适合直接接 production |

Doctor Error 是本阶段停止进入 decode production candidate 的硬边界。它不推翻 encode average、encode points 或 default 的 production-shaped evidence，但要求下一阶段把候选拆分，不能把整个 `ColorCoding` 类写成统一 production-ready。

## Diagnostic 到 production mismatch audit

| question | result |
| --- | --- |
| evidence role | mixed: component_ablation + production_shaped_diagnostic |
| A/B boundary | test helper + production-shaped helper |
| 当前决策问题 | 是否值得进入 PI1 production integration plan（生产接入计划） |
| diagnostic 是否可外推到 production | partial；`pcl::PointXYZRGBA` RGBA offset / stride 已覆盖，但仍没有 public compression entry、entropy coder 或真实 leaf distribution |
| comparison-boundary / baseline mismatch 风险 | yes；bench wrapper 与真实 octree compression caller 仍不同 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | encode/default 可以进入 PI1 计划；decode large case 因 Error 不进入 production probe |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段不支持 clean adoption |

## Optimization Matrix 更新

| candidate family | row source / layout | correctness | board evidence | Evidence Doctor | decision | next action |
| --- | --- | --- | --- | --- | --- | --- |
| RVV indexed encode average | component + `PointXYZRGBA` production-shaped | pass | component median 1.58x-2.53x；ps median 2.49x / 1.94x | warnings require separate reporting | partial-production-candidate for PI1 plan | PI1 scope may include average |
| RVV indexed encode points | component + `PointXYZRGBA` production-shaped | pass | component median 1.20x-1.29x；ps median 1.30x / 1.22x | warning: `ps_encode_points_leaf4096` 1/5退化 | partial-production-candidate with size/fallback caution | PI1 scope may include encode points with large-leaf guard |
| RVV decode | component positive but `PointXYZRGBA` large unstable | pass | ps leaf257 1.06x, ps leaf4096 median 1.00x min 0.82x | Error on `ps_decode_points_leaf4096` | rejected/deferred for production candidate | implementation-shape audit or keep scalar |
| RVV default color | component + `PointXYZRGBA` production-shaped | pass | component 1.23x / 1.27x；ps 1.16x | no default-specific Error | partial-production-candidate for PI1 plan | PI1 scope may include default fill |

## Doc Suite Role Inventory

| role | status | evidence / next action |
| --- | --- | --- |
| topic_navigation | `standalone:README.zh.md` | updated enough for current paths |
| testing_overview | `phase_deferred + unblocked` | should be added before final closeout / review |
| correctness_tests | `phase_deferred + unblocked` | production-shaped TEST is in source, but no standalone dictionary |
| benchmark_and_evidence | `phase_deferred + unblocked` | summary / Doctor exist, but labels and target taxonomy need standalone doc |
| optimization_evidence | `merged:doc/phases/optimization-matrix.zh.md` | current matrix updated |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` | updated with PI1 and decode audit |
| test_support_code_map | `phase_deferred + unblocked` | point-vector helpers and scripts need a stable map before closeout |
| phase_index | `standalone:doc/phases/README.zh.md` | updated |
| evaluation_diagnostic | `standalone:doc/color_coding-evaluation.zh.md` | updated |
| evaluation_production | `not_applicable with evidence` | no production patch yet |
| production_topic_doc | `not_applicable with evidence` | no adopted production behavior |

## 阶段反思

production-shaped evidence split the topic into two paths:

1. encode average / encode points / default have enough signal for a bounded PI1 plan, with explicit fallback and size gates.
2. decode should stay scalar for now. `ps_decode_points_leaf4096` has an Evidence Doctor Error and near-threshold suggestion, so it is not a production candidate.

This phase also showed `pcl::PointXYZRGBA` has a larger stride and different cache behavior from test `ColorPoint`; production-shaped results cannot be inferred from component results case-by-case.

## Continue / Stop Decision

`continue_stop_decision`: stop at production authorization checkpoint.

`stop_condition_hit`: continuing beyond phase 030 plan would modify `io/include/pcl/compression/color_coding.h`, which is production source and requires user confirmation under `AGENTS.md`.

`next_phase_default`: `030-pi1-encode-default-production-integration-plan` has been created. If the user confirms production patch work, resume from that plan and only patch the frozen encode/default scope; keep decode scalar.
