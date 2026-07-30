# historical weighted production-dispatch PointNormal subset 5-run board summary

EvidenceDecision:

```text
historical-subset/superseded-by-production-candidate-full-cloud-f32-aos-layout-gated-weighted-block-dispatch-representative-pointtypes
```

This is a historical summary-only evidence artifact. It remains valid as `PointNormal -> PointNormal` subset evidence, but the current decision index is `production_dispatch_weighted_generic_representative_5run_summary.md`. Raw board run directories and raw per-run logs are not committed; the archive path below is a placeholder for the local run used to compute this table.

## Scope

- Topic: `transformation_estimation_point_to_plane_lls_weighted`.
- Device: Milkv-Jupiter.
- Case filter: `production-dispatch`.
- Sizes: 65536 and 262144 input points.
- Runs: 5.
- Bench binary: `bench_transformation_estimation_point_to_plane_lls_weighted_{std,rvv}`.
- Bench contract: std/RVV both call the real public full-cloud overload with `setCorrespondenceWeights(weights)`.
- Covered production path: full-cloud `PointNormal -> PointNormal`, `Scalar=float`, contiguous `weights_`, size/VL/byte-offset gates satisfied.
- Not covered by this historical subset: source-indexed, dual-indices, correspondences, generic point types, `Scalar=double`, or non-contiguous weights.

QEMU timing is not used as a performance conclusion. QEMU only covers build/log shape and correctness tests; this file is board repeated performance evidence.

## Command

```text
for run in 1 2 3 4 5; do
  make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
    run_board_bench_compare \
    BENCH_ARGS="--size 65536,262144 --case-filter production-dispatch"
  make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
    fetch_board_logs
  cp test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/analyze_bench_compare.log \
    <local-raw-archive>/analyze_bench_compare_${run}.log
done

python3 test-rvv/script/analyze_bench_repeated.py \
  <local-raw-archive>/analyze_bench_compare_1.log \
  <local-raw-archive>/analyze_bench_compare_2.log \
  <local-raw-archive>/analyze_bench_compare_3.log \
  <local-raw-archive>/analyze_bench_compare_4.log \
  <local-raw-archive>/analyze_bench_compare_5.log
```

## Analyzer

- Repeated analyzer: `test-rvv/script/analyze_bench_repeated.py`
- Repeated analyzer sha256: `2453697926ebf988d7854d3e69736c810507e7940628c3b29cc1df99b8995037`
- Compare analyzer: `test-rvv/script/analyze_bench_compare.py`
- Compare analyzer sha256: `5c7937a5a3ca95f382a0c122ae786684f818845fe0963cb1fea87a70b73a9bd8`
- Raw archive placeholder: `<local-raw-archive>/production_dispatch_weighted_full_block_reduction_5run_20260730_153332`

## Results

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `weighted lls production-dispatch full-cloud pointnormal 65536` | 5 | 2.70x | 2.69x | 2.73x | 2.70x | 2.73x | 2.70x, 2.72x, 2.73x, 2.70x, 2.69x |
| `weighted lls production-dispatch full-cloud pointnormal 262144` | 5 | 2.72x | 2.56x | 2.77x | 2.62x | 2.76x | 2.56x, 2.75x, 2.77x, 2.72x, 2.72x |

## Interpretation

The repeated board signal is stable and positive for the historical `PointNormal -> PointNormal` subset: full-cloud, `Scalar=float`, contiguous `weights_`, and the weighted block-reduction helper. The current generic layout-gated decision is supported by the later representative summary. Neither artifact upgrades source-indexed, dual-indices, or correspondences: those paths remain diagnostic-only because their policy evidence is separate and the board signal for dual/correspondences is negative.
