/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 moment of inertia estimation
 * （惯性矩估计）逐点规约 diagnostic candidate（诊断候选）是否和
 * 标量参考链路一致，同时给 public compute（公开入口）的 common typed
 * scope 提供 smoke（冒烟）对拍。测试覆盖非连续 indices（索引）、
 * tail（向量尾段）和 OBB 投影极值；它不修改 production 头文件。
 */

#include "moi.h"

#include <Eigen/Geometry>

#define private public
#include <pcl/features/moment_of_inertia_estimation.h>
#undef private

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace moi = pcl::features::rvv_test::moi;

struct NonFloatXYZ {
  double x = 0.0;
  float y = 0.0f;
  float z = 0.0f;
};

POINT_CLOUD_REGISTER_POINT_STRUCT(NonFloatXYZ, (double, x, x)(float, y, y)(float, z, z))

namespace {

std::vector<pcl::PointXYZ>
makeCloud(const std::size_t count)
{
  std::vector<pcl::PointXYZ> cloud(count);
  for (std::size_t i = 0; i < count; ++i) {
    const float f = static_cast<float>(i);
    cloud[i].x = 0.125f * f - 3.0f;
    cloud[i].y = 0.25f * static_cast<float>((i * 7) % 19) - 2.0f;
    cloud[i].z = 1.0f + 0.03125f * f * f - 0.5f * static_cast<float>(i % 5);
  }
  return cloud;
}

std::vector<std::uint32_t>
makeIndices()
{
  std::vector<std::uint32_t> indices;
  for (std::uint32_t i = 3; i < 113; i += 2)
    indices.push_back(i);
  indices.push_back(0);
  indices.push_back(126);
  indices.push_back(64);
  return indices;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makePointCloud(const std::size_t count)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  const std::vector<pcl::PointXYZ> points = makeCloud(count);
  cloud->points.assign(points.begin(), points.end());
  cloud->width = static_cast<std::uint32_t>(cloud->points.size());
  cloud->height = 1;
  return cloud;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeTypedPointCloud(const std::size_t count)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  const std::vector<pcl::PointXYZ> points = makeCloud(count);
  cloud->points.resize(points.size());
  for (std::size_t i = 0; i < points.size(); ++i) {
    cloud->points[i].x = points[i].x;
    cloud->points[i].y = points[i].y;
    cloud->points[i].z = points[i].z;
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
      cloud->points[i].intensity = 0.5f + static_cast<float>(i % 17);
  }
  cloud->width = static_cast<std::uint32_t>(cloud->points.size());
  cloud->height = 1;
  return cloud;
}

pcl::IndicesPtr
makePclIndices()
{
  auto indices = pcl::make_shared<pcl::Indices>();
  for (const std::uint32_t index : makeIndices())
    indices->push_back(static_cast<int>(index));
  return indices;
}

pcl::IndicesPtr
makeEmptyPclIndices()
{
  return pcl::make_shared<pcl::Indices>();
}

std::vector<std::uint32_t>
toU32Indices(const pcl::Indices& indices)
{
  std::vector<std::uint32_t> converted;
  converted.reserve(indices.size());
  for (const int index : indices)
    converted.push_back(static_cast<std::uint32_t>(index));
  return converted;
}

void
expectNear(const moi::ReductionSummary& actual, const moi::ReductionSummary& expected)
{
  ASSERT_EQ(actual.count, expected.count);
  for (int i = 0; i < 3; ++i) {
    EXPECT_NEAR(actual.mean[i], expected.mean[i], 1.0e-4f);
    EXPECT_NEAR(actual.aabb_min[i], expected.aabb_min[i], 0.0f);
    EXPECT_NEAR(actual.aabb_max[i], expected.aabb_max[i], 0.0f);
    EXPECT_NEAR(actual.obb_min[i], expected.obb_min[i], 2.0e-4f);
    EXPECT_NEAR(actual.obb_max[i], expected.obb_max[i], 2.0e-4f);
  }
  for (int i = 0; i < 6; ++i)
    EXPECT_NEAR(actual.covariance[i], expected.covariance[i], 4.0e-3f);
  EXPECT_NEAR(actual.moment, expected.moment, 5.0e-3f);
}

void
expectPointNear(const pcl::PointXYZ& actual, const pcl::PointXYZ& expected, const float tolerance)
{
  EXPECT_NEAR(actual.x, expected.x, tolerance);
  EXPECT_NEAR(actual.y, expected.y, tolerance);
  EXPECT_NEAR(actual.z, expected.z, tolerance);
}

template <typename PointT>
moi::Covariance6
computeProjectedCovarianceReference(const pcl::PointCloud<PointT>& cloud,
                                    const pcl::Indices& indices,
                                    const Eigen::Vector3f& mean,
                                    const Eigen::Vector3f& normal)
{
  moi::Covariance6 covariance{{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
  const float factor = 1.0f / static_cast<float>((indices.size() > 1) ? (indices.size() - 1) : 1);
  for (const int index : indices) {
    const PointT& point = cloud.points[static_cast<std::size_t>(index)];
    const float dx = point.x - mean.x();
    const float dy = point.y - mean.y();
    const float dz = point.z - mean.z();
    const float dot = dx * normal.x() + dy * normal.y() + dz * normal.z();
    const float px = dx - dot * normal.x();
    const float py = dy - dot * normal.y();
    const float pz = dz - dot * normal.z();

    covariance[0] += px * px;
    covariance[1] += px * py;
    covariance[2] += px * pz;
    covariance[3] += py * py;
    covariance[4] += py * pz;
    covariance[5] += pz * pz;
  }
  for (float& value : covariance)
    value *= factor;
  return covariance;
}

template <typename PointT>
void
expectPublicComputeMatchesPointXYZGeometry()
{
  const auto typed_cloud = makeTypedPointCloud<PointT>(127);
  const auto xyz_cloud = makePointCloud(127);
  const auto indices = makePclIndices();

  pcl::MomentOfInertiaEstimation<PointT> typed_estimator;
  typed_estimator.setInputCloud(typed_cloud);
  typed_estimator.setIndices(indices);
  typed_estimator.setAngleStep(45.0f);
  typed_estimator.compute();

  pcl::MomentOfInertiaEstimation<pcl::PointXYZ> xyz_estimator;
  xyz_estimator.setInputCloud(xyz_cloud);
  xyz_estimator.setIndices(indices);
  xyz_estimator.setAngleStep(45.0f);
  xyz_estimator.compute();

  Eigen::Vector3f typed_mass_center;
  Eigen::Vector3f xyz_mass_center;
  ASSERT_TRUE(typed_estimator.getMassCenter(typed_mass_center));
  ASSERT_TRUE(xyz_estimator.getMassCenter(xyz_mass_center));
  for (int i = 0; i < 3; ++i)
    EXPECT_NEAR(typed_mass_center[i], xyz_mass_center[i], 1.0e-4f);

  PointT typed_min;
  PointT typed_max;
  pcl::PointXYZ xyz_min;
  pcl::PointXYZ xyz_max;
  ASSERT_TRUE(typed_estimator.getAABB(typed_min, typed_max));
  ASSERT_TRUE(xyz_estimator.getAABB(xyz_min, xyz_max));
  EXPECT_NEAR(typed_min.x, xyz_min.x, 0.0f);
  EXPECT_NEAR(typed_min.y, xyz_min.y, 0.0f);
  EXPECT_NEAR(typed_min.z, xyz_min.z, 0.0f);
  EXPECT_NEAR(typed_max.x, xyz_max.x, 0.0f);
  EXPECT_NEAR(typed_max.y, xyz_max.y, 0.0f);
  EXPECT_NEAR(typed_max.z, xyz_max.z, 0.0f);

  std::vector<float> typed_moments;
  std::vector<float> xyz_moments;
  std::vector<float> typed_eccentricities;
  std::vector<float> xyz_eccentricities;
  ASSERT_TRUE(typed_estimator.getMomentOfInertia(typed_moments));
  ASSERT_TRUE(xyz_estimator.getMomentOfInertia(xyz_moments));
  ASSERT_TRUE(typed_estimator.getEccentricity(typed_eccentricities));
  ASSERT_TRUE(xyz_estimator.getEccentricity(xyz_eccentricities));
  ASSERT_EQ(typed_moments.size(), xyz_moments.size());
  ASSERT_EQ(typed_eccentricities.size(), xyz_eccentricities.size());
  for (std::size_t i = 0; i < typed_moments.size(); ++i)
    EXPECT_NEAR(typed_moments[i], xyz_moments[i], 1.0e-3f);
  for (std::size_t i = 0; i < typed_eccentricities.size(); ++i)
    EXPECT_NEAR(typed_eccentricities[i], xyz_eccentricities[i], 1.0e-3f);
}

} // namespace

TEST(MomentOfInertiaReductionRVV, MatchesScalarForIndexedCloudAndTail)
{
  const std::vector<pcl::PointXYZ> cloud = makeCloud(127);
  const std::vector<std::uint32_t> indices = makeIndices();

  const Eigen::Vector3f inertia_axis = Eigen::Vector3f(0.2f, 0.7f, -0.4f).normalized();
  const Eigen::Vector3f major_axis = Eigen::Vector3f(0.8f, 0.2f, 0.1f).normalized();
  const Eigen::Vector3f middle_axis = Eigen::Vector3f(-0.1f, 0.9f, 0.3f).normalized();
  const Eigen::Vector3f minor_axis = major_axis.cross(middle_axis).normalized();
  constexpr float point_mass = 1.0f / (58.0f * 58.0f);

  const moi::ReductionSummary expected =
      moi::computeReductionSummaryStd(cloud.data(),
                                      indices.data(),
                                      indices.size(),
                                      inertia_axis,
                                      major_axis,
                                      middle_axis,
                                      minor_axis,
                                      point_mass);
  const moi::ReductionSummary actual =
      moi::computeReductionSummaryRVV(cloud.data(),
                                      indices.data(),
                                      indices.size(),
                                      inertia_axis,
                                      major_axis,
                                      middle_axis,
                                      minor_axis,
                                      point_mass);

  expectNear(actual, expected);
}

TEST(MomentOfInertiaProjectedCovarianceRVV, MatchesMaterializedProjectionReference)
{
  const std::vector<pcl::PointXYZ> cloud = makeCloud(191);
  const std::vector<std::uint32_t> indices = makeIndices();
  const Eigen::Vector3f mean(1.25f, -0.75f, 5.5f);
  const Eigen::Vector3f normal = Eigen::Vector3f(-0.35f, 0.4f, 0.84f).normalized();

  const moi::Covariance6 expected =
      moi::computeProjectedCovarianceStd(cloud.data(), indices.data(), indices.size(), mean, normal);
  const moi::Covariance6 actual =
      moi::computeProjectedCovarianceRVV(cloud.data(), indices.data(), indices.size(), mean, normal);

  for (int i = 0; i < 6; ++i)
    EXPECT_NEAR(actual[i], expected[i], 6.0e-3f);
}

TEST(MomentOfInertiaProduction, PublicComputeMatchesStdBuildContract)
{
  const auto cloud = makePointCloud(127);
  const auto indices = makePclIndices();

  pcl::MomentOfInertiaEstimation<pcl::PointXYZ> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);
  estimator.setAngleStep(45.0f);
  estimator.compute();

  Eigen::Vector3f mass_center;
  ASSERT_TRUE(estimator.getMassCenter(mass_center));
  pcl::PointXYZ min_point;
  pcl::PointXYZ max_point;
  ASSERT_TRUE(estimator.getAABB(min_point, max_point));

  const std::vector<std::uint32_t> indices_u32 = toU32Indices(*indices);
  const moi::ReductionSummary expected =
      moi::computeReductionSummaryStd(cloud->points.data(),
                                      indices_u32.data(),
                                      indices_u32.size(),
                                      Eigen::Vector3f::UnitX(),
                                      Eigen::Vector3f::UnitX(),
                                      Eigen::Vector3f::UnitY(),
                                      Eigen::Vector3f::UnitZ(),
                                      1.0f);

  EXPECT_NEAR(mass_center.x(), expected.mean[0], 1.0e-4f);
  EXPECT_NEAR(mass_center.y(), expected.mean[1], 1.0e-4f);
  EXPECT_NEAR(mass_center.z(), expected.mean[2], 1.0e-4f);
  EXPECT_NEAR(min_point.x, expected.aabb_min[0], 0.0f);
  EXPECT_NEAR(min_point.y, expected.aabb_min[1], 0.0f);
  EXPECT_NEAR(min_point.z, expected.aabb_min[2], 0.0f);
  EXPECT_NEAR(max_point.x, expected.aabb_max[0], 0.0f);
  EXPECT_NEAR(max_point.y, expected.aabb_max[1], 0.0f);
  EXPECT_NEAR(max_point.z, expected.aabb_max[2], 0.0f);

  std::vector<float> moments;
  std::vector<float> eccentricities;
  ASSERT_TRUE(estimator.getMomentOfInertia(moments));
  ASSERT_TRUE(estimator.getEccentricity(eccentricities));
  EXPECT_FALSE(moments.empty());
  EXPECT_EQ(moments.size(), eccentricities.size());
}

#ifndef __RVV10__
TEST(MomentOfInertiaProductionFallback, StdBuildUsesScalarPublicCompute)
{
  const auto cloud = makePointCloud(127);
  const auto indices = makePclIndices();

  pcl::MomentOfInertiaEstimation<pcl::PointXYZ> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);
  estimator.setAngleStep(45.0f);
  estimator.compute();

  std::vector<float> moments;
  std::vector<float> eccentricities;
  ASSERT_TRUE(estimator.getMomentOfInertia(moments));
  ASSERT_TRUE(estimator.getEccentricity(eccentricities));
  EXPECT_FALSE(moments.empty());
  EXPECT_EQ(moments.size(), eccentricities.size());
}
#endif

TEST(MomentOfInertiaProductionPointTypes, PointXYZIPublicComputeMatchesPointXYZGeometry)
{
  expectPublicComputeMatchesPointXYZGeometry<pcl::PointXYZI>();
}

TEST(MomentOfInertiaProductionPointTypes, PointXYZRGBPublicComputeMatchesPointXYZGeometry)
{
  expectPublicComputeMatchesPointXYZGeometry<pcl::PointXYZRGB>();
}

TEST(MomentOfInertiaProductionPointTypes, PointXYZRGBAPublicComputeMatchesPointXYZGeometry)
{
  expectPublicComputeMatchesPointXYZGeometry<pcl::PointXYZRGBA>();
}

TEST(MomentOfInertiaProductionPointTypes, PointXYZRGBNormalPublicComputeMatchesPointXYZGeometry)
{
  expectPublicComputeMatchesPointXYZGeometry<pcl::PointXYZRGBNormal>();
}

#ifdef __RVV10__
TEST(MomentOfInertiaProductionRVVFallback, MissingInputOrIndicesReturnFalse)
{
  Eigen::Matrix<float, 3, 3> covariance_matrix;
  const Eigen::Vector3f normal = Eigen::Vector3f(-0.35f, 0.4f, 0.84f).normalized();

  pcl::MomentOfInertiaEstimation<pcl::PointXYZ> missing_input;
  EXPECT_FALSE(missing_input.computeProjectedCovarianceRVV(normal, covariance_matrix));

  pcl::MomentOfInertiaEstimation<pcl::PointXYZ> empty_indices;
  empty_indices.setInputCloud(makePointCloud(16));
  empty_indices.setIndices(makeEmptyPclIndices());
  EXPECT_FALSE(empty_indices.computeProjectedCovarianceRVV(normal, covariance_matrix));
}

TEST(MomentOfInertiaProductionRVVFallback, NonFloatXYZUsesScalarFallback)
{
  static_assert(!pcl::rvv::RVVXYZAoSFloatLayout<NonFloatXYZ>::value,
                "NonFloatXYZ intentionally fails the single-float x/y/z RVV layout gate");

  auto cloud = pcl::make_shared<pcl::PointCloud<NonFloatXYZ>>();
  cloud->points.resize(16);
  for (std::size_t i = 0; i < cloud->points.size(); ++i) {
    cloud->points[i].x = static_cast<float>(i);
    cloud->points[i].y = 0.5f * static_cast<float>(i);
    cloud->points[i].z = -0.25f * static_cast<float>(i);
  }
  cloud->width = static_cast<std::uint32_t>(cloud->points.size());
  cloud->height = 1;

  pcl::MomentOfInertiaEstimation<NonFloatXYZ> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(makePclIndices());

  Eigen::Matrix<float, 3, 3> covariance_matrix;
  const Eigen::Vector3f normal = Eigen::Vector3f(-0.35f, 0.4f, 0.84f).normalized();
  EXPECT_FALSE(estimator.computeProjectedCovarianceRVV(normal, covariance_matrix));
}

TEST(MomentOfInertiaProductionRVV, ProjectedCovarianceHelperHitsProductionRVVPath)
{
  const auto cloud = makePointCloud(191);
  const auto indices = makePclIndices();
  const Eigen::Vector3f normal = Eigen::Vector3f(-0.35f, 0.4f, 0.84f).normalized();

  pcl::MomentOfInertiaEstimation<pcl::PointXYZ> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);

  ASSERT_TRUE(estimator.initCompute());
  estimator.computeMeanValue();
  Eigen::Matrix<float, 3, 3> covariance_matrix;
  ASSERT_TRUE(estimator.computeProjectedCovarianceRVV(normal, covariance_matrix));
  estimator.deinitCompute();

  const std::vector<std::uint32_t> indices_u32 = toU32Indices(*indices);
  const moi::Covariance6 expected =
      moi::computeProjectedCovarianceStd(cloud->points.data(), indices_u32.data(), indices_u32.size(), estimator.mean_value_, normal);

  EXPECT_NEAR(covariance_matrix(0, 0), expected[0], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(0, 1), expected[1], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(0, 2), expected[2], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(1, 1), expected[3], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(1, 2), expected[4], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(2, 2), expected[5], 6.0e-3f);
}

TEST(MomentOfInertiaProductionRVV, PointXYZIProjectedCovarianceHelperHitsProductionRVVPath)
{
  const auto cloud = makeTypedPointCloud<pcl::PointXYZI>(191);
  const auto indices = makePclIndices();
  const Eigen::Vector3f normal = Eigen::Vector3f(-0.35f, 0.4f, 0.84f).normalized();

  pcl::MomentOfInertiaEstimation<pcl::PointXYZI> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);

  ASSERT_TRUE(estimator.initCompute());
  estimator.computeMeanValue();
  Eigen::Matrix<float, 3, 3> covariance_matrix;
  ASSERT_TRUE(estimator.computeProjectedCovarianceRVV(normal, covariance_matrix));
  estimator.deinitCompute();

  const moi::Covariance6 expected =
      computeProjectedCovarianceReference(*cloud, *indices, estimator.mean_value_, normal);

  EXPECT_NEAR(covariance_matrix(0, 0), expected[0], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(0, 1), expected[1], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(0, 2), expected[2], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(1, 1), expected[3], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(1, 2), expected[4], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(2, 2), expected[5], 6.0e-3f);
}

TEST(MomentOfInertiaProductionRVV, PointXYZRGBProjectedCovarianceHelperHitsProductionRVVPath)
{
  const auto cloud = makeTypedPointCloud<pcl::PointXYZRGB>(191);
  const auto indices = makePclIndices();
  const Eigen::Vector3f normal = Eigen::Vector3f(-0.35f, 0.4f, 0.84f).normalized();

  pcl::MomentOfInertiaEstimation<pcl::PointXYZRGB> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);

  ASSERT_TRUE(estimator.initCompute());
  estimator.computeMeanValue();
  Eigen::Matrix<float, 3, 3> covariance_matrix;
  ASSERT_TRUE(estimator.computeProjectedCovarianceRVV(normal, covariance_matrix));
  estimator.deinitCompute();

  const moi::Covariance6 expected =
      computeProjectedCovarianceReference(*cloud, *indices, estimator.mean_value_, normal);

  EXPECT_NEAR(covariance_matrix(0, 0), expected[0], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(0, 1), expected[1], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(0, 2), expected[2], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(1, 1), expected[3], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(1, 2), expected[4], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(2, 2), expected[5], 6.0e-3f);
}

TEST(MomentOfInertiaProductionRVV, PointXYZRGBAProjectedCovarianceHelperHitsProductionRVVPath)
{
  const auto cloud = makeTypedPointCloud<pcl::PointXYZRGBA>(191);
  const auto indices = makePclIndices();
  const Eigen::Vector3f normal = Eigen::Vector3f(-0.35f, 0.4f, 0.84f).normalized();

  pcl::MomentOfInertiaEstimation<pcl::PointXYZRGBA> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);

  ASSERT_TRUE(estimator.initCompute());
  estimator.computeMeanValue();
  Eigen::Matrix<float, 3, 3> covariance_matrix;
  ASSERT_TRUE(estimator.computeProjectedCovarianceRVV(normal, covariance_matrix));
  estimator.deinitCompute();

  const moi::Covariance6 expected =
      computeProjectedCovarianceReference(*cloud, *indices, estimator.mean_value_, normal);

  EXPECT_NEAR(covariance_matrix(0, 0), expected[0], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(0, 1), expected[1], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(0, 2), expected[2], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(1, 1), expected[3], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(1, 2), expected[4], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(2, 2), expected[5], 6.0e-3f);
}

TEST(MomentOfInertiaProductionRVV, PointXYZRGBNormalProjectedCovarianceHelperHitsProductionRVVPath)
{
  const auto cloud = makeTypedPointCloud<pcl::PointXYZRGBNormal>(191);
  const auto indices = makePclIndices();
  const Eigen::Vector3f normal = Eigen::Vector3f(-0.35f, 0.4f, 0.84f).normalized();

  pcl::MomentOfInertiaEstimation<pcl::PointXYZRGBNormal> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);

  ASSERT_TRUE(estimator.initCompute());
  estimator.computeMeanValue();
  Eigen::Matrix<float, 3, 3> covariance_matrix;
  ASSERT_TRUE(estimator.computeProjectedCovarianceRVV(normal, covariance_matrix));
  estimator.deinitCompute();

  const moi::Covariance6 expected =
      computeProjectedCovarianceReference(*cloud, *indices, estimator.mean_value_, normal);

  EXPECT_NEAR(covariance_matrix(0, 0), expected[0], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(0, 1), expected[1], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(0, 2), expected[2], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(1, 1), expected[3], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(1, 2), expected[4], 6.0e-3f);
  EXPECT_NEAR(covariance_matrix(2, 2), expected[5], 6.0e-3f);
}
#endif
