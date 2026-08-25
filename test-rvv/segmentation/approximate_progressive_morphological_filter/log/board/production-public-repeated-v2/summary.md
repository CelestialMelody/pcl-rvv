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
| apmf production public dense | 5 | 1.60x | 1.59x | 1.67x | 1.59x, 1.60x, 1.67x, 1.60x, 1.61x | positive |
| apmf production public non-dense | 5 | 1.35x | 1.35x | 1.39x | 1.35x, 1.35x, 1.39x, 1.35x, 1.36x | positive |
