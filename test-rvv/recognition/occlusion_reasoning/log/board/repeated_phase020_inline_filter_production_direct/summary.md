# Occlusion reasoning public inline filter production-direct repeated board summary

- run_label: `occlusion_reasoning_phase020_inline_filter_production_direct_repeated`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production-public`
- A/B boundary: `public_entry`
- timer_boundary: `production_public_inline_filter_includes_copyPointCloud_excludes_scene_model_setup`
- decision_bucket: `positive`
- median_speedup: `1.250x`
- min_speedup: `1.240x`
- max_speedup: `1.260x`
- p10_speedup: `1.244x`
- p90_speedup: `1.256x`
- B/A < 1: `0/5`

| run | std ms | rvv ms | B/A | checksum |
| --- | ---: | ---: | ---: | --- |
| run_01 | 6.080300 | 4.834300 | 1.260000 | std=15352718410989236219;rvv=15352718410989236219 |
| run_02 | 6.029800 | 4.828900 | 1.250000 | std=15352718410989236219;rvv=15352718410989236219 |
| run_03 | 6.028700 | 4.833900 | 1.250000 | std=15352718410989236219;rvv=15352718410989236219 |
| run_04 | 6.009900 | 4.859200 | 1.240000 | std=15352718410989236219;rvv=15352718410989236219 |
| run_05 | 6.010800 | 4.801900 | 1.250000 | std=15352718410989236219;rvv=15352718410989236219 |
