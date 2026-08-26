#pragma once

/*
 * 本文件做什么：
 * 这里保存 CPPF（Colored Point Pair Feature，彩色点对特征）topic 的
 * test-only scalar reference（测试专用标量参考链路）。reference 复刻当前
 * `features/include/pcl/features/impl/cppf.hpp::computeFeature` 的 all-pairs output
 * （所有点对输出）、identity pair（同一点对）NaN、`computeCPPFPairFeature`
 * helper choice（辅助函数选择）和 `alpha_m` 计算顺序，供后续 RVV candidate
 * （候选实现）做 same-chain（同构链路）对拍。
 *
 * 证据边界：
 * 这些 helper 只证明当前 CPPF caller-shaped diagnostic（调用方形态诊断）语义。
 * 它们不修改 production（生产源码），不证明 production dispatch（生产分流），
 * 也不把 `features/src/cppf.cpp` 这个 helper-only（只有辅助函数、没有本地批量循环）
 * 文件当成独立 production 入口。
 */

#include <pcl/features/cppf.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>

namespace pcl::features::rvv_test::cppf
{
using PointT = pcl::PointXYZRGBNormal;
using CloudT = pcl::PointCloud<PointT>;

inline CloudT::Ptr
makeCPPFCloud(const int side)
{
  auto cloud = pcl::make_shared<CloudT>();
  cloud->reserve(static_cast<std::size_t>(side * side));

  for (int y = 0; y < side; ++y)
  {
    for (int x = 0; x < side; ++x)
    {
      const float fx = static_cast<float>(x);
      const float fy = static_cast<float>(y);
      const float wave = 0.021f * std::sin(0.13f * fx + 0.07f * fy) +
                         0.017f * std::cos(0.11f * fy);

      PointT point;
      point.x = 0.043f * fx + 0.004f * static_cast<float>(y % 4);
      point.y = 0.039f * fy - 0.003f * static_cast<float>(x % 3);
      point.z = 0.12f * point.x - 0.09f * point.y + wave + 0.2f;

      Eigen::Vector3f normal(-0.22f + 0.011f * std::sin(0.19f * fx),
                             0.31f + 0.009f * std::cos(0.17f * fy),
                             1.0f);
      normal.normalize();
      point.normal_x = normal.x();
      point.normal_y = normal.y();
      point.normal_z = normal.z();
      point.curvature = 0.0f;

      point.r = static_cast<std::uint8_t>(21 + (17 * x + 11 * y) % 221);
      point.g = static_cast<std::uint8_t>(29 + (7 * x + 23 * y) % 213);
      point.b = static_cast<std::uint8_t>(37 + (19 * x + 5 * y) % 201);
      cloud->push_back(point);
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
makePrefixIndices(const std::size_t size, const int requested_count)
{
  pcl::Indices indices = makeSequentialIndices(size);
  indices.resize(std::min(indices.size(), static_cast<std::size_t>(std::max(requested_count, 0))));
  return indices;
}

inline void
setNaN(pcl::CPPFSignature& signature)
{
  const float nan = std::numeric_limits<float>::quiet_NaN();
  signature.f1 = nan;
  signature.f2 = nan;
  signature.f3 = nan;
  signature.f4 = nan;
  signature.f5 = nan;
  signature.f6 = nan;
  signature.f7 = nan;
  signature.f8 = nan;
  signature.f9 = nan;
  signature.f10 = nan;
  signature.alpha_m = nan;
}

inline float
computeAlphaMReference(const PointT& reference_point,
                       const PointT& reference_normal,
                       const PointT& model_point)
{
  const Eigen::Vector3f model_reference_point = reference_point.getVector3fMap();
  const Eigen::Vector3f model_reference_normal =
      reference_normal.getNormalVector3fMap();
  const Eigen::Vector3f model_point_vector = model_point.getVector3fMap();
  const Eigen::AngleAxisf rotation_mg(
      std::acos(model_reference_normal.dot(Eigen::Vector3f::UnitX())),
      model_reference_normal.cross(Eigen::Vector3f::UnitX()).normalized());
  const Eigen::Affine3f transform_mg(
      Eigen::Translation3f(rotation_mg * ((-1.0f) * model_reference_point)) *
      rotation_mg);

  const Eigen::Vector3f transformed = transform_mg * model_point_vector;
  float angle = std::atan2(-transformed(2), transformed(1));
  if (std::sin(angle) * transformed(2) < 0.0f)
    angle *= -1.0f;
  return -angle;
}

inline void
computeCPPFReference(const CloudT& cloud,
                     const pcl::Indices& indices,
                     pcl::PointCloud<pcl::CPPFSignature>& output)
{
  output.clear();
  output.reserve(indices.size() * cloud.size());
  output.is_dense = true;

  for (const auto raw_i : indices)
  {
    const auto i = static_cast<std::size_t>(raw_i);
    for (std::size_t j = 0; j < cloud.size(); ++j)
    {
      pcl::CPPFSignature signature;
      if (i != j)
      {
        if (pcl::computeCPPFPairFeature(cloud[i].getVector4fMap(),
                                        cloud[i].getNormalVector4fMap(),
                                        cloud[i].getRGBVector4i(),
                                        cloud[j].getVector4fMap(),
                                        cloud[j].getNormalVector4fMap(),
                                        cloud[j].getRGBVector4i(),
                                        signature.f1,
                                        signature.f2,
                                        signature.f3,
                                        signature.f4,
                                        signature.f5,
                                        signature.f6,
                                        signature.f7,
                                        signature.f8,
                                        signature.f9,
                                        signature.f10))
        {
          signature.alpha_m = computeAlphaMReference(cloud[i], cloud[i], cloud[j]);
        }
        else
        {
          setNaN(signature);
          output.is_dense = false;
        }
      }
      else
      {
        setNaN(signature);
        output.is_dense = false;
      }
      output.push_back(signature);
    }
  }

  output.height = 1;
  output.width = static_cast<std::uint32_t>(output.size());
}

inline double
checksumCPPF(const pcl::PointCloud<pcl::CPPFSignature>& output)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < output.size(); ++i)
  {
    const auto& p = output[i];
    if (std::isnan(p.f1))
      continue;
    const double row = static_cast<double>(i + 1);
    checksum += row * (static_cast<double>(p.f1) + 3.0 * static_cast<double>(p.f2) +
                       5.0 * static_cast<double>(p.f3) + 7.0 * static_cast<double>(p.f4) +
                       11.0 * static_cast<double>(p.f5) + 13.0 * static_cast<double>(p.f6) +
                       17.0 * static_cast<double>(p.f7) + 19.0 * static_cast<double>(p.f8) +
                       23.0 * static_cast<double>(p.f9) + 29.0 * static_cast<double>(p.f10) +
                       31.0 * static_cast<double>(p.alpha_m));
  }
  return checksum;
}

} // namespace pcl::features::rvv_test::cppf
