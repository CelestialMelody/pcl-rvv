#pragma once

/*
 * 本文件做什么：
 * 这里保存 PFHRGB（带颜色的点特征直方图）topic 的 test-only scalar
 * reference（测试专用标量参考链路）。它复刻当前 production helper（生产源码
 * helper）的 pair order（点对顺序）、RGB ratio（颜色比例）、bin clamp（桶编号夹紧）
 * 和双 histogram scatter（直方图离散累加），为后续 RVV candidate（候选实现）
 * 提供 same-chain correctness（同构链路正确性）基线。
 *
 * 证据边界：
 * 这不是 production dispatch（生产分流）。它只用于测试资产中复核 PFHRGB
 * descriptor 语义，不能作为真实性能结论。
 */

#include <pcl/features/pfh_tools.h>
#include <pcl/features/pfhrgb.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace pcl::features::rvv_test::pfhrgb
{

using PointT = pcl::PointXYZRGBNormal;
using CloudT = pcl::PointCloud<PointT>;

inline int
binAngularFeature(const float value, const int bins)
{
  const float scaled = static_cast<float>(bins) *
                       ((value + static_cast<float>(M_PI)) *
                        (1.0f / (2.0f * static_cast<float>(M_PI))));
  int bin = static_cast<int>(std::floor(scaled));
  return std::min(bins - 1, std::max(0, bin));
}

inline int
binUnitFeature(const float value, const int bins)
{
  const float scaled = static_cast<float>(bins) * ((value + 1.0f) * 0.5f);
  int bin = static_cast<int>(std::floor(scaled));
  return std::min(bins - 1, std::max(0, bin));
}

inline void
computeRGBPairTupleReference(const PointT& p,
                             const PointT& q,
                             float& f1,
                             float& f2,
                             float& f3,
                             float& f4,
                             float& f5,
                             float& f6,
                             float& f7)
{
  const Eigen::Vector4i colors1(p.r, p.g, p.b, 0);
  const Eigen::Vector4i colors2(q.r, q.g, q.b, 0);
  pcl::computeRGBPairFeatures(p.getVector4fMap(),
                              p.getNormalVector4fMap(),
                              colors1,
                              q.getVector4fMap(),
                              q.getNormalVector4fMap(),
                              colors2,
                              f1,
                              f2,
                              f3,
                              f4,
                              f5,
                              f6,
                              f7);
}

inline void
accumulatePFHRGBHistogramBins(const float f1,
                              const float f2,
                              const float f3,
                              const float f5,
                              const float f6,
                              const float f7,
                              const int nr_split,
                              const float hist_incr,
                              Eigen::VectorXf& histogram)
{
  const int f1_bin = binAngularFeature(f1, nr_split);
  const int f2_bin = binUnitFeature(f2, nr_split);
  const int f3_bin = binUnitFeature(f3, nr_split);
  const int f5_bin = binUnitFeature(f5, nr_split);
  const int f6_bin = binUnitFeature(f6, nr_split);
  const int f7_bin = binUnitFeature(f7, nr_split);

  histogram[f1_bin + nr_split * f2_bin + nr_split * nr_split * f3_bin] += hist_incr;
  histogram[125 + f5_bin + nr_split * f6_bin + nr_split * nr_split * f7_bin] += hist_incr;
}

inline void
computePointPFHRGBReference(const CloudT& cloud,
                            const pcl::Indices& indices,
                            const int nr_split,
                            Eigen::VectorXf& histogram)
{
  histogram.setZero(2 * nr_split * nr_split * nr_split);
  if (indices.size() < 2)
    return;

  const float hist_incr =
      100.0f / static_cast<float>(indices.size() * (indices.size() - 1) / 2);
  for (const auto index_i : indices)
  {
    for (const auto index_j : indices)
    {
      if (index_i == index_j)
        continue;

      float f1 = 0.0f;
      float f2 = 0.0f;
      float f3 = 0.0f;
      float f4 = 0.0f;
      float f5 = 0.0f;
      float f6 = 0.0f;
      float f7 = 0.0f;
      computeRGBPairTupleReference(cloud[static_cast<std::size_t>(index_i)],
                                   cloud[static_cast<std::size_t>(index_j)],
                                   f1,
                                   f2,
                                   f3,
                                   f4,
                                   f5,
                                   f6,
                                   f7);
      if (f4 == 0.0f)
        continue;
      accumulatePFHRGBHistogramBins(f1, f2, f3, f5, f6, f7, nr_split, hist_incr, histogram);
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

} // namespace pcl::features::rvv_test::pfhrgb
