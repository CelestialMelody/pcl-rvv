# Occlusion reasoning ZBuffering production-direct repeated board summary

- run_label: `occlusion_reasoning_phase010_production_direct_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production-public`
- A/B boundary: `public_entry`
- timer_boundary: `production_public_filter_indices_after_computeDepthMap_setup_excludes_copyPointCloud`
- decision_bucket: `positive`
- median_speedup: `1.270x`
- min_speedup: `1.260x`
- max_speedup: `1.270x`
- p10_speedup: `1.264x`
- p90_speedup: `1.270x`
- B/A < 1: `0/5`

| run | std ms | rvv ms | B/A | checksum |
| --- | ---: | ---: | ---: | --- |
| run_01 | 3.271000 | 2.567900 | 1.270000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_02 | 3.268900 | 2.568900 | 1.270000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_03 | 3.275200 | 2.577100 | 1.270000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_04 | 3.296100 | 2.606300 | 1.260000 | std=13446965672972001487;rvv=13446965672972001487 |
| run_05 | 3.295300 | 2.589600 | 1.270000 | std=13446965672972001487;rvv=13446965672972001487 |
