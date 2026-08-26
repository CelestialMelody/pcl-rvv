# Moment invariants production computeFeature repeated board summary

- run_label: `moment_invariants_production_compute_feature_phase030`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production-public`
- A/B boundary: `public overload`
- timer_boundary: `public_compute_feature_with_kdtree_search_and_output_write`
- decision_bucket: `weak_positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `mi_production_compute_feature,points=4096` | 5 | 1.067x | 1.023x | 1.085x | 1.039x | 1.080x | 0/5 | 1.072x, 1.023x, 1.085x, 1.062x, 1.067x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | numerical gate | raw checksum note |
| --- | ---: | ---: | ---: | --- | --- |
| `run-01` | 8159.7500 | 7614.4428 | 1.072x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-02` | 8057.9740 | 7873.4322 | 1.023x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-03` | 8235.9844 | 7591.0364 | 1.085x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-04` | 8122.1562 | 7644.9531 | 1.062x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-05` | 8035.0625 | 7529.1510 | 1.067x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |

## Evidence boundary

这是 production-public（生产公开入口）性能证据。Std/RVV 两侧都调用真实 `MomentInvariantsEstimation<PointXYZ, MomentInvariants>::compute`，计时边界包含 KdTree nearestKSearch（近邻搜索）、indexed moment accumulation（索引矩累加）和 output write（输出写回），不包含点云构造或 KdTree 初始化。

Raw checksum（原始校验值）量化整份浮点摘要。Std/RVV 规约顺序不同会导致该值不同；语义等价性由 `run_test_compare` 的 numerical tolerance gate（数值容差验收）证明，raw checksum 不作为 strict equality gate（严格相等验收）。
