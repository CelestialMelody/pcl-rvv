# GICP 测试概览

## 入口分类

| target | 主测试类型 | 附带类型或 case-filter | 主要日志 / summary | 证据边界 |
| --- | --- | --- | --- | --- |
| `run_test_compare` | correctness aggregate（正确性汇总入口） | `GICPResidualDiagnostic.*`、`GICPCovarianceDiagnostic.*` | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU 正确性，不证明性能 |
| `run_test_residual` | correctness alias（正确性细分入口） | residual components | 同上 | dense-row 和 indexed-gather residual 对拍 |
| `run_test_covariance` | correctness alias | covariance post-KNN component | 同上 | k-loop covariance 对拍 |
| `run_bench_all_smoke` | QEMU smoke alias（QEMU 小型验证入口） | `--case-filter all` | `log/qemu/run_bench_all_rvv.log` | 只证明 RVV build 可运行和日志形状 |
| `run_qemu_smoke_evidence_doctor` | doctor alias（证据体检入口） | summary-md 轻量检查 | `log/qemu/evidence_doctor.md` | metadata 不完整时只作为 reviewer aid |
| `run_board_test_smoke` | board smoke alias（板卡小型验证入口） | RVV test binary | `log/board/test_smoke/` | 证明板卡正确性和可运行 |
| `run_board_bench_repeated` | board repeated alias（板卡重复采集入口） | residual + covariance components | `log/board/component_diagnostic_repeated/summary.md` | pre-production diagnostic 性能证据 |
| `run_board_bench_gicp_production_public_repeated` | production-public board repeated | `production-public-align-pointxyz` | `log/board/production_public_align_pointxyz_dfddf_clean_repeated/summary.md` | clean `dfddfLoopRVV()` public GICP PointXYZ 生产边界性能证据 |
| `evidence_status` | registry alias（证据登记入口） | 文档引用和日志登记检查 | `log/evidence_registry.json` | freshness（新鲜度）和 artifact tracking |

## Target 粒度审计

| target 类别 | 当前状态 | 决策 | 证据 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | adopted | Std/RVV 两侧都编译运行 |
| correctness aliases | residual / covariance | adopted | gtest filter 已拆分 |
| bench diagnostic aliases | `run_bench_all_smoke` 和细分 smoke | adopted | case-filter 可隔离组件 |
| QEMU smoke aliases | RVV only smoke | adopted | 默认不运行完整 QEMU compare |
| board smoke aliases | `run_board_test_smoke` | adopted | 只作可运行和正确性 |
| board repeated aliases | `run_board_bench_repeated` | adopted | 5-run 默认预算 |
| doctor / registry aliases | doctor + registry | adopted | Evidence Doctor 和 registry target 已接入 |
| historical probe guarded aliases | cost-only production probe 已执行，clean `dfddf()` production probe 已执行 | adopted | production public cost-only neutral；clean `dfddfLoopRVV` weak_positive |

## Row-source 边界说明

`run_test_residual` 和 `run_board_bench_repeated` 中的 residual case 现在包含 dense-row diagnostic 和 indexed-gather diagnostic。dense-row 用于隔离 residual / Mahalanobis math pipeline；indexed-gather 额外模拟 production 中 `source_indices[]` / `target_indices[]` 双索引 gather，但仍不覆盖 optimizer 调用频率、PointT 字段 traits、`computeRDerivative()` 或 public GICP entry。
