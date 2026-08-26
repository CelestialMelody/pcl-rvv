#pragma once

/*
 * 本文件做什么：
 * 这里保存 PPF（Point Pair Feature，点对特征）topic 的 test-only scalar reference
 * （测试专用标量参考链路）。reference 复刻当前
 * `features/include/pcl/features/impl/ppf.hpp::computeFeature` 的 output resize
 * （输出预设大小）、identity pair（同一点对）NaN、`computePairFeatures` helper choice
 * （辅助函数选择）和 `alpha_m` 计算顺序，供后续 RVV candidate（候选实现）做 same-chain
 * （同构链路）对拍。
 *
 * 证据边界：
 * 这些 helper 只证明当前 PPF all-pairs 输出语义。它们不证明 production dispatch
 * （生产分流）、不覆盖真实板卡性能，也不把 `features/src/ppf.cpp::computePPFPairFeature`
 * 误当成当前 `impl/ppf.hpp` 的调用路径。
 */

#include <pcl/features/pfh_tools.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>

namespace pcl::features::rvv_test::ppf
{
using XYZCloudT = pcl::PointCloud<pcl::PointXYZ>;
using NormalCloudT = pcl::PointCloud<pcl::Normal>;

inline std::pair<XYZCloudT::Ptr, NormalCloudT::Ptr>
makeXYZAndNormalClouds(const int side)
{
  auto cloud = XYZCloudT::Ptr(new XYZCloudT);
  auto normals = NormalCloudT::Ptr(new NormalCloudT);
  cloud->reserve(static_cast<std::size_t>(side * side));
  normals->reserve(static_cast<std::size_t>(side * side));

  for (int y = 0; y < side; ++y)
  {
    for (int x = 0; x < side; ++x)
    {
      const float fx = static_cast<float>(x) * 0.041f;
      const float fy = static_cast<float>(y) * 0.037f;
      const float wave = 0.012f * std::sin(0.33f * fx + 0.11f * fy) +
                         0.009f * std::cos(0.19f * fy);

      pcl::PointXYZ point;
      point.x = fx + 0.004f * static_cast<float>(y % 3);
      point.y = fy - 0.003f * static_cast<float>(x % 2);
      point.z = 0.18f * fx - 0.13f * fy + wave;
      cloud->push_back(point);

      Eigen::Vector3f normal(-0.18f + 0.003f * std::cos(0.29f * fx),
                             0.13f + 0.002f * std::sin(0.23f * fy),
                             1.0f);
      normal.normalize();
      pcl::Normal pcl_normal;
      pcl_normal.normal_x = normal.x();
      pcl_normal.normal_y = normal.y();
      pcl_normal.normal_z = normal.z();
      pcl_normal.curvature = 0.0f;
      normals->push_back(pcl_normal);
    }
  }

  cloud->width = static_cast<std::uint32_t>(cloud->size());
  cloud->height = 1;
  cloud->is_dense = true;
  normals->width = static_cast<std::uint32_t>(normals->size());
  normals->height = 1;
  normals->is_dense = true;
  return {cloud, normals};
}

inline std::pair<pcl::PointCloud<pcl::PointXYZI>::Ptr, NormalCloudT::Ptr>
makeXYZIAndNormalClouds(const int side)
{
  const auto [xyz_cloud, normals] = makeXYZAndNormalClouds(side);
  auto xyzi_cloud = pcl::PointCloud<pcl::PointXYZI>::Ptr(new pcl::PointCloud<pcl::PointXYZI>);
  xyzi_cloud->reserve(xyz_cloud->size());

  for (std::size_t index = 0; index < xyz_cloud->size(); ++index)
  {
    pcl::PointXYZI point;
    point.x = (*xyz_cloud)[index].x;
    point.y = (*xyz_cloud)[index].y;
    point.z = (*xyz_cloud)[index].z;
    point.intensity = 0.5f + 0.01f * static_cast<float>(index % 17);
    xyzi_cloud->push_back(point);
  }

  xyzi_cloud->width = static_cast<std::uint32_t>(xyzi_cloud->size());
  xyzi_cloud->height = 1;
  xyzi_cloud->is_dense = true;
  return {xyzi_cloud, normals};
}

inline std::pair<XYZCloudT::Ptr, pcl::PointCloud<pcl::PointNormal>::Ptr>
makeXYZAndPointNormalClouds(const int side)
{
  const auto [cloud, normals] = makeXYZAndNormalClouds(side);
  auto point_normals =
      pcl::PointCloud<pcl::PointNormal>::Ptr(new pcl::PointCloud<pcl::PointNormal>);
  point_normals->reserve(normals->size());

  for (std::size_t index = 0; index < normals->size(); ++index)
  {
    pcl::PointNormal normal;
    normal.x = (*cloud)[index].x;
    normal.y = (*cloud)[index].y;
    normal.z = (*cloud)[index].z;
    normal.normal_x = (*normals)[index].normal_x;
    normal.normal_y = (*normals)[index].normal_y;
    normal.normal_z = (*normals)[index].normal_z;
    normal.curvature = (*normals)[index].curvature;
    point_normals->push_back(normal);
  }

  point_normals->width = static_cast<std::uint32_t>(point_normals->size());
  point_normals->height = 1;
  point_normals->is_dense = true;
  return {cloud, point_normals};
}

inline pcl::Indices
makeSequentialIndices(const std::size_t size)
{
  pcl::Indices indices(size);
  std::iota(indices.begin(), indices.end(), 0);
  return indices;
}

inline void
setNaN(pcl::PPFSignature& signature)
{
  const float nan = std::numeric_limits<float>::quiet_NaN();
  signature.f1 = nan;
  signature.f2 = nan;
  signature.f3 = nan;
  signature.f4 = nan;
  signature.alpha_m = nan;
}

inline float
computeAlphaMReference(const pcl::PointXYZ& reference_point,
                       const pcl::Normal& reference_normal,
                       const pcl::PointXYZ& model_point)
{
  const Eigen::Vector3f model_reference_point = reference_point.getVector3fMap();
  const Eigen::Vector3f model_reference_normal =
      reference_normal.getNormalVector3fMap();
  const Eigen::Vector3f model_point_vector = model_point.getVector3fMap();
  const float rotation_angle =
      std::acos(model_reference_normal.dot(Eigen::Vector3f::UnitX()));
  const bool parallel_to_x =
      (model_reference_normal.y() == 0.0f && model_reference_normal.z() == 0.0f);
  const Eigen::Vector3f rotation_axis =
      parallel_to_x ? Eigen::Vector3f::UnitY()
                    : model_reference_normal.cross(Eigen::Vector3f::UnitX()).normalized();
  const Eigen::AngleAxisf rotation_mg(rotation_angle, rotation_axis);
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
computePPFReference(const XYZCloudT& cloud,
                    const NormalCloudT& normals,
                    const pcl::Indices& indices,
                    pcl::PointCloud<pcl::PPFSignature>& output)
{
  output.resize(indices.size() * cloud.size());
  output.height = 1;
  output.width = static_cast<std::uint32_t>(output.size());
  output.is_dense = true;

  for (std::size_t index_i = 0; index_i < indices.size(); ++index_i)
  {
    const auto i = static_cast<std::size_t>(indices[index_i]);
    for (std::size_t j = 0; j < cloud.size(); ++j)
    {
      pcl::PPFSignature signature;
      if (i != j)
      {
        if (pcl::computePairFeatures(cloud[i].getVector4fMap(),
                                     normals[i].getNormalVector4fMap(),
                                     cloud[j].getVector4fMap(),
                                     normals[j].getNormalVector4fMap(),
                                     signature.f1,
                                     signature.f2,
                                     signature.f3,
                                     signature.f4))
        {
          signature.alpha_m = computeAlphaMReference(cloud[i], normals[i], cloud[j]);
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

      output[index_i * cloud.size() + j] = signature;
    }
  }
}

inline double
checksumPPF(const pcl::PointCloud<pcl::PPFSignature>& output)
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
                       11.0 * static_cast<double>(p.alpha_m));
  }
  return checksum;
}

} // namespace pcl::features::rvv_test::ppf
