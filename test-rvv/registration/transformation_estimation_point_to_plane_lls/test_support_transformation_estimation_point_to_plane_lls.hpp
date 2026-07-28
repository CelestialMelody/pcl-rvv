/*
 * 本文件做什么：
 * 这是 transformation_estimation_point_to_plane_lls 主题的 test-rvv support
 * aggregator header（聚合头文件）。外部测试和 bench include 这个稳定入口；具体
 * helper 按 common、RVV math、row sources、reductions 和 candidates 拆到
 * `test_support/` 子目录中的内部头文件。这个子目录是测试证据支撑层，包含 reference、
 * diagnostic/probing、ablation 和 bench-facing helper，不属于 production dispatch。
 *
 * 证据边界：
 * 这些头文件只服务 test-rvv。它们不能单独证明 production dispatch，也不能把当前
 * full-cloud f32 AoS layout-gated production implementation 扩展到 indexed、
 * correspondences、weighted、Scalar=double，或升级成泛型点型性能结论。
 */

#pragma once

#include "test_support/transformation_estimation_point_to_plane_lls_common.hpp"
#include "test_support/transformation_estimation_point_to_plane_lls_rvv_math.hpp"
#include "test_support/transformation_estimation_point_to_plane_lls_row_sources.hpp"
#include "test_support/transformation_estimation_point_to_plane_lls_reductions.hpp"
#include "test_support/transformation_estimation_point_to_plane_lls_candidates.hpp"
