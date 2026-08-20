# Phase 060 result: production detail color-gather ablation

## 当前结论

本阶段验证了一个新的 bench-local production-detail family：`color-gather`。它把 RGB 距离和 RGB 查表搬到 RVV lane 内，保留 depth load、finite skip、reduction 和 unprojection 语义不变，但不再只是缩小 mask/chunk，而是改了颜色 staging 的组织方式。

最新 board smoke 的同边界结果（bench-local direct helper）为：

| case | Std avg | RVV avg | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `production detail color-gather PointXYZRGB 80x60 w3 dense` | 6.6037 ms | 5.0404 ms | 1.31x | positive precursor |
| `production detail color-gather PointXYZRGB 120x90 w4 holes` | 27.0980 ms | 21.1978 ms | 1.28x | positive precursor |
| `production detail color-gather PointXYZRGBA 180x120 w5 dense` | 79.7144 ms | 74.2310 ms | 1.07x | positive precursor |

QEMU correctness（QEMU 正确性）仍为 9/9 passed。`dump_bench_rvv` 也能看到这条新 family 对应的 `vlse8.v`、`vzext.vf2`、`vmaxu.vv`、`vminu.vv`、`vluxei16.v`、`vfredusum.vs`，说明 RVV 指令已经归属到新 family 的 helper。

这个阶段只证明了新 family 值得往前推进，还没把生产接入结论写成 adopted。它是 phase 070 production integration probe 的前置证据，而不是最终采纳。

## 计划回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| 新 color-gather bench helper | done | `src/bench_bilateral_upsampling.cpp` | 新增 bench-local RVV helper 和 case wrapper。 |
| QEMU correctness | done | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | Std/RVV 9/9 通过。 |
| asm refresh | done | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | 新 family 的 RVV 指令可归属。 |
| board smoke | done | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | `1.31x / 1.28x / 1.07x`。 |
| evidence doctor | done | `doc/phases/060-production-detail-color-gather-ablation/evidence_manifest.json` -> `evidence-doctor.md` | `Errors=0`、`Warnings=0`、`Suggestions=0`。 |

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | production-detail-ablation |
| A/B boundary | bench-local direct helper call |
| 当前决策问题 | 新 color-gather family 是否比 helper-only / nan-mask 更值得推进到 production integration |
| diagnostic 是否可外推到 production | 只能作为候选族筛选，不能直接当 adopted |
| comparison-boundary / baseline mismatch 风险 | yes，bench-local helper 仍不是 production direct |
| weak / negative / neutral / unstable 时是否允许 bounded production probe | 本阶段结果正向，允许进入下一步 production probe |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 是，后续仍需要同边界 production 证据 |

## 板卡复跑预算和决策桶

本阶段只跑了一次 board smoke。三个 case 都过了 1.0x，且没有 Evidence Doctor 异常；因此这个 family 可以继续推进，但还不能独立代表 production adoption。

## 继续 / 停止决定

`continue_stop_decision`: continue_to_production_probe。

`stop_condition_hit`: 新 family 首次在板卡上形成稳定正向 precursor，但它仍是 bench-local helper，不是 production direct。

`next_phase_default`: `070-production-detail-color-gather-production-probe`。
