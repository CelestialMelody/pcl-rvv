# moment_invariants RVV topic

本目录评估并验证 `features/include/pcl/features/impl/moment_invariants.hpp` 的 RVV 优化。当前结论是 adopted production behavior（已采纳生产行为）：真实 `MomentInvariantsEstimation::computeFeature` 在 RVV 构建、dense xyz 单 float AoS（结构数组）输入、`PointOutT=pcl::MomentInvariants`、indexed neighbor list（索引邻域列表）和邻域规模达到 16 时，centroid（质心）之后的六个中心矩累加使用 RVV gather（离散加载）和 vector reduction（向量规约）。

正式长期文档位于 `doc-rvv/features/moment_invariants-RVV.zh.md`。本目录保留 topic-local evaluation（函数级评估）、phase plan/result、测试说明、bench 证据和 Evidence Doctor（证据体检）输入，方便 reviewer（审查者）复核生产接入过程。

## 常用命令

| 命令 | 作用 | 证据边界 |
| --- | --- | --- |
| `make run_test_compare` | Std/RVV 两种构建运行同一套 correctness（正确性）测试。 | QEMU（仿真器）只证明正确性、fallback 和路径，不证明性能。 |
| `make run_board_test` | 在板卡运行 RVV gtest。 | 证明目标硬件可运行和输出容差，不是 repeated performance。 |
| `make check_production_rvv_asm` | 检查 `PointXYZ` production 符号范围内的 RVV load/reduction。 | 反汇编归属，不是性能结论。 |
| `make check_production_pointxyzi_rvv_asm` | 检查 `PointXYZI` production 符号范围内的 RVV load/reduction。 | 证明 typed gate 命中。 |
| `make check_production_pointxyzrgb_rvv_asm` | 检查 `PointXYZRGB` production 符号范围内的 RVV load/reduction。 | 证明 typed gate 命中。 |
| `make check_production_pointxyzrgba_rvv_asm` | 检查 `PointXYZRGBA` production 符号范围内的 RVV load/reduction。 | 证明 typed gate 命中。 |
| `make run_board_mi_production_repeated` | 采集 `PointXYZ` production-public 5-run board summary。 | 目标硬件性能证据。 |
| `make run_board_mi_production_pointxyzi_repeated` | 采集 `PointXYZI` production-public 5-run board summary。 | 目标硬件性能证据。 |
| `make run_board_mi_production_pointxyzrgb_repeated` | 采集 `PointXYZRGB` production-public 5-run board summary。 | 目标硬件性能证据。 |
| `make run_board_mi_production_pointxyzrgba_repeated` | 采集 `PointXYZRGBA` production-public 5-run board summary。 | 目标硬件性能证据。 |
| `make record_board_mi_repeated_state` / `make record_board_mi_phase010_repeated_state` | 刷新 Phase 000/010 历史诊断 summary、doctor 和 registry。 | 不重新采集板卡；只维护历史诊断证据。 |

## 文档入口

| 文档 | 作用 |
| --- | --- |
| `doc/moment_invariants-evaluation.zh.md` | 函数级评估、EvidenceDecision（证据决策）、Traceability Map（可追踪性地图）和生产接入判断主归属。 |
| `doc/testing-overview.zh.md` | 测试 target 分类、覆盖矩阵和 QEMU / board 证据边界。 |
| `doc/correctness-tests.zh.md` | gtest 输入、断言和 correctness 证明范围。 |
| `doc/benchmark-and-evidence.zh.md` | bench case、板卡 summary、Evidence Doctor、registry 和提交边界。 |
| `doc/optimization-evidence.zh.md` | candidate family（候选实现族）与证据、取舍和恢复条件。 |
| `doc/test-support-code-map.zh.md` | test support（测试支撑代码）、bench wrapper、script 和 output 的定位地图。 |
| `doc/optimization-roadmap.zh.md` | 跨阶段候选搜索空间和默认恢复队列。 |
| `doc/phases/README.zh.md` | phase index（阶段索引）和当前默认恢复入口。 |
| `doc/phases/optimization-matrix.zh.md` | candidate × row source × point type × evidence 状态矩阵。 |

## 当前可提交证据

| 证据 | 路径 | 状态 |
| --- | --- | --- |
| Phase 000 diagnostic summary / manifest / doctor | `log/board/repeated_phase000_moment_accumulation_diagnostic/` | 历史诊断证据，summary-only。 |
| Phase 010 production-shaped diagnostic summary / manifest / doctor | `log/board/repeated_phase010_public_search_shape_diagnostic/` | 历史生产形态诊断，summary-only。 |
| Phase 030 `PointXYZ` production summary / manifest / doctor | `log/board/repeated_phase030_production_compute_feature/` | 当前 production-public 证据，summary-only。 |
| Phase 040 typed production summary / manifest / doctor | `log/board/repeated_phase040_production_pointxyzi_compute_feature/`、`log/board/repeated_phase040_production_pointxyzrgb_compute_feature/`、`log/board/repeated_phase040_production_pointxyzrgba_compute_feature/` | 当前 production-public 证据，summary-only。 |
| Evidence registry | `log/evidence_registry.json` | 记录 summary、manifest 和 doctor 的 freshness。 |

raw `run-*` 日志、`build/` 输出、远端路径和本机配置不默认提交。
