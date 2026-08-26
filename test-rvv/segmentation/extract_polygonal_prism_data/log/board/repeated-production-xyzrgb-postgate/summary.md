# EPPD full-scan single-polygon repeated board summary (dense_ordered_indices)

- device: Milkv-Jupiter
- evidence_role: production_public
- row_source: dense_ordered_indices
- point_type: PointXYZRGB
- case: eppd single polygon full-scan production indices=dense point-type=xyzrgb
- iterations: 8
- warmup_iterations: 2
- run_count: 5

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| eppd single polygon full-scan production indices=dense point-type=xyzrgb | 5 | 1.83x | 1.75x | 1.88x | 1.77x | 1.87x | 1.82x, 1.75x, 1.83x, 1.88x, 1.86x |
