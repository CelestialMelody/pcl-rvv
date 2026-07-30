# weighted full-cloud block-reduction 5-run board summary

## Scope

- Device: Milkv-Jupiter.
- Case filter: full-cloud current and full-cloud block-reduction only.
- Sizes: 65536 and 262144 input points.
- Runs: 5.
- Bench binary: `bench_transformation_estimation_point_to_plane_lls_weighted_{std,rvv}`.
- Entry shape: test-only direct helper, not production public overload.
- Std path: `estimate_candidate_full` / `estimate_candidate_full_block_reduction` compiled without `__RVV10__`, so both fall back to scalar diagnostic helpers.
- RVV path: same test-only helpers compiled with `__RVV10__`; block case uses weighted full-cloud block-reduction.

QEMU timing is not used as a performance conclusion. This summary only supports the full-cloud + contiguous `weights_` diagnostic candidate; it does not cover source-indexed, dual-indices, correspondences, generic point types, `Scalar=double`, or production dispatch.

## Command

```text
for run in 1 2 3 4 5; do
  make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_board_bench_compare
done
```

The bench program currently runs all 10 cases each round; this artifact summarizes only the four full-cloud current/block rows required for the PI1 gate.

## Analyzer

- Analyzer: `test-rvv/script/analyze_bench_compare.py`
- Analyzer sha256: `5c7937a5a3ca95f382a0c122ae786684f818845fe0963cb1fea87a70b73a9bd8`
- Raw archive placeholder: `<local-raw-archive>/weighted_full_block_reduction_5run_20260730_151509.log`
- Raw run directories are not committed.

## Results

| case | Std ms values | RVV ms values | speedup values | median | min | p10 | max |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: |
| full-cloud current 64K | 7.0701, 7.1128, 7.1303, 7.1371, 7.0319 | 6.0410, 6.1305, 5.9421, 6.0588, 5.9996 | 1.170, 1.160, 1.200, 1.178, 1.172 | 1.172x | 1.160x | 1.160x | 1.200x |
| full-cloud block-reduction 64K | 7.5126, 7.5541, 7.5475, 7.5755, 7.4785 | 4.8973, 4.9450, 4.8954, 4.9300, 4.8881 | 1.534, 1.528, 1.542, 1.537, 1.530 | 1.534x | 1.528x | 1.528x | 1.542x |
| full-cloud current 256K | 28.1669, 28.2819, 28.2665, 28.1687, 28.0278 | 24.4221, 24.3974, 23.5687, 24.1381, 23.9495 | 1.153, 1.159, 1.199, 1.167, 1.170 | 1.167x | 1.153x | 1.153x | 1.199x |
| full-cloud block-reduction 256K | 29.9840, 30.2609, 30.0396, 29.9750, 29.9292 | 20.9185, 19.6708, 20.1506, 19.4940, 19.1884 | 1.433, 1.538, 1.491, 1.538, 1.560 | 1.538x | 1.433x | 1.433x | 1.560x |

## Interpretation

The repeated board signal is stable enough to keep weighted full-cloud block-reduction as a PI1 production-integration candidate. It is still not production evidence: the measured RVV path is a test-only helper, and no production dispatch or production direct public-overload bench exists yet.

Source-indexed, dual-indices and correspondences stay out of production scope. Their policy evidence is separate, and the current board evidence for dual/correspondences is negative.
