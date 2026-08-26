# Moment invariants production PointXYZI computeFeature repeated board summary

- run_label: `moment_invariants_production_pointxyzi_compute_feature_phase040`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production-public`
- A/B boundary: `public overload`
- timer_boundary: `public_compute_feature_with_kdtree_search_and_output_write`
- decision_bucket: `weak_positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `mi_production_compute_feature_pointxyzi,points=4096` | 5 | 1.071x | 1.065x | 1.075x | 1.066x | 1.074x | 0/5 | 1.071x, 1.065x, 1.068x, 1.075x, 1.073x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | numerical gate | raw checksum note |
| --- | ---: | ---: | ---: | --- | --- |
| `run-01` | 8223.0886 | 7675.4324 | 1.071x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-02` | 8164.2760 | 7662.7969 | 1.065x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-03` | 8147.8332 | 7629.1511 | 1.068x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-04` | 8119.4480 | 7553.5520 | 1.075x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-05` | 8099.7240 | 7547.8594 | 1.073x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |

## Evidence boundary

这是 production-public（生产公开入口）性能证据。Std/RVV 两侧都调用真实 `MomentInvariantsEstimation<PointXYZI, MomentInvariants>::compute`，计时边界包含 KdTree nearestKSearch（近邻搜索）、indexed moment accumulation（索引矩累加）和 output write（输出写回），不包含点云构造或 KdTree 初始化。

Raw checksum（原始校验值）量化整份浮点摘要。Std/RVV 规约顺序不同会导致该值不同；语义等价性由 `run_test_compare` 的 numerical tolerance gate（数值容差验收）证明，raw checksum 不作为 strict equality gate（严格相等验收）。
