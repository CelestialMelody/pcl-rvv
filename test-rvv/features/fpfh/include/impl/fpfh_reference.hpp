#pragma once

/*
 * 本文件做什么：
 * 这里保存 FPFH（Fast Point Feature Histogram，快速点特征直方图）topic 的
 * test-only scalar reference（测试专用标量参考链路）。reference 复刻
 * `features/src/pfh.cpp::computePairFeatures` 和
 * `features/include/pcl/features/impl/fpfh.hpp` 中的 SPFH / FPFH histogram
 * 语义，用来给后续 RVV candidate（候选实现）做 same-chain（同构链路）对拍。
 *
 * 证据边界：
 * 这些 helper 只证明局部 histogram 和 weighted SPFH（加权 SPFH）公式正确。
 * 它们不覆盖真实 neighbor search（邻域搜索）成本、不证明 production dispatch，
 * 也不把诊断性能外推为 production evidence（生产证据）。
 */

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>
#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

namespace pcl::features::rvv_test::fpfh
{
using PointT = pcl::PointNormal;
using CloudT = pcl::PointCloud<PointT>;

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

inline void
computePointSPFHReference(const CloudT& cloud,
                          const pcl::index_t p_idx,
                          const int row,
                          const pcl::Indices& indices,
                          Eigen::MatrixXf& hist_f1,
                          Eigen::MatrixXf& hist_f2,
                          Eigen::MatrixXf& hist_f3)
{
  const int bins_f1 = static_cast<int>(hist_f1.cols());
  const int bins_f2 = static_cast<int>(hist_f2.cols());
  const int bins_f3 = static_cast<int>(hist_f3.cols());
  const float hist_incr = 100.0f / static_cast<float>(indices.size() - 1);

  for (const auto index : indices)
  {
    if (p_idx == index)
      continue;

    PairTuple tuple;
    if (!computePairTupleReference(cloud[static_cast<std::size_t>(p_idx)],
                                   cloud[static_cast<std::size_t>(index)],
                                   tuple))
      continue;

    hist_f1(row, binAngularFeature(tuple.f1, bins_f1)) += hist_incr;
    hist_f2(row, binUnitFeature(tuple.f2, bins_f2)) += hist_incr;
    hist_f3(row, binUnitFeature(tuple.f3, bins_f3)) += hist_incr;
  }
}

inline void
weightPointSPFHReference(const Eigen::MatrixXf& hist_f1,
                         const Eigen::MatrixXf& hist_f2,
                         const Eigen::MatrixXf& hist_f3,
                         const pcl::Indices& indices,
                         const std::vector<float>& dists,
                         Eigen::VectorXf& fpfh_histogram)
{
  const Eigen::Index bins_f1 = hist_f1.cols();
  const Eigen::Index bins_f2 = hist_f2.cols();
  const Eigen::Index bins_f3 = hist_f3.cols();
  const Eigen::Index bins_f12 = bins_f1 + bins_f2;

  fpfh_histogram.setZero(bins_f1 + bins_f2 + bins_f3);

  double sum_f1 = 0.0;
  double sum_f2 = 0.0;
  double sum_f3 = 0.0;
  for (std::size_t idx = 0; idx < indices.size(); ++idx)
  {
    if (dists[idx] == 0.0f)
      continue;

    const float weight = 1.0f / dists[idx];
    const Eigen::Index row = static_cast<Eigen::Index>(indices[idx]);
    for (Eigen::Index bin = 0; bin < bins_f1; ++bin)
    {
      const float value = hist_f1(row, bin) * weight;
      sum_f1 += value;
      fpfh_histogram[bin] += value;
    }
    for (Eigen::Index bin = 0; bin < bins_f2; ++bin)
    {
      const float value = hist_f2(row, bin) * weight;
      sum_f2 += value;
      fpfh_histogram[bin + bins_f1] += value;
    }
    for (Eigen::Index bin = 0; bin < bins_f3; ++bin)
    {
      const float value = hist_f3(row, bin) * weight;
      sum_f3 += value;
      fpfh_histogram[bin + bins_f12] += value;
    }
  }

  if (sum_f1 != 0.0)
    sum_f1 = 100.0 / sum_f1;
  if (sum_f2 != 0.0)
    sum_f2 = 100.0 / sum_f2;
  if (sum_f3 != 0.0)
    sum_f3 = 100.0 / sum_f3;

  for (Eigen::Index bin = 0; bin < bins_f1; ++bin)
    fpfh_histogram[bin] *= static_cast<float>(sum_f1);
  for (Eigen::Index bin = 0; bin < bins_f2; ++bin)
    fpfh_histogram[bin + bins_f1] *= static_cast<float>(sum_f2);
  for (Eigen::Index bin = 0; bin < bins_f3; ++bin)
    fpfh_histogram[bin + bins_f12] *= static_cast<float>(sum_f3);
}

inline double
checksumHistogram(const Eigen::VectorXf& histogram)
{
  double checksum = 0.0;
  for (Eigen::Index i = 0; i < histogram.size(); ++i)
    checksum += static_cast<double>(histogram[i]) * static_cast<double>(i + 1);
  return checksum;
}

} // namespace pcl::features::rvv_test::fpfh
