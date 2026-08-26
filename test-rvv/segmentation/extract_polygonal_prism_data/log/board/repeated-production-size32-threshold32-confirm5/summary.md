# EPPD full-scan single-polygon repeated board summary (dense_ordered_indices)

- device: Milkv-Jupiter
- evidence_role: production_public
- row_source: dense_ordered_indices
- point_type: PointXYZ
- case: eppd single polygon full-scan production indices=dense
- iterations: 32
- warmup_iterations: 4
- run_count: 5

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| eppd single polygon full-scan production indices=dense | 5 | 1.19x | 1.18x | 1.38x | 1.18x | 1.30x | 1.19x, 1.19x, 1.18x, 1.38x, 1.18x |
