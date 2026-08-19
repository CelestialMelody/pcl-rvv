# 测试总览

本文件说明 organized_fast_mesh topic 的测试入口、证据边界和 target 粒度。

## 入口

| target | 作用 | 证据层级 |
| --- | --- | --- |
| `run_test` | 跑公开入口形态的 correctness test（正确性测试） | correctness |
| `run_test_compare` | 跑 std / RVV test compare | correctness / fallback |
| `run_bench_compare` | 跑 std / RVV bench compare | diagnostic bench |
| `generate_vec_report` | 生成 missed-vectorization 报告 | 辅助诊断 |
| `run_board_test` | 板卡单次 smoke | board smoke |
| `run_board_bench_compare` | 板卡单次 benchmark compare | board diagnostic |

## target 粒度

| target 类别 | 当前状态 | 说明 |
| --- | --- | --- |
| correctness aggregate | adopted | `run_test` 覆盖 scalar reference / candidate 对拍。 |
| correctness aliases | adopted | `run_test_std`、`run_test_rvv`。 |
| bench diagnostic aliases | adopted | `run_bench_std`、`run_bench_rvv`。 |
| QEMU smoke aliases | adopted | `run_test_compare`、`run_bench_compare` 仅作可运行性和日志形状。 |
| board smoke aliases | adopted | `run_board_test`。 |
| board repeated aliases | phase_deferred + unblocked | 当前只有 `board_smoke` 单次诊断；正式 production gate 前需要 5-run repeated summary。 |
| doctor / registry aliases | adopted | 由 `sanitize_output_logs`、`check_output_logs_sanitized` 和 registry 记录支撑。 |
| historical probe guarded aliases | adopted | 已记录 production public probe 和 rollback/no-production 结论；后续不默认重跑历史探针。 |
