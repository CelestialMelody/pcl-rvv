# Phase 000 Result: current-state-and-diagnostic-scaffold

## 执行范围

本阶段按 plan 创建了 `color_coding` topic-local scaffold（主题本地脚手架），没有修改 production（生产源码）。实际覆盖范围仍是 component diagnostic（组件诊断）：synthetic `ColorPoint`、32-bit RGBA field、indexed leaf gather（按索引读取叶节点）和 contiguous output range（连续输出区间）。

## 动作结果

| action | status | command / evidence | conclusion |
| --- | --- | --- | --- |
| RED tests | done | `make -C test-rvv/io/color_coding run_test_rvv` 初次运行：4 个 reference 测试通过，candidate `ASSERT_TRUE` 失败 | 测试能抓住 candidate 未实现，不是 tautology（同义反复式测试） |
| GREEN candidate | done | `include/impl/color_coding_support.hpp` | RVV candidate 覆盖 indexed sum、decode strided store、default color strided store；diff byte stream 写入仍保持标量 |
| QEMU correctness | done | `make -C test-rvv/io/color_coding run_test_compare` | Std/RVV 共 5 个测试通过；QEMU 只证明正确性和路径可运行 |
| QEMU bench smoke | done | `make -C test-rvv/io/color_coding run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` | 只验证 bench 可运行和输出格式；不作为性能结论 |
| asm attribution | partial | `make -C test-rvv/io/color_coding dump_bench_rvv`；`build/asm/riscv/bench_color_coding_rvv.asm` | binary 中出现 `vluxei32`、`vredsum`、`vlse8`、`vsse32`；存在编译器自动向量化噪声，归属仅写成 candidate binary contains expected RVV instructions |
| board repeated bench | done | `make -C test-rvv/io/color_coding run_board_color_coding_repeated` | 5-run summary / manifest / Evidence Doctor / registry 已生成 |
| evidence freshness | done | `make -C test-rvv/io/color_coding check_evidence_freshness` | registry fresh |

## Board Summary

当前板卡证据路径：

- summary: `test-rvv/io/color_coding/log/board/component_repeat_5/summary.md`
- manifest: `test-rvv/io/color_coding/log/board/component_repeat_5/evidence_manifest.json`
- Evidence Doctor（证据体检）: `test-rvv/io/color_coding/log/board/component_repeat_5/evidence_doctor.md`
- registry: `test-rvv/io/color_coding/log/evidence_registry.json`

| case | mean speedup | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `encode_average_leaf31` | 1.4286x | 1.4286x | 1.4286x | 1.4286x | positive but measurement too small / outlier |
| `encode_points_leaf257` | 1.1355x | 1.1366x | 1.1208x | 1.1463x | positive component diagnostic |
| `decode_points_leaf257` | 1.1020x | 1.1227x | 0.9533x | 1.2227x | unstable / degraded evidence due to 2/5 below 1 |
| `set_default_color_4096` | 1.2474x | 1.2231x | 1.2195x | 1.3014x | positive component diagnostic |

## Evidence Doctor 回填

Evidence Doctor 结果为 `Errors=1, Warnings=2, Suggestions=0`。

| severity | signal | case | handling |
| --- | --- | --- | --- |
| Error | `ba_degradation_frequency` | `decode_points_leaf257` | decode 不能写成 clean positive；降级为 unstable / evidence needs stabilization |
| Warning | `long_tail_or_variance` | `decode_points_leaf257` | phase 010 用更长计时边界和 leaf-size sweep 复核 |
| Warning | `group_outlier` | `encode_average_leaf31` | 该 case 时间太短且偏离组内 median，不能外推到 encode family |

## Diagnostic 到 production mismatch audit

| question | result |
| --- | --- |
| evidence role | component_ablation / diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar component evidence 和 implementation-shape |
| diagnostic 是否可外推到 production | no；它不包含 octree traversal、entropy coder、real leaf-size distribution 或 public compression entry |
| comparison-boundary / baseline mismatch 风险 | yes；component helper 的 synthetic leaf 和完整 compression caller 不同 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | decode 不稳定不能阻止未来 probe，但必须先稳定 leaf-size / timing evidence；本阶段不进入 production |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；phase 000 不满足 clean adoption |

## Optimization Matrix 更新

| candidate family | row source policy | point type / layout | correctness | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| scalar same-chain reference | indexed leaf + contiguous decode | `ColorPoint`, RGBA offset | pass | not_applicable | not_applicable | not_applicable | adopted as test baseline | keep |
| RVV indexed encode average | indexed leaf | `ColorPoint`, RGBA offset | pass | 1.43x but tiny timing / outlier warning | expected RVV instructions present | Warning | attempted positive but not production-ready | phase 010 longer timing |
| RVV indexed encode points | indexed leaf | `ColorPoint`, RGBA offset | pass | median 1.1366x, min 1.1208x | expected RVV instructions present | no case-specific issue | positive component diagnostic | phase 010 leaf-size sweep |
| RVV decode | contiguous output segment | `ColorPoint`, RGBA offset | pass | median 1.1227x but 2/5 below 1 | expected RVV instructions present | Error + Warning | unstable / degraded | phase 010 stabilize or reject decode candidate |
| RVV default color | contiguous output segment | `ColorPoint`, RGBA offset | pass | median 1.2231x, min 1.2195x | expected RVV instructions present | no case-specific issue | positive component diagnostic | phase 010 longer timing / production-shaped precheck |

## 阶段反思

Phase 000 证明了测试支撑、QEMU correctness、板卡和 Evidence Doctor 链路可工作，但当前 bench case 太短，尤其 `encode_average_leaf31` 只有约 `0.001ms/iter`，容易被计时粒度和函数调用开销主导。`decode_points_leaf257` 出现退化频率 Error，说明不能把 median 正向直接写成 production 候选。

新增 candidate / 下一阶段动作：

| new idea | reason | priority |
| --- | --- | --- |
| leaf-size sweep + longer timing boundary | 稳定 leaf size 31/257/1024/4096 的 component evidence，减少 0.001ms 级别噪声 | high |
| split encode vs decode decision | encode points 与 default color 正向较稳，decode 不稳定，应独立批准 | high |
| production-shaped precheck | 若 phase 010 仍显示 encode/default positive，可再评估真实 `pcl::PointXYZRGBA` / caller shape | medium |

## Continue / Stop Decision

`continue_stop_decision`: continue.

`stop_condition_hit`: none.

`next_phase_default`: `010-bench-stabilization-and-leaf-size-sweep`。该动作仍在当前 topic 测试资产和诊断边界内，板卡可用，且 phase 000 暴露了 Evidence Doctor Error，因此不能写 `ready_for_review`。
