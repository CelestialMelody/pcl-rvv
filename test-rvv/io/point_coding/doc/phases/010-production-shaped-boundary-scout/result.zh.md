# Phase 010：生产形态边界侦察结果

## 执行范围

本阶段按 `plan.zh.md` 扩展 small leaf / small segment sweep（小 leaf / 小输出段扫描）。实际改动仍只在 `test-rvv/io/point_coding`，没有修改 production（生产源码）。

本阶段把 `log/board/repeated_phase000/summary.md` 刷新为 12-case current evidence（当前证据）。Phase 000 的 6-case 数字只保留为 historical evidence（历史证据），不再作为当前 truth。

## 计划动作回填

| 动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| 新增 small encode case | done | `src/bench_point_coding.cpp` 新增 `encode_indexed_16/64/256`。 | 小 leaf encode sweep 可测。 |
| 新增 small decode case | done | 新增 `decode_contiguous_16/64/256`。 | 小输出段 decode sweep 可测。 |
| manifest metadata | done | `script/generate_point_coding_evidence_manifest.py` 补 6 个 case。 | Evidence Doctor 能识别 group / size。 |
| correctness | done | `make run_test_compare`。 | Std / RVV 各 3 个 test pass。 |
| QEMU smoke | done | `make run_qemu_bench_smoke`。 | bench binary 和日志形状正常；不作为性能证据。 |
| board repeated | done | `make collect_board_repeated POINT_CODING_REPEATED_RUNS=5`。 | 生成 12-case repeated summary。 |
| Evidence Doctor | done | `make run_board_repeated_evidence_doctor`。 | Errors=0，Warnings=9，Suggestions=0。 |

## 当前板卡结果

| case | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `encode_indexed_16` | 1.17x | 1.17x | 1.25x | weak-positive，小 leaf 最窄收益 |
| `encode_indexed_64` | 1.78x | 1.78x | 1.78x | positive |
| `encode_indexed_256` | 2.13x | 1.66x | 2.15x | positive with long-tail warning |
| `encode_indexed_1024` | 2.06x | 1.75x | 2.13x | positive with long-tail warning |
| `encode_indexed_4096` | 1.74x | 1.59x | 1.88x | positive with long-tail warning |
| `encode_indexed_16384` | 1.27x | 1.20x | 1.37x | positive，但大 leaf 收益下降 |
| `decode_contiguous_16` | 1.29x | 1.29x | 4.71x | weak-positive with outlier |
| `decode_contiguous_64` | 1.29x | 1.23x | 1.29x | weak-positive |
| `decode_contiguous_256` | 1.27x | 1.27x | 1.28x | weak-positive |
| `decode_contiguous_1024` | 1.27x | 1.27x | 1.27x | weak-positive |
| `decode_contiguous_4096` | 1.18x | 1.17x | 1.19x | weak-positive |
| `decode_contiguous_16384` | 1.18x | 0.91x | 1.22x | unstable / weak-positive |

## Evidence Doctor 处理

`log/board/repeated_phase000/evidence_doctor.md` 报告 `Errors=0，Warnings=9，Suggestions=0`。

| finding | 处理动作 | 对结论影响 |
| --- | --- | --- |
| `decode_contiguous_16` long-tail | 保留 4.71x outlier，不把它写成稳定收益。 | 小 decode 只按 median 1.29x 写 weak-positive。 |
| `decode_contiguous_16384` degradation frequency + long-tail | 保留 1/5 退化到 0.91x 的事实。 | decode 大规模降级为 unstable / weak-positive，不允许 production probe。 |
| `encode_indexed_1024/256/4096` long-tail | 保留 min / median / max。 | encode 仍 positive，但需要按 size 分开 gate。 |
| `encode_indexed_16/16384/256` group outlier | 不按组整体外推。 | 真实 leaf size 分布会影响收益，production-shaped context 仍需单独证据。 |

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | diagnostic / component ablation。 |
| A/B boundary | test helper。 |
| 当前决策问题 | small leaf 下局部 RVV-vs-scalar 是否仍可继续。 |
| diagnostic 是否可外推到 production | no。小 leaf encode 正向只说明继续 encode 量化语义值得做；decode warning 不支持 production probe。 |
| comparison-boundary / baseline mismatch 风险 | helper A/B 低；production 外推高。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | decode 不允许；encode 也必须先解决 double 量化语义。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。 |

## 决策

Phase 010 结论：

- encode：small leaf 到 large leaf 均未出现方向性退化，`encode_indexed_16` 仍有 1.17x median。局部收益足够支撑下一阶段专攻量化语义。
- decode：多数 case weak-positive，但 `decode_contiguous_16384` 出现 1/5 退化，且 `decode_contiguous_16` 有异常高 outlier。decode 不作为下一 production-shaped probe 主线。
- production：仍不修改。当前 blocker（阻塞项）不是板卡，而是 encode full RVV quantize 的 same-chain correctness（同构正确性）未闭合。

`continue_stop_decision`：继续当前 topic。下一 phase 默认是 `020-encode-quantize-safety`，范围仍在 test-rvv 内，目标是尝试 double-semantics RVV quantize（双精度语义 RVV 量化）或边界 lane fallback。

`stop_condition_hit`：none。
