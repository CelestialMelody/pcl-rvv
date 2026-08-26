# EPPD full-scan single-polygon repeated board summary (dense_ordered_indices)

- device: Milkv-Jupiter
- evidence_role: production_public
- row_source: dense_ordered_indices
- point_type: PointXYZI
- case: eppd single polygon full-scan production indices=dense point-type=xyzi
- iterations: 8
- warmup_iterations: 2
- run_count: 5

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| eppd single polygon full-scan production indices=dense point-type=xyzi | 5 | 1.83x | 1.76x | 1.85x | 1.78x | 1.84x | 1.76x, 1.83x, 1.85x, 1.83x, 1.80x |
