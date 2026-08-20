# Phase 070 result: production detail color-gather production probe

## 当前结论

当前 production helper 已接入 color-gather family 后，板卡 truth 明显转正，并已被用户确认保留：

| case | Std avg | RVV avg | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `production public PointXYZRGB 80x60 w3 dense` | 6.3710 ms | 4.6208 ms | 1.38x | positive |
| `production public PointXYZRGB 120x90 w4 holes` | 25.9836 ms | 21.3784 ms | 1.22x | positive |
| `production public PointXYZRGBA 180x120 w5 dense` | 76.6836 ms | 75.4038 ms | 1.02x | weak-positive |
| `production steady public PointXYZRGB 80x60 w3 dense` | 6.5479 ms | 4.8860 ms | 1.34x | positive |
| `production steady public PointXYZRGB 120x90 w4 holes` | 26.2345 ms | 21.2183 ms | 1.24x | positive |
| `production steady public PointXYZRGBA 180x120 w5 dense` | 78.1475 ms | 73.9974 ms | 1.06x | positive |

QEMU correctness 仍为 9/9 passed。`dump_bench_rvv` 仍能看到 `vlse8.v`、`vzext.vf2`、`vmaxu.vv`、`vminu.vv`、`vluxei16.v`、`vfredusum.vs`，说明当前 production helper 确实落在新的 color-gather family 上。

Evidence Doctor 对本阶段 manifest 结果为 `Errors=0`、`Warnings=0`、`Suggestions=1`。这说明本阶段的 board truth 没有错误或警告，但 `PointXYZRGBA 180x120` 的 public case 为 1.02x，属于 near-threshold 弱收益。用户已经确认可接入，因此它保留为 adopted 中的历史风险提示，而不是采纳阻塞。Phase 071/072/073 后当前二进制的性能 freshness 已由 `doc/phases/073-cross-rgb-rgba-production-probe/result.zh.md` 覆盖。

## 计划回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| production helper 接入 color-gather | done | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` | 新 family 已接入真实公开入口，且已用户确认保留。 |
| QEMU correctness | done | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | 9/9 passed。 |
| asm refresh | done | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | 关键 RVV 指令归属到 production helper。 |
| board refresh | done | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | production public / steady public 已转正。 |
| evidence doctor | done | `doc/phases/070-production-detail-color-gather-production-probe/evidence_manifest.json` -> `evidence-doctor.md` | `Errors=0`、`Warnings=0`、`Suggestions=1`。 |

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | production-public / production-detail |
| A/B boundary | real public entry + production helper |
| 当前决策问题 | 当前 color-gather family 是否应该被保留为 production patch |
| diagnostic 是否可外推到 production | 已经外推到了 production public / steady public，并且保持正向 |
| comparison-boundary / baseline mismatch 风险 | Evidence Doctor 已不再报告 A/B 角色冲突；剩余风险是大 RGBA case 只有 weak-positive |
| weak / negative / neutral / unstable 时是否允许 bounded production probe | 当前不是 weak/negative；已完成 bounded probe |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 已完成；当前 family 已进入 adopted 语义 |

## 板卡复跑预算和决策桶

本阶段复跑预算用在一次 board_smoke 上。决策桶现在明显偏正向，并已由用户确认采纳，因此后续不再停在用户确认点。

## 继续 / 停止决定

`continue_stop_decision`: proceed_to_adopted_closeout。

`stop_condition_hit`: adopted production behavior 已确认。

`next_phase_default`: production closeout / 文档收口。
