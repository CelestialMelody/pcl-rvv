# Moment invariants public-search-shaped diagnostic repeated board summary

- run_label: `moment_invariants_public_search_shape_diagnostic_phase010`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production_shaped_diagnostic`
- A/B boundary: `production-shaped helper`
- timer_boundary: `kd_tree_search_plus_moment_accumulation_helper_replacement`
- decision_bucket: `weak_positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `mi_public_search_shape,points=4096` | 5 | 1.031x | 1.026x | 1.037x | 1.026x | 1.036x | 0/5 | 1.031x, 1.027x, 1.034x, 1.037x, 1.026x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | numerical gate | raw checksum note |
| --- | ---: | ---: | ---: | --- | --- |
| `run-01` | 10995.1771 | 10664.1094 | 1.031x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-02` | 10869.7916 | 10582.6145 | 1.027x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-03` | 10987.4062 | 10621.4947 | 1.034x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-04` | 11012.1146 | 10622.3699 | 1.037x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-05` | 10945.9322 | 10672.9480 | 1.026x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |

## Evidence boundary

这是未接 production 的 production-shaped diagnostic（生产形态诊断）性能证据。它把真实 `pcl::search::KdTree<PointXYZ>` nearestKSearch（近邻搜索）外层和测试专用 Std/RVV helper replacement（helper 替换）放进同一计时边界，仍不能替代 `MomentInvariantsEstimation::computeFeature` production direct（真实生产路径）证据。

Raw checksum（原始校验值）量化整份浮点摘要。Std/RVV 规约顺序不同会导致该值不同；语义等价性由 `run_test_compare` 的 numerical tolerance gate（数值容差验收）证明，raw checksum 不作为 strict equality gate（严格相等验收）。
