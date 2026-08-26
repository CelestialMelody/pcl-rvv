# EPPD full-scan nested-polygon repeated board summary (source_indexed_points)

- device: Milkv-Jupiter
- evidence_role: production_public
- row_source: source_indexed_points
- case: eppd nested polygon full-scan production indices=indexed
- iterations: 8
- warmup_iterations: 2
- run_count: 5

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| eppd nested polygon full-scan production indices=indexed | 5 | 2.13x | 2.11x | 2.17x | 2.12x | 2.16x | 2.11x, 2.17x, 2.12x, 2.13x, 2.14x |
