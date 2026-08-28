/*
 * 本文件做什么：
 * 这些测试为 SampleConsensusModelCircle3D 建立 Phase 000 correctness
 * （正确性）入口。它们先保护 count/select 投影距离核的 public path
 * （公开入口路径）语义，再要求 test-only projection candidate（仅测试使用的投影候选）
 * 与公开入口在乱序 indices（索引子集）和退化投影点上保持一致。
 */

#include <pcl/test/gtest.h>

#include "sac_model_circle3d.h"

#include <pcl/point_types.h>
#include <pcl/rvv_point_traits.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

using Circle3DModelAccess =
    pcl_rvv_test::sac_model_circle3d::SampleConsensusModelCircle3DAccess<pcl::PointXYZ>;

static Eigen::VectorXf
circle3dCoefficients ()
{
  Eigen::VectorXf coeffs (7);
  coeffs << 0.25f, -0.10f, 0.30f, 1.20f, 0.20f, -0.30f, 0.9327379f;
  return coeffs;
}

static pcl::PointXYZ
makeCircle3DPoint (const Eigen::Vector3d& center,
                   const Eigen::Vector3d& normal,
                   double radius,
                   double angle,
                   double radial_offset,
                   double normal_offset)
{
  Eigen::Vector3d seed (1.0, 0.0, 0.0);
  if (std::abs (normal.dot (seed)) > 0.95)
    seed = Eigen::Vector3d (0.0, 1.0, 0.0);

  const Eigen::Vector3d axis_u = (seed - normal.dot (seed) * normal).normalized ();
  const Eigen::Vector3d axis_v = normal.cross (axis_u).normalized ();
  const Eigen::Vector3d in_plane =
      (radius + radial_offset) * (std::cos (angle) * axis_u + std::sin (angle) * axis_v);
  const Eigen::Vector3d p = center + in_plane + normal_offset * normal;

  pcl::PointXYZ point;
  point.x = static_cast<float> (p.x ());
  point.y = static_cast<float> (p.y ());
  point.z = static_cast<float> (p.z ());
  return point;
}

template <typename PointT>
static void
fillCircle3DExtraFields (PointT& point, std::size_t i)
{
  if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
    point.intensity = static_cast<float> (10 + i);
  else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGB>)
  {
    point.r = static_cast<std::uint8_t> (20 + i);
    point.g = static_cast<std::uint8_t> (40 + i);
    point.b = static_cast<std::uint8_t> (60 + i);
  }
  else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBA>)
  {
    point.r = static_cast<std::uint8_t> (20 + i);
    point.g = static_cast<std::uint8_t> (40 + i);
    point.b = static_cast<std::uint8_t> (60 + i);
    point.a = 255;
  }
  else
    (void)point;
}

template <typename PointT>
static typename pcl::PointCloud<PointT>::Ptr
makeCircle3DDispatchCloud ()
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>> ();
  const Eigen::VectorXf coeffs = circle3dCoefficients ();
  const Eigen::Vector3d center (coeffs[0], coeffs[1], coeffs[2]);
  const Eigen::Vector3d normal =
      Eigen::Vector3d (coeffs[4], coeffs[5], coeffs[6]).normalized ();
  constexpr double radius = 1.20;
  const double angles[] = {0.0, 0.4, 0.9, 1.7, 2.4, 3.1, 4.0, 5.2, 5.8, 6.1};
  const double radial_offsets[] = {0.0, 0.03, -0.04, 0.11, -0.13, 0.20, -0.19, 0.06, -0.02, 0.14};
  const double normal_offsets[] = {0.0, 0.02, -0.03, 0.01, 0.04, -0.05, 0.0, 0.08, -0.07, 0.02};

  cloud->resize (10);
  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    const pcl::PointXYZ xyz =
        makeCircle3DPoint (center, normal, radius, angles[i], radial_offsets[i], normal_offsets[i]);
    (*cloud)[i].x = xyz.x;
    (*cloud)[i].y = xyz.y;
    (*cloud)[i].z = xyz.z;
    fillCircle3DExtraFields ((*cloud)[i], i);
  }
  return cloud;
}

static pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCircle3DDispatchCloud ()
{
  return makeCircle3DDispatchCloud<pcl::PointXYZ> ();
}

template <typename PointT>
static void
selectWithinDistanceReference (const pcl::PointCloud<PointT>& cloud,
                               const pcl::Indices& indices,
                               const Eigen::VectorXf& model_coefficients,
                               const double threshold,
                               pcl::Indices& inliers,
                               std::vector<double>& error_sqr_dists)
{
  inliers.clear ();
  error_sqr_dists.clear ();
  inliers.reserve (indices.size ());
  error_sqr_dists.reserve (indices.size ());

  const auto squared_threshold = threshold * threshold;
  for (std::size_t i = 0; i < indices.size (); ++i)
  {
    const Eigen::Vector3d p (cloud[indices[i]].x, cloud[indices[i]].y, cloud[indices[i]].z);
    const Eigen::Vector3d c (
        model_coefficients[0], model_coefficients[1], model_coefficients[2]);
    const Eigen::Vector3d n (
        model_coefficients[4], model_coefficients[5], model_coefficients[6]);
    const double r = model_coefficients[3];

    const Eigen::Vector3d pc = p - c;
    const double lambda = (-(pc.dot (n))) / n.dot (n);
    const Eigen::Vector3d p_proj = p + lambda * n;
    const Eigen::Vector3d p_proj_c = p_proj - c;
    const Eigen::Vector3d k = c + r * p_proj_c.normalized ();
    const double sqr_dist = (p - k).squaredNorm ();
    if (sqr_dist < squared_threshold)
    {
      inliers.push_back (indices[i]);
      error_sqr_dists.push_back (sqr_dist);
    }
  }
}

static void
expectSameSelectAndCount (Circle3DModelAccess& model,
                          const Eigen::VectorXf& coeffs,
                          double threshold)
{
  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);

  pcl::Indices candidate_inliers;
  model.error_sqr_dists_.clear ();
  model.selectWithinDistanceProjectionCandidate (coeffs, threshold, candidate_inliers);
  const std::vector<double> candidate_errors = model.error_sqr_dists_;
  const std::size_t candidate_count =
      model.countWithinDistanceProjectionCandidate (coeffs, threshold);

  ASSERT_EQ (public_count, candidate_count);
  ASSERT_EQ (public_inliers, candidate_inliers);
  ASSERT_EQ (public_errors.size (), candidate_errors.size ());
  for (std::size_t i = 0; i < public_errors.size (); ++i)
    EXPECT_NEAR (public_errors[i], candidate_errors[i], 1e-5);
}

template <typename PointT>
static void
expectPublicSelectMatchesStdHelperForPointType ()
{
  static_assert (pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value);
  auto cloud = makeCircle3DDispatchCloud<PointT> ();
  pcl::Indices indices = {7, 1, 8, 0, 5, 2, 9, 4, 3, 6};
  pcl_rvv_test::sac_model_circle3d::SampleConsensusModelCircle3DAccess<PointT> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  const Eigen::VectorXf coeffs = circle3dCoefficients ();

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, 0.12, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;

  pcl::Indices standard_inliers;
  std::vector<double> standard_errors;
  selectWithinDistanceReference (*cloud, indices, coeffs, 0.12, standard_inliers, standard_errors);

  ASSERT_EQ (standard_inliers, public_inliers);
  ASSERT_EQ (standard_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < standard_errors.size (); ++i)
    EXPECT_NEAR (standard_errors[i], public_errors[i], 1e-5);
}

TEST (SampleConsensusModelCircle3D, CountAndSelectCandidateMatchesPublicPath)
{
  // 乱序 indices 保护输入顺序和输出顺序。candidate 后续会尝试 RVV gather
  // （离散加载）和 mask（掩码），但不能改变 public path 的筛选结果。
  auto cloud = makeCircle3DDispatchCloud ();
  pcl::Indices indices = {7, 1, 8, 0, 5, 2, 9, 4, 3, 6};
  Circle3DModelAccess model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectSameSelectAndCount (model, circle3dCoefficients (), 0.12);
}

TEST (SampleConsensusModelCircle3D, DegenerateProjectionFallsBackToScalarPath)
{
  // 投影点接近圆心时，production 标量路径会走 normalized() 的退化语义。
  // Phase 000 candidate 必须保守回到标量同构链路，避免把 NaN/Inf lane
  // （无效向量通道）误写成可进入 production 的证据。
  auto cloud = makeCircle3DDispatchCloud ();
  const Eigen::VectorXf coeffs = circle3dCoefficients ();
  const Eigen::Vector3d center (coeffs[0], coeffs[1], coeffs[2]);
  (*cloud)[3].x = static_cast<float> (center.x ());
  (*cloud)[3].y = static_cast<float> (center.y ());
  (*cloud)[3].z = static_cast<float> (center.z ());

  pcl::Indices indices = {3, 0, 1, 2, 4, 5};
  Circle3DModelAccess model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectSameSelectAndCount (model, coeffs, 0.12);
}

TEST (SampleConsensusModelCircle3D, PointXYZILayoutMatchesStandardSelectPath)
{
  // PointXYZI 只把 intensity 作为额外字段保留。Phase 020 后它不再命中
  // RVV helper；这里保护 public entry 经标量 fallback 后的输出语义。
  expectPublicSelectMatchesStdHelperForPointType<pcl::PointXYZI> ();
}

TEST (SampleConsensusModelCircle3D, PointXYZRGBAndRGBALayoutsMatchStandardSelectPath)
{
  // RGB/RGBA 点型有额外颜色字段。当前证据不支持它们进入 RVV 路径，
  // 所以本测试只证明标量 fallback 不改变 inlier 顺序和误差。
  expectPublicSelectMatchesStdHelperForPointType<pcl::PointXYZRGB> ();
  expectPublicSelectMatchesStdHelperForPointType<pcl::PointXYZRGBA> ();
}
