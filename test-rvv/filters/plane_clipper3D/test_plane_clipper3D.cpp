#include <pcl/filters/plane_clipper3D.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <vector>

namespace {

using PointCloudXYZ = pcl::PointCloud<pcl::PointXYZ>;

PointCloudXYZ
makeSmallCloud()
{
  PointCloudXYZ cloud;
  cloud.width = 8;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(cloud.width);

  cloud[0] = pcl::PointXYZ(-1.0f, 0.0f, 0.0f);
  cloud[1] = pcl::PointXYZ(-0.25f, 0.5f, 0.0f);
  cloud[2] = pcl::PointXYZ(0.0f, -0.5f, 0.0f);
  cloud[3] = pcl::PointXYZ(0.25f, 0.0f, 0.5f);
  cloud[4] = pcl::PointXYZ(0.5f, 0.5f, 0.5f);
  cloud[5] = pcl::PointXYZ(1.0f, -0.5f, 0.0f);
  cloud[6] = pcl::PointXYZ(-0.75f, -0.5f, 1.0f);
  cloud[7] = pcl::PointXYZ(0.75f, 0.25f, -1.0f);
  return cloud;
}

PointCloudXYZ
makeLargeCloud(std::size_t n)
{
  PointCloudXYZ cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    const float x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 1600.0f;
    const float y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 1700.0f;
    const float z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 1800.0f;
    cloud[i] = pcl::PointXYZ(x, y, z);
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZI>
makeLargeCloudXYZI(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZI> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 257) - 128) / 100.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 5) % 263) - 131) / 110.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 11) % 269) - 134) / 120.0f;
    cloud[i].intensity = static_cast<float>(i % 17);
  }
  return cloud;
}

template <typename PointT>
pcl::Indices
expectedPlane(const pcl::PointCloud<PointT>& cloud, const pcl::Indices* subset, const Eigen::Vector4f& plane)
{
  pcl::Indices expected;
  const std::size_t n = subset ? subset->size() : cloud.size();
  expected.reserve(n);
  for (std::size_t pos = 0; pos < n; ++pos) {
    const int index = subset ? (*subset)[pos] : static_cast<int>(pos);
    const auto& p = cloud[index];
    if (plane[0] * p.x + plane[1] * p.y + plane[2] * p.z >= -plane[3])
      expected.push_back(index);
  }
  return expected;
}

} // namespace

TEST(PlaneClipper3DRVV, SmallCloudMatchesScalarFormula)
{
  const auto cloud = makeSmallCloud();
  const Eigen::Vector4f plane(1.0f, -0.5f, 0.25f, -0.1f);
  pcl::PlaneClipper3D<pcl::PointXYZ> clipper(plane);

  pcl::Indices clipped;
  clipper.clipPointCloud3D(cloud, clipped);

  EXPECT_EQ(clipped, expectedPlane(cloud, nullptr, plane));
}

TEST(PlaneClipper3DRVV, LargeDensePointXYZKeepsOrder)
{
  const auto cloud = makeLargeCloud(513);
  const Eigen::Vector4f plane(0.75f, -0.5f, 0.35f, -0.15f);
  pcl::PlaneClipper3D<pcl::PointXYZ> clipper(plane);

  pcl::Indices clipped;
  clipper.clipPointCloud3D(cloud, clipped);

  EXPECT_EQ(clipped, expectedPlane(cloud, nullptr, plane));
}

TEST(PlaneClipper3DRVV, AppendsToExistingOutput)
{
  const auto cloud = makeLargeCloud(257);
  const Eigen::Vector4f plane(0.5f, 0.25f, -0.75f, 0.05f);
  pcl::PlaneClipper3D<pcl::PointXYZ> clipper(plane);

  pcl::Indices clipped = {42, 99};
  clipper.clipPointCloud3D(cloud, clipped);

  auto expected = expectedPlane(cloud, nullptr, plane);
  expected.insert(expected.begin(), {42, 99});
  EXPECT_EQ(clipped, expected);
}

TEST(PlaneClipper3DRVV, ExplicitIndicesFallbackPreservesSubsetValues)
{
  const auto cloud = makeLargeCloud(257);
  const Eigen::Vector4f plane(0.75f, -0.5f, 0.35f, -0.15f);
  const pcl::Indices subset = {3, 5, 7, 97, 111, 131, 173, 199, 201};
  pcl::PlaneClipper3D<pcl::PointXYZ> clipper(plane);

  pcl::Indices clipped;
  clipper.clipPointCloud3D(cloud, clipped, subset);

  EXPECT_EQ(clipped, expectedPlane(cloud, &subset, plane));
}

TEST(PlaneClipper3DRVV, PointXYZIFallbackPreservesGenericSemantics)
{
  const auto cloud = makeLargeCloudXYZI(257);
  const Eigen::Vector4f plane(0.25f, 0.75f, -0.5f, 0.2f);
  pcl::PlaneClipper3D<pcl::PointXYZI> clipper(plane);

  pcl::Indices clipped;
  clipper.clipPointCloud3D(cloud, clipped);

  EXPECT_EQ(clipped, expectedPlane(cloud, nullptr, plane));
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
