# EPPD full-scan single-polygon repeated board summary (dense_ordered_indices)

- device: Milkv-Jupiter
- evidence_role: production_public
- row_source: dense_ordered_indices
- point_type: PointXYZINormal
- case: eppd single polygon full-scan production indices=dense point-type=xyzinormal
- iterations: 8
- warmup_iterations: 2
- run_count: 5

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| eppd single polygon full-scan production indices=dense point-type=xyzinormal | 5 | 1.09x | 0.99x | 1.45x | 1.01x | 1.31x | 1.45x, 0.99x, 1.03x, 1.11x, 1.09x |
