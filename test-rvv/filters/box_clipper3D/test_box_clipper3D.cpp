#include <pcl/filters/box_clipper3D.h>
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

  cloud[0] = pcl::PointXYZ(0.0f, 0.0f, 0.0f);
  cloud[1] = pcl::PointXYZ(0.9f, 0.9f, 0.9f);
  cloud[2] = pcl::PointXYZ(1.1f, 0.0f, 0.0f);
  cloud[3] = pcl::PointXYZ(0.0f, -1.2f, 0.0f);
  cloud[4] = pcl::PointXYZ(-0.75f, 0.25f, 0.8f);
  cloud[5] = pcl::PointXYZ(0.2f, 0.4f, -1.3f);
  cloud[6] = pcl::PointXYZ(-0.95f, -0.95f, -0.95f);
  cloud[7] = pcl::PointXYZ(1.0f, 1.0f, 1.0f);
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
    const float x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 1400.0f;
    const float y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 1500.0f;
    const float z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 1600.0f;
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
    cloud[i].x = static_cast<float>(static_cast<int>(i % 257) - 128) / 95.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 5) % 263) - 131) / 105.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 11) % 269) - 134) / 115.0f;
    cloud[i].intensity = static_cast<float>(i % 17);
  }
  return cloud;
}

Eigen::Affine3f
makeAffineBox()
{
  Eigen::Affine3f transform = Eigen::Affine3f::Identity();
  transform.translate(Eigen::Vector3f(0.15f, -0.08f, 0.05f));
  transform.rotate(Eigen::AngleAxisf(0.17f, Eigen::Vector3f::UnitZ()));
  transform.scale(Eigen::Vector3f(0.82f, 1.10f, 0.95f));
  return transform;
}

template <typename PointT>
pcl::Indices
expectedBox(const pcl::PointCloud<PointT>& cloud, const pcl::Indices* subset, const Eigen::Affine3f& transform)
{
  pcl::Indices expected;
  const std::size_t n = subset ? subset->size() : cloud.size();
  expected.reserve(n);
  for (std::size_t pos = 0; pos < n; ++pos) {
    const int index = subset ? (*subset)[pos] : static_cast<int>(pos);
    const Eigen::Vector4f point = transform.matrix() * cloud[index].getVector4fMap();
    if ((point.array().abs() <= 1.0f).all())
      expected.push_back(index);
  }
  return expected;
}

} // namespace

TEST(BoxClipper3DRVV, SmallCloudMatchesScalarFormula)
{
  const auto cloud = makeSmallCloud();
  const Eigen::Affine3f transform = Eigen::Affine3f::Identity();
  pcl::BoxClipper3D<pcl::PointXYZ> clipper(transform);

  pcl::Indices clipped;
  clipper.clipPointCloud3D(cloud, clipped);

  EXPECT_EQ(clipped, expectedBox(cloud, nullptr, transform));
}

TEST(BoxClipper3DRVV, LargeDensePointXYZKeepsOrder)
{
  const auto cloud = makeLargeCloud(513);
  const Eigen::Affine3f transform = makeAffineBox();
  pcl::BoxClipper3D<pcl::PointXYZ> clipper(transform);

  pcl::Indices clipped;
  clipper.clipPointCloud3D(cloud, clipped);

  EXPECT_EQ(clipped, expectedBox(cloud, nullptr, transform));
}

TEST(BoxClipper3DRVV, ClearsExistingOutput)
{
  const auto cloud = makeLargeCloud(257);
  const Eigen::Affine3f transform = makeAffineBox();
  pcl::BoxClipper3D<pcl::PointXYZ> clipper(transform);

  pcl::Indices clipped = {42, 99};
  clipper.clipPointCloud3D(cloud, clipped);

  EXPECT_EQ(clipped, expectedBox(cloud, nullptr, transform));
}

TEST(BoxClipper3DRVV, ExplicitIndicesFallbackPreservesSubsetValues)
{
  const auto cloud = makeLargeCloud(257);
  const Eigen::Affine3f transform = makeAffineBox();
  const pcl::Indices subset = {3, 5, 7, 97, 111, 131, 173, 199, 201};
  pcl::BoxClipper3D<pcl::PointXYZ> clipper(transform);

  pcl::Indices clipped;
  clipper.clipPointCloud3D(cloud, clipped, subset);

  EXPECT_EQ(clipped, expectedBox(cloud, &subset, transform));
}

TEST(BoxClipper3DRVV, NonFiniteValuesMatchScalarComparison)
{
  auto cloud = makeLargeCloud(257);
  cloud[64].x = std::numeric_limits<float>::quiet_NaN();
  cloud[128].y = std::numeric_limits<float>::infinity();
  cloud[192].z = -std::numeric_limits<float>::infinity();
  const Eigen::Affine3f transform = Eigen::Affine3f::Identity();
  pcl::BoxClipper3D<pcl::PointXYZ> clipper(transform);

  pcl::Indices clipped;
  clipper.clipPointCloud3D(cloud, clipped);

  EXPECT_EQ(clipped, expectedBox(cloud, nullptr, transform));
}

TEST(BoxClipper3DRVV, PointXYZIFallbackPreservesGenericSemantics)
{
  const auto cloud = makeLargeCloudXYZI(257);
  const Eigen::Affine3f transform = makeAffineBox();
  pcl::BoxClipper3D<pcl::PointXYZI> clipper(transform);

  pcl::Indices clipped;
  clipper.clipPointCloud3D(cloud, clipped);

  EXPECT_EQ(clipped, expectedBox(cloud, nullptr, transform));
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
