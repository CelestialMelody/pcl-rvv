# Phase 072 plan: production same-type gate alignment

## 阶段意图和边界

本阶段是 production scope alignment（生产范围对齐）：closeout 审计发现 `kBilateralUpsamplingRVVCompatible` 允许 `PointXYZRGB -> PointXYZRGBA` 与 `PointXYZRGBA -> PointXYZRGB` 交叉点型组合，但当前 correctness tests、bench manifest 和 phase 070 板卡证据只覆盖 same-type `PointXYZRGB -> PointXYZRGB` 与 `PointXYZRGBA -> PointXYZRGBA`。

本阶段不扩大点型，也不新增交叉点型 production direct 证据；采用保守策略，把 production RVV gate 收窄到 same-type。交叉 RGB/RGBA 输出继续走标量 fallback，后续若需要支持，另起 point-type expansion phase。

## 实现动作

1. 在 `kBilateralUpsamplingRVVCompatible` 中加入 `std::is_same_v<PointInT, PointOutT>`。
2. 同步 `doc-rvv`、evaluation、testing overview、correctness、benchmark/evidence、optimization evidence、roadmap、phase index、optimization matrix 和 Handoff 的 coverage 描述。
3. 用当前二进制刷新 QEMU correctness、asm attribution 和 QEMU bench log-shape。
4. 板卡可用时刷新 `board_smoke`，并根据结果更新 phase result、长期文档和 Evidence Doctor 状态。

## 验证计划

| action | 命令 | 证据角色 |
| --- | --- | --- |
| QEMU correctness | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | same-type public-entry correctness、finite mask correctness 和 fallback smoke |
| asm refresh | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | 确认 color-gather 与 finite mask 指令仍可归属到 production helper |
| QEMU bench smoke | `make -C test-rvv/surface/bilateral_upsampling run_bench_rvv BENCH_ARGS='1 0'` | 只看 bench 输出合同和路径命中，不采信 timing |
| board freshness | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | 当前二进制的 production direct 性能 freshness |

## 停止条件

若 QEMU correctness 或 asm attribution 失败，先修实现。若只有板卡 SSH 或远端运行失败，则保留当前 same-type gate，记录 `board_refresh_pending` 和恢复命令，不把 QEMU timing 写成性能结论。
