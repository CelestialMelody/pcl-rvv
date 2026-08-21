# Phase 010: rgb-segment-store-probe Result

## 当前结论

Phase 010 将测试专用 RGB full-size RVV candidate 的输出写回从 6 次 `vsse8` 改为 2 次 `vssseg3e8`。该候选在 correctness（正确性）、QEMU smoke（小型验证）、asm attribution（反汇编归属）和 focused board smoke（聚焦板卡验证）上均成立；板卡 focused RGB full-size 结果为 `Std 2.3034 ms`、`RVV 1.5785 ms`，speedup（加速比）约 `1.46x`。

EvidenceDecision：`partial-production-candidate`。它足以建议下一步进入 PI1 production integration plan（生产接入计划），但仍不是 production-ready（可直接生产接入）或 adopted production behavior（已采用生产行为），因为证据仍是 test helper 边界，且 Evidence Doctor 提醒 run count 低。

## 执行动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| 编译探针 | done | stdin probe 编译 `vuint8mf2x3_t` + `__riscv_vssseg3e8_v_u8mf2x3` | 当前 GCC / RVV intrinsic 支持 u8 segment-store。 |
| 候选实现 | done | `include/image_yuv422.h` 的 `storeRgbTriplet` 和 `fillRgbFullSizeRVV` | 只改变 RGB full-size RVV 写回；灰度和 downsample 保持标量 fallback。 |
| correctness | done | `make -C test-rvv/io/image_yuv422 run_test_compare` | Std / RVV 各 3 个 gtest 通过。 |
| QEMU smoke | done | `make -C test-rvv/io/image_yuv422 run_bench_rvv BENCH_ARGS="--case-filter rgb_full_640x480 --iterations 1 --warmup-iterations 1"` | 可运行，checksum 非空；QEMU timing 不进入性能结论。 |
| asm | done | `make -C test-rvv/io/image_yuv422 dump_bench_rvv`，`build/asm/riscv/bench_image_yuv422_rvv.asm` | 可见 `vssseg3e8.v`、`vlse8.v`、`vmul.vx`、`vsra.vi`、`vmax/vmin`。 |
| board focused RGB | done | `make -C test-rvv/io/image_yuv422 run_board_yuv422_rgb_full fetch_board_logs` | checksum 一致；RGB full-size 约 `1.46x`，进入 positive diagnostic bucket。 |
| Evidence Doctor | done | `make -C test-rvv/io/image_yuv422 run_evidence_doctor`；`log/board/evidence_doctor.md` | `Errors=0, Warnings=1, Suggestions=0`；warning 是 `low_run_count`。 |

## Optimization Matrix 更新

| candidate family | decision | 理由 |
| --- | --- | --- |
| rgb-segment-store-u8 | partial-production-candidate | 同边界 correctness / asm / focused board smoke 成立；`vssseg3e8` 写回显著改善 RGB full-size。仍需 production direct 和 repeated board。 |
| contiguous-rgb-strided-u8 | superseded | Phase 000 弱正向，被 segment-store 写回替代为更强候选。 |
| grayscale-rvv | rejected | active candidate 不再包含灰度 RVV；保持标量 fallback。 |
| downsample-rvv | deferred | 当前 focused run 只覆盖 RGB full-size；downsample 需要另开 phase，不随本结论外推。 |

## Evidence Doctor 解释

当前 doctor report 的唯一 warning 是 `low_run_count`：manifest 只有一个 focused board comparison。处理方式是把结论限制为 production-shaped diagnostic 的 `partial-production-candidate`，不写成 production performance（生产性能）结论；PI4 若进入生产接入闭环，必须补 production direct repeated board summary（真实生产路径重复板卡摘要）和新的 Evidence Doctor。

## Diagnostic 到 Production Mismatch Audit

| question | result |
| --- | --- |
| evidence role | production-shaped diagnostic。 |
| A/B boundary | test helper Std/RVV。 |
| 当前决策问题 | RVV-vs-scalar 候选筛选；不做 RVV-family-selection。 |
| 能证明什么 | `fillRGB` full-size 的测试专用同构 helper 中，segment-store RVV 写回在 Milkv-Jupiter 上有 positive 信号。 |
| 不能证明什么 | 不能证明真实 `ImageYUV422::fillRGB` public entry 已命中 RVV dispatch；不能覆盖 `__RVV10__` 关闭、fallback gate、downsample、OpenNI legacy parity。 |
| bounded production probe 条件 | 只建议进入 PI1 计划；PI1 必须冻结 full-size even-width RGB、line_step、fallback 和不触碰 OpenNI legacy 的边界。 |
| clean adoption 条件 | 需要 PI2-PI5：production patch、production direct correctness / fallback、production asm attribution、repeated board 和 Evidence Doctor；PI5 后还需用户确认采纳。 |

## Continue / Stop Decision

本轮停止在 production 授权边界。继续的默认动作是 PI1 production integration plan；该动作会准备修改 `io/src/image_yuv422.cpp` 的生产源码，因此需要用户确认进入 production integration loop（生产接入闭环）。在确认前，不修改 production，不创建 `doc-rvv/io/image_yuv422-RVV.zh.md`。
