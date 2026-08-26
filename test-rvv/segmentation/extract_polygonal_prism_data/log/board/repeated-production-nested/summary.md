# EPPD full-scan nested-polygon repeated board summary (dense_ordered_indices)

- device: Milkv-Jupiter
- evidence_role: production_public
- row_source: dense_ordered_indices
- case: eppd nested polygon full-scan production indices=dense
- iterations: 8
- warmup_iterations: 2
- run_count: 5

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| eppd nested polygon full-scan production indices=dense | 5 | 2.18x | 2.11x | 2.23x | 2.14x | 2.22x | 2.23x, 2.18x, 2.22x, 2.18x, 2.11x |
