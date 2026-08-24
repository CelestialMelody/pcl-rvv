# point_coding 测试总览

## 测试层级

| 层级 | 入口 | 作用 | 证据边界 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make run_test_compare` | 构建 Std / RVV 两个 test binary，并运行同一组 gtest。 | Phase 070 后 Std / RVV 各 11 个 gtest 通过，覆盖 component、production-shaped、production-direct、traits gate、fallback 和 public smoke。 |
| QEMU smoke（QEMU 小型验证） | `make run_qemu_bench_smoke` | RVV bench 只跑一个小 case，确认二进制和日志形状。 | 不使用 QEMU timing 写性能结论。 |
| asm dump（反汇编导出） | `make dump_bench_rvv` | 生成 RVV bench 反汇编并检查关键指令。 | Phase 070 可归属到真实 production `decodePoints`，仍不代表完整 public end-to-end hot path。 |
| board smoke（板卡小型验证） | `make run_board_test`、`make run_board_bench_compare` | 在目标硬件运行 correctness 和单次 bench compare。 | 单次 bench 只辅助检查，不替代 repeated summary。 |
| board repeated（板卡重复采集） | `make collect_board_repeated` with `POINT_CODING_REPEATED_DIR` + `BENCH_ARGS` | 采集 repeated bench compare 并保存每轮日志。 | 支撑 diagnostic 或 production-direct 性能结论，取决于 case evidence role。 |
| doctor / manifest（证据体检与清单） | `make run_board_repeated_evidence_doctor` | 生成 manifest 并运行 Evidence Doctor。 | Errors / Warnings / Suggestions 必须进入 phase result 和 Handoff。 |

## Target 粒度审计

| target 类别 | 当前入口 | 状态 | 说明 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | adopted | Std / RVV 两侧同一 test suite。 |
| correctness aliases | 暂无细分 alias | not_applicable with evidence | 当前 11 个 gtest 仍可由 aggregate 定位。 |
| bench diagnostic aliases | `BENCH_ARGS=--case-filter <label>` | adopted | `encode_indexed_*`、`decode_contiguous_*`、`decode_context_*`、`decode_multileaf_*`、`decode_production_direct_*` 和 `decode_production_direct_traits_*` 可隔离候选族。 |
| QEMU smoke aliases | `POINT_CODING_QEMU_BENCH_SMOKE_ARGS` | adopted | Phase 070 用 `decode_production_direct_traits_xyzi_1024` 做日志形状 smoke。 |
| board repeated aliases | `collect_board_repeated` | adopted | Phase-specific 目录由 `POINT_CODING_REPEATED_DIR` 指定。 |
| doctor / registry aliases | `run_board_repeated_evidence_doctor` | adopted for doctor；registry not_available | 当前有 manifest 和 doctor；尚无 `log/evidence_registry.json`，恢复时人工检查被文档引用的 summary。 |

## 覆盖矩阵

| 维度 | 已覆盖 | 未覆盖 |
| --- | --- | --- |
| point type（点类型） | exact `PointXYZ`；traits-gated representative `PointXYZI`、`PointXYZRGB`。 | 所有自定义 xyz AoS、非标准布局、非 xyz AoS、其它目标硬件。 |
| row source（行来源） | encode source-indexed leaf；decode contiguous output；object-state decode；multi-leaf context；production direct `PointCoding<PointT>` decode stream；public roundtrip smoke。 | 完整 public entry 性能、真实 leaf distribution、entropy context。 |
| Scalar / precision | production 现有 float coordinate + double reference + float resolution。 | 其它输入域和误差合同。 |
| correctness | encode、clamp、decode same-chain、production-shaped object-state、production-direct rounding、traits extra-field preservation、public roundtrip smoke。 | 全点型枚举和完整端到端逐点误差。 |
| performance | component board repeated、production-shaped repeated、Phase 060 exact production-direct repeated、Phase 070 traits production-direct repeated。 | full octree encode/decode、entropy context、I/O pipeline。 |

## 生成产物和提交边界

`build/`、`log/qemu/` 和 `log/board/` 是生成产物，默认不提交。当前 production 结论引用
`log/board/repeated_phase060_production_decode/summary.md` 和
`log/board/repeated_phase070_traits_decode/summary.md`；如果需要提交证据，应另走 summary evidence 或 sanitized logs（脱敏日志）选择流程。
