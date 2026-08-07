# 重复 Benchmark Speedup 摘要

运行上下文：
- device：记录在每轮 raw compare log 中
- case filter：`production-dispatch`
- size：`262144`
- runs：`5`
- iterations：`20`
- warm-up iterations：`5`
- raw compare logs 默认使用临时目录，不进入提交边界。

使用 topic-local target 重新生成：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_production_dispatch_repeated \
  TEPTPLW_COMPARE_CASE_FILTER=<case-filter> \
  TEPTPLW_COMPARE_SIZE=<size> \
  TEPTPLW_COMPARE_RUNS=<runs> \
  TEPTPLW_COMPARE_ITERATIONS=<iterations> \
  TEPTPLW_COMPARE_WARMUP_ITERATIONS=<warmup-iterations>
```

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| weighted lls production-dispatch full-cloud pointnormal 262144 | 5 | 2.76x | 2.73x | 2.77x | 2.73x | 2.77x | 2.73x, 2.76x, 2.77x, 2.73x, 2.76x |
| weighted lls production-dispatch full-cloud pointxyz-to-pointnormal 262144 | 5 | 2.98x | 2.95x | 3.03x | 2.95x | 3.02x | 2.95x, 2.98x, 3.01x, 2.96x, 3.03x |
| weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal 262144 | 5 | 3.00x | 2.96x | 3.02x | 2.97x | 3.01x | 2.97x, 3.00x, 3.02x, 2.96x, 3.01x |
