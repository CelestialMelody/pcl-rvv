# 重复 Benchmark Speedup 摘要

运行上下文：
- device：记录在每轮 raw compare log 中
- case filter：`source-indexed-family`
- size：`262144`
- runs：`5`
- iterations：`20`
- warm-up iterations：`5`
- raw compare logs：默认使用临时目录，脚本结束后清理，不进入提交边界。

使用 topic-local target 重新生成：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_source_indexed_family_repeated \
  TEPTPLW_SOURCE_INDEXED_FAMILY_SIZE=<size> \
  TEPTPLW_SOURCE_INDEXED_FAMILY_RUNS=<runs> \
  TEPTPLW_SOURCE_INDEXED_FAMILY_ITERATIONS=<iterations> \
  TEPTPLW_SOURCE_INDEXED_FAMILY_WARMUP_ITERATIONS=<warmup-iterations>
```

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| weighted lls component source-indexed-family block-baseline pointnormal no-solve 262144 | 5 | 1.16x | 1.00x | 1.29x | 1.07x | 1.28x | 1.00x, 1.16x, 1.29x, 1.16x, 1.27x |
| weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve 262144 | 5 | 0.98x | 0.85x | 1.18x | 0.88x | 1.12x | 1.18x, 0.98x, 0.93x, 1.03x, 0.85x |
| weighted lls source-indexed-family block-baseline pointnormal 262144 | 5 | 1.05x | 0.92x | 1.38x | 0.95x | 1.33x | 1.05x, 1.26x, 0.92x, 1.00x, 1.38x |
| weighted lls source-indexed-family block-fused-abcd-ilp pointnormal 262144 | 5 | 0.90x | 0.76x | 1.20x | 0.79x | 1.09x | 0.83x, 0.93x, 0.90x, 1.20x, 0.76x |
| weighted lls source-indexed-family staged-gather pointnormal 262144 | 5 | 1.04x | 0.95x | 1.32x | 0.97x | 1.30x | 1.04x, 1.25x, 1.32x, 0.95x, 0.99x |
