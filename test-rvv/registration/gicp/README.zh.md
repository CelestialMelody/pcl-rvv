# GICP RVV 诊断入口

本 topic 针对 `registration/include/pcl/registration/impl/gicp.hpp`。当前阶段包含接入生产前诊断（pre-production diagnostic，用测试专用代码判断候选价值）和两个 production probe（生产探针）。最终决策是 no-production（不接入生产）：生产源码已回到标量实现，测试资产保留诊断和板卡证据。

## 当前结论

`gicp.hpp` 当前不接入 RVV 生产补丁。Phase 001 的 cost-only production probe 结果为 neutral：1024 点 5-run repeated median `1.019x`，4096 点 3-run 扩展 median `1.011x`。Phase 003 只保留 `dfddfLoopRVV()` 后，public GICP `PointXYZ -> PointXYZ` 虽为弱正向：1024 点 median `1.080x`，4096 点 median `1.058x`，但收益不足以抵消生产源码维护成本和窄覆盖范围。Phase 004 尝试把 `dfddfLoopRVV()` 的 matrix gather 从 `vluxei64` 改成 `vluxei32`，1024 点 median `1.073x`，没有优于 Phase 003。最终已回退全部 GICP 生产源码改动，只保留 topic-local 诊断、bench 和 no-production 证据。

## 阅读路径

| 目的 | 路径 |
| --- | --- |
| 函数级评估和生产接入判断 | `doc/gicp-evaluation.zh.md` |
| 测试入口分类 | `doc/testing-overview.zh.md` |
| 正确性测试说明 | `doc/correctness-tests.zh.md` |
| bench 与证据边界 | `doc/benchmark-and-evidence.zh.md` |
| 候选和取舍 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码地图 | `doc/test-support-code-map.zh.md` |
| 阶段计划和结果 | `doc/phases/000-current-state-and-gaps/` |
| 优化路线图 | `doc/optimization-roadmap.zh.md` |

## 常用命令

```bash
make run_test_compare
make run_bench_all_smoke
make run_qemu_smoke_evidence_doctor
make run_board_test_smoke
make run_board_bench_repeated
make run_board_bench_gicp_production_public_repeated
make evidence_status
```

QEMU（仿真器）只用于 correctness（正确性）、路径和日志形状。性能结论只来自板卡或目标硬件。

## 证据白名单

当前可提交证据默认是 summary-only（只提交摘要）：`log/qemu/evidence_doctor.md`、`log/board/*/summary.md` 和 `log/board/*/evidence_doctor.md`。`log/qemu/*.log`、`log/board/*/evidence_manifest.json`、`log/evidence_registry.json`、raw board run 目录和 build 产物默认留在本机；提交前仍用本机 registry 做 freshness（新鲜度）检查。

已登记证据包括：`test-rvv/registration/gicp/log/qemu/run_test_std.log`、`test-rvv/registration/gicp/log/qemu/run_test_rvv.log`、`test-rvv/registration/gicp/log/qemu/run_bench_all_rvv.log`、`test-rvv/registration/gicp/build/asm/riscv/bench_gicp_rvv.asm`、`test-rvv/registration/gicp/log/qemu/evidence_doctor.md`、`test-rvv/registration/gicp/log/board/component_diagnostic_repeated/summary.md`、`test-rvv/registration/gicp/log/board/dfddf_loop_dense_repeated/summary.md`、`test-rvv/registration/gicp/log/board/production_public_align_pointxyz_repeated/summary.md`、`test-rvv/registration/gicp/log/board/production_public_align_pointxyz_4096_repeated/summary.md`、`test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_repeated/summary.md`、`test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_4096_repeated/summary.md`、`test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_repeated/summary.md`、`test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_4096_repeated/summary.md`、Phase 004 的 `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_gather32_repeated/summary.md` 和 `test-rvv/registration/gicp/log/evidence_registry.json`。

没有适用的 `doc-rvv/registration/gicp-RVV.zh.md` 长期生产文档；本 topic 的结论保留在 topic-local 文档和 phase result 中。
