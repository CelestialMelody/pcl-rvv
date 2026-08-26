# Moment invariants accumulation diagnostic repeated board summary

- run_label: `moment_invariants_accumulation_diagnostic_phase000`
- expected_runs: `5`
- collected_runs: `5`
- evidence_role: `diagnostic`
- A/B boundary: `test helper`
- timer_boundary: `helper_only_moment_accumulation_after_centroid`
- decision_bucket: `weak_positive`

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `mi_accumulation_indexed,points=262144` | 5 | 1.141x | 1.095x | 1.206x | 1.101x | 1.189x | 0/5 | 1.111x, 1.206x, 1.141x, 1.164x, 1.095x |

## 每轮明细

| run | Std us/iter | RVV us/iter | speedup | numerical gate | raw checksum note |
| --- | ---: | ---: | ---: | --- | --- |
| `run-01` | 9313.7542 | 8385.4292 | 1.111x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-02` | 9231.6056 | 7657.0264 | 1.206x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-03` | 9285.3375 | 8137.2722 | 1.141x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-04` | 9254.2403 | 7948.2292 | 1.164x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |
| `run-05` | 9247.6361 | 8448.8319 | 1.095x | QEMU gtest tolerance passed | differs as expected for non-associative float reduction |

## Evidence boundary

这是未接 production 的 diagnostic（诊断）性能证据。它证明 test helper boundary（测试 helper 边界）下中心矩累加的板卡性能信号，不能替代真实 `MomentInvariantsEstimation::computeFeature` production direct（真实生产路径）证据。

Raw checksum（原始校验值）量化整份浮点摘要。Std/RVV 规约顺序不同会导致该值不同；语义等价性由 `run_test_compare` 的 numerical tolerance gate（数值容差验收）证明，raw checksum 不作为 strict equality gate（严格相等验收）。
