#include <pcl/filters/frustum_culling.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>

namespace {

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCloud(std::size_t n, bool dense = true)
{
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = dense;
  cloud->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    const float x = static_cast<float>(static_cast<int>(i % 4099) - 1024) / 220.0f;
    const float y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 500.0f;
    const float z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 520.0f;
    (*cloud)[i] = pcl::PointXYZ(x, y, z);
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZI>::Ptr
makeCloudXYZI(std::size_t n)
{
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 257) - 64) / 60.0f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 5) % 263) - 131) / 90.0f;
    (*cloud)[i].z = static_cast<float>(static_cast<int>((i * 11) % 269) - 134) / 95.0f;
    (*cloud)[i].intensity = static_cast<float>(i % 17);
  }
  return cloud;
}

pcl::IndicesPtr
makeAllIndices(std::size_t n)
{
  auto indices = std::make_shared<pcl::Indices>();
  indices->resize(n);
  for (std::size_t i = 0; i < n; ++i)
    (*indices)[i] = static_cast<int>(i);
  return indices;
}

template <typename PointT>
pcl::FrustumCulling<PointT>
configuredFrustum(bool extract_removed_indices = false)
{
  pcl::FrustumCulling<PointT> fc(extract_removed_indices);
  fc.setVerticalFOV(76.0f);
  fc.setHorizontalFOV(68.0f);
  fc.setNearPlaneDistance(0.05f);
  fc.setFarPlaneDistance(6.0f);
  fc.setRegionOfInterest(0.52f, 0.48f, 0.72f, 0.66f);

  Eigen::Matrix4f camera_pose = Eigen::Matrix4f::Identity();
  camera_pose.block<3, 1>(0, 3) = Eigen::Vector3f(-1.25f, 0.15f, -0.20f);
  fc.setCameraPose(camera_pose);
  return fc;
}

pcl::Indices
filterFullCloud(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud, bool negative = false)
{
  auto fc = configuredFrustum<pcl::PointXYZ>();
  fc.setInputCloud(cloud);
  fc.setNegative(negative);
  pcl::Indices out;
  fc.filter(out);
  return out;
}

pcl::Indices
filterExplicitAll(const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud, bool negative = false)
{
  auto fc = configuredFrustum<pcl::PointXYZ>();
  fc.setInputCloud(cloud);
  fc.setIndices(makeAllIndices(cloud->size()));
  fc.setNegative(negative);
  pcl::Indices out;
  fc.filter(out);
  return out;
}

} // namespace

TEST(FrustumCullingRVV, SmallCloudFallsBackAndMatchesExplicitAll)
{
  const auto cloud = makeCloud(31);
  EXPECT_EQ(filterFullCloud(cloud), filterExplicitAll(cloud));
}

TEST(FrustumCullingRVV, LargeDensePointXYZMatchesExplicitAll)
{
  const auto cloud = makeCloud(4097);
  EXPECT_EQ(filterFullCloud(cloud), filterExplicitAll(cloud));
}

TEST(FrustumCullingRVV, NegativeMatchesExplicitAll)
{
  const auto cloud = makeCloud(4097);
  EXPECT_EQ(filterFullCloud(cloud, true), filterExplicitAll(cloud, true));
}

TEST(FrustumCullingRVV, ExtractRemovedIndicesMatchesExplicitAll)
{
  const auto cloud = makeCloud(4097);
  auto fc_rvv = configuredFrustum<pcl::PointXYZ>(true);
  fc_rvv.setInputCloud(cloud);
  pcl::Indices rvv_indices;
  fc_rvv.filter(rvv_indices);

  auto fc_std = configuredFrustum<pcl::PointXYZ>(true);
  fc_std.setInputCloud(cloud);
  fc_std.setIndices(makeAllIndices(cloud->size()));
  pcl::Indices std_indices;
  fc_std.filter(std_indices);

  EXPECT_EQ(rvv_indices, std_indices);
  EXPECT_EQ(*fc_rvv.getRemovedIndices(), *fc_std.getRemovedIndices());
}

TEST(FrustumCullingRVV, FarPlaneInfinityMatchesExplicitAll)
{
  const auto cloud = makeCloud(4097);
  auto fc_rvv = configuredFrustum<pcl::PointXYZ>();
  fc_rvv.setInputCloud(cloud);
  fc_rvv.setFarPlaneDistance(std::numeric_limits<float>::max());
  pcl::Indices rvv_indices;
  fc_rvv.filter(rvv_indices);

  auto fc_std = configuredFrustum<pcl::PointXYZ>();
  fc_std.setInputCloud(cloud);
  fc_std.setIndices(makeAllIndices(cloud->size()));
  fc_std.setFarPlaneDistance(std::numeric_limits<float>::max());
  pcl::Indices std_indices;
  fc_std.filter(std_indices);

  EXPECT_EQ(rvv_indices, std_indices);
}

TEST(FrustumCullingRVV, NonDenseFlagFallbackPreservesScalarSemantics)
{
  const auto cloud = makeCloud(4097, false);
  EXPECT_EQ(filterFullCloud(cloud), filterExplicitAll(cloud));
}

TEST(FrustumCullingRVV, LargeDensePointXYZIMatchesExplicitAll)
{
  const auto cloud = makeCloudXYZI(257);
  auto fc = configuredFrustum<pcl::PointXYZI>();
  fc.setInputCloud(cloud);
  pcl::Indices full_indices;
  fc.filter(full_indices);

  fc.setIndices(makeAllIndices(cloud->size()));
  pcl::Indices explicit_indices;
  fc.filter(explicit_indices);
  EXPECT_EQ(full_indices, explicit_indices);
}

int
main(int argc, char** argv)
{
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
