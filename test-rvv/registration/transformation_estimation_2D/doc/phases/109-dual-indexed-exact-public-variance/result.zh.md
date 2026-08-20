# Phase 109 Result: dual-indexed-exact-public-variance

## 结论

本阶段已完成 Phase 109 计划、correctness（正确性）、QEMU smoke（QEMU 小型验证）、
asm attribution（反汇编归属）、20-run board repeated（板卡重复测试）、Evidence Doctor
（证据体检）和 registry freshness（证据登记新鲜度）刷新。

Phase 109 的 production-detail family A/B 结论为 `positive with retained 4K caveat`：
64K / 256K direct dual-indexed public RVV 明显快于 materialize+ordered public RVV；
4K median 仍为正向，但存在 1/20 below-1 和长尾 Warning。因此当前 exact
`PointXYZ -> PointXYZ` dual-indexed dispatch 继续保留，4K caveat 继续保留，不扩大到
generic point type、correspondence、source-indexed 或其它 `Scalar`。

## 实际执行范围

| 动作 | 状态 | 证据 / 结果 |
| --- | --- | --- |
| Phase 109 plan | done | `doc/phases/109-dual-indexed-exact-public-variance/plan.zh.md` |
| Phase 109 board registry wrapper | done | `run_board_bench_dual_indexed_family_ab_phase109_repeated` 使用独立 run label；registry 登记为 `board-te2d-dual-indexed-family-ab-repeated-phase109-variance`。 |
| correctness | done | `run_test_compare` 和 `record_qemu_correctness_state` 均完成；Std/RVV `84/84` pass。 |
| QEMU / asm / Doctor | done | `record_qemu_dual_indexed_family_ab_state` 完成；Doctor `0/0/0`；`production_public_dual_indexed_boundary` 50 RVV lines。 |
| Phase 109 QEMU doc-ref registry wrapper | done | `record_qemu_dual_indexed_family_ab_phase109_state` 已登记 Phase 109 doc-ref。 |
| board 20-run | done | `dual_indexed_family_ab_phase109_variance_repeated` 独立 evidence dir 完成；summary / manifest / Doctor 已生成。 |
| evidence_status | done | `evidence_status` 为 fresh。 |

## Correctness

命令：

```bash
make -C test-rvv/registration/transformation_estimation_2D run_test_compare
make -C test-rvv/registration/transformation_estimation_2D record_qemu_correctness_state
```

结果：

- Std：`84/84` pass。
- RVV：`84/84` pass。

这证明当前公开入口、fallback 和同边界 family A/B correctness 仍一致；不提供性能结论。

## QEMU / asm / Evidence Doctor

命令：

```bash
make -C test-rvv/registration/transformation_estimation_2D record_qemu_dual_indexed_family_ab_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_dual_indexed_family_ab_phase109_state
```

结果：

- QEMU smoke 可运行；case-filter 为 `dual-indexed-family-ab`。
- Evidence Doctor：`0/0/0`。
- asm attribution 中 `production_public_dual_indexed_boundary` 为 50 RVV lines。

QEMU 计时不作为性能证据；本阶段只把它作为路径命中、日志形状和反汇编归属证据。

## Board 20-run

命令：

```bash
make -C test-rvv/registration/transformation_estimation_2D \
  run_board_bench_dual_indexed_family_ab_phase109_repeated \
  TE2D_BOARD_REPEATED_RUNS=20 \
  TE2D_BOARD_DUAL_INDEXED_FAMILY_AB_RUN_LABEL=dual_indexed_family_ab_phase109_variance_repeated
```

证据路径：

```text
test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase109_variance_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase109_variance_repeated/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase109_variance_repeated/evidence_doctor.md
```

结果摘要：

| size | median B/A | min | max | below-1 | bucket | materialize_ms | direct_ms | checksum |
| --- | ---: | ---: | ---: | ---: | --- | ---: | ---: | --- |
| 4K | 1.080x | 0.772x | 1.194x | 1/20 | negative as case bucket; caveat retained | 0.3285 | 0.3047 | same |
| 64K | 1.661x | 1.583x | 1.729x | 0/20 | positive | 10.0039 | 6.0085 | same |
| 256K | 1.646x | 1.607x | 1.670x | 0/20 | positive | 38.3543 | 23.3141 | same |

`B/A = materialize ordered public RVV ms / direct dual-indexed public RVV ms`，大于 1 表示
direct dual-indexed public RVV 更快。两侧使用同一 RVV binary、同一 dual-indexed row semantics、
同一 `PointXYZ -> PointXYZ` / `Scalar=float` 输入、同一 2D solve 和同一 checksum policy。

## Evidence Doctor 解释

Board Evidence Doctor：Errors=0，Warnings=3，Suggestions=0。

| finding | scope | 处理 |
| --- | --- | --- |
| `ba_degradation_frequency` | 4K | 4K 有 1/20 below-1；按计划不能只看 median，因此保留小规模 caveat。 |
| `long_tail_or_variance` | 4K | 4K min/max 为 `0.772x / 1.194x`，max/min `1.55`；不剔除异常点，作为目标板卡小规模方差风险记录。 |
| `group_outlier` | 4K | 4K median `1.080x` 与同组 64K/256K 明显不同；不把大规模收益外推到小规模。 |

Doctor 无 Error，64K/256K 无 below-1，说明当前 exact dual-indexed direct gather family 在主决策规模上
仍可保留。Warning 全部集中于 4K，所以 production gate 不扩大，文档继续保留 4K caveat。

## Matrix 更新

Phase 109 新增矩阵条目状态更新为 `completed / positive with retained 4K caveat`：

| candidate family | row source | scope | status | next action |
| --- | --- | --- | --- | --- |
| dual-indexed exact public variance | dual-indexed-cloud-pair | exact `PointXYZ -> PointXYZ`, `Scalar=float`, production-detail family A/B | completed / positive with retained 4K caveat | 保留当前 exact dual-indexed dispatch；不扩大到 generic 或 correspondence。 |

## Continue / Stop Decision

`continue_stop_decision`：Phase 109 completed；未命中 board blocker。Phase 109 positive / weak-positive
分支允许进入下一 bounded optimization phase。

`next_phase_default`：按 roadmap 选择新的有界阶段；候选是 source-indexed generic negative-case
investigation 或 correspondence new bounded candidate。任何后续 production gate 变化仍需另建 phase、
独立 evidence dir、correctness、QEMU、asm、board repeated、Evidence Doctor 和用户决策。

本阶段不自动提交、不自动回滚、不改变 production source。
