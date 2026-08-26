# EPPD full-scan single-polygon repeated board summary (dense_ordered_indices)

- device: Milkv-Jupiter
- evidence_role: production_public
- row_source: dense_ordered_indices
- case: eppd single polygon full-scan production indices=dense
- iterations: 8
- warmup_iterations: 2
- run_count: 5

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| eppd single polygon full-scan production indices=dense | 5 | 1.75x | 1.74x | 1.75x | 1.74x | 1.75x | 1.75x, 1.75x, 1.75x, 1.74x, 1.75x |
