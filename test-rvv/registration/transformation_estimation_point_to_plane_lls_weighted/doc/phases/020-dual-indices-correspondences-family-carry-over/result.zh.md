# Phase 020: dual-indices / correspondences family carry-over 结果

## 计划与实际范围

本 phase 按 `plan.zh.md` 完成 dual-indices（双索引路径）和 correspondences（对应关系路径）的同 family candidate / test / bench / board 审计。实际结果是：这两条 row source policy 仍不进入 production，新的 family carry-over 只能停在 pre-production diagnostic（接入生产前诊断）边界。

## 已完成项

| 动作 | 状态 | 证据路径 / 说明 |
| --- | --- | --- |
| A1 恢复 phase 020 计划 | done | `doc/phases/020-dual-indices-correspondences-family-carry-over/plan.zh.md` 先于本阶段 test / bench / board 修改存在。 |
| A2 补 dual-indices / correspondences 同 family candidate | done | `src/test_teptplw_row_sources.cpp`、`include/impl/teptplw_bench_cases.hpp` 中已有 `block-baseline` 与 `block-fused-abcd-ilp` 的 dual/correspondence candidate。 |
| A3 补 QEMU correctness | done | `log/qemu/run_test_dual_correspondence_family_std.log`、`log/qemu/run_test_dual_correspondence_family_rvv.log`，共 4 tests passed。 |
| A4 补 bench / board 诊断 | done | `log/qemu/run_bench_dual_correspondence_family_std.log`、`log/qemu/run_bench_dual_correspondence_family_rvv.log`、`log/board/run_board_bench_dual_correspondence_family/analyze_bench_compare.log`。 |
| A5 补 Evidence Doctor / manifest 边界 | done | `log/board/run_board_bench_dual_correspondence_family/evidence_manifest.json`、`evidence_doctor.md`。doctor 结果 20 Errors / 21 Warnings / 0 Suggestions。 |
| A6 更新 phase result、topic docs 和 Handoff | done | 当前文件、topic docs、`tmp/rvv-work-logs/registration/transformation_estimation_point_to_plane_lls_weighted/current-handoff/current-handoff.zh.md`。 |

## 当前状态清单

| row source policy | 当前实现族 | 当前 production 状态 | 当前证据 | 当前缺口 |
| --- | --- | --- | --- | --- |
| full-cloud | block-reduction + A/B/C/N + fused-abcd-ilp | adopted | 现有 production direct、repeated board、asm 和 doctor 均闭合。 | 不外推。 |
| source-indexed | staged-gather / compressed-tail | adopted | 现有 production direct、repeated board 和 doctor 已闭合到当前边界。 | 仍保留 source-indexed-specific asm / binary identity warning。 |
| dual-indices | staged-gather / compressed-tail、block-baseline、block-fused-abcd-ilp | diagnostic attempted / no-production | `run_test_dual_correspondence_family`、`run_bench_dual_correspondence_family`、`run_board_bench_dual_correspondence_family/analyze_bench_compare.log`、`evidence_manifest.json`、`evidence_doctor.md`。 | 缺 production direct、fallback、asm 和 repeated board。 |
| correspondences | staged-gather / compressed-tail、block-baseline、block-fused-abcd-ilp | diagnostic attempted / no-production | 同上。 | 还要拆 query/match/weight 展开成本，且目前 board 结果仍负向。 |

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | test | bench | board | asm | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| staged-gather / compressed-tail | dual-indices | `PointNormal` / `Scalar=float` / f32 AoS gated | `run_test_dual_correspondence_family` | `run_bench_dual_correspondence_family` | `run_board_bench_dual_correspondence_family` | missing | `20 Errors / 21 Warnings / 0 Suggestions` | attempted / no-production |
| block-baseline | dual-indices | `PointNormal` / `Scalar=float` / f32 AoS gated | `run_test_dual_correspondence_family` | `run_bench_dual_correspondence_family` | `run_board_bench_dual_correspondence_family` | missing | `20 Errors / 21 Warnings / 0 Suggestions` | rejected for production |
| block-fused-abcd-ilp | dual-indices | `PointNormal` / `Scalar=float` / f32 AoS gated | `run_test_dual_correspondence_family` | `run_bench_dual_correspondence_family` | `run_board_bench_dual_correspondence_family` | missing | `20 Errors / 21 Warnings / 0 Suggestions` | rejected for production |
| staged-gather / compressed-tail | correspondences | `PointNormal` / `Scalar=float` / f32 AoS gated | `run_test_dual_correspondence_family` | `run_bench_dual_correspondence_family` | `run_board_bench_dual_correspondence_family` | missing | `20 Errors / 21 Warnings / 0 Suggestions` | attempted / no-production |
| block-baseline | correspondences | `PointNormal` / `Scalar=float` / f32 AoS gated | `run_test_dual_correspondence_family` | `run_bench_dual_correspondence_family` | `run_board_bench_dual_correspondence_family` | missing | `20 Errors / 21 Warnings / 0 Suggestions` | rejected for production |
| block-fused-abcd-ilp | correspondences | `PointNormal` / `Scalar=float` / f32 AoS gated | `run_test_dual_correspondence_family` | `run_bench_dual_correspondence_family` | `run_board_bench_dual_correspondence_family` | missing | `20 Errors / 21 Warnings / 0 Suggestions` | rejected for production |

## Correctness 与 bench 结果

QEMU 正确性：

| target | 结果 | 证据 |
| --- | --- | --- |
| `run_test_dual_correspondence_family_std` | 4 tests passed | `log/qemu/run_test_dual_correspondence_family_std.log` |
| `run_test_dual_correspondence_family_rvv` | 4 tests passed | `log/qemu/run_test_dual_correspondence_family_rvv.log` |

bench / board 诊断：

| 证据 | 结果 | 边界 |
| --- | --- | --- |
| `run_bench_dual_correspondence_family_std.log` / `run_bench_dual_correspondence_family_rvv.log` | dual-indices 和 correspondences 的 staged-gather、block-baseline、block-fused-abcd-ilp、component no-solve 都可跑通；RVV 在所有列出的 compare case 上都慢于 std。 | 只说明 QEMU bench 形状，不是性能结论。 |
| `log/board/run_board_bench_dual_correspondence_family/analyze_bench_compare.log` | dual-indices 65536 / 262144 的 observed speedup 约为 `0.33x~0.39x`；correspondences 65536 / 262144 的 observed speedup 约为 `0.60x~0.86x`。 | 这是单次板卡诊断信号，不是 repeated board performance conclusion。 |
| `log/board/run_board_bench_dual_correspondence_family/evidence_manifest.json` | `20 comparisons`，`Errors=20`，`Warnings=21`，`Suggestions=0`。 | 所有错误都来自 `ba_degradation_frequency`，未给 production 留出升级空间。 |

best observed case 仍然是 `weighted lls dual-correspondence-family correspondences block-fused-abcd-ilp pointnormal 262144`，speedup 只有 `0.859x`，依然低于 1.0x。

## Evidence Doctor 处理

| 证据 | doctor 结果 | 处理动作 |
| --- | --- | --- |
| dual-indices / correspondences family single-run board diagnostic | `20 Errors / 21 Warnings / 0 Suggestions` | Errors 全部是 `ba_degradation_frequency`；Warnings 主要是 low run count、B/A 退化频率和 boundary 风险；结论保持 diagnostic attempted，不进入 production。 |

这一轮没有补 source-indexed-specific asm，也没有补 dual/correspondence production asm。对于当前 topic，这意味着 new family carry-over 的证据边界已经足够支持 no-production closeout，但不足以升级成 production integration loop。

## 继续 / 停止决策

Phase 020 已完成，且没有 unblocked next action。

当前结论：

- dual-indices / correspondences 仍保持标量 production 边界。
- full-cloud adopted family 不会自动外推到这两条 row source policy。
- 这次补的 family compare 只证明 diagnostic attempted + negative board signal，不证明 production ready。

下一阶段默认入口：无。若未来继续这条线，需要另开新的 phase 计划并重新写 plan.zh.md。
