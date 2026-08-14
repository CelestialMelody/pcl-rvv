# ICP transformCloud 重复板卡 Benchmark 摘要

运行上下文：
- device：`Milkv-Jupiter`
- evidence role：`production_direct`
- case filter：`transform-cloud`
- runs：`5`
- iterations：`20`
- warm-up iterations：`3`
- taskset：`not_pinned`
- governor：`performance`
- freq：`1600000`
- temperature：`41000`
- binary hash：`bench_icp_std=sha256:02ba9726ea8ef6593d12acd7d515fb1d422157f4ae2c50b1fea09e5fbb85ed35; bench_icp_rvv=sha256:98da78b855645774f2d53d8c7f2cce31046a82321d895172d3d336bbecdc38fb`
- raw compare logs：默认使用临时目录，脚本结束后清理，不进入提交边界。

使用 topic-local target 重新生成：

```bash
make -C test-rvv/registration/icp collect_board_transform_cloud_repeated \
  ICP_BOARD_RUNS=5
```

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| icp transform-cloud xyz 256K | 5 | 5.30x | 5.17x | 5.32x | 5.21x | 5.32x | 5.17x, 5.32x, 5.28x, 5.30x, 5.32x |
| icp transform-cloud xyz 64K | 5 | 5.68x | 5.29x | 6.50x | 5.34x | 6.37x | 5.29x, 6.17x, 6.50x, 5.68x, 5.40x |
| icp transform-cloud xyz-normal 256K | 5 | 3.93x | 3.85x | 4.07x | 3.87x | 4.02x | 3.93x, 3.89x, 4.07x, 3.85x, 3.94x |
| icp transform-cloud xyz-normal 64K | 5 | 3.76x | 3.62x | 3.97x | 3.62x | 3.89x | 3.97x, 3.62x, 3.63x, 3.77x, 3.76x |
