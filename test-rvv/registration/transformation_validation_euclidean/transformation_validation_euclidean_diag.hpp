#pragma once

// 本文件做什么：
// 这里的 helper 只服务 transformation_validation_euclidean 的 test-rvv 诊断。
// 它把 production 标量入口拆成 transform staging（变换暂存）、KdTree setup
//（搜索树构建）、nearestKSearch（最近邻查询）和 score tail（分数累加尾段），
// 方便测试和 bench 分别证明“局部 RVV staging 成立”和“完整入口是否被搜索成本稀释”。
// 这些函数不会修改 production dispatch（生产分流），也不能单独证明 production direct。

#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_store.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <Eigen/Core>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace pcl::registration::transformation_validation_euclidean_diag {

using Matrix4f = Eigen::Matrix4f;
using KdTree = pcl::search::KdTree<pcl::PointXYZ>;

struct ValidationResult {
  double score{std::numeric_limits<double>::max()};
  double distance_sum{0.0};
  int accepted_points{0};
};

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
// RVV 诊断链路只覆盖 PointXYZ AoS（结构数组）里的 x/y/z 字段。
// 它复刻 production 入口前置 4x4 affine transform（仿射变换）公式，
// 后续 KdTree 查询和 score 累加仍由标量 helper 执行。
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

inline void
setupTargetTree(KdTree& tree, const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target)
{
  tree.setInputCloud(target);
}

// search-only 负向对照使用这段 helper：输入已经完成 transform staging，
// 计时内只保留 nearestKSearch 和 max_range 过滤。若 std/RVV 构建在这个
// case 上接近 1x，说明 RVV staging 并没有覆盖入口主成本。
inline ValidationResult
scoreTransformedWithTree(const pcl::PointCloud<pcl::PointXYZ>& transformed,
                         KdTree& tree,
                         double max_range)
{
  pcl::Indices nn_indices(1);
  std::vector<float> nn_dists(1);
  ValidationResult result;
  for (const auto& point : transformed) {
    tree.nearestKSearch(point, 1, nn_indices, nn_dists);
    if (nn_dists[0] > max_range)
      continue;
    result.distance_sum += nn_dists[0];
    ++result.accepted_points;
  }

  if (result.accepted_points > 0)
    result.score = result.distance_sum / result.accepted_points;
  return result;
}

inline ValidationResult
validateTransformationStdWithTree(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& src,
                                  KdTree& tree,
                                  const Matrix4f& transformation,
                                  double max_range)
{
  pcl::PointCloud<pcl::PointXYZ> transformed;
  transformPointXYZStd(*src, transformed, transformation);
  return scoreTransformedWithTree(transformed, tree, max_range);
}

inline ValidationResult
validateTransformationCandidateWithTree(
    const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& src,
    KdTree& tree,
    const Matrix4f& transformation,
    double max_range)
{
  pcl::PointCloud<pcl::PointXYZ> transformed;
  transformPointXYZCandidate(*src, transformed, transformation);
  return scoreTransformedWithTree(transformed, tree, max_range);
}

// fresh-tree 形态对应 production 默认行为：每次 validation 设置 target tree。
// prebuilt-tree 形态对应 setSearchMethodTarget(..., force_no_recompute=true)
// 这类 tree reuse 场景：计时内跳过 tree setup，但仍保留 transform staging 和 search tail。
inline ValidationResult
validateTransformationStdDetailed(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& src,
                                  const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                                  const Matrix4f& transformation,
                                  double max_range)
{
  KdTree tree;
  setupTargetTree(tree, target);
  return validateTransformationStdWithTree(src, tree, transformation, max_range);
}

inline ValidationResult
validateTransformationCandidateDetailed(
    const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& src,
    const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
    const Matrix4f& transformation,
    double max_range)
{
  // Full diagnostic keeps the original KdTree search scalar. This measures
  // whether RVV transform staging survives the search cost that dominates the
  // production validateTransformation entry.
  KdTree tree;
  setupTargetTree(tree, target);
  return validateTransformationCandidateWithTree(src, tree, transformation, max_range);
}

inline double
validateTransformationStd(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& src,
                          const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                          const Matrix4f& transformation,
                          double max_range)
{
  return validateTransformationStdDetailed(src, target, transformation, max_range).score;
}

inline double
validateTransformationCandidate(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& src,
                                const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                                const Matrix4f& transformation,
                                double max_range)
{
  return validateTransformationCandidateDetailed(src, target, transformation, max_range)
      .score;
}

} // namespace pcl::registration::transformation_validation_euclidean_diag
