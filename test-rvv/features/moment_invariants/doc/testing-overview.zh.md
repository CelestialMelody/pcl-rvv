# moment_invariants testing overview

## 本文职责

本文说明测试入口、target（Make 目标）分类和证据边界。每个 gtest（Google Test 单元测试）的断言细节见 `doc/correctness-tests.zh.md`；bench（性能测试）、summary（摘要）和 Evidence Doctor（证据体检）见 `doc/benchmark-and-evidence.zh.md`。

## 运行入口分类

| 类别 | 当前入口 | 证明范围 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make run_test_compare` | Std/RVV 两种构建运行同一套 helper、production direct 和 fallback 测试。 | 不证明目标硬件性能。 |
| correctness alias（正确性细分入口） | `make run_test_std`、`make run_test_rvv` | 分别验证标量构建和 RVV 构建可运行。 | 不单独关闭 A/B 性能。 |
| board smoke（板卡小型验证） | `make run_board_test` | RVV gtest 在板卡通过，证明目标硬件可运行和输出容差。 | 不作为 repeated performance。 |
| bench diagnostic alias（bench 诊断入口） | `make run_bench_std`、`make run_bench_rvv`，配合 `BENCH_ARGS=--case-filter ...` | 输出 helper-only、public-search-shaped 和 production-public case 的日志形状和 checksum。 | QEMU timing（QEMU 计时）不作为性能证据。 |
| production asm alias（生产反汇编入口） | `make check_production_rvv_asm`、`make check_production_pointxyzi_rvv_asm`、`make check_production_pointxyzrgb_rvv_asm`、`make check_production_pointxyzrgba_rvv_asm` | 检查 production helper 或 public symbol 范围内有 `vlux*ei32` 和 `vfred*sum`。 | 不证明速度，只证明路径归属。 |
| board repeated（板卡重复采集） | `make run_board_mi_production_repeated`、`make run_board_mi_production_pointxyzi_repeated`、`make run_board_mi_production_pointxyzrgb_repeated`、`make run_board_mi_production_pointxyzrgba_repeated` | 采集 production-public 5-run summary、manifest、doctor 和 registry。 | 不外推到未覆盖点型或 full-cloud overload。 |
| historical diagnostic repeated（历史诊断重复采集） | `make run_board_mi_repeated`、`make run_board_mi_phase010_repeated` | 维护 Phase 000/010 helper-only 和 production-shaped diagnostic 证据。 | 不作为最终生产性能结论。 |
| doctor / registry（证据体检和登记） | `make record_board_mi_repeated_state`、`make record_board_mi_phase010_repeated_state`、各 production repeated target 内部 record 步骤 | 从已有 run 目录刷新 summary、Evidence Doctor 和 `log/evidence_registry.json`。 | 不重新采集板卡 raw run。 |

## 输入数据总览

当前 production direct 测试范围为 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA`，`Scalar=float`，输入为有限 dense AoS（结构数组）点云。row source（行来源）是 `kd_tree_k_neighbor_query`，即 `computeFeature` 先通过 KdTree nearestKSearch（近邻搜索）得到邻域 indices，再调用 indexed `computePointMomentInvariants`。

历史 helper-only 诊断还覆盖 full cloud（连续点云）和 indexed cloud（索引点云）两种测试专用入口，但 full-cloud production overload 当前保持标量。

未覆盖范围为 `Scalar=double`、`PointXYZRGBNormal`、`PointXYZINormal`、用户自定义点型、非 dense surface、NaN / Inf surface lane 的 RVV mask、correspondences（对应关系索引）、其它输出类型和 KdTree/search 优化。

## 推荐流程

1. `make run_test_compare`
2. `make check_production_rvv_asm`
3. `make check_production_pointxyzi_rvv_asm`
4. `make check_production_pointxyzrgb_rvv_asm`
5. `make check_production_pointxyzrgba_rvv_asm`
6. 需要目标硬件性能复核时，运行对应 `make run_board_mi_production*_repeated` target。
7. 只需刷新已有 summary / doctor / registry 时，运行对应 record target 或 summary generator，不把 raw `run-*` 加入默认提交。

## 当前结论边界

Phase 030/040 production-public repeated board evidence（重复板卡证据）均为 weak_positive，且四个 adopted 点型都是 0/5 退化。QEMU 只支撑 correctness 和路径，性能结论只来自 board summary。Evidence Doctor 没有 Error / Warning；metadata 和 binary identity suggestion 作为后续 evidence hardening（证据加固）方向保留。
