# Moment invariants production PointXYZRGB computeFeature repeated board summary

- run_label: `moment_invariants_production_pointxyzrgb_compute_feature_phase040`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production-public`
- A/B boundary: `public overload`
- timer_boundary: `public_compute_feature_with_kdtree_search_and_output_write`
- decision_bucket: `weak_positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `mi_production_compute_feature_pointxyzrgb,points=4096` | 5 | 1.078x | 1.066x | 1.091x | 1.067x | 1.087x | 0/5 | 1.091x, 1.078x, 1.080x, 1.067x, 1.066x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | numerical gate | raw checksum note |
| --- | ---: | ---: | ---: | --- | --- |
| `run-01` | 8294.0990 | 7600.9062 | 1.091x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-02` | 8239.0781 | 7639.9271 | 1.078x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-03` | 8260.2969 | 7649.9375 | 1.080x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-04` | 8220.8855 | 7702.1824 | 1.067x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-05` | 8241.9062 | 7732.0312 | 1.066x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |

## Evidence boundary

这是 production-public（生产公开入口）性能证据。Std/RVV 两侧都调用真实 `MomentInvariantsEstimation<PointXYZRGB, MomentInvariants>::compute`，计时边界包含 KdTree nearestKSearch（近邻搜索）、indexed moment accumulation（索引矩累加）和 output write（输出写回），不包含点云构造或 KdTree 初始化。

Raw checksum（原始校验值）量化整份浮点摘要。Std/RVV 规约顺序不同会导致该值不同；语义等价性由 `run_test_compare` 的 numerical tolerance gate（数值容差验收）证明，raw checksum 不作为 strict equality gate（严格相等验收）。
