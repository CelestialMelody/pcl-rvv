/*
 * normal_3d.hpp RVV topic correctness tests.
 *
 * 本文件验证 normal_3d 公开入口和 computePointNormal helper 的基础语义。它不证明
 * production dispatch（生产分流）已经接入 normal_3d.hpp；当前 RVV 证据来自
 * common 层 computeMeanAndCovarianceMatrix 的 __RVV10__ 路径。
 */

#include <pcl/test/gtest.h>
#include <pcl/features/normal_3d.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <Eigen/Core>

#include <cmath>
#include <limits>

namespace {

using Cloud = pcl::PointCloud<pcl::PointXYZ>;

Cloud::Ptr
makePlaneCloud(const int width = 8, const int height = 8)
{
  auto cloud = Cloud::Ptr(new Cloud);
  cloud->reserve(static_cast<std::size_t>(width * height));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float fx = static_cast<float>(x) * 0.25f - 0.75f;
      const float fy = static_cast<float>(y) * 0.20f - 0.70f;
      const float fz = 0.5f * fx - 0.25f * fy + 2.0f;
      cloud->push_back(pcl::PointXYZ(fx, fy, fz));
    }
  }
  cloud->width = static_cast<std::uint32_t>(cloud->size());
  cloud->height = 1;
  cloud->is_dense = true;
  return cloud;
}

pcl::Indices
makeAllIndices(const Cloud& cloud)
{
  pcl::Indices indices(cloud.size());
  for (std::size_t i = 0; i < indices.size(); ++i)
    indices[i] = static_cast<pcl::index_t>(i);
  return indices;
}

Eigen::Vector3f
expectedPlaneNormal()
{
  Eigen::Vector3f normal(-0.5f, 0.25f, 1.0f);
  normal.normalize();
  return normal;
}

void
expectNormalParallelToPlane(const float nx, const float ny, const float nz)
{
  const Eigen::Vector3f normal(nx, ny, nz);
  ASSERT_NEAR(normal.norm(), 1.0f, 1e-4f);
  EXPECT_GT(std::abs(normal.dot(expectedPlaneNormal())), 0.999f);
}

} // namespace

TEST(Normal3D, ComputePointNormalIndexedDenseNeighborhood)
{
  const auto cloud = makePlaneCloud();
  const pcl::Indices indices = makeAllIndices(*cloud);

  Eigen::Vector4f plane_parameters;
  float curvature = std::numeric_limits<float>::quiet_NaN();
  ASSERT_TRUE(pcl::computePointNormal(*cloud, indices, plane_parameters, curvature));

  expectNormalParallelToPlane(plane_parameters[0], plane_parameters[1], plane_parameters[2]);
  EXPECT_NEAR(curvature, 0.0f, 1e-5f);
}

TEST(Normal3D, ComputePointNormalRejectsTooSmallNeighborhood)
{
  const auto cloud = makePlaneCloud();
  const pcl::Indices two_points{0, 1};

  Eigen::Vector4f plane_parameters;
  float curvature = 0.0f;
  EXPECT_FALSE(pcl::computePointNormal(*cloud, two_points, plane_parameters, curvature));
  EXPECT_TRUE(std::isnan(curvature));
  for (int i = 0; i < 4; ++i)
    EXPECT_TRUE(std::isnan(plane_parameters[i]));
}

TEST(Normal3D, PublicNormalEstimationMatchesPlane)
{
  const auto cloud = makePlaneCloud();
  pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimation;
  normal_estimation.setInputCloud(cloud);
  normal_estimation.setSearchMethod(pcl::search::KdTree<pcl::PointXYZ>::Ptr(new pcl::search::KdTree<pcl::PointXYZ>));
  normal_estimation.setKSearch(16);

  pcl::PointCloud<pcl::Normal> normals;
  normal_estimation.compute(normals);

  ASSERT_EQ(normals.size(), cloud->size());
  ASSERT_TRUE(normals.is_dense);
  for (const auto& normal : normals) {
    expectNormalParallelToPlane(normal.normal_x, normal.normal_y, normal.normal_z);
    EXPECT_NEAR(normal.curvature, 0.0f, 2e-4f);
  }
}

TEST(Normal3D, PublicNormalEstimationMarksInvalidQueryAsNonDense)
{
  auto cloud = makePlaneCloud();
  (*cloud)[3].x = std::numeric_limits<float>::quiet_NaN();
  cloud->is_dense = false;

  pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimation;
  normal_estimation.setInputCloud(cloud);
  normal_estimation.setSearchMethod(pcl::search::KdTree<pcl::PointXYZ>::Ptr(new pcl::search::KdTree<pcl::PointXYZ>));
  normal_estimation.setKSearch(16);

  pcl::PointCloud<pcl::Normal> normals;
  normal_estimation.compute(normals);

  ASSERT_EQ(normals.size(), cloud->size());
  EXPECT_FALSE(normals.is_dense);
  EXPECT_TRUE(std::isnan(normals[3].normal_x));
  EXPECT_TRUE(std::isnan(normals[3].normal_y));
  EXPECT_TRUE(std::isnan(normals[3].normal_z));
  EXPECT_TRUE(std::isnan(normals[3].curvature));
}
