#pragma once

/*
 * 本文件做什么：
 * 这里保存 PFH（Point Feature Histogram，点特征直方图）topic 的 test-only
 * scalar reference（测试专用标量参考链路）。reference 复刻
 * `features/src/pfh.cpp::computePairFeatures` 和 `pfh.hpp::computePointPFHSignature`
 * 的 pair order（成对顺序）、bin clamp（直方图区间夹取）和 histogram scatter
 * （直方图离散累加）语义，用来给后续 RVV candidate（候选实现）做 same-chain
 * （同构链路）对拍。
 *
 * 证据边界：
 * 这些 helper 只证明局部 PFH histogram 语义正确。它们不覆盖真实 neighbor search
 * （邻域搜索）成本、不证明 production dispatch（生产分流），也不把诊断性能外推为
 * production evidence（生产证据）。
 */

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>
#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <utility>

namespace pcl::features::rvv_test::pfh
{
using PointT = pcl::PointNormal;
using CloudT = pcl::PointCloud<PointT>;
using XYZCloudT = pcl::PointCloud<pcl::PointXYZ>;
using NormalCloudT = pcl::PointCloud<pcl::Normal>;

struct PairTuple
{
  float f1 = 0.0f;
  float f2 = 0.0f;
  float f3 = 0.0f;
  float f4 = 0.0f;
};

inline CloudT::Ptr
makeFeatureCloud(const int side)
{
  auto cloud = CloudT::Ptr(new CloudT);
  cloud->reserve(static_cast<std::size_t>(side * side));

  for (int y = 0; y < side; ++y)
  {
    for (int x = 0; x < side; ++x)
    {
      const float fx = static_cast<float>(x) * 0.045f;
      const float fy = static_cast<float>(y) * 0.040f;
      const float wave = 0.015f * std::sin(0.31f * fx) + 0.010f * std::cos(0.23f * fy);

      PointT p;
      p.x = fx;
      p.y = fy;
      p.z = 0.22f * fx - 0.16f * fy + wave;

      Eigen::Vector3f normal(-0.22f + 0.00465f * std::cos(0.31f * fx),
                             0.16f + 0.00230f * std::sin(0.23f * fy),
                             1.0f);
      normal.normalize();
      p.normal_x = normal.x();
      p.normal_y = normal.y();
      p.normal_z = normal.z();
      p.curvature = 0.0f;
      cloud->push_back(p);
    }
  }

  cloud->width = static_cast<std::uint32_t>(cloud->size());
  cloud->height = 1;
  cloud->is_dense = true;
  return cloud;
}

inline pcl::Indices
makeSequentialIndices(const std::size_t size)
{
  pcl::Indices indices(size);
  std::iota(indices.begin(), indices.end(), 0);
  return indices;
}

inline pcl::Indices
makeWrappedNeighborhood(const CloudT& cloud, const int center, const int count)
{
  pcl::Indices indices;
  indices.reserve(static_cast<std::size_t>(count));
  const int cloud_size = static_cast<int>(cloud.size());
  const int half = count / 2;
  for (int i = 0; i < count; ++i)
  {
    const int raw = center + i - half;
    const int wrapped = ((raw % cloud_size) + cloud_size) % cloud_size;
    indices.push_back(static_cast<pcl::index_t>(wrapped));
  }
  return indices;
}

inline std::pair<XYZCloudT::Ptr, NormalCloudT::Ptr>
makeXYZAndNormalClouds(const int side)
{
  const auto feature_cloud = makeFeatureCloud(side);
  auto xyz_cloud = XYZCloudT::Ptr(new XYZCloudT);
  auto normal_cloud = NormalCloudT::Ptr(new NormalCloudT);
  xyz_cloud->reserve(feature_cloud->size());
  normal_cloud->reserve(feature_cloud->size());

  for (const auto& point : *feature_cloud)
  {
    pcl::PointXYZ xyz;
    xyz.x = point.x;
    xyz.y = point.y;
    xyz.z = point.z;
    xyz_cloud->push_back(xyz);

    pcl::Normal normal;
    normal.normal_x = point.normal_x;
    normal.normal_y = point.normal_y;
    normal.normal_z = point.normal_z;
    normal.curvature = point.curvature;
    normal_cloud->push_back(normal);
  }

  xyz_cloud->width = static_cast<std::uint32_t>(xyz_cloud->size());
  xyz_cloud->height = 1;
  xyz_cloud->is_dense = true;
  normal_cloud->width = static_cast<std::uint32_t>(normal_cloud->size());
  normal_cloud->height = 1;
  normal_cloud->is_dense = true;
  return {xyz_cloud, normal_cloud};
}

inline pcl::Indices
makeWrappedNeighborhood(const XYZCloudT& cloud, const int center, const int count)
{
  pcl::Indices indices;
  indices.reserve(static_cast<std::size_t>(count));
  const int cloud_size = static_cast<int>(cloud.size());
  const int half = count / 2;
  for (int i = 0; i < count; ++i)
  {
    const int raw = center + i - half;
    const int wrapped = ((raw % cloud_size) + cloud_size) % cloud_size;
    indices.push_back(static_cast<pcl::index_t>(wrapped));
  }
  return indices;
}

inline bool
isFinitePoint(const PointT& point)
{
  return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

inline bool
computePairTupleReference(const PointT& p1, const PointT& p2, PairTuple& tuple)
{
  Eigen::Vector4f delta = p2.getVector4fMap() - p1.getVector4fMap();
  delta[3] = 0.0f;
  tuple.f4 = delta.norm();

  if (tuple.f4 == 0.0f)
  {
    tuple = {};
    return false;
  }

  Eigen::Vector4f n1 = p1.getNormalVector4fMap();
  Eigen::Vector4f n2 = p2.getNormalVector4fMap();
  n1[3] = 0.0f;
  n2[3] = 0.0f;

  const float angle1 = n1.dot(delta) / tuple.f4;
  const float angle2 = n2.dot(delta) / tuple.f4;
  if (std::acos(std::fabs(angle1)) > std::acos(std::fabs(angle2)))
  {
    n1 = p2.getNormalVector4fMap();
    n2 = p1.getNormalVector4fMap();
    n1[3] = 0.0f;
    n2[3] = 0.0f;
    delta *= -1.0f;
    tuple.f3 = -angle2;
  }
  else
    tuple.f3 = angle1;

  Eigen::Vector4f v = delta.cross3(n1);
  v[3] = 0.0f;
  const float v_norm = v.norm();
  if (v_norm == 0.0f)
  {
    tuple = {};
    return false;
  }
  v /= v_norm;

  Eigen::Vector4f w = n1.cross3(v);
  v[3] = 0.0f;
  w[3] = 0.0f;
  tuple.f2 = v.dot(n2);
  tuple.f1 = std::atan2(w.dot(n2), n1.dot(n2));
  return true;
}

inline int
binAngularFeature(const float value, const int bins)
{
  constexpr float kPi = 3.14159265358979323846f;
  const float scale = 1.0f / (2.0f * kPi);
  return std::clamp(static_cast<int>(std::floor(static_cast<float>(bins) * ((value + kPi) * scale))),
                    0,
                    bins - 1);
}

inline int
binUnitFeature(const float value, const int bins)
{
  return std::clamp(static_cast<int>(std::floor(static_cast<float>(bins) * ((value + 1.0f) * 0.5f))),
                    0,
                    bins - 1);
}

inline int
histogramIndex(const int f1, const int f2, const int f3, const int bins)
{
  return f1 + bins * f2 + bins * bins * f3;
}

inline void
computePointPFHReference(const CloudT& cloud,
                         const pcl::Indices& indices,
                         const int nr_split,
                         Eigen::VectorXf& pfh_histogram)
{
  pfh_histogram.setZero(nr_split * nr_split * nr_split);
  const float hist_incr =
      100.0f / static_cast<float>(indices.size() * (indices.size() - 1) / 2);

  for (std::size_t i_idx = 0; i_idx < indices.size(); ++i_idx)
  {
    for (std::size_t j_idx = 0; j_idx < i_idx; ++j_idx)
    {
      const auto i = static_cast<std::size_t>(indices[i_idx]);
      const auto j = static_cast<std::size_t>(indices[j_idx]);
      if (!isFinitePoint(cloud[i]) || !isFinitePoint(cloud[j]))
        continue;

      PairTuple tuple;
      if (!computePairTupleReference(cloud[i], cloud[j], tuple))
        continue;

      const int f1 = binAngularFeature(tuple.f1, nr_split);
      const int f2 = binUnitFeature(tuple.f2, nr_split);
      const int f3 = binUnitFeature(tuple.f3, nr_split);
      pfh_histogram[histogramIndex(f1, f2, f3, nr_split)] += hist_incr;
    }
  }
}

inline double
checksumHistogram(const Eigen::VectorXf& histogram)
{
  double checksum = 0.0;
  for (Eigen::Index i = 0; i < histogram.size(); ++i)
    checksum += static_cast<double>(histogram[i]) * static_cast<double>(i + 1);
  return checksum;
}

} // namespace pcl::features::rvv_test::pfh
