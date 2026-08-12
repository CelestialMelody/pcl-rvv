# transformation_estimation_point_to_plane_lls_weighted 总览

本目录是 `registration/transformation_estimation_point_to_plane_lls_weighted` 的 RVV 专项测试工程。它包含 gtest、bench、topic-local 脚本、文档和可提交证据摘要。

production 文件：

```text
registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp
```

当前 production 结论覆盖 full-cloud public overload 和 source-indexed public overload。full-cloud 采用 block-reduction + A/B/C/N block groups + fused-abcd-ilp code shape。source-indexed 当前采用 staged-gather / compressed-tail family。两条 RVV 分流都要求 `Scalar=float`、连续 `weights_`、source xyz f32 AoS layout、target xyz+normal f32 AoS layout、规模/VLEN/byte-offset gate 均满足。dual-indices、correspondences、`Scalar=double`、layout miss 和 invalid source index gate miss 路径保持标量；它们在当前 topic 里先作为 pre-production diagnostic（接入生产前诊断）和 family carry-over audit（实现族迁移审计）对象，而不是直接 production。

## 先读哪份文档

| 问题 | 文档 |
| --- | --- |
| 有哪些测试类型，`run_test` 和 `run_bench` 属于什么 | `doc/testing-overview.zh.md` |
| 每个 gtest 名称是什么意思，输入和断言是什么 | `doc/correctness-tests.zh.md` |
| bench label、case-filter、checksum、trace、asm 和日志提交边界怎么解释 | `doc/benchmark-and-evidence.zh.md` |
| 每种 RVV 优化方式对应哪些代码、target 和证据 | `doc/optimization-evidence.zh.md` |
| `include/impl` 与 `src` 的函数族和调用关系是什么 | `doc/test-support-code-map.zh.md` |
| 为什么采用当前 production 方案，历史候选如何取舍 | `doc/transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md` |
| 当前 phase loop 如何恢复，哪些动作还没闭合 | `doc/phases/README.zh.md` |
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

QEMU bench 形状检查（`qemu_smoke_only`，不作为性能结论）：

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

source-indexed production correctness：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  run_test_source_indices_compare
```

source-indexed implementation-family correctness：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  run_test_source_indexed_family_compare
```

source-indexed implementation-family bench 形状检查：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  run_bench_source_indexed_family
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

source-indexed implementation-family 板卡单次运行：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  run_board_bench_source_indexed_family
```

该板卡 target 默认带 `TEPTPLW_BOARD_WARMUP_ITERATIONS=5`。旧 `Warmup Iterations: 0` 的
`run_board_bench_source_indexed_family` 日志只作为 historical diagnostic，不再作为当前性能判断。

source-indexed implementation-family repeated board：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_source_indexed_family_repeated
```

该 target 默认使用 `source-indexed-family`、`size=262144`、`runs=5`、`iterations=20` 和
`warmup=5`，输出到 `log/board/source_indexed_family_repeated/summary.md`。它是后续校准
block-fused-abcd-ilp 是否值得继续进入 production-candidate investigation 的重复板卡入口。

summary 生成后再跑 Evidence Doctor wrapper：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  doctor_board_source_indexed_family_repeated
```

这个 target 会生成 `log/board/source_indexed_family_repeated/evidence_manifest.json`、
`evidence_doctor.md` 和 `evidence_doctor.json`。它仍是 pre-production diagnostic（接入生产前诊断）
边界，不替代 production direct（真实生产路径）证据。

row source repeated board：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_row_sources_repeated
```

source-indexed production repeated board：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_production_source_indices_repeated
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
| `log/qemu/run_test_std.log` | QEMU std gtest correctness；45 passed + 1 skipped。 |
| `log/qemu/run_test_rvv.log` | QEMU RVV gtest correctness；47 passed。 |
| `log/qemu/run_test_source_indices_std.log` | source-indexed production direct 细粒度 std correctness；6 passed。 |
| `log/qemu/run_test_source_indices_rvv.log` | source-indexed production direct 细粒度 RVV correctness；7 passed。 |
| `log/board/run_board_bench_row_sources/analyze_bench_compare.log` | row-source 诊断触发原始日志；source-indexed 65536/262144 正向，dual-indices 和 correspondences 负向。 |
| `log/board/run_board_bench_row_sources/evidence_manifest.json` | row-source diagnostic manifest；记录 single-run observed speedup 和 diagnostic boundary。 |
| `log/board/run_board_bench_row_sources/evidence_doctor.md` | row-source diagnostic doctor；0 Errors / 0 Warnings / 0 Suggestions。 |
| `log/board/production_dispatch_fused_abcd_ilp/evidence_manifest.json` | production-dispatch repeated board evidence manifest；给 Evidence Doctor 的 machine-readable 边界。 |
| `log/board/production_dispatch_fused_abcd_ilp/evidence_doctor.md` | production-dispatch Evidence Doctor 摘要；当前为 0 Errors / 0 Warnings / 3 Suggestions。 |
| `log/board/production_dispatch_fused_abcd_ilp/summary.md` | production-dispatch repeated std/RVV speedup summary。 |
| `log/board/production_source_indices_staged_gather/evidence_manifest.json` | source-indexed repeated board evidence manifest；当前 Evidence Doctor 边界。 |
| `log/board/production_source_indices_staged_gather/evidence_doctor.md` | source-indexed Evidence Doctor 摘要；0 Errors / 7 Warnings / 6 Suggestions。 |
| `log/board/production_source_indices_staged_gather/summary.md` | source-indexed production repeated std/RVV speedup summary。 |
| `log/board/run_board_bench_source_indexed_family/evidence_manifest.json` | source-indexed implementation-family diagnostic manifest；记录 single-run board smoke，不能替代 production evidence。 |
| `log/board/run_board_bench_source_indexed_family/evidence_doctor.md` | source-indexed family Evidence Doctor；Phase 030 起会额外检查 zero-warmup、component/full sink 和 solve-delta 异常，结论保持 diagnostic / reopened。 |
| `log/board/source_indexed_family_repeated/summary.md` | source-indexed implementation-family repeated board summary；当前 5-run 结果显示 `block-fused-abcd-ilp` full estimate median `0.90x`。 |
| `log/board/source_indexed_family_repeated/evidence_manifest.json` | source-indexed implementation-family repeated manifest；由 `doctor_board_source_indexed_family_repeated` 生成。 |
| `log/board/source_indexed_family_repeated/evidence_doctor.md` | source-indexed implementation-family repeated doctor；当前为 3 Errors / 6 Warnings / 13 Suggestions。 |
| `log/board/source_indexed_family_repeated/evidence_doctor.json` | source-indexed implementation-family repeated doctor 机器可读输出。 |
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

QEMU `run_test_compare` 已覆盖 47 个 gtest。std 构建为 `45 passed + 1 skipped`，RVV 构建为 `47 passed`。输入语义测试会打印预期的 `PCL_ERROR` 行，用于证明 public overload 在数量不匹配时直接返回并保持输出矩阵不变。

board production-dispatch repeated summary 使用 262144 点、5 runs、20 iterations 和 5 warm-up iterations。三类代表点型结果为：

| case | median/min |
| --- | ---: |
| `weighted lls production-dispatch full-cloud pointnormal 262144` | `2.76x / 2.73x` |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointnormal 262144` | `2.98x / 2.95x` |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal 262144` | `3.00x / 2.96x` |

board source-indexed production repeated summary 使用 65536/262144 点、5 runs、20 iterations 和 5 warm-up iterations。三类代表点型在两个规模上均为正向，summary 路径为 `log/board/production_source_indices_staged_gather/summary.md`。对应的 machine-readable 边界已经补成 `log/board/production_source_indices_staged_gather/evidence_manifest.json` 和 `evidence_doctor.md`；doctor 目前给出 0 Errors / 7 Warnings / 6 Suggestions，主要在于 source-indexed-specific asm boundary 仍缺、binary identity 仍缺，但并不推翻 repeated board 的正向结果。

`log/board/run_board_bench_row_sources/analyze_bench_compare.log` 是 source-indexed 接入的诊断触发原始日志，不是最终 production 性能结论。它现在也有配套的 diagnostic manifest / doctor：`log/board/run_board_bench_row_sources/evidence_manifest.json`、`evidence_doctor.md`，用于记录 pre-production diagnostic 边界，不会把 row-source 负向信号误写成 production direct。

Phase 010 的 source-indexed implementation-family audit 已补 `source-indexed-family` case-filter。该阶段的旧单次板卡 smoke 没有 warmup，且 component no-solve 与 full estimate 的 sink 口径不够隔离。Phase 030 已把它降级为 historical diagnostic，并完成带 warmup 的 source-indexed-family repeated board：`staged-gather` median `1.04x`、`block-baseline` median `1.05x`、`block-fused-abcd-ilp` full estimate median `0.90x` 且 `4/5` 低于 `1.0x`。新 Evidence Doctor 为 3 Errors / 6 Warnings / 13 Suggestions。因此当前不替换 production，也不把 `block-fused-abcd-ilp` 推进 production-candidate；若继续，只应另开 root-cause / extended-run phase，补 raw logs、环境字段、binary hash 和 source-indexed-specific asm。
