# Phase 010: rgb-segment-store-probe Plan

## 阶段意图和边界

本阶段只在 `test-rvv/io/image_yuv422/include/image_yuv422.h` 中调整测试专用 RGB full-size RVV candidate（候选链路）的写回形态：把每个 YUYV pair 的 RGB 输出从 6 次 `vsse8`（跨步单通道写）改为 2 次 `vssseg3e8`（三通道段写）。目标是验证 interleaved RGB（RGBRGB 交错输出）是否能减少写回指令数量并改善 Phase 000 的 weak-positive（弱正向）板卡结果。

本阶段不修改 `io/src/image_yuv422.cpp`，不接入 production dispatch（生产分流），不覆盖 downsample（下采样）路径，也不重新启用灰度 RVV。

## 当前证据

| 证据 | 当前状态 |
| --- | --- |
| correctness（正确性） | Phase 000 的 `run_test_compare` 已证明 RGB full-size、RGB downsample、grayscale full/downsample 与标量 reference（参考链路）一致。 |
| asm（反汇编） | Phase 000 RVV asm 可见 `vlse8.v`、`vmul.vx`、`vsra.vi`、`vmax/vmin`、`vsse8.v`，写回使用 6 条跨步 store。 |
| board（板卡） | `run_board_yuv422_smoke`：RGB full-size 约 `1.06x`，padded RGB 约 `1.07x`；downsample 和灰度不作为 active RVV candidate。 |
| Evidence Doctor（证据体检） | 当前 all-case smoke 为 `Errors=2, Warnings=7, Suggestions=0`；Error 均来自 downsample fallback 对比退化，不能支撑全组外推。 |

## 候选假设

| candidate family | 假设 | 风险 | 完成判据 |
| --- | --- | --- | --- |
| rgb-segment-store-u8 | 用 2 次 `vssseg3e8` 写 `R1/G1/B1` 和 `R2/G2/B2`，减少 6 次 `vsse8` 写回的指令压力。 | tuple 构造可能增加额外指令；段写对当前板卡的 byte stride 不一定更快。 | correctness 通过、asm 可见 `vssseg3e8`，板卡 full RGB bucket 至少不劣于 Phase 000。 |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 编译探针 | stdin probe 编译 `vuint8mf2x3_t` + `__riscv_vssseg3e8_v_u8mf2x3`。 | 当前工具链接受 intrinsic，允许进入实现。 |
| 候选实现 | 修改 `fillRgbFullSizeRVV` 的 store block。 | 只改变 RGB full-size RVV 写回形态，公式、clip、fallback 不变。 |
| correctness | `make -C test-rvv/io/image_yuv422 run_test_compare`。 | Std / RVV 三个 gtest 均通过。 |
| QEMU smoke | `make -C test-rvv/io/image_yuv422 run_bench_rvv BENCH_ARGS="--case-filter rgb_full_640x480 --iterations 1 --warmup-iterations 1"`。 | 可运行，checksum 非空；QEMU timing 不作为性能结论。 |
| asm | `make -C test-rvv/io/image_yuv422 dump_bench_rvv`。 | 当前 RVV bench asm 出现 `vssseg3e8.v`，并保留公式计算相关 RVV 指令。 |
| board | `make -C test-rvv/io/image_yuv422 run_board_yuv422_rgb_full fetch_board_logs`，预算 1 次。 | checksum 一致；若 bucket 不提升或退化，candidate 标为 attempted/rejected，不进入 production。 |
| doctor | `make -C test-rvv/io/image_yuv422 run_evidence_doctor`。 | 生成 manifest 和 report；解释 Errors / Warnings 并决定是否降级。 |

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断）。 |
| A/B boundary | test helper（测试专用 helper）Std/RVV，不是真实 public overload（公开入口重载）。 |
| 当前决策问题 | RVV-vs-scalar 的候选筛选，不做 production adoption（生产采纳）。 |
| diagnostic 是否可外推到 production | unknown；只证明测试专用 helper 的同边界趋势，production 需要 PI1-PI5。 |
| comparison-boundary / baseline mismatch 风险 | RGB full-size helper 边界较低；downsample / grayscale 不可外推。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 segment-store 仍只是 weak / neutral / negative，默认不建议 production probe。 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | 若后续真的接 production，需要同一 production boundary 内比较当前写回形态和替代实现族。 |

## Board Budget 与停止条件

本阶段使用 1 次 focused RGB full-size board run（板卡运行）作为 Phase 010 决策输入。允许停止条件：segment-store 编译失败、correctness 失败、asm 未出现目标 store、板卡 bucket 不提升或退化、Evidence Doctor Error 仍阻止把结果升级为 production 证据。若结果仍只有 weak-positive，本 topic 停在 diagnostic / no-production closeout，并把 downsample、OpenNI legacy 和 production integration 写入后续选项。
