# weighted production-dispatch generic representative 5-run board summary

EvidenceDecision:

```text
production-candidate/full-cloud-f32-aos-layout-gated-weighted-block-dispatch-representative-pointtypes
```

This is a summary-only evidence artifact. Raw board run directories and raw per-run logs are not committed; the archive path below is a placeholder for the local run used to compute this table.

## Scope

- Topic: `transformation_estimation_point_to_plane_lls_weighted`.
- Device: Milkv-Jupiter.
- Case filter: `production-dispatch`.
- Sizes: 65536 and 262144 input points.
- Runs: 5.
- Bench binary: `bench_transformation_estimation_point_to_plane_lls_weighted_{std,rvv}`.
- Bench contract: std/RVV both call the real public full-cloud overload with `setCorrespondenceWeights(weights)`.
- Covered production path: full-cloud, `Scalar=float`, contiguous `weights_`, source `RVVXYZAoSFloatLayout`, target `RVVXYZNormalFloatLayout`, and size/VL/byte-offset gates satisfied.
- Representative point types: `PointNormal -> PointNormal`, `PointXYZ -> PointNormal`, and `PointXYZ -> PointXYZINormal`.
- Not covered: source-indexed, dual-indices, correspondences, `Scalar=double`, non-contiguous weights, or per-type board proof for every gate-allowed point type.

QEMU timing is not used as a performance conclusion. QEMU only covers build/log shape and correctness tests; this file is board repeated performance evidence for the representative full-cloud weighted production dispatch.

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
- Raw archive placeholder: `<local-raw-archive>/production_dispatch_weighted_generic_representative_5run_20260730_171909`

## Results

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `weighted lls production-dispatch full-cloud pointnormal 65536` | 5 | 2.69x | 2.66x | 2.74x | 2.67x | 2.73x | 2.67x, 2.66x, 2.74x, 2.70x, 2.69x |
| `weighted lls production-dispatch full-cloud pointnormal 262144` | 5 | 2.71x | 2.11x | 2.75x | 2.35x | 2.73x | 2.70x, 2.71x, 2.71x, 2.75x, 2.11x |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointnormal 65536` | 5 | 2.81x | 2.79x | 2.85x | 2.80x | 2.84x | 2.80x, 2.79x, 2.81x, 2.83x, 2.85x |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointnormal 262144` | 5 | 2.83x | 2.69x | 2.91x | 2.70x | 2.90x | 2.83x, 2.91x, 2.70x, 2.69x, 2.89x |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal 65536` | 5 | 2.81x | 2.80x | 2.83x | 2.80x | 2.83x | 2.80x, 2.81x, 2.80x, 2.82x, 2.83x |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal 262144` | 5 | 2.85x | 2.83x | 2.88x | 2.84x | 2.87x | 2.85x, 2.83x, 2.85x, 2.88x, 2.86x |

## Interpretation

The repeated board signal is positive for the representative production path set: full-cloud `Scalar=float`, contiguous `weights_`, source xyz f32 AoS layout gate, target xyz+normal f32 AoS layout gate, and weighted block-reduction dispatch. The `PointNormal 262144` row has one lower but still positive run (`2.11x`), so the evidence should be described as positive representative board signal, not as zero-variance performance.

This does not upgrade source-indexed, dual-indices, or correspondences: those row source policies remain diagnostic-only because their evidence is separate and the board signal for dual/correspondences is negative. It also does not prove every gate-allowed point type individually; only the three representative point-type combinations above were run on board.
