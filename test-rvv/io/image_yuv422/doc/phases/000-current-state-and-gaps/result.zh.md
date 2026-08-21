# Phase 000: current-state-and-gaps Result

## 当前结论

Phase 000 建立了 `io/src/image_yuv422.cpp` 的 production-shaped diagnostic（生产形态诊断）测试资产：标量 reference（参考链路）、RVV candidate（候选链路）、QEMU correctness（QEMU 正确性验证）、bench（性能测试）输出、反汇编和板卡 smoke。首个 RGB full-size 候选使用 6 次 `vsse8` 写回，板卡表现为 weak-positive（弱正向）；灰度 RVV 诊断负收益，已从 active candidate 中收回；downsample（下采样）保持标量 fallback（回退路径）。

本阶段没有修改 production（生产源码）。

## 执行动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| 建立测试资产 | done | `include/image_yuv422.h`、`src/test_image_yuv422.cpp`、`src/bench_image_yuv422.cpp`、`Makefile`、`board.mk` | 覆盖 RGB full-size、RGB downsample、grayscale full/downsample 和 padding。 |
| RED / GREEN correctness | done | `make -C test-rvv/io/image_yuv422 run_test_compare` | Std / RVV 各 3 个 gtest 通过；候选与标量 reference 字节一致。 |
| QEMU bench smoke | done | `make -C test-rvv/io/image_yuv422 run_bench_rvv BENCH_ARGS="--case-filter all --iterations 1 --warmup-iterations 1"` | bench 输出字段可解析；QEMU timing 不作为性能结论。 |
| 反汇编 | done | `build/asm/riscv/bench_image_yuv422_rvv.asm` | RGB 候选可见 `vlse8.v`、`vmul.vx`、`vsra.vi`、`vmax/vmin`、`vsse8.v`。 |
| 板卡 all-case smoke | done / historical | `log/board/analyze_bench_compare.log` 曾由 `run_board_yuv422_smoke` 生成，随后被 Phase 010 focused run 覆盖 | 历史观测：RGB full-size 约 `1.06x`，padded RGB 约 `1.07x`；downsample / grayscale 不能外推。 |
| Evidence Doctor | done / historical | `log/board/evidence_doctor.md` 曾报告 all-case `Errors=2, Warnings=7`，随后被 Phase 010 focused run 覆盖 | Error 来自 downsample fallback 退化，说明不能把全组收益外推到 downsample。 |

## Optimization Matrix 更新

| candidate family | decision | 理由 |
| --- | --- | --- |
| contiguous-rgb-strided-u8 | attempted | correctness、QEMU smoke 和 asm 成立；板卡 full-size RGB 只有 weak-positive，不足以直接进入 production。 |
| grayscale-strided-y-copy | rejected | 灰度路径是 byte copy，RVV 诊断在板卡 smoke 中不稳定或负向；active candidate 已保持标量 fallback。 |
| downsample-rvv | deferred | 当前没有 RVV downsample 候选；all-case smoke 暴露 downsample 对比不可外推，需要另开窄 phase 或保持标量。 |

## Diagnostic 到 Production Mismatch Audit

| question | result |
| --- | --- |
| evidence role | production-shaped diagnostic，不是 production direct。 |
| A/B boundary | test helper Std/RVV。 |
| 当前决策问题 | RVV-vs-scalar 候选筛选。 |
| 能证明什么 | 测试专用 helper 的 RGB full-size 公式和输出布局可以用 RVV 对拍，并在板卡上出现弱正向信号。 |
| 不能证明什么 | 不能证明真实 `ImageYUV422::fillRGB` 已接入 production dispatch，不能覆盖 downsample、OpenNI legacy 或 fallback gate。 |
| 弱 / 负 / 中性处理 | 6×`vsse8` 写回形态不建议直接 production；进入 Phase 010 尝试 segment-store 写回。 |

## Continue / Stop Decision

Phase 000 不作为 topic 终点。由于 RGB full-size 仍有改写写回形态的未阻塞候选，本轮继续进入 `010-rgb-segment-store-probe`。
