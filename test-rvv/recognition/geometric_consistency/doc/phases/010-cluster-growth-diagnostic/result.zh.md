# Phase 010 Result: cluster-growth-diagnostic

## 执行范围

本阶段复刻了 `clusterCorrespondences()` 的 outer consensus growth loop，但仍把
production `recognize()` 之外的排序、RANSAC rejector 和 transformation 输出留在外侧。
当前实现和测试支撑都仍以 `PointXYZ / float / AoS` 为主，不尝试扩展到其它 row source
policy 或点类型。

## 当前结论

Phase 010 的 diagnostic helper 已完成，且 repeated board 结果为正向：
median `2.530x`，min `2.490x`，max `2.650x`，`B/A < 1` 为 `0/5`，
checksum 稳定。

相关证据：

- summary：`test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/summary.md`
- manifest：`test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/evidence_manifest.json`
- Evidence Doctor：`test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/evidence_doctor.md`

生产直连正确性也已补上：

- `make -C test-rvv/recognition/geometric_consistency run_upstream_test_compare`

该 upstream test 直接覆盖 `test/recognition/test_recognition_cg.cpp` 中的
`GeometricConsistencyGrouping<PointType, PointType>::recognize()`，Std/RVV 两侧均通过。
因此当前 production patch 已具备可采纳的 correctness 证据；后续若要继续推进，
应进入新的 production board / public-entry phase，而不是回写 Phase 010 的 diagnostic 边界。
