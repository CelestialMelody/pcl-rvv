/*
 * 本文件做什么：
 * transformation_estimation_point_to_plane_lls_weighted 的确定性测试输入、点型转换和
 * row source fixture。它只构造 test-rvv/bench 证据输入，不证明 production dispatch。
 */

#pragma once

#include <pcl/common/transforms.h>
#include <pcl/correspondence.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/register_point_struct.h>
#include <pcl/types.h>

#include <Eigen/Core>

#include <cmath>
#include <cstddef>
#include <vector>

struct TEPTPLWDoubleNormalTarget {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  union {
    struct {
      double normal_x;
      double normal_y;
      double normal_z;
    };
    double normal[3];
  };
};

POINT_CLOUD_REGISTER_POINT_STRUCT(TEPTPLWDoubleNormalTarget,
                                  (float, x, x)
                                  (float, y, y)
                                  (float, z, z)
                                  (double, normal_x, normal_x)
                                  (double, normal_y, normal_y)
                                  (double, normal_z, normal_z))

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag {

inline pcl::PointCloud<pcl::PointNormal>
make_surface_cloud(const int grid_radius, const float step)
{
  // 生成非平面的解析曲面；法线来自解析偏导，避免 6x6 normal-equation 退化。
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.height = 1;
  cloud.is_dense = true;
  for (int ix = -grid_radius; ix <= grid_radius; ++ix) {
    for (int iy = -grid_radius; iy <= grid_radius; ++iy) {
      const float x = static_cast<float>(ix) * step;
      const float y = static_cast<float>(iy) * step;
      pcl::PointNormal point;
      point.x = x;
      point.y = y;
      point.z = 0.11f * x * x + 0.17f * x * y - 0.24f * y + 0.9f;
      point.normal_x = -0.22f * x - 0.17f * y;
      point.normal_y = -0.17f * x + 0.24f;
      point.normal_z = 1.0f;
      const float norm = std::sqrt(point.normal_x * point.normal_x +
                                   point.normal_y * point.normal_y +
                                   point.normal_z * point.normal_z);
      point.normal_x /= norm;
      point.normal_y /= norm;
      point.normal_z /= norm;
      cloud.push_back(point);
    }
  }
  cloud.width = cloud.size();
  return cloud;
}

inline pcl::PointCloud<pcl::PointNormal>
make_bench_cloud_with_at_least(const std::size_t target_size)
{
  // bench 输入固定为解析曲面，避免随机数让 std/RVV checksum（校验和）不可复现。
  const int radius = static_cast<int>(std::ceil(std::sqrt(target_size) / 2.0));
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.reserve(static_cast<std::size_t>((2 * radius + 1) * (2 * radius + 1)));
  for (int ix = -radius; ix <= radius; ++ix) {
    for (int iy = -radius; iy <= radius; ++iy) {
      const float x = static_cast<float>(ix) * 0.025f;
      const float y = static_cast<float>(iy) * 0.025f;
      pcl::PointNormal point;
      point.x = x;
      point.y = y;
      point.z = 0.09f * x * x + 0.14f * x * y - 0.19f * y + 0.8f;
      point.normal_x = -0.18f * x - 0.14f * y;
      point.normal_y = -0.14f * x + 0.19f;
      point.normal_z = 1.0f;
      const float norm = std::sqrt(point.normal_x * point.normal_x +
                                   point.normal_y * point.normal_y +
                                   point.normal_z * point.normal_z);
      point.normal_x /= norm;
      point.normal_y /= norm;
      point.normal_z /= norm;
      cloud.push_back(point);
      if (cloud.size() == target_size) {
        cloud.width = cloud.size();
        return cloud;
      }
    }
  }
  cloud.width = cloud.size();
  return cloud;
}

inline Eigen::Matrix4f
make_transform()
{
  // 温和刚体变换让测试聚焦 normal-equation 构造，而不是大角度线性化误差。
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  transform.row(0) << 0.9938f, 0.0988f, 0.0517f, 0.1000f;
  transform.row(1) << -0.0997f, 0.9949f, 0.0149f, -0.2000f;
  transform.row(2) << -0.0500f, -0.0200f, 0.9986f, 0.3000f;
  transform.row(3) << 0.0000f, 0.0000f, 0.0000f, 1.0000f;
  return transform;
}

inline pcl::PointCloud<pcl::PointNormal>
make_target_cloud(const pcl::PointCloud<pcl::PointNormal>& source)
{
  pcl::PointCloud<pcl::PointNormal> target;
  pcl::transformPointCloudWithNormals(source, target, make_transform());
  return target;
}

inline std::vector<float>
make_weights(const std::size_t n)
{
  // 确定性周期权重覆盖小于、等于和大于 1 的 normal 缩放，不依赖随机数。
  std::vector<float> weights;
  weights.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    weights.push_back(0.55f + 0.07f * static_cast<float>(i % 9));
  return weights;
}

inline pcl::Correspondences
make_weighted_correspondences(const std::size_t n)
{
  // 乱序、重复和不同 weight 值共同覆盖 correspondences gather path。
  pcl::Correspondences correspondences;
  correspondences.reserve(n + n / 13);
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>(i), 0.60f + 0.03f * (i % 7));
  for (std::size_t i = 1; i < n; i += 5)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>(i), 0.75f + 0.02f * (i % 5));
  for (std::size_t i = 11; i < n; i += 41)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 1.15f);
  return correspondences;
}

inline pcl::Correspondences
make_bench_correspondences(const std::size_t n)
{
  // 这个确定性子集专门给 gather bench 使用，暴露 index/weight 展开成本。
  pcl::Correspondences correspondences;
  correspondences.reserve(n);
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>(i), 0.60f + 0.03f * (i % 7));
  for (std::size_t i = 3; i < n; i += 5)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>(i), 0.75f + 0.02f * (i % 5));
  return correspondences;
}

inline pcl::PointCloud<pcl::PointXYZ>
copy_source_as_xyz(const pcl::PointCloud<pcl::PointNormal>& source)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.reserve(source.size());
  cloud.height = source.height;
  cloud.is_dense = source.is_dense;
  for (const auto& point : source) {
    pcl::PointXYZ copied;
    copied.x = point.x;
    copied.y = point.y;
    copied.z = point.z;
    cloud.push_back(copied);
  }
  cloud.width = cloud.size();
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZINormal>
copy_target_as_xyzinormal(const pcl::PointCloud<pcl::PointNormal>& target)
{
  pcl::PointCloud<pcl::PointXYZINormal> cloud;
  cloud.reserve(target.size());
  cloud.height = target.height;
  cloud.is_dense = target.is_dense;
  for (const auto& point : target) {
    pcl::PointXYZINormal copied;
    copied.x = point.x;
    copied.y = point.y;
    copied.z = point.z;
    copied.normal_x = point.normal_x;
    copied.normal_y = point.normal_y;
    copied.normal_z = point.normal_z;
    copied.intensity = 0.5f;
    copied.curvature = 0.0f;
    cloud.push_back(copied);
  }
  cloud.width = cloud.size();
  return cloud;
}

inline pcl::PointCloud<TEPTPLWDoubleNormalTarget>
copy_target_as_double_normal(const pcl::PointCloud<pcl::PointNormal>& target)
{
  // 这个点型已注册 PCL fields，但 normal 是 double；f32 normal layout gate 应回退。
  pcl::PointCloud<TEPTPLWDoubleNormalTarget> cloud;
  cloud.reserve(target.size());
  cloud.height = target.height;
  cloud.is_dense = target.is_dense;
  for (const auto& point : target) {
    TEPTPLWDoubleNormalTarget copied;
    copied.x = point.x;
    copied.y = point.y;
    copied.z = point.z;
    copied.normal_x = point.normal_x;
    copied.normal_y = point.normal_y;
    copied.normal_z = point.normal_z;
    copied.normal[0] = static_cast<double>(point.normal_x);
    copied.normal[1] = static_cast<double>(point.normal_y);
    copied.normal[2] = static_cast<double>(point.normal_z);
    cloud.push_back(copied);
  }
  cloud.width = cloud.size();
  return cloud;
}

inline pcl::Indices
make_source_indices(const std::size_t n)
{
  // 只生成有效索引，覆盖乱序和重复 row。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    indices.push_back(static_cast<int>((i * 37 + 11) % n));
  return indices;
}

inline pcl::Indices
make_target_indices(const std::size_t n)
{
  // target 侧使用另一条 index stream，避免 dual-indices 退化成 source-indexed。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    indices.push_back(static_cast<int>((i * 19 + 5) % n));
  return indices;
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag
