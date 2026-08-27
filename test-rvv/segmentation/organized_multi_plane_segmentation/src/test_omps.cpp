#include "omps.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

pcl::PointCloud<pcl::PointXYZ>
makePlaneDCloud()
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = 4;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.push_back(pcl::PointXYZ(1.0f, 2.0f, 3.0f));
  cloud.push_back(pcl::PointXYZ(-2.0f, 0.5f, 4.0f));
  cloud.push_back(pcl::PointXYZ(0.25f, -1.0f, 2.0f));
  cloud.push_back(pcl::PointXYZ(3.0f, 2.0f, -1.0f));
  return cloud;
}

pcl::PointCloud<pcl::Normal>
makePlaneDNormals()
{
  pcl::PointCloud<pcl::Normal> normals;
  normals.width = 4;
  normals.height = 1;
  normals.is_dense = true;
  pcl::Normal n0;
  n0.normal_x = 0.5f;
  n0.normal_y = 0.25f;
  n0.normal_z = -0.5f;
  normals.push_back(n0);
  pcl::Normal n1;
  n1.normal_x = -1.0f;
  n1.normal_y = 2.0f;
  n1.normal_z = 0.125f;
  normals.push_back(n1);
  pcl::Normal n2;
  n2.normal_x = 4.0f;
  n2.normal_y = -0.25f;
  n2.normal_z = 0.5f;
  normals.push_back(n2);
  pcl::Normal n3;
  n3.normal_x = 0.0f;
  n3.normal_y = 1.5f;
  n3.normal_z = 2.0f;
  normals.push_back(n3);
  return normals;
}

pcl::PointCloud<pcl::PointXYZ>
makeBoundaryCloud()
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = 6;
  cloud.height = 1;
  cloud.is_dense = true;
  for (int i = 0; i < 6; ++i)
    cloud.push_back(pcl::PointXYZ(static_cast<float>(i),
                                  static_cast<float>(i * 2),
                                  static_cast<float>(10 - i)));
  return cloud;
}

void
expectNearVector(const std::vector<float>& actual,
                 const std::vector<float>& expected,
                 const float tolerance)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "i=" << i;
}

void
expectNearCloud(const pcl::PointCloud<pcl::PointXYZ>& actual,
                const std::vector<pcl::PointXYZ>& expected,
                const float tolerance)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i) {
    EXPECT_NEAR(actual[i].x, expected[i].x, tolerance) << "x i=" << i;
    EXPECT_NEAR(actual[i].y, expected[i].y, tolerance) << "y i=" << i;
    EXPECT_NEAR(actual[i].z, expected[i].z, tolerance) << "z i=" << i;
  }
}

} // namespace

// 这个测试用手算的四行点积保护 `segment` 里的 plane_d 预处理语义。
// 如果 candidate 漏掉 z、normal 字段顺序错或只返回零，这个 gate（失败会返回非 0 的检查）会失败。
TEST(OrganizedMultiPlaneSegmentationRvvDiagnostic, PlaneDValuesMatchHandCheckedDots)
{
  const auto cloud = makePlaneDCloud();
  const auto normals = makePlaneDNormals();
  const std::vector<float> expected = {-0.5f, 3.5f, 2.25f, 1.0f};

  std::vector<float> scalar;
  const auto scalar_summary =
      pcl_rvv_segmentation_omps::computePlaneDValuesStd(cloud, normals, &scalar);
  expectNearVector(scalar, expected, 1e-6f);
  EXPECT_NEAR(scalar_summary.checksum, 17.25, 1e-6);

#if defined(__RVV10__)
  std::vector<float> actual;
  const auto actual_summary =
      pcl_rvv_segmentation_omps::computePlaneDValuesRVV(cloud, normals, &actual);
  expectNearVector(actual, expected, 1e-6f);
  EXPECT_NEAR(actual_summary.checksum, scalar_summary.checksum, 1e-6);
#endif
}

// 这个测试把 boundary indices（边界索引）写成非单调顺序，保护 PlanarRegion
// 输出前的 gather（离散加载）语义：复制后的 boundary cloud 必须保持索引给定顺序。
TEST(OrganizedMultiPlaneSegmentationRvvDiagnostic, BoundaryGatherPreservesIndexOrder)
{
  const auto cloud = makeBoundaryCloud();
  const std::vector<int> boundary_indices = {4, 1, 5, 0};
  const std::vector<pcl::PointXYZ> expected = {
      pcl::PointXYZ(4.0f, 8.0f, 6.0f),
      pcl::PointXYZ(1.0f, 2.0f, 9.0f),
      pcl::PointXYZ(5.0f, 10.0f, 5.0f),
      pcl::PointXYZ(0.0f, 0.0f, 10.0f),
  };

  pcl::PointCloud<pcl::PointXYZ> scalar;
  const auto scalar_summary =
      pcl_rvv_segmentation_omps::gatherBoundaryCloudStd(cloud, boundary_indices, &scalar);
  expectNearCloud(scalar, expected, 1e-6f);
  EXPECT_NEAR(scalar_summary.checksum, 222.0, 1e-6);

#if defined(__RVV10__)
  pcl::PointCloud<pcl::PointXYZ> actual;
  const auto actual_summary =
      pcl_rvv_segmentation_omps::gatherBoundaryCloudRVV(cloud, boundary_indices, &actual);
  expectNearCloud(actual, expected, 1e-6f);
  EXPECT_NEAR(actual_summary.checksum, scalar_summary.checksum, 1e-6);
#endif
}

// 这个测试覆盖 viewpoint projection（视点投影）的非平凡比例 u。期望值来自
// 平面 z=2、视点原点和输入 z={4,6,8} 的手算交点，保护 RVV 公式不能只复制原点。
TEST(OrganizedMultiPlaneSegmentationRvvDiagnostic, ProjectionMatchesHandCheckedIntersections)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = 3;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.push_back(pcl::PointXYZ(2.0f, 0.0f, 4.0f));
  cloud.push_back(pcl::PointXYZ(0.0f, 3.0f, 6.0f));
  cloud.push_back(pcl::PointXYZ(-4.0f, 2.0f, 8.0f));

  const Eigen::Vector4f normal(0.0f, 0.0f, 1.0f, -2.0f);
  const Eigen::Vector3f centroid(0.0f, 0.0f, 2.0f);
  const Eigen::Vector3f viewpoint(0.0f, 0.0f, 0.0f);
  const std::vector<pcl::PointXYZ> expected = {
      pcl::PointXYZ(1.0f, 0.0f, 2.0f),
      pcl::PointXYZ(0.0f, 1.0f, 2.0f),
      pcl::PointXYZ(-1.0f, 0.5f, 2.0f),
  };

  pcl::PointCloud<pcl::PointXYZ> scalar;
  const auto scalar_summary =
      pcl_rvv_segmentation_omps::projectBoundaryFromViewpointStd(
          cloud, normal, centroid, viewpoint, &scalar);
  expectNearCloud(scalar, expected, 1e-6f);

#if defined(__RVV10__)
  pcl::PointCloud<pcl::PointXYZ> actual;
  const auto actual_summary =
      pcl_rvv_segmentation_omps::projectBoundaryFromViewpointRVV(
          cloud, normal, centroid, viewpoint, &actual);
  expectNearCloud(actual, expected, 1e-5f);
  EXPECT_NEAR(actual_summary.checksum, scalar_summary.checksum, 1e-5);
#endif
}

// 这个测试模拟 production 末段的 region boundary 输出：每个 region 已经有
// boundary indices（边界索引）、centroid（质心）和 plane model（平面模型），helper
// 负责复制边界点，并在 project_points=true 时执行 viewpoint projection（视点投影）。
// 它不覆盖 boundary discovery（边界查找）或真实 PlanarRegion 构造。
TEST(OrganizedMultiPlaneSegmentationRvvDiagnostic, RegionBoundaryProjectionMatchesScalarShape)
{
  const auto cloud = makeBoundaryCloud();
  const std::vector<pcl_rvv_segmentation_omps::RegionBoundaryInput> regions = {
      {{0, 2, 4}, Eigen::Vector4f(0.0f, 0.0f, 1.0f, -2.0f),
       Eigen::Vector3f(0.0f, 0.0f, 2.0f), 9},
      {{1, 3, 5}, Eigen::Vector4f(0.0f, 0.0f, 1.0f, -4.0f),
       Eigen::Vector3f(0.0f, 0.0f, 4.0f), 7},
  };

  std::vector<pcl::PointCloud<pcl::PointXYZ>> scalar_boundaries;
  const auto scalar_summary = pcl_rvv_segmentation_omps::assembleRegionBoundariesStd(
      cloud, regions, true, &scalar_boundaries);
  ASSERT_EQ(scalar_boundaries.size(), regions.size());
  ASSERT_EQ(scalar_boundaries[0].size(), 3);
  ASSERT_EQ(scalar_boundaries[1].size(), 3);

#if defined(__RVV10__)
  std::vector<pcl::PointCloud<pcl::PointXYZ>> actual_boundaries;
  const auto actual_summary = pcl_rvv_segmentation_omps::assembleRegionBoundariesRVV(
      cloud, regions, true, &actual_boundaries);
  ASSERT_EQ(actual_boundaries.size(), scalar_boundaries.size());
  for (std::size_t i = 0; i < scalar_boundaries.size(); ++i) {
    std::vector<pcl::PointXYZ> expected;
    expected.reserve(scalar_boundaries[i].size());
    for (const auto& point : scalar_boundaries[i])
      expected.push_back(point);
    expectNearCloud(actual_boundaries[i], expected, 1e-5f);
  }
  EXPECT_NEAR(actual_summary.checksum, scalar_summary.checksum, 1e-4);
#endif
}
