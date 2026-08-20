# Phase 071 result: production finite-mask correctness refresh

## 当前结论

本阶段修复了 adopted color-gather helper 的 finite mask 语义：production RVV 现在与标量 `std::isfinite` 一样，同时跳过 NaN 和 `+/-infinity`。这补上了 phase 070 closeout 后发现的 correctness 边界缺口。

本阶段不改变 adopted family 的主体：仍是 color-gather RVV helper 优先，失败后回退旧 RVV helper，再回退标量 helper。改变的是 finite-depth mask 的语义完整性。

## 实现回填

| action | 状态 | 证据 |
| --- | --- | --- |
| production color-gather finite mask | done | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp`，`z == z && abs(z) < infinity` |
| bench-local color-gather finite mask | done | `src/bench_bilateral_upsampling.cpp` 同步语义 |
| direct-depth diagnostic finite mask | done | `include/impl/bilateral_upsampling_core.hpp` 同步语义 |
| diagnostic infinity correctness | done | `BilateralUpsamplingDiagnostic.DirectDepthCandidateSkipsInfiniteDepth` |
| production public infinity correctness | done | `BilateralUpsamplingProductionPublic.PointXYZRGBSkipsInfiniteDepth` |

## 验证结果

| action | 结果 | 说明 |
| --- | --- | --- |
| QEMU correctness | pass | `make -C test-rvv/surface/bilateral_upsampling run_test_compare`，Std/RVV 均为 11/11 passed。 |
| asm refresh | pass | `dump_bench_rvv` 后可见 `vlse8.v`、`vzext.vf2`、`vmaxu.vv`、`vminu.vv`、`vluxei16.v`、`vfredusum.vs`，并可见 finite mask 的 `vfabs.v` / `vmflt.vf`。 |
| QEMU bench smoke | pass | `make -C test-rvv/surface/bilateral_upsampling run_bench_rvv BENCH_ARGS='1 0'` 跑通，所有 case 输出 checksum 和误差统计；QEMU timing 不作为性能结论。 |
| board freshness | pending | 默认板卡地址和历史备用地址均 SSH 超时，无法刷新 board performance；具体地址不写入提交文档。 |

## Evidence boundary

Phase 070 的 board performance 是采用 color-gather family 的历史 production direct 证据；phase 071 改变了 finite mask 指令流后，board freshness 已在 phase 072 通过当前二进制 `board_smoke` 刷新。当前性能结论见 `doc/phases/072-production-same-type-gate-alignment/result.zh.md`。

如果后续再次改变 production helper、gate 或 bench harness，仍必须重跑 board summary 和 Evidence Doctor，并同步 `doc-rvv/surface/bilateral_upsampling-RVV.zh.md`、`doc/bilateral_upsampling-evaluation.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md` 和 Handoff。

## 继续 / 停止决定

`continue_stop_decision`: superseded_by_phase072_board_refresh。

`stop_condition_hit`: none_after_phase072_refresh。

`next_phase_default`: 当前不从 phase 071 恢复；如需扩交叉点型、其它点型、layout 或 `Scalar`，另开 point-type expansion phase。
