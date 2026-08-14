# Phase 030 Result: row-source-family-carryover

## 当前结论

本阶段完成了三类非 ordered row source 的 test-only materialize-to-ordered 诊断：

- `source-indexed-cloud-pair`：物化 source row，再复用两遍中心化 fused 2D 累加器。
- `dual-indexed-cloud-pair`：物化 source / target 两侧 row，再复用同一数学流水线。
- `correspondence-pair`：物化 query / match 点对，再复用同一数学流水线。

三类 candidate 都把索引 / correspondence 展开成本纳入 bench 计时，没有实现 production gather kernel，也没有修改 production dispatch。

## 计划动作回填

| 动作 | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| A1 row-source materialization helpers | done | `include/impl/te2d_candidates.hpp` | 合法索引 / correspondence 物化失败时回退；生产路径不变。 |
| A2 correctness tests | done | `make -C test-rvv/registration/transformation_estimation_2D run_test_compare` | Std 16/16、RVV 16/16；三类 candidate 与真实 public scalar overload 对拍通过。 |
| A3 row-source bench case-filter | done | `run_bench_row_source_smoke` | 3 种 row source × 4K/64K/256K，共 9 个 case。 |
| A4 QEMU correctness / smoke | done | `record_qemu_row_source_state` | QEMU 只证明可运行、日志形状和路径归属；Doctor 0/0/0。 |
| A5 Evidence Doctor / registry | done | `record_qemu_row_source_state`、`record_board_row_source_state` | QEMU 和 board manifest 均登记；board Doctor 为 1/2/6。 |
| A6 board repeated | done | `make -C test-rvv/registration/transformation_estimation_2D run_board_bench_row_source_repeated` | `Milkv-Jupiter`、5 runs、每 run 20 iterations、5 warmup 已完成。 |

## Board 证据

`B/A = Std_ms / RVV_ms`，大于 1 表示 RVV build 更快。以下结果来自真实板卡，不是 QEMU：

| row source | 4K median | 64K median | 256K median | 主要异常 | 当前决策 |
| --- | ---: | ---: | ---: | --- | --- |
| source-indexed | 1.073x | 1.023x | 1.038x | 64K 接近阈值 | test-only diagnostic |
| dual-indexed | 1.081x | 1.014x | 1.038x | 64K 有 0.956x 长尾 | test-only diagnostic |
| correspondence | 1.067x | 1.013x | 1.035x | 64K 2/5 低于 1 | test-only diagnostic |

完整 summary：

```text
test-rvv/registration/transformation_estimation_2D/log/board/row_source_fused_repeated/summary.md
```

Evidence Doctor：

```text
test-rvv/registration/transformation_estimation_2D/log/board/row_source_fused_repeated/evidence_doctor.md
```

结果为 Errors=1、Warnings=2、Suggestions=6。Error 是 correspondence 64K 的退化频率；Warnings 是 dual-indexed 64K 的退化频率和长尾；Suggestions 主要要求扩大 runs 或补充板卡环境字段。异常没有被删除，已经进入当前决策边界。

## 证据分层

- correctness：Std / RVV 各 16/16。
- QEMU：9-case row-source smoke、manifest、asm 和 Doctor 0/0/0；不使用 QEMU timing 做性能结论。
- asm：`row_source_lambda_boundary` 能归属 wrapper；共享数学 RVV 仍属于 test-support fixture，不是 production symbol。
- board：真实 `Milkv-Jupiter` repeated summary；包含 materialize-to-ordered 展开成本。
- production boundary：三类 row source 没有 production dispatch；不得从这些弱收益推导 production-ready。

## Optimization Matrix 更新

| row source | board bucket | Evidence Doctor | decision |
| --- | --- | --- | --- |
| source-indexed-cloud-pair | weak_positive / unstable at 64K | 1/2/6 | attempted / diagnostic-only |
| dual-indexed-cloud-pair | weak_positive with 64K long tail | 1/2/6 | attempted / diagnostic-only |
| correspondence-pair | weak_positive except 64K neutral | 1/2/6 | attempted / diagnostic-only |

## Continue / Stop Decision

`continue_stop_decision`：Phase 030 已完成，board blocker 已解除；不能再把本阶段写成 `board_pending`。

`stop_condition_hit`：本阶段 test-only row-source 证据已经闭合；继续修改 production 会扩大到新的 row source 和新的 PI1，需先完成新的实现设计与范围审阅。

`next_phase_default`：`060-production-candidate-review-and-row-source-boundaries`。

下一阶段动作：

1. 审阅当前 ordered-cloud-pair production diff、fallback gate 和 16/16 correctness。
2. 解释 row-source 64K 的退化 / 长尾，优先分析 materialize、缓存局部性和计时边界。
3. 只有形成新的 gather/staging 实现族后，才为 indexed / correspondence 另建 PI1；不恢复旧的 production 接入范围。
