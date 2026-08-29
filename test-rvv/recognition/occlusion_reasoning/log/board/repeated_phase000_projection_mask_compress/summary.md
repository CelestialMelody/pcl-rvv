# Occlusion reasoning ZBuffering filter diagnostic repeated board summary

- run_label: `occlusion_reasoning_phase000_projection_mask_compress_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_shaped_diagnostic`
- A/B boundary: `test_helper`
- timer_boundary: `filter indices only; excludes computeDepthMap and copyPointCloud`
- decision_bucket: `positive`
- median_speedup: `1.920x`
- min_speedup: `1.920x`
- max_speedup: `1.960x`
- p10_speedup: `1.920x`
- p90_speedup: `1.944x`
- B/A < 1: `0/5`

| run | std ms | rvv ms | B/A | checksum |
| --- | ---: | ---: | ---: | --- |
| run_01 | 4.917000 | 2.561200 | 1.920000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_02 | 4.913600 | 2.558400 | 1.920000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_03 | 4.922300 | 2.563700 | 1.920000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_04 | 5.016800 | 2.565800 | 1.960000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_05 | 4.916200 | 2.561000 | 1.920000 | std=13446965672972001487;rvv=13446965672972001487 |
