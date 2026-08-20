/*
 * 本文件做什么：
 * 本文件负责 TE2D 测试支撑的共享类型和公共依赖。
 *
 * 它只服务 test-rvv 诊断、正确性和 bench harness，不能证明 production dispatch。
 */

#pragma once

#include <pcl/common/point_tests.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/rvv_point_traits.h>
#include <pcl/registration/transformation_estimation_2D.h>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#ifdef __RVV10__
#include <pcl/rvv_point_load.h>
#include <riscv_vector.h>
#endif

namespace pcl::registration::rvv_te2d_support {

struct CandidateStats {
  bool used_rvv{false};
  bool used_fallback{false};
  bool layout_supported{false};
  bool source_layout_supported{false};
  bool target_layout_supported{false};
  bool dense_finite_input{false};
  std::size_t input_points{0};
  std::size_t accepted_points{0};
  std::size_t source_finite_points{0};
  std::size_t target_finite_points{0};
};

struct Fused2DAccumulation {
  float source_centroid[2]{0.0f, 0.0f};
  float target_centroid[2]{0.0f, 0.0f};
  float correlation[4]{0.0f, 0.0f, 0.0f, 0.0f};
  std::size_t count{0};
};

} // namespace pcl::registration::rvv_te2d_support
