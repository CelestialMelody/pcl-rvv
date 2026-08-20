# Phase 040 result: production detail helper-only ablation

## 当前结论

本阶段把计时边界切到 production detail helper（生产内部 helper）本体，排除了 `process()` 的
`initCompute`、organized 检查、`projection_matrix_.inverse()`、stdout 打印和 `computeDistances()`。

bounded board run 的两次结果分别是：

| case | run 1 | run 2 | 结论 |
| --- | ---: | ---: | --- |
| `production detail helper PointXYZRGB 80x60 w3 dense` | 1.07x | 0.94x | 不稳定 |
| `production detail helper PointXYZRGB 120x90 w4 holes` | 1.02x | 0.90x | 不稳定 |
| `production detail helper PointXYZRGBA 180x120 w5 dense` | 1.01x | 0.95x | 不稳定 |

Evidence Doctor（证据体检）在这组 helper-only 证据上给出 `Errors=3`、`Warnings=13`、`Suggestions=1`。
这说明 helper 本体并没有稳定地把生产补丁抬成可采纳实现。phase 050 随后又继续做了
mask/chunk 消融，并把当前 truth 推进到更细一层；因此 phase 040 现在只保留为历史证据。

## 计划回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| helper-only bench | done | `src/bench_bilateral_upsampling.cpp` | 新增 `production detail helper` 三个 case。 |
| QEMU correctness | done | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | Std/RVV 9/9 通过。 |
| QEMU bench smoke | done | `make -C test-rvv/surface/bilateral_upsampling run_bench_rvv BENCH_ARGS='1 0'` | helper-only case 的输出形状和 checksum 可见；QEMU timing 不采信。 |
| asm refresh | done | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | RVV 指令仍归属到 helper 相关符号。 |
| board refresh | done | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | 两次 bounded run 方向冲突。 |
| doctor refresh | done-with-errors | `doc/phases/040-production-detail-helper-only-ablation/evidence_manifest.json` -> `evidence-doctor.md` | `Errors=3`，helper-only 不稳定。 |

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | production-detail-ablation |
| A/B boundary | production detail helper；helper-only direct call。 |
| 当前决策问题 | implementation-shape：helper 本体是否稳定快于标量。 |
| diagnostic 是否可外推到 production | 不能直接外推到采纳；它只说明 helper 本体是否值得继续拆。 |
| comparison-boundary / baseline mismatch 风险 | 有；helper-only 仍与 public boundary 不同，且两次 run 方向冲突。 |
| weak / negative / neutral / unstable 时是否允许 bounded probe | 已完成 bounded probe；结果仍不支持采纳。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 是；当前没有进入该门。 |

## 板卡复跑预算和决策桶

本阶段预算已用完：两次 bounded run 已给出冲突方向，决策桶应视为 unstable-negative，不再继续自动复跑 helper-only。

## 继续 / 停止决定

`continue_stop_decision`: stop_for_user_judgment。

`stop_condition_hit`: helper-only 已不能稳定解释生产补丁收益；phase 050 随后转入更细的 mask/chunk 消融。

`next_phase_default`: 参考 phase 050 的结果；phase 040 自身不再作为当前 truth。
