# Phase 050 result: production detail mask/chunk ablation

## 当前结论

本阶段在 bench-only 前提下新增了测试专用 local RVV helper：`production detail local nan-mask k64`。
它保留真实 `PointXYZRGB` / `PointXYZRGBA` 点型、预计算 tables / unprojection 和 helper-only 计时边界，
但把当前 production helper 的 strict finite mask 简化为 NaN mask，并把 chunk 上限从 256 收窄到 64。

最新 board_smoke 结果：

| case | Std avg | RVV avg | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `production detail helper PointXYZRGB 80x60 w3 dense` | 6.5328 ms | 7.1486 ms | 0.91x | 当前 production helper 仍负向 |
| `production detail helper PointXYZRGB 120x90 w4 holes` | 27.2432 ms | 30.0582 ms | 0.91x | 当前 production helper 仍负向 |
| `production detail helper PointXYZRGBA 180x120 w5 dense` | 79.9518 ms | 84.3803 ms | 0.95x | 当前 production helper 仍负向 |
| `production detail local nan-mask k64 PointXYZRGB 80x60 w3 dense` | 6.5499 ms | 6.7130 ms | 0.98x | 仍低于标量 |
| `production detail local nan-mask k64 PointXYZRGB 120x90 w4 holes` | 27.0949 ms | 29.6387 ms | 0.92x | 仍低于标量 |
| `production detail local nan-mask k64 PointXYZRGBA 180x120 w5 dense` | 79.9672 ms | 79.4233 ms | 1.01x | 仅大图边界略过线 |

QEMU correctness（QEMU 正确性）Std/RVV 9/9 仍通过；`dump_bench_rvv` 仍能在反汇编里看到
`vlse32.v`、`vmfeq.vv`、`vmerge.vvm`、`vfmul.vv`、`vfredusum.vs`，说明 RVV 路径仍在目标符号上。

当前证据不足以把 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` 的当前 RVV 生产补丁写成 adopted：
local nan-mask k64 只是在 `PointXYZRGBA 180x120` 上微弱过线，而两个 `PointXYZRGB` case 仍低于 1.0x，
且 helper-only 结果仍呈现 case 间不稳定。这个阶段把“strict finite mask / chunk size 是主因”缩小了一点，
但没有给出足够稳定、可采纳的 production evidence（生产证据）。

## 计划回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| local helper ablation | done | `src/bench_bilateral_upsampling.cpp` | 新增 `production detail local nan-mask k64` 三个 case。 |
| QEMU correctness | done | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | Std/RVV 9/9 通过。 |
| QEMU smoke | done | `make -C test-rvv/surface/bilateral_upsampling run_bench_rvv BENCH_ARGS='1 0'` | 新 case 的输出形状和 checksum 可见；QEMU timing 不作为性能结论。 |
| asm refresh | done | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | RVV 指令仍归属到目标 helper / local helper。 |
| board refresh | done | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | local nan-mask k64 结果为 `0.98x / 0.92x / 1.01x`。 |
| doctor refresh | done-with-errors | `doc/phases/050-production-detail-mask-chunk-ablation/evidence_manifest.json` -> `evidence-doctor.md` | `Errors=2`，退化频率仍阻塞接入结论。 |

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | production-detail-ablation |
| A/B boundary | production detail helper；helper-only direct call。 |
| 当前决策问题 | implementation-shape：strict finite mask / chunk 形态是否足以解释 helper-only 负向。 |
| diagnostic 是否可外推到 production | 不能直接外推到采纳；它只能告诉我们当前实现族是否值得继续。 |
| comparison-boundary / baseline mismatch 风险 | yes，local nan-mask k64 改了 mask 语义，只能算 component ablation。 |
| weak / negative / neutral / unstable 时是否允许 bounded production probe | 已完成有界 board smoke；结果仍不支持采纳。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 是；当前还没到该门。 |

## 板卡复跑预算和决策桶

本阶段只允许一次 board_smoke。`PointXYZRGBA` 大图 case 仅 `1.01x`，仍在近阈值区域；`PointXYZRGB`
两项仍低于 1.0x，决策桶仍应视作负向 / 不稳定，不再自动复跑同一实现族。

## 继续 / 停止决定

`continue_stop_decision`: stop_for_user_judgment。

`stop_condition_hit`: 当前 production detail 实现族经过 helper-only 和 mask/chunk 消融后仍未形成稳定正向；
继续扩大只会进入新的实现族分叉，而这一步需要你判断是要保留当前生产补丁继续作为实验，
还是回滚并切换到新的候选家族。

`next_phase_default`: 等待用户判断；若要继续，只建议另起新的 production candidate family，
不要把当前 helper 直接写成 adopted。
