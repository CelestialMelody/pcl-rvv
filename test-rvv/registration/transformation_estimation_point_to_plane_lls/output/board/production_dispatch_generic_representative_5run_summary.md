# TEPTPL production-dispatch generic representative board summary

EvidenceDecision:

```text
production-candidate/full-cloud-f32-aos-layout-gated-source-xyz-target-xyznormal-float-rvv-block-dispatch-representative-pointtypes
```

本文件是 summary-only evidence artifact（只提交摘要的证据文件）。原始板卡日志不提交到仓库；下方 host-local archive（本机临时归档）只记录当轮取证位置。长期审计以本文件内的 values、命令和 analyzer hash 为准。

## Scope

- Topic: `transformation_estimation_point_to_plane_lls`
- Device: `Milkv-Jupiter`
- Case filter: `production-dispatch`
- Sizes: `65536,262144`
- Runs: `5`
- Bench contract: std/RVV 两侧都调用真实 public full-cloud overload。
- Board-covered point combinations:
  - `PointNormal -> PointNormal`
  - `PointXYZ -> PointNormal`
  - `PointXYZ -> PointXYZINormal`
- Gate-allowed but not individually board-covered: 其它满足 source `RVVXYZAoSFloatLayout` 和 target `RVVXYZNormalFloatLayout` 的 f32 AoS 点型组合。

## Commands

```text
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_board_bench_compare BENCH_ARGS="--size 65536,262144 --case-filter production-dispatch"
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls fetch_board_logs
python3 test-rvv/script/analyze_bench_repeated.py \
  /tmp/teptpl_generic_board_5run_fetch_20260728_173246/analyze_bench_compare_1.log \
  /tmp/teptpl_generic_board_5run_fetch_20260728_173246/analyze_bench_compare_2.log \
  /tmp/teptpl_generic_board_5run_fetch_20260728_173246/analyze_bench_compare_3.log \
  /tmp/teptpl_generic_board_5run_fetch_20260728_173246/analyze_bench_compare_4.log \
  /tmp/teptpl_generic_board_5run_fetch_20260728_173246/analyze_bench_compare_5.log
```

Analyzer:

```text
2453697926ebf988d7854d3e69736c810507e7940628c3b29cc1df99b8995037  test-rvv/script/analyze_bench_repeated.py
```

Raw evidence archive:

```text
/tmp/teptpl_generic_board_5run_fetch_20260728_173246
```

该路径是当轮本机临时归档，不是长期提交内容，也不要求永久存在。用户后续单轮 production-dispatch smoke 与该 5-run 结论方向一致，但不覆盖或替代本 summary。

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

| Benchmark Item                                                     | runs | 64K speedup median/min/p10 | 256K speedup median/min/p10 | values                                                                                 |
| ------------------------------------------------------------------ | ---: | -------------------------- | --------------------------- | -------------------------------------------------------------------------------------- |
| `lls production-dispatch full-cloud pointnormal`                 |    5 | `2.73x/2.69x/2.70x`      | `2.71x/2.49x/2.53x`       | 64K:`2.69x, 2.74x, 2.74x, 2.73x, 2.73x`; 256K: `2.49x, 2.76x, 2.61x, 2.76x, 2.71x` |
| `lls production-dispatch full-cloud pointxyz-to-pointnormal`     |    5 | `2.93x/2.90x/2.91x`      | `2.77x/2.24x/2.39x`       | 64K:`2.91x, 2.96x, 2.90x, 2.93x, 2.96x`; 256K: `2.24x, 2.86x, 2.77x, 2.95x, 2.62x` |
| `lls production-dispatch full-cloud pointxyz-to-pointxyzinormal` |    5 | `2.92x/2.89x/2.90x`      | `2.81x/1.52x/2.04x`       | 64K:`2.92x, 2.94x, 2.89x, 2.92x, 2.93x`; 256K: `2.92x, 1.52x, 2.81x, 2.81x, 2.92x` |

`PointXYZ -> PointXYZINormal` 的 256K min `1.52x` 是当前 generic target representative evidence（代表性目标点类型证据）的稳定性风险；median 仍为正向，但不能把它写成所有 gate-allowed target 点型都已稳定。

QEMU timing 不作为性能结论。本文件只索引板卡 repeated summary；QEMU correctness、反汇编和本地测试结果见 evaluation 文档。
