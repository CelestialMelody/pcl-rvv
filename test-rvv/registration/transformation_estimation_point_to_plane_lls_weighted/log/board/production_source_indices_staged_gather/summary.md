# 重复 Benchmark Speedup 摘要

运行上下文：
- device：记录在每轮 raw compare log 中
- case filter：`production-source-indices`
- size：`65536,262144`
- runs：`5`
- iterations：`20`
- warm-up iterations：`5`
- raw compare logs 默认使用临时目录，不进入提交边界。

使用 topic-local target 重新生成：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_production_source_indices_repeated \
  TEPTPLW_SOURCE_INDICES_SIZE=<size> \
  TEPTPLW_SOURCE_INDICES_RUNS=<runs> \
  TEPTPLW_SOURCE_INDICES_ITERATIONS=<iterations> \
  TEPTPLW_SOURCE_INDICES_WARMUP_ITERATIONS=<warmup-iterations>
```

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| weighted lls production-dispatch source-indices pointnormal 262144 | 5 | 2.33x | 1.99x | 2.39x | 2.12x | 2.37x | 2.33x, 1.99x, 2.39x, 2.34x, 2.32x |
| weighted lls production-dispatch source-indices pointnormal 65536 | 5 | 2.59x | 2.47x | 2.61x | 2.51x | 2.61x | 2.60x, 2.47x, 2.61x, 2.59x, 2.57x |
| weighted lls production-dispatch source-indices pointxyz-to-pointnormal 262144 | 5 | 2.22x | 2.18x | 2.27x | 2.18x | 2.25x | 2.22x, 2.18x, 2.23x, 2.27x, 2.19x |
| weighted lls production-dispatch source-indices pointxyz-to-pointnormal 65536 | 5 | 2.57x | 2.49x | 2.62x | 2.50x | 2.61x | 2.62x, 2.49x, 2.57x, 2.60x, 2.52x |
| weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 262144 | 5 | 2.37x | 2.28x | 2.42x | 2.31x | 2.42x | 2.36x, 2.42x, 2.41x, 2.37x, 2.28x |
| weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 65536 | 5 | 2.78x | 2.70x | 2.82x | 2.73x | 2.81x | 2.82x, 2.70x, 2.77x, 2.78x, 2.79x |
