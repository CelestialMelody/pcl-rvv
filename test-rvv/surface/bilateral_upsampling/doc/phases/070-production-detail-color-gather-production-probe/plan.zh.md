# Phase 070 plan: production detail color-gather production probe

## 阶段意图和边界

本阶段把 phase 060 的 positive precursor 推进到 production integration probe：验证 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` 里的真实公开入口是否能在 color-gather family 下形成稳定正向，并确认这不是 bench-local 假象。

当前生产头已经接入 color-gather RVV helper，旧 helper 仍保留作回退。这个 phase 只负责把新 family 的 production-public / steady-state / asm / board / doctor 证据写实，不再继续沿 helper-only 或 nan-mask 线扩分支。

## 当前状态清单

- phase 060 bench-local color-gather helper 已经给出正向 precursor。
- 当前生产头已经有 color-gather RVV helper + 原 helper fallback。
- 目前板卡 truth 来看，public / steady-state / color-gather direct helper 都是正向，且 current manifest 已通过 Evidence Doctor。
- PI5 对称用户判断点已经闭合：用户确认保留当前 production 补丁，这条生产补丁进入 adopted 语义。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production detail color-gather production probe | organized grid | `pcl::PointXYZRGB` / `pcl::PointXYZRGBA`, production AoS layout | `BilateralUpsampling::process` 真实公开入口 + steady-state + production helper | `run_test_compare` 已通过 | `board_smoke` 以当前 truth 复核 public / steady-state | current board truth 见 analyze_bench_compare.log | `vlse8.v` / `vzext.vf2` / `vmaxu.vv` / `vminu.vv` / `vluxei16.v` / `vfredusum.vs` | `Errors=0`、`Warnings=0`、`Suggestions=1` | adopted | production closeout |

## 实现和测试动作

1. 复跑 `run_test_compare`，确保生产头接入后 correctness 仍通过。
2. 复跑 `dump_bench_rvv`，确认 color-gather RVV 指令仍可归属到 production helper。
3. 复跑 `board_smoke`，刷新当前 board truth。
4. 生成当前 production-public manifest，并跑 Evidence Doctor。
5. 整理 phase 070 result、roadmap、matrix、evaluation 和 handoff。

## 继续 / 停止条件

- 当前生产补丁已确认保留，下一步进入 adopted / production closeout 文档收口。
- 若未来需要回滚，应另开 rollback/no-production closeout，不在当前 adopted 分支自动执行。
- 若未来要扩大点型、layout 或 `Scalar`，需要新 phase。

## 当前默认恢复动作

production closeout / 文档新鲜度检查。
