# Testing Overview

## 入口分类

本 topic 的测试资产只服务 diagnostic（诊断）和 production-shaped diagnostic（生产形态诊断）。它不修改 production 源码，也不提供 production direct（真实生产路径证据）。

| 类别 | 入口 | 作用 | 边界 |
| --- | --- | --- | --- |
| correctness aggregate | `make run_test_compare` | 分别运行 Std/RVV 单测 | QEMU 正确性，不是性能证据 |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS='--size 1024 --iterations 1 --warmup 0 --case-filter <case>'` | 检查 bench case 可运行和日志形状 | 不做性能结论 |
| asm attribution | `make dump_bench_rvv` | 生成 `build/asm/riscv/bench_omps_rvv.asm` | 证明关键 RVV 指令可归属到 helper / callee |
| board repeated | `make run_board_omps_repeated` | 板卡 5-run summary / manifest / Evidence Doctor | 只有该类结果进入性能判断 |
| evidence registry | `python3 ../../script/evidence_registry.py record/check ...` | 登记 summary / manifest / doctor 新鲜度 | raw per-run logs 默认不提交 |

## 覆盖矩阵

| case family | tests | bench case-filter | board summary | 当前状态 |
| --- | --- | --- | --- | --- |
| `plane_d_dot_rvv` | `PlaneDValuesMatchHandCheckedDots` | `plane_d_dot` | `log/board/repeated/summary.md` | rejected，negative |
| `boundary_gather_rvv` | `BoundaryGatherPreservesIndexOrder` | `boundary_gather` | `log/board/repeated/summary.md` | attempted，neutral |
| `viewpoint_projection_rvv` | `ProjectionMatchesHandCheckedIntersections` | `projection` | `log/board/repeated/summary.md` | component positive，但只触发 Phase 010 |
| `region_boundary_projection_rvv` | `RegionBoundaryProjectionMatchesScalarShape` | `region_projected` | `log/board/phase010-region_projected/summary.md` | rejected，production-shaped negative |
| `region_boundary_gather_only_rvv` | 同上 | `region_gather_only` | `log/board/phase010-region_gather_only/summary.md` | rejected，production-shaped negative |

## Target 粒度审计

| target 类别 | 当前形态 | 决策 |
| --- | --- | --- |
| correctness aggregate | `run_test_compare` 覆盖 Std/RVV 4 个测试 | adopted |
| correctness aliases | 未拆 gtest filter target；当前测试量小，单入口可读 | not_applicable with evidence |
| bench diagnostic aliases | 通过 `--case-filter` 隔离 5 个 case | adopted |
| QEMU smoke aliases | 复用 `run_bench_rvv` + 小规模参数；文档明确不作性能证据 | adopted |
| board smoke aliases | 单次 smoke 被 repeated target 覆盖；不单独发布 | not_applicable with evidence |
| board repeated aliases | `run_board_omps_repeated` + `OMPS_REPEATED_DIR` / `OMPS_BENCH_ARGS` 支持分 case 采集 | adopted |
| doctor / registry aliases | `run_board_evidence_doctor`、`generate_board_evidence_manifest`、通用 registry 脚本 | adopted |
| historical probe guarded aliases | 无 production probe 或回滚探针 | not_applicable with evidence |
