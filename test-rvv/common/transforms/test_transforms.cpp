/*
 * PCL transforms.hpp RVV-focused unit tests.
 */

#include <pcl/test/gtest.h>
#include <pcl/pcl_tests.h>

#include <pcl/common/transforms.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <cstdint>
#include <cmath>
#include <limits>
#include <random>

namespace {

Eigen::Matrix4f
makeTransform()
{
  Eigen::Affine3f t = Eigen::Affine3f::Identity();
  t.translation() = Eigen::Vector3f(0.25f, -1.5f, 3.0f);
  t.linear() =
      (Eigen::AngleAxisf(0.31f, Eigen::Vector3f::UnitX()) *
       Eigen::AngleAxisf(-0.17f, Eigen::Vector3f::UnitY()) *
       Eigen::AngleAxisf(0.09f, Eigen::Vector3f::UnitZ()))
          .toRotationMatrix();
  return t.matrix();
}

void
fillXYZ(pcl::PointCloud<pcl::PointXYZ>& cloud, std::size_t n, std::uint32_t seed)
{
  cloud.clear();
  cloud.resize(n);
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;

  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> u(-20.0f, 20.0f);
  for (auto& p : cloud) {
    p.x = u(rng);
    p.y = u(rng);
    p.z = u(rng);
  }
}

void
fillXYZRGBNormal(pcl::PointCloud<pcl::PointXYZRGBNormal>& cloud,
                 std::size_t n,
                 std::uint32_t seed)
{
  cloud.clear();
  cloud.resize(n);
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;

  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> u(-20.0f, 20.0f);
  std::uniform_int_distribution<int> color(0, 255);
  for (auto& p : cloud) {
    p.x = u(rng);
    p.y = u(rng);
    p.z = u(rng);
    Eigen::Vector3f normal(u(rng), u(rng), u(rng));
    normal.normalize();
    p.normal_x = normal.x();
    p.normal_y = normal.y();
    p.normal_z = normal.z();
    p.r = static_cast<std::uint8_t>(color(rng));
    p.g = static_cast<std::uint8_t>(color(rng));
    p.b = static_cast<std::uint8_t>(color(rng));
  }
}

void
expectXYZNear(const pcl::PointXYZ& p, const Eigen::Vector4f& expected, float eps)
{
  EXPECT_NEAR(p.x, expected.x(), eps);
  EXPECT_NEAR(p.y, expected.y(), eps);
  EXPECT_NEAR(p.z, expected.z(), eps);
}

void
expectXYZNormalNear(const pcl::PointXYZRGBNormal& p,
                    const Eigen::Vector4f& expected_xyz,
                    const Eigen::Vector3f& expected_normal,
                    float eps)
{
  EXPECT_NEAR(p.x, expected_xyz.x(), eps);
  EXPECT_NEAR(p.y, expected_xyz.y(), eps);
  EXPECT_NEAR(p.z, expected_xyz.z(), eps);
  EXPECT_NEAR(p.normal_x, expected_normal.x(), eps);
  EXPECT_NEAR(p.normal_y, expected_normal.y(), eps);
  EXPECT_NEAR(p.normal_z, expected_normal.z(), eps);
}

} // namespace

TEST(PCL, TransformPointCloudXYZDenseLarge)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  pcl::PointCloud<pcl::PointXYZ> out;
  fillXYZ(cloud, 257, 42);
  const Eigen::Matrix4f transform = makeTransform();

  pcl::transformPointCloud(cloud, out, transform, true);

  ASSERT_EQ(out.size(), cloud.size());
  ASSERT_EQ(out.width, cloud.width);
  ASSERT_EQ(out.height, cloud.height);
  ASSERT_TRUE(out.is_dense);
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const Eigen::Vector4f src(cloud[i].x, cloud[i].y, cloud[i].z, 1.0f);
    expectXYZNear(out[i], transform * src, 1e-4f);
  }
}

TEST(PCL, TransformPointCloudXYZDenseInPlace)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  fillXYZ(cloud, 257, 43);
  const auto original = cloud;
  const Eigen::Matrix4f transform = makeTransform();

  pcl::transformPointCloud(cloud, cloud, transform, true);

  ASSERT_EQ(cloud.size(), original.size());
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const Eigen::Vector4f src(original[i].x, original[i].y, original[i].z, 1.0f);
    expectXYZNear(cloud[i], transform * src, 1e-4f);
  }
}

TEST(PCL, TransformPointCloudXYZSparseKeepsInvalidScalarPath)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  pcl::PointCloud<pcl::PointXYZ> out;
  fillXYZ(cloud, 32, 44);
  cloud.is_dense = false;
  cloud[3].x = std::numeric_limits<float>::quiet_NaN();
  const Eigen::Matrix4f transform = makeTransform();

  pcl::transformPointCloud(cloud, out, transform, true);

  ASSERT_FALSE(out.is_dense);
  ASSERT_TRUE(std::isnan(out[3].x));
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    if (i == 3)
      continue;
    const Eigen::Vector4f src(cloud[i].x, cloud[i].y, cloud[i].z, 1.0f);
    expectXYZNear(out[i], transform * src, 1e-4f);
  }
}

TEST(PCL, TransformPointCloudWithNormalsDenseLarge)
{
  pcl::PointCloud<pcl::PointXYZRGBNormal> cloud;
  pcl::PointCloud<pcl::PointXYZRGBNormal> out;
  fillXYZRGBNormal(cloud, 257, 45);
  const Eigen::Matrix4f transform = makeTransform();

  pcl::transformPointCloudWithNormals(cloud, out, transform, true);

  ASSERT_EQ(out.size(), cloud.size());
  ASSERT_EQ(out.width, cloud.width);
  ASSERT_EQ(out.height, cloud.height);
  ASSERT_TRUE(out.is_dense);
  const Eigen::Matrix3f rot = transform.template block<3, 3>(0, 0);
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const Eigen::Vector4f src(cloud[i].x, cloud[i].y, cloud[i].z, 1.0f);
    const Eigen::Vector3f normal(cloud[i].normal_x, cloud[i].normal_y, cloud[i].normal_z);
    expectXYZNormalNear(out[i], transform * src, rot * normal, 1e-4f);
    EXPECT_EQ(out[i].rgba, cloud[i].rgba);
  }
}

TEST(PCL, TransformPointCloudWithNormalsDenseNoCopy)
{
  pcl::PointCloud<pcl::PointXYZRGBNormal> cloud;
  pcl::PointCloud<pcl::PointXYZRGBNormal> out;
  fillXYZRGBNormal(cloud, 257, 46);
  const Eigen::Matrix4f transform = makeTransform();

  pcl::transformPointCloudWithNormals(cloud, out, transform, false);

  ASSERT_EQ(out.size(), cloud.size());
  const Eigen::Matrix3f rot = transform.template block<3, 3>(0, 0);
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const Eigen::Vector4f src(cloud[i].x, cloud[i].y, cloud[i].z, 1.0f);
    const Eigen::Vector3f normal(cloud[i].normal_x, cloud[i].normal_y, cloud[i].normal_z);
    expectXYZNormalNear(out[i], transform * src, rot * normal, 1e-4f);
  }
}

TEST(PCL, TransformPointCloudIndexedRemainsCovered)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  pcl::PointCloud<pcl::PointXYZ> out;
  fillXYZ(cloud, 64, 47);
  pcl::Indices indices;
  for (std::size_t i = 0; i < cloud.size(); i += 2)
    indices.push_back(static_cast<int>(i));
  const Eigen::Matrix4f transform = makeTransform();

  pcl::transformPointCloud(cloud, indices, out, transform, true);

  ASSERT_EQ(out.size(), indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i) {
    const auto& src_point = cloud[indices[i]];
    const Eigen::Vector4f src(src_point.x, src_point.y, src_point.z, 1.0f);
    expectXYZNear(out[i], transform * src, 1e-4f);
  }
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}