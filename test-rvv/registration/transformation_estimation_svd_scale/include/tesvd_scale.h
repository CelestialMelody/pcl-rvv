/*
 * 本文件做什么：
 * 这是 transformation_estimation_svd_scale 的 test-rvv support（RVV 测试支撑）
 * 聚合入口。`tesvd_scale` 是 transformation_estimation_svd_scale 的短标识，供
 * correctness（正确性）测试和 bench（性能测试）使用稳定 include 路径。
 *
 * 证据边界：
 * 本聚合入口只属于 test-rvv 资产；它不修改 production dispatch（生产分流）。
 * 其中 `include/impl` helper 仍是 test-only diagnostic（测试专用诊断），`src/test_*`
 * 和 `src/bench_*` 还会验证 Phase 010 的 production direct（真实生产路径）证据。
 * ordered-cloud-pair（顺序点云对）的正向结果不能外推到 indices（索引路径）、
 * correspondences（对应关系路径）或 `Scalar=double`。
 */

#pragma once

#include "impl/tesvd_scale_candidates.hpp"
