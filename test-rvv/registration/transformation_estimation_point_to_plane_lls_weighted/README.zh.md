# transformation_estimation_point_to_plane_lls_weighted 总览

本目录是 `registration/transformation_estimation_point_to_plane_lls_weighted` 的 RVV 专项测试工程。它包含 gtest、bench、topic-local 脚本、文档和可提交证据摘要。

production 文件：

```text
registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp
```

当前 production 结论只覆盖 full-cloud public overload、`Scalar=float`、连续 `weights_`、source xyz f32 AoS layout、target xyz+normal f32 AoS layout、规模/VLEN/byte-offset gate 均满足的路径。source-indexed、dual-indices、correspondences、`Scalar=double` 和 layout miss 路径保持标量。

## 先读哪份文档

| 问题 | 文档 |
| --- | --- |
| 有哪些测试类型，`run_test` 和 `run_bench` 属于什么 | `doc/testing-overview.zh.md` |
| 每个 gtest 名称是什么意思，输入和断言是什么 | `doc/correctness-tests.zh.md` |
| bench label、case-filter、checksum、trace、asm 和日志提交边界怎么解释 | `doc/benchmark-and-evidence.zh.md` |
| 每种 RVV 优化方式对应哪些代码、target 和证据 | `doc/optimization-evidence.zh.md` |
| `include/impl` 与 `src` 的函数族和调用关系是什么 | `doc/test-support-code-map.zh.md` |
| 为什么采用当前 production 方案，历史候选如何取舍 | `doc/transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md` |
| production 实现长期说明 | `../../../doc-rvv/registration/transformation_estimation_point_to_plane_lls_weighted-RVV.zh.md` |

## 目录分工

| 路径 | 作用 |
| --- | --- |
| `include/teptplw.h` | test/bench 共用聚合入口。 |
| `include/test_teptplw.h` | gtest-only 聚合入口。 |
| `include/bench_teptplw.h` | bench-only 聚合入口。 |
| `include/impl/` | fixtures、assertions、row source reference、candidate、reduction、bench harness 和 case registry。 |
| `src/test_teptplw_*.cpp` | gtest 源码，按公开入口语义、公开入口输入语义、候选正确性、行来源和真实生产路径分层。 |
| `src/bench_teptplw.cpp` | bench CLI 薄入口。 |
| `script/` | topic-local board 采集、trace 汇总、bench 分析和 asm attribution 脚本。 |
| `doc/` | 本 topic 的测试、证据、代码地图和评估文档。 |
| `log/qemu/` | QEMU correctness logs。 |
| `log/board/` | board summary evidence 和本机可再生成 raw artifacts。 |

## 常用命令

QEMU correctness：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_compare
```

QEMU bench 形状检查：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_bench_compare
```

fused formula bench 子集：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_bench_fused_formula
```

row source 诊断子集：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_bench_row_sources
```

公开入口语义板卡单次运行：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  run_board_test_public_semantics
```

row source bench 板卡单次运行：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  run_board_bench_row_sources
```

row source repeated board：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_row_sources_repeated
```

production-dispatch repeated board：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_production_dispatch_repeated \
  TEPTPLW_COMPARE_CASE_FILTER=production-dispatch \
  TEPTPLW_COMPARE_SIZE=262144 \
  TEPTPLW_COMPARE_RUNS=5 \
  TEPTPLW_COMPARE_ITERATIONS=20 \
  TEPTPLW_COMPARE_WARMUP_ITERATIONS=5
```

production-default trace/checksum/asm：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_production_default_fused_abcd_ilp \
  TEPTPLW_BOARD_SIZE=262144 \
  TEPTPLW_BOARD_RUNS=5 \
  TEPTPLW_BOARD_ITERATIONS=20 \
  TEPTPLW_BOARD_WARMUP_ITERATIONS=5
```

production-default RVV-only 形状检查：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  run_bench_production_default_fused_abcd_ilp_rvv
```

只重建 asm attribution：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  asm_production_default_fused_abcd_ilp
```

## 当前可提交证据

| 文件 | 证据角色 |
| --- | --- |
| `log/qemu/run_test_std.log` | QEMU std gtest correctness；39 passed + 1 skipped。 |
| `log/qemu/run_test_rvv.log` | QEMU RVV gtest correctness；40 passed。 |
| `log/board/production_dispatch_fused_abcd_ilp/summary.md` | production-dispatch repeated std/RVV speedup summary。 |
| `log/board/production_default_fused_abcd_ilp/trace_summary.md` | 默认 RVV path 多轮 trace summary。 |
| `log/board/production_default_fused_abcd_ilp/checksum_validation.md` | 默认 RVV path checksum 序列一致性。 |
| `log/board/production_default_fused_abcd_ilp/asm_production_symbol_attribution.md` | 默认 production helper 反汇编归因。 |

## 默认不提交的生成产物

这些文件可以用于本机排查。文档没有明确引用为证据时，不进入提交候选。

| 文件或模式 | 原因 |
| --- | --- |
| `log/qemu/run_bench_std.log`、`run_bench_rvv.log`、`analyze_bench_compare.log` | QEMU timing 不作为性能结论。 |
| `log/board/**/run*.log` | raw board logs；当前采用 summary-only 策略。 |
| `log/board/**/board_env_*.log` | 板卡环境 raw log；默认不提交。 |
| `log/board/**/collection_manifest.json` | 采集 manifest 可能包含本机或远端信息；默认不提交。 |
| `log/board/**/analyze_rvv_ba.md`、`ba_below_1_frequency.txt` | 只有被文档指定为当前证据时才提交。 |
| `log/vec_missed_log/` | compiler auto-vectorization 诊断 raw log；默认不提交。 |

## 当前结果

QEMU `run_test_compare` 已覆盖 40 个 gtest。std 构建为 `39 passed + 1 skipped`，RVV 构建为 `40 passed`。新增输入语义测试会打印预期的 `PCL_ERROR` 行，用于证明 public overload 在数量不匹配时直接返回并保持输出矩阵不变。

board production-dispatch repeated summary 使用 262144 点、5 runs、20 iterations 和 5 warm-up iterations。三类代表点型结果为：

| case | median/min |
| --- | ---: |
| `weighted lls production-dispatch full-cloud pointnormal 262144` | `2.76x / 2.73x` |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointnormal 262144` | `2.98x / 2.95x` |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal 262144` | `3.00x / 2.96x` |
