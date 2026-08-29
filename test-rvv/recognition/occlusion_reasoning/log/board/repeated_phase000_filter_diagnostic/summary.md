# Occlusion reasoning ZBuffering filter diagnostic repeated board summary

- run_label: `occlusion_reasoning_phase000_filter_diagnostic_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_shaped_diagnostic`
- A/B boundary: `test_helper`
- timer_boundary: `filter indices only; excludes computeDepthMap and copyPointCloud`
- decision_bucket: `positive`
- median_speedup: `1.930x`
- min_speedup: `1.910x`
- max_speedup: `1.940x`
- p10_speedup: `1.914x`
- p90_speedup: `1.936x`
- B/A < 1: `0/5`

| run | std ms | rvv ms | B/A | checksum |
| --- | ---: | ---: | ---: | --- |
| run_01 | 4.921400 | 2.577100 | 1.910000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_02 | 4.964800 | 2.562200 | 1.940000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_03 | 4.967600 | 2.578400 | 1.930000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_04 | 4.948800 | 2.560800 | 1.930000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_05 | 4.917900 | 2.563500 | 1.920000 | std=13446965672972001487;rvv=13446965672972001487 |
