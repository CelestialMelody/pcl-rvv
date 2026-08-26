# EPPD full-scan single-polygon repeated board summary (dense_ordered_indices)

- device: Milkv-Jupiter
- evidence_role: production_public
- row_source: dense_ordered_indices
- point_type: PointXYZRGBA
- case: eppd single polygon full-scan production indices=dense point-type=xyzrgba
- iterations: 8
- warmup_iterations: 2
- run_count: 5

| Benchmark Item | runs | median | min | max | p10 | p90 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| eppd single polygon full-scan production indices=dense point-type=xyzrgba | 5 | 1.82x | 1.77x | 1.85x | 1.78x | 1.84x | 1.82x, 1.85x, 1.77x, 1.80x, 1.83x |
