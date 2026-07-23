#pragma once

#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_store.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <Eigen/Core>

#include <cstddef>
#include <limits>
#include <vector>

namespace pcl::registration::transformation_validation_euclidean_diag {

using Matrix4f = Eigen::Matrix4f;

inline void
transformPointXYZStd(const pcl::PointCloud<pcl::PointXYZ>& src,
                     pcl::PointCloud<pcl::PointXYZ>& dst,
                     const Matrix4f& transformation)
{
  dst.resize(src.size());
  dst.width = src.width;
  dst.height = src.height;
  dst.is_dense = src.is_dense;

  for (std::size_t i = 0; i < src.size(); ++i) {
    const auto& p = src[i];
    auto& q = dst[i];
    q.x = transformation(0, 0) * p.x + transformation(0, 1) * p.y +
          transformation(0, 2) * p.z + transformation(0, 3);
    q.y = transformation(1, 0) * p.x + transformation(1, 1) * p.y +
          transformation(1, 2) * p.z + transformation(1, 3);
    q.z = transformation(2, 0) * p.x + transformation(2, 1) * p.y +
          transformation(2, 2) * p.z + transformation(2, 3);
  }
}

#if defined(__RVV10__)
inline void
transformPointXYZRVV(const pcl::PointCloud<pcl::PointXYZ>& src,
                     pcl::PointCloud<pcl::PointXYZ>& dst,
                     const Matrix4f& transformation)
{
  constexpr std::size_t kStride = sizeof(pcl::PointXYZ);
  constexpr std::size_t kXOff = offsetof(pcl::PointXYZ, x);
  constexpr std::size_t kYOff = offsetof(pcl::PointXYZ, y);
  constexpr std::size_t kZOff = offsetof(pcl::PointXYZ, z);

  dst.resize(src.size());
  dst.width = src.width;
  dst.height = src.height;
  dst.is_dense = src.is_dense;

  const auto* src_base = reinterpret_cast<const std::uint8_t*>(src.points.data());
  const auto* dst_base = reinterpret_cast<const std::uint8_t*>(dst.points.data());
  std::size_t i = 0;
  while (i < src.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(src.size() - i);
    vfloat32m2_t x;
    vfloat32m2_t y;
    vfloat32m2_t z;

    // Diagnostic-only staging: source/target are PointXYZ AoS, so the common
    // strided xyz wrapper selects segment or field loads without adding new
    // production semantics.
    pcl::rvv_load::strided_load3_f32m2<kStride, kXOff, kYOff, kZOff>(
        src_base + i * kStride, vl, x, y, z);

    vfloat32m2_t tx = __riscv_vfmul_vf_f32m2(x, transformation(0, 0), vl);
    tx = __riscv_vfmacc_vf_f32m2(tx, transformation(0, 1), y, vl);
    tx = __riscv_vfmacc_vf_f32m2(tx, transformation(0, 2), z, vl);
    tx = __riscv_vfadd_vf_f32m2(tx, transformation(0, 3), vl);

    vfloat32m2_t ty = __riscv_vfmul_vf_f32m2(x, transformation(1, 0), vl);
    ty = __riscv_vfmacc_vf_f32m2(ty, transformation(1, 1), y, vl);
    ty = __riscv_vfmacc_vf_f32m2(ty, transformation(1, 2), z, vl);
    ty = __riscv_vfadd_vf_f32m2(ty, transformation(1, 3), vl);

    vfloat32m2_t tz = __riscv_vfmul_vf_f32m2(x, transformation(2, 0), vl);
    tz = __riscv_vfmacc_vf_f32m2(tz, transformation(2, 1), y, vl);
    tz = __riscv_vfmacc_vf_f32m2(tz, transformation(2, 2), z, vl);
    tz = __riscv_vfadd_vf_f32m2(tz, transformation(2, 3), vl);

    pcl::rvv_store::strided_store3_f32m2<kStride, kXOff, kYOff, kZOff>(
        dst_base + i * kStride, vl, tx, ty, tz);
    i += vl;
  }
}
#endif

inline void
transformPointXYZCandidate(const pcl::PointCloud<pcl::PointXYZ>& src,
                           pcl::PointCloud<pcl::PointXYZ>& dst,
                           const Matrix4f& transformation)
{
#if defined(__RVV10__)
  if (src.size() >= 64) {
    transformPointXYZRVV(src, dst, transformation);
    return;
  }
#endif
  transformPointXYZStd(src, dst, transformation);
}

inline double
validateTransformationStd(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& src,
                          const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                          const Matrix4f& transformation,
                          double max_range)
{
  pcl::PointCloud<pcl::PointXYZ> transformed;
  transformPointXYZStd(*src, transformed, transformation);

  pcl::search::KdTree<pcl::PointXYZ> tree;
  tree.setInputCloud(target);

  pcl::Indices nn_indices(1);
  std::vector<float> nn_dists(1);
  double fitness_score = 0.0;
  int nr = 0;
  for (const auto& point : transformed) {
    tree.nearestKSearch(point, 1, nn_indices, nn_dists);
    if (nn_dists[0] > max_range)
      continue;
    fitness_score += nn_dists[0];
    ++nr;
  }

  if (nr > 0)
    return fitness_score / nr;
  return std::numeric_limits<double>::max();
}

inline double
validateTransformationCandidate(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& src,
                                const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                                const Matrix4f& transformation,
                                double max_range)
{
  pcl::PointCloud<pcl::PointXYZ> transformed;
  transformPointXYZCandidate(*src, transformed, transformation);

  // Full diagnostic keeps the original KdTree search scalar. This measures
  // whether RVV transform staging survives the search cost that dominates the
  // production validateTransformation entry.
  pcl::search::KdTree<pcl::PointXYZ> tree;
  tree.setInputCloud(target);

  pcl::Indices nn_indices(1);
  std::vector<float> nn_dists(1);
  double fitness_score = 0.0;
  int nr = 0;
  for (const auto& point : transformed) {
    tree.nearestKSearch(point, 1, nn_indices, nn_dists);
    if (nn_dists[0] > max_range)
      continue;
    fitness_score += nn_dists[0];
    ++nr;
  }

  if (nr > 0)
    return fitness_score / nr;
  return std::numeric_limits<double>::max();
}

} // namespace pcl::registration::transformation_validation_euclidean_diag
