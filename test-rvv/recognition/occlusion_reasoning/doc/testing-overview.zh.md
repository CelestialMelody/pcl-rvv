# 测试总览

本 topic 的测试已经分成三层：production direct（真实生产路径）、证据体检和归档。
Phase 020 覆盖 public inline `filter()` / `getOccludedCloud()`；Phase 010 覆盖
`ZBuffering::filter(model, indices, thres)`。

| target | 主测试类型 | 主要日志 / 输出 | 证据边界 |
| --- | --- | --- | --- |
| `run_test_compare` | correctness aggregate（正确性汇总入口） | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Std/RVV 都通过；RVV build 需命中 production hook |
| `run_qemu_smoke` | QEMU smoke（小型验证） | 同 `run_test_compare` | 只证明构建、正确性和日志形状，不证明性能 |
| `check_inline_filter_rvv_asm` | asm attribution（反汇编归属） | `build/asm/riscv/bench_occlusion_reasoning_public_inline_rvv.*.asm` | 检查 public inline bench hot path 里有 RVV 指令 |
| `check_occlusion_filter_rvv_asm` | asm attribution（反汇编归属） | `build/asm/riscv/bench_occlusion_reasoning_rvv.*.asm` | 检查 production bench hot path 里有 RVV 指令 |
| `inline_board_repeated` | board repeated（板卡重复测试） | `log/board/repeated_phase020_inline_filter_production_direct/**` | public inline 目标硬件性能证据，需配合 summary / doctor |
| `record_inline_evidence_state_repeated` | registry refresh（证据登记刷新） | `log/evidence_registry.json` | 把 public inline summary / manifest / doctor 记入登记表 |
| `board_repeated` | board repeated（板卡重复测试） | `log/board/repeated_phase010_production_direct/**` | 目标硬件性能证据，需配合 summary / doctor |
| `evidence_doctor_repeated` | Evidence Doctor（证据体检） | summary、manifest、doctor | 检查 benchmark 元数据、边界和异常 |
| `record_evidence_state_repeated` | registry refresh（证据登记刷新） | `log/evidence_registry.json` | 把 summary / manifest / doctor 记入登记表 |
| `check_evidence_freshness` | freshness check（新鲜度检查） | registry + 证据输出 | 检查是否存在未登记覆盖或 stale 文档 |

QEMU 不运行完整 `run_bench_compare`；若需要日志形状检查，只运行小规模 smoke。
