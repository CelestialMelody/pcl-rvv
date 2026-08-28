/*
 * sac_model_normal_sphere correctness tests 的聚合入口。
 * src/test_sac_model_normal_sphere.cpp 只保留 gtest case；fixture、candidate
 * access wrapper 和断言 helper 放在 include/impl，便于后续点型扩展复用。
 */

#pragma once

#include <impl/sac_model_normal_sphere_access.hpp>

#include <pcl/test/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <type_traits>
#include <vector>

namespace pcl_rvv_normal_sphere_test_support
{

inline Eigen::VectorXf
normalSphereCoefficients ()
{
  Eigen::VectorXf coeffs (4);
  coeffs << 1.0f, -2.0f, 0.5f, 2.0f;
  return coeffs;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeNormalSphereCloud ()
{
  typename pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  const float cx = 1.0f;
  const float cy = -2.0f;
  const float cz = 0.5f;
  const float radii[] = {2.0f, 2.16f, 1.82f, 2.55f, 1.62f, 0.0f, 2.08f, 2.32f};
  const float y_offsets[] = {0.0f, 0.12f, -0.08f, 0.25f, -0.20f, 0.0f, 0.04f, -0.16f};
  cloud->resize (sizeof (radii) / sizeof (radii[0]));
  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    (*cloud)[i].x = cx + radii[i];
    (*cloud)[i].y = cy + y_offsets[i];
    (*cloud)[i].z = cz + ((i % 3) == 0 ? 0.06f : 0.0f);
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
      (*cloud)[i].intensity = static_cast<float> (100 + i) * 0.125f;
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGB>)
    {
      (*cloud)[i].r = static_cast<std::uint8_t> (20 + i);
      (*cloud)[i].g = static_cast<std::uint8_t> (40 + i);
      (*cloud)[i].b = static_cast<std::uint8_t> (60 + i);
    }
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBA>)
    {
      (*cloud)[i].r = static_cast<std::uint8_t> (20 + i);
      (*cloud)[i].g = static_cast<std::uint8_t> (40 + i);
      (*cloud)[i].b = static_cast<std::uint8_t> (60 + i);
      (*cloud)[i].a = 255;
    }
  }
  return cloud;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeLargeNormalSphereCloud (const std::size_t nr_points)
{
  typename pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  const float cx = 1.0f;
  const float cy = -2.0f;
  const float cz = 0.5f;
  cloud->resize (nr_points);
  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    const float t = static_cast<float> (i % 257) / 257.0f;
    const float radius = 1.75f + 0.75f * static_cast<float> ((i * 13) % 17) / 16.0f;
    (*cloud)[i].x = cx + radius * std::cos (t * 6.283185307179586f);
    (*cloud)[i].y = cy + radius * std::sin (t * 6.283185307179586f);
    (*cloud)[i].z = cz + 0.25f * std::sin (t * 18.84955592153876f);
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
      (*cloud)[i].intensity = static_cast<float> ((i * 11) % 1024) / 1024.0f;
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGB>)
    {
      (*cloud)[i].r = static_cast<std::uint8_t> ((i * 3) % 255);
      (*cloud)[i].g = static_cast<std::uint8_t> ((i * 5) % 255);
      (*cloud)[i].b = static_cast<std::uint8_t> ((i * 7) % 255);
    }
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBA>)
    {
      (*cloud)[i].r = static_cast<std::uint8_t> ((i * 3) % 255);
      (*cloud)[i].g = static_cast<std::uint8_t> ((i * 5) % 255);
      (*cloud)[i].b = static_cast<std::uint8_t> ((i * 7) % 255);
      (*cloud)[i].a = 255;
    }
  }
  return cloud;
}

template <typename PointNT, typename PointT>
typename pcl::PointCloud<PointNT>::Ptr
makeNormalSphereNormals (const pcl::PointCloud<PointT>& cloud)
{
  typename pcl::PointCloud<PointNT>::Ptr normals (new pcl::PointCloud<PointNT>);
  normals->resize (cloud.size ());
  const Eigen::Vector3f center (1.0f, -2.0f, 0.5f);
  for (std::size_t i = 0; i < cloud.size (); ++i)
  {
    const Eigen::Vector3f p (cloud[i].x, cloud[i].y, cloud[i].z);
    Eigen::Vector3f dir = p - center;
    if (dir.norm () > 0.0f)
      dir.normalize ();
    else
      dir = Eigen::Vector3f::UnitX ();

    if (i == 3)
      dir = Eigen::Vector3f (-dir.y (), dir.x (), 0.0f).normalized ();
    else if (i == 4)
      dir = -dir;
    else if (i == 7)
      dir = (dir + Eigen::Vector3f (0.0f, 0.8f, 0.2f)).normalized ();

    (*normals)[i].normal_x = dir.x ();
    (*normals)[i].normal_y = dir.y ();
    (*normals)[i].normal_z = dir.z ();
  }
  return normals;
}

inline pcl::Indices
makeShuffledNormalSphereIndices (const std::size_t nr_points)
{
  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  for (std::size_t i = 1; i < indices.size (); i += 4)
    std::swap (indices[i - 1], indices[i]);
  return indices;
}

template <typename PointT, typename PointNT>
void
expectSameNormalSphereOutputs (SampleConsensusModelNormalSphereAccess<PointT, PointNT>& model,
                               const Eigen::VectorXf& coeffs,
                               const double threshold)
{
  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  pcl::Indices reference_inliers;
  model.selectWithinDistanceScalarReference (coeffs, threshold, reference_inliers);
  const std::vector<double> reference_errors = model.error_sqr_dists_;
  const std::size_t reference_count = model.countWithinDistanceScalarReference (coeffs, threshold);
  std::vector<double> reference_distances;
  model.getDistancesToModelScalarReference (coeffs, reference_distances);

  ASSERT_EQ (reference_count, public_count);
  ASSERT_EQ (reference_inliers, public_inliers);
  ASSERT_EQ (reference_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < reference_errors.size (); ++i)
    EXPECT_NEAR (reference_errors[i], public_errors[i], 1e-6);

  ASSERT_EQ (reference_distances.size (), public_distances.size ());
  for (std::size_t i = 0; i < reference_distances.size (); ++i)
    EXPECT_NEAR (reference_distances[i], public_distances[i], 1e-6);

  pcl::Indices candidate_inliers;
  model.selectWithinDistanceCandidate (coeffs, threshold, candidate_inliers);
  const std::vector<double> candidate_errors = model.error_sqr_dists_;
  const std::size_t candidate_count = model.countWithinDistanceCandidate (coeffs, threshold);
  std::vector<double> candidate_distances;
  model.getDistancesToModelCandidate (coeffs, candidate_distances);

  ASSERT_EQ (reference_count, candidate_count);
  ASSERT_EQ (reference_inliers, candidate_inliers);
  ASSERT_EQ (reference_errors.size (), candidate_errors.size ());
  for (std::size_t i = 0; i < reference_errors.size (); ++i)
    EXPECT_NEAR (reference_errors[i], candidate_errors[i], 1e-4);

  ASSERT_EQ (reference_distances.size (), candidate_distances.size ());
  for (std::size_t i = 0; i < reference_distances.size (); ++i)
    EXPECT_NEAR (reference_distances[i], candidate_distances[i], 1e-4);
}

template <typename PointT, typename PointNT>
void
expectPublicNormalSphereOutputsNearReference (
    SampleConsensusModelNormalSphereAccess<PointT, PointNT>& model,
    const Eigen::VectorXf& coeffs,
    const double threshold,
    const double tolerance)
{
  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  pcl::Indices reference_inliers;
  model.selectWithinDistanceScalarReference (coeffs, threshold, reference_inliers);
  const std::vector<double> reference_errors = model.error_sqr_dists_;
  const std::size_t reference_count = model.countWithinDistanceScalarReference (coeffs, threshold);
  std::vector<double> reference_distances;
  model.getDistancesToModelScalarReference (coeffs, reference_distances);

  ASSERT_EQ (reference_count, public_count);
  ASSERT_EQ (reference_inliers, public_inliers);
  ASSERT_EQ (reference_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < reference_errors.size (); ++i)
    EXPECT_NEAR (reference_errors[i], public_errors[i], tolerance);

  ASSERT_EQ (reference_distances.size (), public_distances.size ());
  for (std::size_t i = 0; i < reference_distances.size (); ++i)
    EXPECT_NEAR (reference_distances[i], public_distances[i], tolerance);
}

}  // namespace pcl_rvv_normal_sphere_test_support
