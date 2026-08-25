# APMF production public repeated board

## Context

- device: `Milkv-Jupiter`
- run_count: `5`
- iterations: `8`
- warmup_iterations: `2`
- governor: `not_recorded`
- freq: `not_recorded`
- temperature: `not_recorded`
- vlen: `not_recorded`

## Results

| Benchmark Item | runs | median speedup | min | max | values | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| apmf production public PointXYZI dense | 5 | 1.51x | 1.49x | 1.52x | 1.51x, 1.49x, 1.52x, 1.51x, 1.52x | positive |
| apmf production public PointXYZI non-dense | 5 | 1.46x | 1.44x | 1.46x | 1.46x, 1.44x, 1.46x, 1.45x, 1.46x | positive |
| apmf production public PointXYZRGB dense | 5 | 1.49x | 1.49x | 1.51x | 1.49x, 1.49x, 1.49x, 1.51x, 1.50x | positive |
| apmf production public PointXYZRGB non-dense | 5 | 1.45x | 1.44x | 1.46x | 1.45x, 1.44x, 1.45x, 1.45x, 1.46x | positive |
| apmf production public PointXYZRGBA dense | 5 | 1.51x | 1.51x | 1.51x | 1.51x, 1.51x, 1.51x, 1.51x, 1.51x | positive |
| apmf production public PointXYZRGBA non-dense | 5 | 1.46x | 1.45x | 1.46x | 1.46x, 1.46x, 1.45x, 1.46x, 1.46x | positive |
| apmf production public dense | 5 | 1.53x | 1.52x | 1.53x | 1.52x, 1.52x, 1.53x, 1.53x, 1.53x | positive |
| apmf production public non-dense | 5 | 1.30x | 1.30x | 1.31x | 1.30x, 1.30x, 1.30x, 1.30x, 1.31x | positive |
