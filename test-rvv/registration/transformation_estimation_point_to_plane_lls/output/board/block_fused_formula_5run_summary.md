# TEPTPL block fused-formula board summary

Production EvidenceDecision after the later production-dispatch closeout:

```text
production-candidate/full-cloud-f32-aos-layout-gated-source-xyz-target-xyznormal-float-rvv-fused-formula-block-dispatch-representative-pointtypes
```

本文件是 summary-only evidence artifact（只提交摘要的证据文件）。本轮 fused-formula 结果只覆盖 `test-rvv` direct block A/B；它本身不替代 production-dispatch evidence。fused-formula 进入默认 production hot path 的依据见 `production_dispatch_generic_representative_5run_summary.md`。

2026-07-29 closeout note: `FullCloudBlockFusedFormulaNearCancellationMatchesStdWithinBudget` 已补入 `run_test_compare`，覆盖 d 公式 near-cancellation、accepted_points、ATA/ATb、matrix、invalid lane 和 scale-stress。后续 production-facing correctness、production-symbol asm 归属和三类代表点型 production-dispatch 5-run A/B 已补齐，fused-formula 因此升级为默认 full-cloud production block。

## Scope

- Topic: `transformation_estimation_point_to_plane_lls`
- Device: `Milkv-Jupiter`
- Case filter: `block`
- Sizes: `65536,262144`
- Runs: `5`
- Bench contract: std/RVV 对比同名 diagnostic direct block helper；`block-fused-formula` 不走 production public dispatch。
- Compared variants:
  - current `lls normal-equation full-cloud block-reduction pointnormal`
  - fused-formula `lls normal-equation full-cloud block-fused-formula pointnormal`
- Non-evidence rows: `public-entry-shaped full-cloud block-reduction` 同时出现在 filter 输出中，但它不是 fused-formula evidence。

## Commands

```text
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_board_bench_compare BENCH_ARGS="--size 65536,262144 --case-filter block"
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls fetch_board_logs
python3 test-rvv/script/analyze_bench_repeated.py \
  <local-raw-archive>/block-fused-formula/analyze_bench_compare_1.log \
  <local-raw-archive>/block-fused-formula/analyze_bench_compare_2.log \
  <local-raw-archive>/block-fused-formula/analyze_bench_compare_3.log \
  <local-raw-archive>/block-fused-formula/analyze_bench_compare_4.log \
  <local-raw-archive>/block-fused-formula/analyze_bench_compare_5.log
```

Analyzer:

```text
2453697926ebf988d7854d3e69736c810507e7940628c3b29cc1df99b8995037  test-rvv/script/analyze_bench_repeated.py
```

Raw evidence archive:

```text
<local-raw-archive>/block-fused-formula
```

该路径是当轮本机临时归档，不是长期提交内容，也不要求永久存在。长期审计以本文件内的 values、命令和 analyzer hash 为准。

Run files in the raw archive:

```text
analyze_bench_compare_1.log
analyze_bench_compare_2.log
analyze_bench_compare_3.log
analyze_bench_compare_4.log
analyze_bench_compare_5.log
run_bench_rvv_1.log
run_bench_rvv_2.log
run_bench_rvv_3.log
run_bench_rvv_4.log
run_bench_rvv_5.log
run_bench_std_1.log
run_bench_std_2.log
run_bench_std_3.log
run_bench_std_4.log
run_bench_std_5.log
repeated_summary.log
```

## Summary

| Benchmark Item | runs | 64K speedup median/min/p10 | 256K speedup median/min/p10 | values |
| --- | ---: | --- | --- | --- |
| `lls normal-equation full-cloud block-reduction pointnormal` | 5 | `1.37x/1.24x/1.29x` | `1.33x/1.29x/1.30x` | 64K: `1.24x, 1.38x, 1.39x, 1.36x, 1.37x`; 256K: `1.29x, 1.33x, 1.33x, 1.38x, 1.38x` |
| `lls normal-equation full-cloud block-fused-formula pointnormal` | 5 | `1.43x/1.29x/1.34x` | `1.38x/1.34x/1.36x` | 64K: `1.29x, 1.44x, 1.45x, 1.42x, 1.43x`; 256K: `1.34x, 1.38x, 1.38x, 1.44x, 1.41x` |

Fused-formula direct block rows are positive in this 5-run diagnostic A/B. They are not production-dispatch results; production 接入决策使用单独的 fused-specific production-dispatch representative point type A/B、asm hotspot attribution 和 production-facing correctness。

QEMU timing is not a performance conclusion. This file only indexes the board repeated summary for the diagnostic fused-formula A/B.
