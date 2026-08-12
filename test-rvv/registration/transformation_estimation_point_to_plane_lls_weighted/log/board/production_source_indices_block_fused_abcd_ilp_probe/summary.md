# 重复 Benchmark Speedup 摘要

运行上下文：
- device：记录在每轮 raw compare log 中
- case filter：`production-source-indices`
- size：`65536,262144`
- runs：`5`
- iterations：`20`
- warm-up iterations：`5`
- collection profile：`source-indexed-block-fused-abcd-ilp-production-probe`
- raw compare logs：默认使用临时目录，脚本结束后清理，不进入提交边界。

使用 topic-local target 重新生成：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_production_source_indices_probe_repeated \
  TEPTPLW_SOURCE_INDICES_SIZE=<size> \
  TEPTPLW_SOURCE_INDICES_RUNS=<runs> \
  TEPTPLW_SOURCE_INDICES_ITERATIONS=<iterations> \
  TEPTPLW_SOURCE_INDICES_WARMUP_ITERATIONS=<warmup-iterations>
```

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| weighted lls production-dispatch source-indices pointnormal 262144 | 5 | 1.64x | 1.17x | 1.67x | 1.25x | 1.66x | 1.64x, 1.65x, 1.67x, 1.37x, 1.17x |
| weighted lls production-dispatch source-indices pointnormal 65536 | 5 | 1.71x | 1.66x | 1.74x | 1.68x | 1.74x | 1.66x, 1.74x, 1.74x, 1.71x, 1.70x |
| weighted lls production-dispatch source-indices pointxyz-to-pointnormal 262144 | 5 | 1.54x | 1.06x | 1.71x | 1.15x | 1.69x | 1.65x, 1.06x, 1.71x, 1.54x, 1.29x |
| weighted lls production-dispatch source-indices pointxyz-to-pointnormal 65536 | 5 | 1.64x | 1.59x | 1.68x | 1.60x | 1.66x | 1.62x, 1.64x, 1.68x, 1.64x, 1.59x |
| weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 262144 | 5 | 1.69x | 1.41x | 1.79x | 1.45x | 1.79x | 1.49x, 1.77x, 1.79x, 1.69x, 1.41x |
| weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 65536 | 5 | 1.69x | 1.68x | 1.75x | 1.68x | 1.73x | 1.69x, 1.69x, 1.75x, 1.70x, 1.68x |
