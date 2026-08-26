# Moment invariants production PointXYZRGBA computeFeature repeated board summary

- run_label: `moment_invariants_production_pointxyzrgba_compute_feature_phase040`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `production-public`
- A/B boundary: `public overload`
- timer_boundary: `public_compute_feature_with_kdtree_search_and_output_write`
- decision_bucket: `weak_positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `mi_production_compute_feature_pointxyzrgba,points=4096` | 5 | 1.078x | 1.059x | 1.087x | 1.064x | 1.085x | 0/5 | 1.083x, 1.059x, 1.072x, 1.078x, 1.087x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | numerical gate | raw checksum note |
| --- | ---: | ---: | ---: | --- | --- |
| `run-01` | 8201.4688 | 7572.4428 | 1.083x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-02` | 8205.7604 | 7749.8438 | 1.059x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-03` | 8252.5886 | 7701.6199 | 1.072x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-04` | 8238.3803 | 7642.4844 | 1.078x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-05` | 8236.2344 | 7576.5938 | 1.087x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |

## Evidence boundary

这是 production-public（生产公开入口）性能证据。Std/RVV 两侧都调用真实 `MomentInvariantsEstimation<PointXYZRGBA, MomentInvariants>::compute`，计时边界包含 KdTree nearestKSearch（近邻搜索）、indexed moment accumulation（索引矩累加）和 output write（输出写回），不包含点云构造或 KdTree 初始化。

Raw checksum（原始校验值）量化整份浮点摘要。Std/RVV 规约顺序不同会导致该值不同；语义等价性由 `run_test_compare` 的 numerical tolerance gate（数值容差验收）证明，raw checksum 不作为 strict equality gate（严格相等验收）。
