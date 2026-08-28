/*
 * sac_model_sphere correctness tests 的聚合入口。
 * src/test_sac_model_sphere.cpp 只保留 gtest case；fixture、candidate
 * access wrapper 和断言 helper 放在 include/impl 方便后续点型扩展复用。
 */

#pragma once

#include <impl/sac_model_sphere_access.hpp>

#include <pcl/test/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace pcl_rvv_sphere_test_support
{

inline Eigen::VectorXf
sphereCoefficients ()
{
  Eigen::VectorXf coeffs (4);
  coeffs << 1.0f, -2.0f, 0.5f, 2.0f;
  return coeffs;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeSphereShellCloud ()
{
  typename pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  const float cx = 1.0f;
  const float cy = -2.0f;
  const float cz = 0.5f;
  const float radii[] = {1.75f, 2.25f, 2.0f, 1.50f, 2.60f, 0.0f, 1.90f, 2.10f};
  cloud->resize (sizeof (radii) / sizeof (radii[0]));
  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    (*cloud)[i].x = cx + radii[i];
    (*cloud)[i].y = cy + static_cast<float> ((i % 2) == 0 ? 0.0f : 0.125f);
    (*cloud)[i].z = cz;
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
      (*cloud)[i].intensity = static_cast<float> (10 + i);
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
void
expectSameSphereOutputs (SampleConsensusModelSphereAccess<PointT>& model,
                         const Eigen::VectorXf& coeffs,
                         const double threshold)
{
  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  const std::size_t standard_count = model.countWithinDistanceStandard (coeffs, threshold);
  pcl::Indices candidate_inliers;
  model.selectWithinDistanceCandidate (coeffs, threshold, candidate_inliers);
  const std::vector<double> candidate_errors = model.error_sqr_dists_;
  std::vector<double> candidate_distances;
  model.getDistancesToModelCandidate (coeffs, candidate_distances);

  ASSERT_EQ (standard_count, public_count);
  ASSERT_EQ (public_inliers, candidate_inliers);
  ASSERT_EQ (public_errors.size (), candidate_errors.size ());
  for (std::size_t i = 0; i < public_errors.size (); ++i)
    EXPECT_NEAR (public_errors[i], candidate_errors[i], 1e-6);

  ASSERT_EQ (public_distances.size (), candidate_distances.size ());
  for (std::size_t i = 0; i < public_distances.size (); ++i)
    EXPECT_NEAR (public_distances[i], candidate_distances[i], 1e-6);

#if defined (__RVV10__)
  ASSERT_EQ (standard_count, model.countWithinDistanceRVV (coeffs, threshold));
#endif
}

}  // namespace pcl_rvv_sphere_test_support
