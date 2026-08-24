# Phase 010 Result: bench-stabilization-and-leaf-size-sweep

## 执行范围

本阶段只修改并复跑 `test-rvv/io/color_coding` 下的 component bench（组件性能测试）与证据脚本，没有修改 `io/include/pcl/compression/color_coding.h`。实际范围仍是 component_ablation（组件消融）：synthetic `ColorPoint`、RGBA 字段 offset、indexed leaf（按 leaf 索引读取颜色）和 contiguous output range（连续输出区间）。

phase 010 覆盖了 phase 000 使用的 `log/board/component_repeat_5/` 路径。该路径现在的 summary / manifest / Evidence Doctor 是本阶段 current truth（当前证据事实）；phase 000 result 中的原板卡数字只保留为 historical evidence（历史证据）。

## 动作结果

| action | status | command / evidence | conclusion |
| --- | --- | --- | --- |
| bench case leaf-size sweep | done | `src/bench_color_coding.cpp` | `encode_average`、`encode_points`、`decode_points` 覆盖 leaf 31/257/1024/4096；`set_default_color` 覆盖 4096/16384 |
| longer timing boundary | done | bench 默认 `--iterations 20 --warmup-iterations 3 --batch-repeats 64` | phase 000 中 0.001ms 级别短 case 被更长批次替代 |
| QEMU correctness | done | `make -C test-rvv/io/color_coding run_test_compare` | Std/RVV correctness（正确性）通过；QEMU 不作为性能证据 |
| QEMU bench smoke | done | `make -C test-rvv/io/color_coding run_bench_rvv ...` | 只验证 RVV bench binary 可运行和输出格式 |
| asm attribution | done | `make -C test-rvv/io/color_coding dump_bench_rvv`；`build/asm/riscv/bench_color_coding_rvv.asm` | candidate binary 中可见 `vluxei32`、`vredsum`、`vlse8`、`vsse32`；归属仍是测试候选二进制，不是 production hot symbol |
| board repeated bench | done | `make -C test-rvv/io/color_coding run_board_color_coding_repeated` | 5-run summary / manifest / Evidence Doctor 已刷新 |
| evidence freshness | done | `make -C test-rvv/io/color_coding check_evidence_freshness` | registry fresh；本结果会把 doc ref 更新到 phase 010 |

## Board Summary

当前板卡证据路径：

- summary: `test-rvv/io/color_coding/log/board/component_repeat_5/summary.md`
- manifest: `test-rvv/io/color_coding/log/board/component_repeat_5/evidence_manifest.json`
- Evidence Doctor（证据体检）: `test-rvv/io/color_coding/log/board/component_repeat_5/evidence_doctor.md`
- registry: `test-rvv/io/color_coding/log/evidence_registry.json`

| case family | leaf / size | median speedup | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `encode_average` | 31 | 1.6326x | 1.4734x | 1.6740x | positive, but report separately due to Doctor warnings |
| `encode_average` | 257 | 2.5489x | 2.3802x | 2.7972x | positive, long-tail / group outlier |
| `encode_average` | 1024 | 1.9802x | 1.9374x | 2.0265x | positive, group outlier |
| `encode_average` | 4096 | 1.9374x | 1.9302x | 1.9446x | positive, group outlier |
| `encode_points` | 31 | 1.1321x | 1.1266x | 1.2020x | positive component diagnostic |
| `encode_points` | 257 | 1.2609x | 1.2564x | 1.3037x | positive component diagnostic |
| `encode_points` | 1024 | 1.2419x | 1.2341x | 1.2457x | positive component diagnostic |
| `encode_points` | 4096 | 1.2500x | 1.2467x | 1.2543x | positive component diagnostic |
| `decode_points` | 31 | 1.0805x | 1.0628x | 1.0864x | weak-positive / positive edge, stabilized versus phase 000 |
| `decode_points` | 257 | 1.1154x | 1.1031x | 1.1239x | positive component diagnostic |
| `decode_points` | 1024 | 1.0873x | 1.0796x | 1.0932x | weak-positive / positive edge |
| `decode_points` | 4096 | 1.0873x | 1.0762x | 1.1001x | weak-positive / positive edge |
| `set_default_color` | 4096 | 1.2294x | 1.2218x | 1.2380x | positive component diagnostic |
| `set_default_color` | 16384 | 1.2466x | 1.2366x | 1.2735x | positive component diagnostic |

## Evidence Doctor 回填

Evidence Doctor 结果为 `Errors=0, Warnings=5, Suggestions=0`。

| severity | signal | case | handling |
| --- | --- | --- | --- |
| Warning | `long_tail_or_variance` | `encode_average_leaf257` | 保留 min / median / max，不能只用 mean；不阻塞 phase 010 |
| Warning | `group_outlier` | `encode_average_leaf31` | `encode_average` 必须单独报告，不能把它外推成整个 color_coding 组的统一收益 |
| Warning | `group_outlier` | `encode_average_leaf257` | 同上；若进入 production-shaped diagnostic，需要收窄到 average helper 或补同边界 trace / asm |
| Warning | `group_outlier` | `encode_average_leaf1024` | 同上 |
| Warning | `group_outlier` | `encode_average_leaf4096` | 同上 |

phase 000 的 `decode_points_leaf257` degradation-frequency Error 已不再出现。当前 decode 仍只写成 component diagnostic 正向或弱正向：它证明连续输出 helper 在测试边界内稳定，不证明完整 decompression caller 已经适合生产接入。

## Diagnostic 到 production mismatch audit

| question | result |
| --- | --- |
| evidence role | component_ablation / diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar component evidence 和 implementation-shape |
| diagnostic 是否可外推到 production | no；当前 bench 不包含 octree traversal、entropy coder、公开压缩入口、真实 leaf-size distribution 或 `pcl::PointXYZRGBA` 字段 traits |
| comparison-boundary / baseline mismatch 风险 | yes；synthetic `ColorPoint` 与真实 PCL 点类型、allocator、caller 状态和压缩上下文不同 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes, bounded；只允许先做 production-shaped diagnostic（生产形态诊断），不允许直接改 production |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；phase 010 没有 production boundary 内的 detail A/B |

## Optimization Matrix 更新

| candidate family | row source policy | point type / layout | correctness | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| scalar same-chain reference | indexed leaf + contiguous decode | `ColorPoint`, RGBA offset | pass | not_applicable | not_applicable | not_applicable | adopted as test baseline | keep |
| RVV indexed encode average | indexed leaf | `ColorPoint`, RGBA offset | pass | median 1.63x / 2.55x / 1.98x / 1.94x | expected RVV instructions present | 5 Warnings, all encode_average | positive component diagnostic, report separately | production-shaped precheck; do not generalize group-wide |
| RVV indexed encode points | indexed leaf | `ColorPoint`, RGBA offset | pass | median 1.13x / 1.26x / 1.24x / 1.25x | expected RVV instructions present | no case-specific warning | positive component diagnostic | production-shaped precheck |
| RVV decode | contiguous output segment | `ColorPoint`, RGBA offset | pass | median 1.08x / 1.12x / 1.09x / 1.09x | expected RVV instructions present | no Error after phase 010 | weak-positive to positive component diagnostic | production-shaped precheck, keep decode separate |
| RVV default color | contiguous output segment | `ColorPoint`, RGBA offset | pass | median 1.23x / 1.25x | expected RVV instructions present | no case-specific warning | positive component diagnostic | production-shaped precheck |

## Doc Suite Role Inventory

| role | status | evidence / next action |
| --- | --- | --- |
| topic_navigation | `standalone:README.zh.md` | 已有导航；phase 010 后需要指向 current phase 和 board evidence |
| testing_overview | `phase_deferred + unblocked` | 当前 README 与 evaluation 只覆盖摘要；下一 phase 可补独立 `doc/testing-overview.zh.md` |
| correctness_tests | `phase_deferred + unblocked` | gtest 语义在源码注释中较清楚，但缺少独立 TEST 字典；下一结构 phase 可补 |
| benchmark_and_evidence | `phase_deferred + unblocked` | phase result 与 summary 有当前证据，但缺少稳定 bench label / target 字典；下一 phase 可补 |
| optimization_evidence | `merged:doc/phases/optimization-matrix.zh.md` | 当前 candidate 状态先由 matrix 承载 |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` | 本阶段同步更新 |
| test_support_code_map | `phase_deferred + unblocked` | evaluation 有 Traceability Map，但未形成完整 helper / script / output code map |
| phase_index | `standalone:doc/phases/README.zh.md` | 本阶段同步更新 |
| evaluation_diagnostic | `standalone:doc/color_coding-evaluation.zh.md` | 本阶段同步更新“当前诊断证据链” |
| evaluation_production | `not_applicable with evidence` | 尚无 production patch 或 production direct |
| production_topic_doc | `not_applicable with evidence` | 没有 adopted production behavior 或 PI5 通过并经用户确认 |

doc-suite 缺口仍在当前 topic 授权范围内，且没有工具或权限阻塞；不能把 phase 010 写成 `ready_for_review`。如果 phase 020 执行 production-shaped precheck，可把 testing / benchmark / code-map 文档补齐合并进同一阶段，避免证据继续散落。

## 阶段反思

phase 010 把 phase 000 的不稳定 decode 结论改成弱正向 / 正向 component diagnostic，并把 `encode_average` 的收益从“短 case outlier”修正为“多 leaf size 正向但必须单独解释”。这增加了一个窄范围 production-shaped diagnostic 候选：使用真实 PCL 点类型和 `ColorCoding` 调用形态做预检，判断 component helper 正向是否能越过 synthetic `ColorPoint` 边界。

新增 candidate / 下一阶段动作：

| new idea | reason | priority |
| --- | --- | --- |
| production-shaped `pcl::PointXYZRGBA` precheck | phase 010 无 Error，encode / default / decode 在 component 边界内均有正向信号 | high |
| doc-suite parity essentials | 当前 topic 有 board summary、Evidence Doctor 和多 phase loop，但缺 testing / benchmark / code-map 独立 role | high |
| split average / points / decode / default production decision | Doctor warning 只集中在 `encode_average`，不能把组件收益合并成单一 production 结论 | high |
| full compression public entry | 仍可能被 traversal / entropy coder 主导 | deferred |

## Continue / Stop Decision

`continue_stop_decision`: continue.

`stop_condition_hit`: none.

`next_phase_default`: `020-production-shaped-color-coder-precheck`。该阶段仍应先保持 diagnostic / pre-production（生产前诊断）边界：补 production-shaped test/bench 计划、doc-suite essentials、mismatch audit 和 board / Doctor 证据计划；不得直接修改 `io/include/pcl/compression/color_coding.h`。
