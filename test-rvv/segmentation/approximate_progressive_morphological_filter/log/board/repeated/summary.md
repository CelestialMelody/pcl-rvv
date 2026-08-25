# APMF component repeated board summary

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
| apmf grid z-min dense component | 5 | 2.27x | 2.22x | 2.28x | 2.28x, 2.27x, 2.28x, 2.27x, 2.22x | positive |
| apmf grid z-min non-dense component | 5 | 2.45x | 2.44x | 2.46x | 2.46x, 2.45x, 2.46x, 2.45x, 2.44x | positive |
| apmf tail compress component | 5 | 2.32x | 2.29x | 2.33x | 2.33x, 2.29x, 2.32x, 2.33x, 2.32x | positive |
| apmf window open component | 5 | 1.02x | 1.01x | 1.02x | 1.02x, 1.02x, 1.01x, 1.02x, 1.02x | neutral |
